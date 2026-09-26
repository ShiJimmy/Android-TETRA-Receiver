/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Host tool: decode a raw RTL-SDR IQ file (uint8 I/Q) with the TETRA engine
 * and dump the demodulated bits, so the phone's runtime bit stream can be
 * compared against an offline decode of the phone's own captured IQ.
 *
 *   usage: iq_decode <iq.bin> <bits.bin> [offset_hz]
 *
 * offset_hz is the IF offset of the TETRA channel inside the capture
 * (channel - centre); it drives the demodulator's NCO.  Default 0.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "tetra_engine.h"

static FILE *g_out;

static void dump_cb(const uint8_t *bits, size_t n)
{
	if (g_out)
		fwrite(bits, 1, n, g_out);
}

int main(int argc, char **argv)
{
	FILE *in;
	tetra_engine_t *e;
	static uint8_t buf[1 << 20];
	size_t n;
	long offset_hz = 0;

	if (argc < 3 || argc > 4) {
		fprintf(stderr, "usage: %s iq.bin bits.bin [offset_hz]\n", argv[0]);
		return 2;
	}
	if (argc == 4)
		offset_hz = strtol(argv[3], NULL, 10);

	in = fopen(argv[1], "rb");
	if (!in) {
		perror("iq");
		return 3;
	}
	g_out = fopen(argv[2], "wb");
	if (!g_out) {
		perror("bits");
		return 3;
	}

	e = tetra_engine_create(1800000, 0.0f);
	if (!e)
		return 4;

	if (offset_hz != 0)
		tetra_engine_set_channel_offset(e, (int)offset_hz);

	tetra_bitdump_cb = dump_cb;

	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		n &= ~(size_t)1;        /* even number of bytes */
		if (n)
			tetra_engine_process(e, buf, n);
	}

	tetra_engine_destroy(e);
	fclose(in);
	fclose(g_out);
	return 0;
}
