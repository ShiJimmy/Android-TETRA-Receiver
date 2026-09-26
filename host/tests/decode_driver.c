/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Host round-trip decoder driver: reads 432 interleaved channel-coded hard
 * bits (0/1) and runs the wrapped TETRA vocoder.
 *
 *   argv[1]  coded.bin       - 432 bytes (0/1) input
 *   argv[2]  params_out.bin  - 274 bytes (0/1) channel-decoded params
 *
 * Prints the bad-frame flag and PCM statistics; exits non-zero if the frame
 * fails CRC (BFI set).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "tetra_vocoder.h"

int main(int argc, char **argv)
{
	FILE *fin, *fout;
	uint8_t bits[432];
	uint8_t params[274];
	int16_t pcm[480];
	tetra_vocoder_t *v;
	int bfi;
	int i;
	int16_t pmin = 32767, pmax = -32768;

	if (argc != 3) {
		fprintf(stderr, "usage: %s coded.bin params_out.bin\n", argv[0]);
		return 2;
	}

	fin = fopen(argv[1], "rb");
	if (!fin)
		return 3;
	if (fread(bits, 1, sizeof(bits), fin) != sizeof(bits)) {
		fclose(fin);
		return 4;
	}
	fclose(fin);

	v = tetra_vocoder_create();
	if (!v)
		return 5;

	bfi = tetra_vocoder_channel_decode(v, bits, params);

	fout = fopen(argv[2], "wb");
	if (!fout) {
		tetra_vocoder_destroy(v);
		return 6;
	}
	fwrite(params, 1, sizeof(params), fout);
	fclose(fout);

	if (tetra_vocoder_decode(v, bits, pcm) != bfi) {
		fprintf(stderr, "BFI mismatch between channel and full decode\n");
		tetra_vocoder_destroy(v);
		return 7;
	}

	for (i = 0; i < 480; i++) {
		if (pcm[i] < pmin) pmin = pcm[i];
		if (pcm[i] > pmax) pmax = pcm[i];
	}

	printf("BFI=%d PCM[min=%d max=%d first=%d]\n",
	       bfi, pmin, pmax, pcm[0]);

	tetra_vocoder_destroy(v);

	return bfi ? 1 : 0;
}
