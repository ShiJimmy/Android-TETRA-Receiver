/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* TETRA receive engine implementation. */

#include "tetra_engine.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dqpsk_demod.h"
#include "tetra_vocoder.h"

#include <tetra_common.h>
#include <phy/tetra_burst.h>
#include <phy/tetra_burst_sync.h>
#include <crypto/tetra_crypto.h>

#define PCM_RING_SIZE 16384   /* samples; ~2 s at 8 kHz */

#define TETRA_CHANNEL_RATE 36000u

/* Optional debug hook: receives the demodulated bit stream (set by the JNI). */
void (*tetra_bitdump_cb)(const uint8_t *bits, size_t n);

struct tetra_engine {
	dqpsk_demod_t           *demod;
	struct tetra_rx_state   *trs;
	struct tetra_mac_state  *tms;
	struct tetra_crypto_state *tcs;
	tetra_vocoder_t         *vocoder;

	spectrum_t              *spectrum;
	int                      spectrum_enabled;

	int      in_call;
	uint32_t call_ssi;

	/* Last time a TCH/S voice frame was successfully decoded (ms, monotonic).
	 * Used to mark a call active even when we joined it mid-call and never
	 * saw the CMCE D-SETUP primitive. */
	volatile unsigned long long last_voice_ms;

	/* SPSC PCM ring buffer (single producer = decode, single consumer = audio) */
	int16_t ring[PCM_RING_SIZE];
	size_t  ring_head;      /* producer index */
	size_t  ring_tail;      /* consumer index */
};

static unsigned long long engine_now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000ull
	     + (unsigned long long)ts.tv_nsec / 1000000ull;
}

static void engine_bit_sink(const uint8_t *bits, size_t n, void *priv)
{
	tetra_engine_t *e = priv;
	extern volatile unsigned long g_tetra_bitcount;
	g_tetra_bitcount += (unsigned long)n;
	if (tetra_bitdump_cb)
		tetra_bitdump_cb(bits, n);
	tetra_burst_sync_in(e->trs, (uint8_t *)bits, (unsigned int)n);
}

volatile unsigned long g_tetra_bitcount;
volatile unsigned long g_tetra_voice_blocks;

unsigned long tetra_engine_get_bitcount(tetra_engine_t *e)
{
	(void)e;
	return g_tetra_bitcount;
}

static void engine_voice_sink(const uint8_t type4[432], void *priv)
{
	tetra_engine_t *e = priv;
	int16_t pcm[480];
	int bfi;

	bfi = tetra_vocoder_decode(e->vocoder, type4, pcm);
	if (bfi < 0)
		return;

	/* A decodable TCH/S frame means a call is (still) active, even if we
	 * joined it mid-call and never saw the CMCE call-setup primitive. */
	e->last_voice_ms = engine_now_ms();
	e->in_call = 1;

	/* debug: report vocoder output */
	{
		extern volatile unsigned long g_tetra_voice_blocks;
		g_tetra_voice_blocks++;
	}

	for (int i = 0; i < 480; i++) {
		size_t next = (e->ring_head + 1) % PCM_RING_SIZE;
		if (next == e->ring_tail)
			return;                 /* ring full: drop the rest */
		e->ring[e->ring_head] = pcm[i];
		e->ring_head = next;
	}
}

static void engine_call_sink(int event, uint32_t ssi, void *priv)
{
	tetra_engine_t *e = priv;
	e->in_call = event ? 1 : 0;
	e->call_ssi = event ? ssi : 0;
}

tetra_engine_t *tetra_engine_create(uint32_t sample_rate, float freq_offset)
{
	tetra_engine_t *e = calloc(1, sizeof(*e));
	if (!e)
		return NULL;

	e->demod = dqpsk_demod_create(sample_rate, TETRA_CHANNEL_RATE, freq_offset);
	if (!e->demod)
		goto fail;

	e->trs = calloc(1, sizeof(*e->trs));
	e->tms = calloc(1, sizeof(*e->tms));
	e->tcs = calloc(1, sizeof(*e->tcs));
	if (!e->trs || !e->tms || !e->tcs)
		goto fail;

	tetra_mac_state_init(e->tms);
	tetra_crypto_state_init(e->tcs);
	e->tms->tcs = e->tcs;

	e->trs->burst_cb_priv = e->tms;

	e->vocoder = tetra_vocoder_create();
	if (!e->vocoder)
		goto fail;

	/* Route demodulated bits into the burst synchronizer. */
	dqpsk_demod_set_sink(e->demod, engine_bit_sink, e);

	/* Spectrum / waterfall: feed the raw IQ to the periodogram. */
	e->spectrum = spectrum_create(sample_rate);
	if (!e->spectrum)
		goto fail;
	/* NOTE: disabled by default to save CPU (the decimation chain runs at the
	 * full 1.8 MSps); enable via tetra_engine_set_spectrum_enabled(). */
	e->spectrum_enabled = 0;

	/* Route decoded TCH/S blocks into the vocoder. */
	tetra_voice_cb = engine_voice_sink;
	tetra_voice_cb_priv = e;

	/* Route call setup/release events into the status fields. */
	tetra_call_cb = engine_call_sink;
	tetra_call_cb_priv = e;

	return e;

fail:
	tetra_engine_destroy(e);
	return NULL;
}

