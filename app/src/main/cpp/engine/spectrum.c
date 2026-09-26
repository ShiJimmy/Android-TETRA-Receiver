/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Spectrum / waterfall support for the TETRA engine. */

#include "spectrum.h"

#include <liquid.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STAGES      6          /* up to 4^6 = 4096x decimation */
#define DECIM_STAGE     4
#define SPECTRUM_TMP_MAX         65536      /* per-stage ping-pong buffer length */

struct spectrum {
	uint32_t sample_rate;
	uint32_t span_hz;

	/* decimation chain (only touched by the demod thread) */
	unsigned int D;                 /* total decimation factor */
	unsigned int nstages;           /* number of /4 stages in use */
	firdecim_crcf st[MAX_STAGES];
	uint32_t rate;                  /* decimated sample rate */

	spgramcf sg;

	float complex *tmpA;
	float complex *tmpB;

	/* PSD double buffer, fft-shifted, dB */
	float psd[2][SPECTRUM_NFFT];
	volatile int active;

	unsigned int since;             /* decimated samples since last PSD */

	volatile uint32_t pending_span;
	volatile int have_psd;
};

static void spectrum_rebuild(spectrum_t *s)
{
	unsigned int i;
	uint32_t ratio;
	unsigned int D, nst;

	/* tear down */
	for (i = 0; i < MAX_STAGES; i++) {
		if (s->st[i]) {
			firdecim_crcf_destroy(s->st[i]);
			s->st[i] = NULL;
		}
	}
	if (s->sg) {
		spgramcf_destroy(s->sg);
		s->sg = NULL;
	}

	ratio = s->sample_rate / s->span_hz;
	if (ratio < 1)
		ratio = 1;

	D = 1;
	nst = 0;
	while (nst < MAX_STAGES && (D * DECIM_STAGE) <= ratio) {
		D *= DECIM_STAGE;
		nst++;
	}

	s->D = D;
	s->nstages = nst;
	s->rate = s->sample_rate / D;

	for (i = 0; i < nst; i++)
		s->st[i] = firdecim_crcf_create_kaiser(DECIM_STAGE, 4, 60.0f);

	s->sg = spgramcf_create(SPECTRUM_NFFT, LIQUID_WINDOW_HANN,
				SPECTRUM_NFFT / 2, SPECTRUM_NFFT / 2);

	s->since = 0;
}

spectrum_t *spectrum_create(uint32_t sample_rate)
{
	spectrum_t *s = calloc(1, sizeof(*s));
	if (!s)
		return NULL;

	s->sample_rate = sample_rate;
	s->span_hz = 25000;             /* sensible TETRA default */
	s->active = 0;
	s->since = 0;

	s->tmpA = malloc(SPECTRUM_TMP_MAX * sizeof(float complex));
	s->tmpB = malloc(SPECTRUM_TMP_MAX * sizeof(float complex));
	if (!s->tmpA || !s->tmpB) {
		spectrum_destroy(s);
		return NULL;
	}

	spectrum_rebuild(s);
	if (!s->sg) {
		spectrum_destroy(s);
		return NULL;
	}
	return s;
}

void spectrum_destroy(spectrum_t *s)
{
	unsigned int i;

	if (!s)
		return;
	for (i = 0; i < MAX_STAGES; i++)
		if (s->st[i])
			firdecim_crcf_destroy(s->st[i]);
	if (s->sg)
		spgramcf_destroy(s->sg);
	free(s->tmpA);
	free(s->tmpB);
	free(s);
}

void spectrum_set_span(spectrum_t *s, uint32_t span_hz)
{
	if (span_hz < SPECTRUM_SPAN_MIN)
		span_hz = SPECTRUM_SPAN_MIN;
	if (span_hz > SPECTRUM_SPAN_MAX)
		span_hz = SPECTRUM_SPAN_MAX;
	s->pending_span = span_hz;
}

uint32_t spectrum_get_span(spectrum_t *s)
{
	return s->span_hz;
}

void spectrum_push(spectrum_t *s, const float complex *samples, size_t n,
                   float rssi_db, unsigned int update_every)
{
	const float complex *in = samples;
	size_t len = n;
	float complex *a = s->tmpA, *b = s->tmpB;
	unsigned int i;
	unsigned int threshold;

	(void)rssi_db;
	(void)update_every;

	/* apply a pending span change on the producer thread */
	if (s->pending_span && s->pending_span != s->span_hz) {
		s->span_hz = s->pending_span;
		s->pending_span = 0;
		spectrum_rebuild(s);
	}

	if (!s->sg || n == 0)
		return;

	for (i = 0; i < s->nstages; i++) {
		size_t out_len = len / DECIM_STAGE;
		if (out_len == 0) {
			len = 0;
			break;
		}
		/* NB: firdecim_crcf_execute_block() takes the number of *output*
		 * samples and reads out_len*M input samples. */
		if (!s->st[i]) {
			len = 0;
			break;
		}
		firdecim_crcf_execute_block(s->st[i],
					    (liquid_float_complex *)in, (unsigned)out_len,
					    (liquid_float_complex *)a);
		in = a;
		len = out_len;
		/* ping-pong */
		float complex *t = a; a = b; b = t;
	}

	if (len == 0)
		return;
	if (!s->sg)
		return;

	spgramcf_write(s->sg, (liquid_float_complex *)in, (unsigned)len);

	s->since += (unsigned int)len;

	threshold = s->rate / 20;               /* aim for ~20 updates/s ... */
	if (threshold < SPECTRUM_NFFT)          /* ... but at least one FFT window */
		threshold = SPECTRUM_NFFT;

	if (s->since >= threshold) {
		int next = 1 - s->active;
		spgramcf_get_psd(s->sg, s->psd[next]);
		spgramcf_reset(s->sg);
		s->active = next;
		s->have_psd = 1;
		s->since = 0;
	}
}

int spectrum_get_psd(spectrum_t *s, float *out, int max_bins,
                     int *nfft, uint32_t *rate_hz)
{
	int n;
	int a;
	int start = 0;

	if (!s || !s->have_psd)
		return 0;

	a = s->active;
	n = SPECTRUM_NFFT;
	if (n > max_bins)
		n = max_bins;

	/* The decimation chain only realises the span quantised to powers of 4
	 * (rate in [span, 4*span)).  Crop the PSD to the *requested* span so the
	 * waterfall axis and the channel marker line up with the data exactly,
	 * regardless of which decimation stage was selected. */
	if (n == SPECTRUM_NFFT && s->rate > s->span_hz) {
		int vis = (int)(((uint64_t)SPECTRUM_NFFT * s->span_hz) / s->rate);
		if (vis < 1)
			vis = 1;
		if (vis > n)
			vis = n;
		start = (SPECTRUM_NFFT - vis) / 2;
		n = vis;
	}

	memcpy(out, s->psd[a] + start, n * sizeof(float));

	if (nfft)
		*nfft = SPECTRUM_NFFT;
	if (rate_hz)
		*rate_hz = s->span_hz;   /* displayed bandwidth */

	return n;
}
