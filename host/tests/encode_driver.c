/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Reference TETRA channel encoder driver, used only by the host round-trip
 * test.  Reads 274 parameter bits (0/1, as 16-bit samples) and writes the
 * 432 interleaved channel-coded hard bits (0/1, as bytes).
 *
 * This links the amr-code/ encoder (ccod_tet.c + sub_cc.c) which cannot be
 * linked into the same binary as the decoder (both include the codec's global
 * data definitions), so it is built as a separate executable.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "channel.h"

int main(int argc, char **argv)
{
	FILE *fin, *fout;
	Word16 params[274];
	Word16 coded[432];
	Word16 interleaved[432];
	uint8_t bits[432];
	int i;

	if (argc != 3) {
		fprintf(stderr, "usage: %s params.bin coded.bin\n", argv[0]);
		return 2;
	}

	fin = fopen(argv[1], "rb");
	if (!fin)
		return 3;
	if (fread(params, sizeof(short), 274, fin) != 274) {
		fclose(fin);
		return 4;
	}
	fclose(fin);

	init_params(0);		/* TETRA codec mode */
	Channel_Encoding(1, 0, params, coded);
	Interleaving_Speech(coded, interleaved);

	for (i = 0; i < 432; i++)
		bits[i] = interleaved[i] < 0 ? 1 : 0;

	fout = fopen(argv[2], "wb");
	if (!fout)
		return 5;
	fwrite(bits, 1, sizeof(bits), fout);
	fclose(fout);

	return 0;
}
