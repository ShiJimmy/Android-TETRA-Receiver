/* SPDX-License-Identifier: GPL-3.0-or-later (port of tetra-rtlsdr)
 * Copyright (C) 2026 Shi Jimmy (Android port)
 */
/*
 * π/4-DQPSK demodulator for TETRA with band-edge FLL carrier recovery.
 *
 * Ported from tetra-rtlsdr (CEMAXECUTER LLC, GPL-3.0-or-later); the only
 * functional change is that decoded bits are handed to a sink callback
 * instead of being written to a file descriptor.
 *
 * DSP chain:
 *   raw uint8 IQ (1.8 MSps)
 *     �?float complex
 *     �?nco_crcf           (fixed IF offset to avoid RTL-SDR DC spike)
 *     �?firdecim_crcf      (50× �?36 kSps Kaiser LPF + decimation)
 *     �?feedforward AGC     (amplitude normalization �?MUST precede FLL)
 *     �?band-edge FLL      (carrier recovery, 2nd-order PLL at 36 kSps)
 *     �?symsync_crcf       (RRC timing recovery, sps=2)
 *     �?π/4-DQPSK differential decode �?2 bits/symbol
 */

#include "dqpsk_demod.h"

#include <liquid.h>
#include <stdlib.h>
#include <string.h>
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <complex.h>

#define TETRA_CHANNEL_RATE  36000u
#define TETRA_SYMBOL_RATE   18000u
#define TETRA_SPS           2
#define TETRA_ROLLOFF       0.35f

#define SYMSYNC_NFILTS      32
#define SYMSYNC_M           5

#define BITBUF_SIZE         4096

#define FLL_FILTER_SIZE 45
#define FLL_BW (M_PI / 100.0)

#define FF_AGC_WINDOW 8

static float sincf_gr(float x) {
    float arg = (float)M_PI * x;
    return (x == 0.0f) ? 1.0f : sinf(arg) / arg;
}

struct dqpsk_demod {
    firdecim_crcf   decim;
    unsigned int    decim_factor;
    uint32_t        sample_rate;

    nco_crcf        nco;

    firfilt_cccf    fll_upper;
    firfilt_cccf    fll_lower;
    float           fll_phase;
    float           fll_freq;
    float           fll_alpha;
    float           fll_beta;

    float           ff_agc_pwr[FF_AGC_WINDOW];
    unsigned int    ff_agc_idx;
    float           ff_agc_sum;

    symsync_crcf    symsync;

    float complex   dqpsk_prev;

    float complex   *conv_buf;
    float complex   *decim_buf;
    float complex   *fll_buf;
    float complex   *agc_buf;
    float complex   *sync_buf;
    size_t          decim_block;

    uint8_t         bitbuf[BITBUF_SIZE];
    size_t          bitbuf_pos;

    /* leftover IQ bytes carried between process() calls so that a partial
     * block is never discarded (the caller's read size need not be a multiple
     * of the demod block size) */
    uint8_t        *pending;
    size_t          pending_len;

    float           rssi_db;

    spectrum_t     *spectrum;

    dqpsk_bit_sink_fn sink;
    void           *sink_priv;
};

static int g_demod_quiet = 0;

void dqpsk_demod_set_quiet(int quiet) {
    g_demod_quiet = quiet;
}