void tetra_engine_destroy(tetra_engine_t *e)
{
	if (!e)
		return;

	if (tetra_voice_cb == engine_voice_sink)
		tetra_voice_cb = NULL;
	if (tetra_call_cb == engine_call_sink)
		tetra_call_cb = NULL;

	if (e->vocoder)
		tetra_vocoder_destroy(e->vocoder);
	if (e->spectrum)
		spectrum_destroy(e->spectrum);
	if (e->demod)
		dqpsk_demod_destroy(e->demod);
	free(e->tcs);
	free(e->tms);
	free(e->trs);
	free(e);
}

int tetra_engine_process(tetra_engine_t *e, const uint8_t *iq, size_t len)
{
	/* Never reject an odd-length read: the demodulator carries leftover
	 * bytes across calls, so a stray half I/Q sample is harmless. Dropping
	 * the whole buffer here would punch a hole in the IQ stream and knock
	 * the burst synchroniser out of frame alignment. */
	if (!e || !iq || len == 0)
		return -1;
	return dqpsk_demod_process(e->demod, iq, len);
}

size_t tetra_engine_read_pcm(tetra_engine_t *e, int16_t *out, size_t max_samples)
{
	size_t n = 0;

	while (n < max_samples && e->ring_tail != e->ring_head) {
		out[n++] = e->ring[e->ring_tail];
		e->ring_tail = (e->ring_tail + 1) % PCM_RING_SIZE;
	}
	return n;
}

void tetra_engine_get_status(tetra_engine_t *e, tetra_engine_status_t *st)
{
	memset(st, 0, sizeof(*st));

	if (e->trs->state == RX_S_LOCKED)
		st->locked = 1;
	st->mcc = e->tcs->mcc > 0 ? (uint16_t)e->tcs->mcc : 0;
	st->mnc = e->tcs->mnc > 0 ? (uint16_t)e->tcs->mnc : 0;
	st->colour_code = e->tcs->cc > 0 ? (uint8_t)e->tcs->cc : 0;
	st->rssi_db = dqpsk_demod_get_rssi_db(e->demod);
	st->fll_hz = dqpsk_demod_get_fll_freq_hz(e->demod);

	/* A call is active while TCH/S voice frames keep arriving.  This also
	 * covers calls we joined in progress (no CMCE D-SETUP seen).  Clear the
	 * state a few seconds after the voice traffic stops. */
	{
		unsigned long long now = engine_now_ms();
		if (e->last_voice_ms != 0) {
			if (now - e->last_voice_ms < 2500ull)
				e->in_call = 1;
			else {
				e->in_call = 0;
				e->call_ssi = 0;
			}
		}
	}
	st->in_call = e->in_call;
	st->call_ssi = e->call_ssi;
}

void tetra_engine_set_channel_offset(tetra_engine_t *e, int offset_hz)
{
	if (e && e->demod)
		dqpsk_demod_set_offset(e->demod, (float)offset_hz);
}

void tetra_engine_set_span(tetra_engine_t *e, uint32_t span_hz)
{
	if (e && e->spectrum)
		spectrum_set_span(e->spectrum, span_hz);
}

void tetra_engine_set_spectrum_enabled(tetra_engine_t *e, int on)
{
	if (!e || !e->demod || !e->spectrum)
		return;
	e->spectrum_enabled = on ? 1 : 0;
	dqpsk_demod_set_spectrum(e->demod, on ? e->spectrum : NULL);
}

uint32_t tetra_engine_get_span(tetra_engine_t *e)
{
	return (e && e->spectrum) ? spectrum_get_span(e->spectrum) : 0;
}

int tetra_engine_get_psd(tetra_engine_t *e, float *out, int max_bins,
			 int *nfft, uint32_t *rate_hz)
{
	if (!e || !e->spectrum)
		return 0;
	return spectrum_get_psd(e->spectrum, out, max_bins, nfft, rate_hz);
}
