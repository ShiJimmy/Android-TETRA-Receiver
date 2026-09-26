/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Tetra vocoder wrapper implementation. */

#include "tetra_vocoder.h"

#include <stdlib.h>
#include <string.h>

/* The vendored codec typedefs Word16 as int16_t.  Keep local extern
 * declarations in terms of int16_t to avoid pulling in the codec headers
 * (whose include guards and basenames collide between amr-code/ and c-code/).
 */
typedef int16_t Word16;

/* amr-code/ channel decoder */
extern void init_params(int CoderType);
extern Word16 Desinterleaving_Speech(Word16 Input_Frame[], Word16 Output_frame[]);
extern Word16 Channel_Decoding(short first_pass, Word16 Frame_Stealing,
			       Word16 Input_Frame[], Word16 Output_Frame[]);

/* c-code/ speech decoder */
extern void Init_Decod_Tetra(void);
extern void Bits2prm_Tetra(Word16 *bits, Word16 prm[]);
extern void Decod_Tetra(Word16 parm[], Word16 synth[]);
extern void Post_Process(Word16 signal[], Word16 lg);

struct tetra_vocoder {
	int first_pass;
};

tetra_vocoder_t *tetra_vocoder_create(void)
{
	tetra_vocoder_t *v = calloc(1, sizeof(*v));
	if (!v)
		return NULL;

	init_params(0);		/* TETRA codec mode */
	v->first_pass = 1;
	Init_Decod_Tetra();

	return v;
}

void tetra_vocoder_destroy(tetra_vocoder_t *v)
{
	free(v);
}

static void hard_to_soft(const uint8_t *bits, Word16 *soft, int n)
{
	int i;
	for (i = 0; i < n; i++)
		soft[i] = bits[i] ? (Word16)-127 : (Word16)127;
}

int tetra_vocoder_channel_decode(tetra_vocoder_t *v,
				 const uint8_t coded_bits[432],
				 uint8_t params[274])
{
	Word16 soft[432];
	Word16 deint[432];
	Word16 reordered[274];
	Word16 bfi;
	int i;

	if (!v || !coded_bits || !params)
		return -1;

	hard_to_soft(coded_bits, soft, 432);
	Desinterleaving_Speech(soft, deint);
	bfi = Channel_Decoding((short)v->first_pass, 0, deint, reordered);
	v->first_pass = 0;

	for (i = 0; i < 274; i++)
		params[i] = reordered[i] & 1;

	return bfi;
}

int tetra_vocoder_decode(tetra_vocoder_t *v,
			 const uint8_t coded_bits[432],
			 int16_t pcm[480])
{
	Word16 soft[432];
	Word16 deint[432];
	Word16 reordered[274];
	Word16 serial[138];
	Word16 parm[24];
	Word16 synth[240];
	Word16 bfi;
	int f;

	if (!v || !coded_bits || !pcm)
		return -1;

	hard_to_soft(coded_bits, soft, 432);
	Desinterleaving_Speech(soft, deint);
	bfi = Channel_Decoding((short)v->first_pass, 0, deint, reordered);
	v->first_pass = 0;

	for (f = 0; f < 2; f++) {
		serial[0] = bfi;
		memcpy(serial + 1, reordered + f * 137, 137 * sizeof(Word16));
		Bits2prm_Tetra(serial, parm);
		Decod_Tetra(parm, synth);
		Post_Process(synth, (Word16)240);
		memcpy(pcm + f * 240, synth, 240 * sizeof(int16_t));
	}

	return bfi;
}
