/* SPDX-License-Identifier: GPL-3.0-or-later (port of tetra-rtlsdr)
 * Copyright (C) 2026 Shi Jimmy (Android port)
 */
/*
 * π/4-DQPSK demodulator for TETRA using liquid-dsp.
 *
 * Ported from tetra-rtlsdr (CEMAXECUTER LLC, GPL-3.0-or-later) and adapted
 * for the Android engine: instead of writing decoded bits to a file
 * descriptor, the demodulator streams them to a caller-supplied sink
 * callback (see dqpsk_demod_set_sink()).
 *
 * Input:  raw RTL-SDR uint8 IQ bytes at `sample_rate` S/s
 * Output: bits (1 byte per bit, 0x00 or 0x01) via the sink callback
 */

#ifndef DQPSK_DEMOD_H
#define DQPSK_DEMOD_H

#include <stdint.h>
#include <stddef.h>

#include "spectrum.h"

typedef struct dqpsk_demod dqpsk_demod_t;

/* Sink called with batches of decoded bits (0x00/0x01). */
typedef void (*dqpsk_bit_sink_fn)(const uint8_t *bits, size_t n, void *priv);

dqpsk_demod_t *dqpsk_demod_create(uint32_t sample_rate,
                                   uint32_t channel_rate,
                                   float    freq_offset);

void dqpsk_demod_destroy(dqpsk_demod_t *d);

/*
 * Process a block of raw RTL-SDR IQ bytes (interleaved uint8 I,Q; len even).
 * Returns 0 on success, -1 on error.
 */
int dqpsk_demod_process(dqpsk_demod_t *d, const uint8_t *buf, size_t len);

/* Install a bit sink; pass NULL to disable. */
void dqpsk_demod_set_sink(dqpsk_demod_t *d, dqpsk_bit_sink_fn sink, void *priv);

/* Current RSSI from AGC (dB). */
float dqpsk_demod_get_rssi_db(dqpsk_demod_t *d);

/* FLL carrier offset in Hz. */
float dqpsk_demod_get_fll_freq_hz(dqpsk_demod_t *d);

/* Attach an optional spectrum object (NULL disables waterfall). */
void dqpsk_demod_set_spectrum(dqpsk_demod_t *d, spectrum_t *spectrum);

/* Reset demodulator state (for retuning). */
void dqpsk_demod_reset(dqpsk_demod_t *d);

/*
 * Select the analysis (channel) frequency relative to the RTL-SDR centre, in
 * Hz, via the NCO.  This decouples the demodulated carrier from the dongle
 * centre frequency (so the DC spike can be avoided).  Resets the FLL/AGC/
 * symbol-timing state so the loop re-acquires at the new offset.
 */
void dqpsk_demod_set_offset(dqpsk_demod_t *d, float freq_offset_hz);

/* Suppress verbose init messages (call before create). */
void dqpsk_demod_set_quiet(int quiet);

#endif /* DQPSK_DEMOD_H */