dqpsk_demod_t *dqpsk_demod_create(uint32_t sample_rate,
                                   uint32_t channel_rate,
                                   float    freq_offset) {
    if (sample_rate % channel_rate != 0)
        return NULL;

    dqpsk_demod_t *d = calloc(1, sizeof(*d));
    if (!d) return NULL;

    d->decim_factor = sample_rate / channel_rate;
    d->sample_rate  = sample_rate;
    d->dqpsk_prev   = 1.0f + 0.0f * I;

    d->decim = firdecim_crcf_create_kaiser(d->decim_factor, 12u, 60.0f);
    if (!d->decim) {
        free(d);
        return NULL;
    }

    d->nco = nco_crcf_create(LIQUID_NCO);
    {
        float omega = 2.0f * (float)M_PI * freq_offset / (float)sample_rate;
        nco_crcf_set_frequency(d->nco, omega);
    }

    {
        const unsigned int fll_len = FLL_FILTER_SIZE;
        const int M = (int)rintf((float)fll_len / (float)TETRA_SPS);
        const float half_sps_inv = 2.0f / (float)TETRA_SPS;

        float bb[FLL_FILTER_SIZE];
        float power = 0.0f;
        for (unsigned int i = 0; i < fll_len; i++) {
            float k = (float)(-M) + (float)i * half_sps_inv;
            float pos = TETRA_ROLLOFF * k;
            float tap = sincf_gr(pos - 0.5f) + sincf_gr(pos + 0.5f);
            power += tap * tap;
            bb[i] = tap;
        }

        float complex *h_lower = malloc(fll_len * sizeof(float complex));
        float complex *h_upper = malloc(fll_len * sizeof(float complex));

        const int N = ((int)fll_len - 1) / 2;
        const float invpower = 1.0f / power;
        const float inv_twice_sps = 0.5f / (float)TETRA_SPS;

        for (unsigned int i = 0; i < fll_len; i++) {
            float tap = bb[i] * invpower;
            float ph = 2.0f * (float)M_PI
                        * (1.0f + TETRA_ROLLOFF) * inv_twice_sps
                        * (float)((int)i - N);
            h_lower[i] = tap * cexpf(-I * ph);
            h_upper[i] = tap * cexpf( I * ph);
        }

        d->fll_lower = firfilt_cccf_create(h_lower, fll_len);
        d->fll_upper = firfilt_cccf_create(h_upper, fll_len);
        free(h_lower);
        free(h_upper);

        {
            float bw = (float)FLL_BW;
            float damping = sqrtf(2.0f) / 2.0f;
            float denom = 1.0f + 2.0f * damping * bw + bw * bw;
            d->fll_alpha = 4.0f * damping * bw / denom;
            d->fll_beta  = 4.0f * bw * bw / denom;
        }
        d->fll_phase = 0.0f;
        d->fll_freq  = 0.0f;
    }

    memset(d->ff_agc_pwr, 0, sizeof(d->ff_agc_pwr));
    d->ff_agc_idx = 0;
    d->ff_agc_sum = 0.0f;

    d->symsync = symsync_crcf_create_rnyquist(
        LIQUID_FIRFILT_RRC,
        TETRA_SPS,
        SYMSYNC_M,
        TETRA_ROLLOFF,
        SYMSYNC_NFILTS);
    if (!d->symsync) {
        dqpsk_demod_destroy(d);
        return NULL;
    }
    symsync_crcf_set_lf_bw(d->symsync, 0.01f);

    d->decim_block = TETRA_SPS * SYMSYNC_NFILTS * 4;

    size_t raw_block = d->decim_block * d->decim_factor;
    d->conv_buf  = malloc(raw_block        * sizeof(float complex));
    d->decim_buf = malloc(d->decim_block   * sizeof(float complex));
    d->fll_buf   = malloc(d->decim_block   * sizeof(float complex));
    d->agc_buf   = malloc(d->decim_block   * sizeof(float complex));
    d->sync_buf  = malloc(d->decim_block   * sizeof(float complex));
    d->pending   = malloc(raw_block * 2);

    if (!d->conv_buf || !d->decim_buf || !d->fll_buf ||
        !d->agc_buf  || !d->sync_buf || !d->pending) {
        dqpsk_demod_destroy(d);
        return NULL;
    }

    return d;
}

void dqpsk_demod_destroy(dqpsk_demod_t *d) {
    if (!d) return;
    if (d->decim)     firdecim_crcf_destroy(d->decim);
    if (d->nco)       nco_crcf_destroy(d->nco);
    if (d->fll_upper) firfilt_cccf_destroy(d->fll_upper);
    if (d->fll_lower) firfilt_cccf_destroy(d->fll_lower);
    if (d->symsync)   symsync_crcf_destroy(d->symsync);
    free(d->conv_buf);
    free(d->decim_buf);
    free(d->fll_buf);
    free(d->agc_buf);
    free(d->sync_buf);
    free(d->pending);
    free(d);
}

