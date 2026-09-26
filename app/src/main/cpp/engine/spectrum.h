/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Spectrum / waterfall support for the TETRA engine.
 *
 * Computes a power spectral density (PSD) of the raw (pre-decimation) IQ using
 * liquid-dsp's spectral periodogram.  To support narrow display spans down to
 * 1 kHz, the IQ is first decimated by a cascade of /4 stages so that the FFT
 * always covers roughly the requested span:
 *
 *   span 1 MHz .. 1.8 MHz  ->  no decimation    (1.8 MSps, ~879 Hz/bin)
 *   span ~450 kHz          ->  /4               (450 kSps, ~220 Hz/bin)
 *   span ~28 kHz           ->  /64              (28 kSps, ~13.7 Hz/bin)
 *   span ~1.8 kHz          ->  /1024            (1.76 kSps, ~0.86 Hz/bin)
 *   span 1 kHz             ->  /1024 (clamped)
 *
 * The PSD is produced fft-shifted (bin 0 = lowest frequency), in dB.
 */
#ifndef SPECTRUM_H
#define SPECTRUM_H

#include <stdint.h>
#include <stddef.h>
#include <complex.h>

/* FFT size (number of PSD bins). */
#define SPECTRUM_NFFT 2048

/* Span limits in Hz. */
#define SPECTRUM_SPAN_MIN 1000u
#define SPECTRUM_SPAN_MAX 1000000u

typedef struct spectrum spectrum_t;

spectrum_t *spectrum_create(uint32_t sample_rate);
void spectrum_destroy(spectrum_t *s);

/* Set the displayed span (Hz); clamped to [SPECTRUM_SPAN_MIN, SPECTRUM_SPAN_MAX].
 * The change is applied lazily by the next spectrum_push(). */
void spectrum_set_span(spectrum_t *s, uint32_t span_hz);
uint32_t spectrum_get_span(spectrum_t *s);

/* Feed raw (pre-decimation) IQ samples (demod thread). */
void spectrum_push(spectrum_t *s, const float complex *samples, size_t n,
                   float rssi_db, unsigned int update_every);

/*
 * Copy the latest fft-shifted PSD (dB) into out[].  Returns the number of bins
 * copied (<= max_bins), and stores the FFT size in *nfft and the covered
 * bandwidth in *rate_hz.  Returns 0 if no PSD is available yet.
 */
int spectrum_get_psd(spectrum_t *s, float *out, int max_bins,
                     int *nfft, uint32_t *rate_hz);

#endif /* SPECTRUM_H */
