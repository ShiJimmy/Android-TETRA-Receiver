/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Host smoke test for the TETRA receive engine.
 *
 * Feeds raw pseudo-noise IQ into the full chain (demodulator → burst sync →
 * MAC → vocoder) and checks that the pipeline runs without crashing.  Noise
 * contains no TETRA bursts, so nothing should lock or produce PCM.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tetra_engine.h"

#define SAMPLE_RATE 1800000u

int main(void)
{
	tetra_engine_t *e;
	tetra_engine_status_t st;
	size_t i, blocks = 40;
	uint8_t *iq;
	int16_t pcm[480];
	size_t got;
	int failures = 0;

	e = tetra_engine_create(SAMPLE_RATE, 0.0f);
	if (!e) {
		printf("FAIL: tetra_engine_create returned NULL\n");
		return 1;
	}

	/* ~25.6k bytes per demod block; feed 40 blocks of noise. */
	size_t block = 256 * 50 * 2;
	iq = malloc(block);
	if (!iq) {
		tetra_engine_destroy(e);
		return 1;
	}

	for (i = 0; i < blocks; i++) {
		size_t j;
		for (j = 0; j < block; j++)
			iq[j] = (uint8_t)(rand() & 0xff);
		if (tetra_engine_process(e, iq, block) < 0) {
			printf("FAIL: tetra_engine_process returned < 0\n");
			failures++;
			break;
		}
	}

	got = tetra_engine_read_pcm(e, pcm, 480);
	if (got != 0) {
		printf("FAIL: expected 0 PCM samples from noise, got %u\n", (unsigned)got);
		failures++;
	}

	tetra_engine_get_status(e, &st);
	if (st.locked) {
		printf("FAIL: should not lock onto noise\n");
		failures++;
	}
	printf("status: locked=%d mcc=%u mnc=%u cc=%u rssi=%.1f fll=%.1f\n",
	       st.locked, st.mcc, st.mnc, st.colour_code, st.rssi_db, st.fll_hz);

	tetra_engine_destroy(e);
	free(iq);

	if (failures) {
		printf("%d test(s) failed\n", failures);
		return 1;
	}
	printf("engine smoke test passed\n");
	return 0;
}