void dqpsk_demod_set_sink(dqpsk_demod_t *d, dqpsk_bit_sink_fn sink, void *priv) {
    d->sink = sink;
    d->sink_priv = priv;
}

static int flush_bits(dqpsk_demod_t *d) {
    if (d->bitbuf_pos == 0) return 0;
    if (d->sink)
        d->sink(d->bitbuf, d->bitbuf_pos, d->sink_priv);
    d->bitbuf_pos = 0;
    return 0;
}

static inline int emit_bit(dqpsk_demod_t *d, uint8_t bit) {
    d->bitbuf[d->bitbuf_pos++] = bit & 1u;
    if (d->bitbuf_pos >= BITBUF_SIZE)
        return flush_bits(d);
    return 0;
}

static int process_block(dqpsk_demod_t *d, const uint8_t *p) {
    size_t n_raw = d->decim_block * d->decim_factor;

    for (size_t i = 0; i < n_raw; i++) {
        float re = ((float)p[2*i]     - 127.5f) * (1.0f / 127.5f);
        float im = ((float)p[2*i + 1] - 127.5f) * (1.0f / 127.5f);
        d->conv_buf[i] = re + im * I;
    }

    nco_crcf_mix_block_down(d->nco, d->conv_buf, d->conv_buf, n_raw);

    /* Feed the spectrum AFTER the IF shift so the waterfall is centred on the
     * TETRA channel rather than on the RTL-SDR centre frequency.  This also
     * keeps a narrow span around the channel (and away from the DC spike). */
    if (d->spectrum)
        spectrum_push(d->spectrum, d->conv_buf, n_raw, d->rssi_db, 8);

    firdecim_crcf_execute_block(d->decim,
                                d->conv_buf,
                                d->decim_block,
                                d->decim_buf);

    {
        float pwr_sum = d->ff_agc_sum;
        for (size_t i = 0; i < d->decim_block; i++) {
            float complex s = d->decim_buf[i];
            float pwr = crealf(s)*crealf(s) + cimagf(s)*cimagf(s);

            pwr_sum -= d->ff_agc_pwr[d->ff_agc_idx];
            d->ff_agc_pwr[d->ff_agc_idx] = pwr;
            pwr_sum += pwr;
            d->ff_agc_idx = (d->ff_agc_idx + 1) % FF_AGC_WINDOW;

            float avg = pwr_sum / (float)FF_AGC_WINDOW;
            float gain = (avg > 1e-20f) ? 1.0f / sqrtf(avg) : 1.0f;
            d->agc_buf[i] = s * gain;
        }
        d->ff_agc_sum = pwr_sum;
        d->rssi_db = (d->ff_agc_sum > 1e-20f)
                   ? 10.0f * log10f(d->ff_agc_sum / (float)FF_AGC_WINDOW)
                   : -200.0f;
    }

    for (size_t i = 0; i < d->decim_block; i++) {
        float complex x = d->agc_buf[i] *
                          cexpf(I * d->fll_phase);

        float complex fl_out, fu_out;
        firfilt_cccf_push(d->fll_lower, x);
        firfilt_cccf_execute(d->fll_lower, &fl_out);
        firfilt_cccf_push(d->fll_upper, x);
        firfilt_cccf_execute(d->fll_upper, &fu_out);

        float fl_pwr = crealf(fl_out) * crealf(fl_out) +
                       cimagf(fl_out) * cimagf(fl_out);
        float fu_pwr = crealf(fu_out) * crealf(fu_out) +
                       cimagf(fu_out) * cimagf(fu_out);

        float error = 0.5f * (fl_pwr - fu_pwr);

        d->fll_freq  += d->fll_beta * error;
        d->fll_phase += d->fll_freq + d->fll_alpha * error;

        while (d->fll_phase >  (float)M_PI) d->fll_phase -= 2.0f*(float)M_PI;
        while (d->fll_phase < -(float)M_PI) d->fll_phase += 2.0f*(float)M_PI;

        float fll_max = 2.0f * (float)M_PI;
        if (d->fll_freq >  fll_max) d->fll_freq =  fll_max;
        if (d->fll_freq < -fll_max) d->fll_freq = -fll_max;

        d->fll_buf[i] = x;
    }

    unsigned int n_syms = 0;
    symsync_crcf_execute(d->symsync,
                         d->fll_buf, d->decim_block,
                         d->sync_buf, &n_syms);

    for (unsigned int i = 0; i < n_syms; i++) {
        float complex z = d->sync_buf[i] * conjf(d->dqpsk_prev);
        d->dqpsk_prev = d->sync_buf[i];

        float dphi = cargf(z);

        uint8_t db1, db2;
        if (dphi >= 0.0f) {
            db1 = 0;
            db2 = (dphi >= (float)M_PI / 2.0f) ? 1u : 0u;
        } else {
            db1 = 1;
            db2 = (dphi < -(float)M_PI / 2.0f) ? 1u : 0u;
        }

        if (emit_bit(d, db1) < 0) return -1;
        if (emit_bit(d, db2) < 0) return -1;
    }

    return 0;
}

