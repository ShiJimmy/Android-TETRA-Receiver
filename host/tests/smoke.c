/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Host smoke test for the vendored TETRA decoder + compat layer.
 *
 * Exercises the compatibility layer (bit packing) and the soft-decision
 * Viterbi decoders used for the TETRA control and traffic channels, using a
 * known all-zero coded sequence (all-zero data encodes to all-zero codewords,
 * so decoding a clean all-zero soft-bit input must return metric 0 and an
 * all-zero output).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <osmocom/core/bits.h>
#include <lower_mac/viterbi_cch.h>
#include <lower_mac/viterbi_tch.h>

static int failures = 0;

static void check(int cond, const char *what)
{
	if (cond) {
		printf("PASS: %s\n", what);
	} else {
		printf("FAIL: %s\n", what);
		failures++;
	}
}

static void test_bit_roundtrip(void)
{
	ubit_t in[24];
	pbit_t packed[3];
	ubit_t out[24];
	int i;

	for (i = 0; i < 24; i++)
		in[i] = (i * 7 + 3) & 1;

	osmo_ubit2pbit(packed, in, 24);
	osmo_pbit2ubit(out, packed, 24);

	check(memcmp(in, out, sizeof(in)) == 0, "osmo_ubit2pbit/osmo_pbit2ubit round-trip");
}

/* Feed an all-zero encoded word (n data bits) and expect metric 0 + zeros. */
static void test_viterbi_zeros(int n, int (*dec)(int8_t *, uint8_t *, int),
			       const char *name)
{
	int in_len = n * 4 + 4 * 4;	/* N=4 data bits + (K-1)=4 flush steps */
	int8_t *soft = malloc(in_len);
	uint8_t *out = malloc(n);
	int rv;
	int i;

	for (i = 0; i < in_len; i++)
		soft[i] = 127;		/* strong "0" */

	rv = dec(soft, out, n);

	{
		int all_zero = 1;
		for (i = 0; i < n; i++)
			if (out[i])
				all_zero = 0;
		check(rv == 0 && all_zero, name);
	}

	free(soft);
	free(out);
}

int main(void)
{
	test_bit_roundtrip();
	test_viterbi_zeros(80, conv_cch_decode, "conv_cch_decode all-zeros (metric 0)");
	test_viterbi_zeros(216, conv_tch_decode, "conv_tch_decode all-zeros (metric 0)");

	if (failures) {
		printf("%d test(s) failed\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}
