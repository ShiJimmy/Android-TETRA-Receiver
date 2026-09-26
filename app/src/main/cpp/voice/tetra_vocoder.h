/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Tetra vocoder wrapper: ETSI EN 300 395-2 TETRA speech decoder.
 *
 * Wraps the vendored ETSI reference codec (amr-code/ channel decoder and
 * c-code/ speech decoder) behind a small, self-contained C API.  The decoder
 * is not re-entrant: the ETSI code uses process-global state, so create one
 * tetra_vocoder_t and use it from a single thread (or guard it externally).
 *
 * The input to tetra_vocoder_decode() is a full-rate TCH/S traffic block of
 * 432 hard bits (0/1) in the "interleaved" order produced by osmo-tetra's
 * lower_mac (the `type4` bits).  The output is 480 samples of 16-bit linear
 * PCM (two 240-sample frames @ 8 kHz).
 */
#ifndef TETRA_VOCODER_H
#define TETRA_VOCODER_H

#include <stdint.h>

typedef struct tetra_vocoder tetra_vocoder_t;

/* Create and initialize a TETRA speech decoder (single instance). */
tetra_vocoder_t *tetra_vocoder_create(void);

void tetra_vocoder_destroy(tetra_vocoder_t *v);

/*
 * Decode one full-rate TCH/S traffic block.
 *
 *   coded_bits[432] - 432 hard bits (0/1), interleaved order (lower_mac type4)
 *   pcm[480]        - out: 480 samples of 16-bit PCM (2 x 240 @ 8 kHz)
 *
 * Returns 0 if the frame CRC is good, 1 if the bad-frame flag is set, and
 * -1 on invalid arguments.
 */
int tetra_vocoder_decode(tetra_vocoder_t *v,
			 const uint8_t coded_bits[432],
			 int16_t pcm[480]);

/*
 * Channel-decode only (diagnostics/tests): convert the same 432 hard bits
 * into 274 codec parameter bits (0/1, two 137-bit speech frames).
 * Returns the bad-frame flag (0 = OK, 1 = bad).
 */
int tetra_vocoder_channel_decode(tetra_vocoder_t *v,
				 const uint8_t coded_bits[432],
				 uint8_t params[274]);

#endif /* TETRA_VOCODER_H */