int dqpsk_demod_process(dqpsk_demod_t *d, const uint8_t *buf, size_t len) {
    size_t bytes_per_block = d->decim_block * d->decim_factor * 2;

    /* Accumulate whole blocks; carry any partial block to the next call so
     * that samples are never dropped (the caller's read size is arbitrary). */
    while (len > 0) {
        if (d->pending_len > 0) {
            size_t need = bytes_per_block - d->pending_len;
            size_t take = (len < need) ? len : need;

            memcpy(d->pending + d->pending_len, buf, take);
            d->pending_len += take;
            buf += take;
            len -= take;

            if (d->pending_len == bytes_per_block) {
                if (process_block(d, d->pending) < 0)
                    return -1;
                d->pending_len = 0;
            }
        } else if (len >= bytes_per_block) {
            if (process_block(d, buf) < 0)
                return -1;
            buf += bytes_per_block;
            len -= bytes_per_block;
        } else {
            memcpy(d->pending, buf, len);
            d->pending_len = len;
            len = 0;
        }
    }

    return flush_bits(d);
}

float dqpsk_demod_get_rssi_db(dqpsk_demod_t *d) {
    return d->rssi_db;
}

float dqpsk_demod_get_fll_freq_hz(dqpsk_demod_t *d) {
    if (!d) return 0.0f;
    return d->fll_freq * 36000.0f / (2.0f * (float)M_PI);
}

void dqpsk_demod_set_spectrum(dqpsk_demod_t *d, spectrum_t *spectrum) {
    d->spectrum = spectrum;
}

void dqpsk_demod_reset(dqpsk_demod_t *d) {
    d->fll_phase = 0.0f;
    d->fll_freq  = 0.0f;
    firfilt_cccf_reset(d->fll_lower);
    firfilt_cccf_reset(d->fll_upper);

    memset(d->ff_agc_pwr, 0, sizeof(d->ff_agc_pwr));
    d->ff_agc_idx = 0;
    d->ff_agc_sum = 0.0f;

    symsync_crcf_reset(d->symsync);

    d->dqpsk_prev = 1.0f + 0.0f * I;

    d->bitbuf_pos = 0;
    d->pending_len = 0;
}

void dqpsk_demod_set_offset(dqpsk_demod_t *d, float freq_offset_hz) {
    float omega;

    if (!d || !d->nco)
        return;

    omega = 2.0f * (float)M_PI * freq_offset_hz / (float)d->sample_rate;
    nco_crcf_set_frequency(d->nco, omega);
    dqpsk_demod_reset(d);
}
