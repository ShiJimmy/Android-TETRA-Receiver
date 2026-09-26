/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* TETRA receive engine: ties together the π/4-DQPSK demodulator, the
 * osmo-tetra burst synchronizer / MAC decoder, and the ETSI speech vocoder.
 *
 *   raw uint8 IQ → dqpsk_demod → bits → tetra_burst_sync_in → lower MAC
 *     → (TCH/S) → tetra_vocoder_decode → PCM ring buffer
 *
 * The engine is designed for a single producer (the capture thread calling
 * tetra_engine_process) and a single consumer (the audio thread calling
 * tetra_engine_read_pcm), matching the classic SPSC ring-buffer pattern.
 */
#ifndef TETRA_ENGINE_H
#define TETRA_ENGINE_H

#include <stdint.h>
#include <stddef.h>

#include "spectrum.h"

typedef struct tetra_engine tetra_engine_t;

typedef struct {
	int      locked;        /* 1 once the burst sync is locked */
	uint16_t mcc;           /* Mobile Country Code (0 if unknown) */
	uint16_t mnc;           /* Mobile Network Code (0 if unknown) */
	uint8_t  colour_code;   /* colour code (0 if unknown) */
	float    rssi_db;       /* current RSSI from the demod AGC */
	float    fll_hz;        /* residual carrier offset from the FLL */
	int      in_call;       /* 1 while a traffic call is active */
	uint32_t call_ssi;      /* SSI of the current call (0 if none) */
} tetra_engine_status_t;

/*
 * Create the engine.
 *   sample_rate - RTL-SDR sample rate in S/s (e.g. 1800000)
 *   freq_offset - channel frequency offset in Hz (0 = tuned to center)
 */
tetra_engine_t *tetra_engine_create(uint32_t sample_rate, float freq_offset);

void tetra_engine_destroy(tetra_engine_t *e);

/* Feed a block of raw RTL-SDR IQ bytes (interleaved uint8 I,Q; len even). */
int tetra_engine_process(tetra_engine_t *e, const uint8_t *iq, size_t len);

/* Drain decoded PCM. Returns the number of samples copied (0 if none). */
size_t tetra_engine_read_pcm(tetra_engine_t *e, int16_t *out, size_t max_samples);

/* Snapshot of the current receive status. */
void tetra_engine_get_status(tetra_engine_t *e, tetra_engine_status_t *st);

/*
 * Set the analysis (channel) frequency relative to the RTL-SDR centre, in Hz.
 * Decouples the demodulated carrier from the dongle centre frequency.
 */
void tetra_engine_set_channel_offset(tetra_engine_t *e, int offset_hz);

/* ---- spectrum / waterfall -------------------------------------------- */

/* Set the waterfall display span in Hz (clamped to [1 kHz, 1 MHz]). */
void tetra_engine_set_span(tetra_engine_t *e, uint32_t span_hz);
uint32_t tetra_engine_get_span(tetra_engine_t *e);

/* Enable/disable the spectrum computation (saves CPU when the waterfall is off). */
void tetra_engine_set_spectrum_enabled(tetra_engine_t *e, int on);

/*
 * Copy the latest fft-shifted PSD (dB) into out[].  Returns the number of bins
 * copied (<= max_bins), stores the FFT size in *nfft and the covered bandwidth
 * in *rate_hz.  Returns 0 if no PSD is available yet.
 */
int tetra_engine_get_psd(tetra_engine_t *e, float *out, int max_bins,
			 int *nfft, uint32_t *rate_hz);

/* Total number of demodulated bits produced so far (debug). */
unsigned long tetra_engine_get_bitcount(tetra_engine_t *e);

/* Optional debug hook: receives the demodulated bit stream. */
extern void (*tetra_bitdump_cb)(const uint8_t *bits, size_t n);

#endif /* TETRA_ENGINE_H */
