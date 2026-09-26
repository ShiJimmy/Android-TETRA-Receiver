/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* USB capture layer implementation. */

#include "tetra_usb.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>

#include <rtl-sdr.h>

#include "engine/tetra_engine.h"

/* Debug facilities default to OFF; the Gradle debug build defines it to 1. */
#ifndef TETRA_DEBUG
#define TETRA_DEBUG 0
#endif

#define SAMPLE_RATE    1800000u   /* 1.8 MSps */
/* Should ideally be a multiple of the demod block size (25600 bytes); the
 * demodulator buffers partial blocks regardless, but a multiple avoids copies. */
#define CAPTURE_BUF_LEN 51200      /* bytes (25600 IQ samples) per read */

/* IQ ring buffer between the USB reader and the demod consumer (~1.4 s). */
#define RING_SIZE (5u * 1024u * 1024u)

#if TETRA_DEBUG
/* debug: dump raw IQ to a file for offline analysis (~12 MB ≈ 3.3 s) */
static FILE *g_iqdump;
static size_t g_iqdump_len;
#define IQDUMP_MAX (12u * 1024u * 1024u)

/* debug: mirror the start line and periodic status to a file so it can be
 * pulled via adb even though the in-app log view is not reachable. */
static FILE *g_diag;

static void diag(const char *fmt, ...)
{
	va_list ap;
	if (!g_diag)
		return;
	va_start(ap, fmt);
	vfprintf(g_diag, fmt, ap);
	va_end(ap);
	fflush(g_diag);
}
#else
#define diag(...) ((void)0)
#endif /* TETRA_DEBUG */

struct tetra_usb {
	rtlsdr_dev_t    *dev;
	tetra_engine_t  *engine;

	pthread_t        thread;         /* consumer: demod + decode */
	pthread_t        reader_thread;  /* producer: USB capture */
	volatile int     stop_flag;
	int              running;

	int              ppm;
	uint32_t         center_hz;     /* RTL-SDR centre frequency */
	uint32_t         channel_hz;    /* analysis (channel) frequency */

	/* SPSC byte ring between the USB reader and the demod consumer.  Keeping
	 * the (slow) demod + decode off the reader thread is essential: the
	 * RTL-SDR's internal FIFO overflows if we don't drain the USB endpoint
	 * at ~1.8 MSps * 2 bytes = 3.6 MB/s, which drops samples and destroys
	 * the TETRA frame alignment. */
	uint8_t         *ring;
	volatile unsigned ring_head;     /* producer index (monotonic) */
	volatile unsigned ring_tail;     /* consumer index (monotonic) */
	volatile unsigned long long ring_overflow;
};

static void ring_push(tetra_usb_t *u, const uint8_t *src, unsigned len)
{
	unsigned head = u->ring_head;
	unsigned tail = u->ring_tail;
	unsigned used = head - tail;
	unsigned free = RING_SIZE - used;

	if (len > free) {
		u->ring_overflow += len - free;
		len = free;
	}

	unsigned pos = head % RING_SIZE;
	unsigned first = RING_SIZE - pos;
	if (first > len) first = len;
	memcpy(u->ring + pos, src, first);
	if (len > first)
		memcpy(u->ring, src + first, len - first);

	__sync_synchronize();
	u->ring_head = head + len;
}

static unsigned ring_pop(tetra_usb_t *u, uint8_t *dst, unsigned max)
{
	unsigned head = u->ring_head;
	unsigned tail = u->ring_tail;
	unsigned used = head - tail;
	unsigned n = used < max ? used : max;

	if (n == 0)
		return 0;

	unsigned pos = tail % RING_SIZE;
	unsigned first = RING_SIZE - pos;
	if (first > n) first = n;
	memcpy(dst, u->ring + pos, first);
	if (n > first)
		memcpy(dst + first, u->ring, n - first);

	__sync_synchronize();
	u->ring_tail = tail + n;
	return n;
}

static void iq_async_cb(unsigned char *buf, uint32_t len, void *ctx)
{
	ring_push((tetra_usb_t *)ctx, buf, len);
}

static void *reader_thread(void *arg)
{
	tetra_usb_t *u = arg;
	/* Multiple URBs in flight keep the USB endpoint drained continuously so
	 * the RTL-SDR FIFO never overflows. */
	rtlsdr_read_async(u->dev, iq_async_cb, u, 0, 0);
	return NULL;
}

static void *capture_thread(void *arg)
{
	tetra_usb_t *u = arg;
	uint8_t buf[CAPTURE_BUF_LEN];
	unsigned n_read;
	unsigned int n_blocks = 0;
	struct timespec ts_prev = { 0, 0 };
	unsigned long long bytes_total = 0, bytes_prev = 0;

	while (!u->stop_flag) {
		n_read = ring_pop(u, buf, sizeof(buf));
		if (n_read == 0) {
			struct timespec ts = { 0, 2000000 }; /* 2 ms */
			nanosleep(&ts, NULL);
			continue;
		}
		{
			bytes_total += (unsigned long long)n_read;
#if TETRA_DEBUG
			if (g_iqdump) {
				/* circular: keep the most recent IQDUMP_MAX bytes so a
				 * pull always reflects the current operating state */
				if (g_iqdump_len + (size_t)n_read > IQDUMP_MAX) {
					fseek(g_iqdump, 0, SEEK_SET);
					g_iqdump_len = 0;
				}
				fwrite(buf, 1, (size_t)n_read, g_iqdump);
				g_iqdump_len += (size_t)n_read;
			}
#endif
			tetra_engine_process(u->engine, buf, (size_t)n_read);
			if ((++n_blocks % 50) == 0) {
				tetra_engine_status_t st;
				unsigned long bc;
				struct timespec now;
				double dt = 0.0;
				double mbps = 0.0;

				tetra_engine_get_status(u->engine, &st);
				bc = tetra_engine_get_bitcount(u->engine);

				clock_gettime(CLOCK_MONOTONIC, &now);
				if (ts_prev.tv_sec || ts_prev.tv_nsec)
					dt = (double)(now.tv_sec - ts_prev.tv_sec)
					   + (double)(now.tv_nsec - ts_prev.tv_nsec) / 1e9;
				if (dt > 1e-6)
					mbps = (double)(bytes_total - bytes_prev) / dt / 1e6;
				ts_prev = now;
				bytes_prev = bytes_total;

#if TETRA_DEBUG
				{
					extern volatile unsigned long g_tetra_voice_blocks;
					printf("[tetra] rssi=%.1f locked=%d fll=%.0fHz gain=%.1f bits=%lu voice=%lu iq=%.2fMB/s ovf=%llu\n",
					       st.rssi_db, st.locked, st.fll_hz,
					       rtlsdr_get_tuner_gain(u->dev) / 10.0f,
					       bc, g_tetra_voice_blocks, mbps,
					       (unsigned long long)u->ring_overflow);
					diag("[tetra] rssi=%.1f locked=%d fll=%.0fHz gain=%.1f bits=%lu voice=%lu iq=%.2fMB/s ovf=%llu\n",
					     st.rssi_db, st.locked, st.fll_hz,
					     rtlsdr_get_tuner_gain(u->dev) / 10.0f,
					     bc, g_tetra_voice_blocks, mbps,
					     (unsigned long long)u->ring_overflow);
				}
				fflush(stdout);
#else
				(void)st;
				(void)bc;
				(void)mbps;
#endif
			}
		}
	}
	return NULL;
}

tetra_usb_t *tetra_usb_create(void)
{
	tetra_usb_t *u = calloc(1, sizeof(*u));
	if (!u)
		return NULL;

	u->engine = tetra_engine_create(SAMPLE_RATE, 0.0f);
	if (!u->engine) {
		free(u);
		return NULL;
	}

#if TETRA_DEBUG
	/* debug: raw IQ dump */
	mkdir("/data/data/org.tetra.receiver/files", 0700);
	g_iqdump = fopen("/data/data/org.tetra.receiver/files/iq.bin", "wb");
	if (g_iqdump)
		setvbuf(g_iqdump, NULL, _IONBF, 0);
	g_iqdump_len = 0;

	g_diag = fopen("/data/data/org.tetra.receiver/files/diag.txt", "w");
	if (g_diag)
		setvbuf(g_diag, NULL, _IOLBF, 0);
#endif

	u->ring = malloc(RING_SIZE);
	if (!u->ring) {
		free(u);
		return NULL;
	}

	return u;
}

void tetra_usb_destroy(tetra_usb_t *u)
{
	if (!u)
		return;
	tetra_usb_stop(u);
	tetra_engine_destroy(u->engine);
	free(u->ring);
	free(u);
}

int tetra_usb_init(tetra_usb_t *u, int fd)
{
	if (!u || fd < 0)
		return -1;
	if (u->dev)
		tetra_usb_stop(u);

	return rtlsdr_open_fd(fd, &u->dev);
}

int tetra_usb_start(tetra_usb_t *u, uint32_t freq_hz, int gain_tenths_db)
{
	int r;

	if (!u || !u->dev)
		return -1;
	if (u->running)
		return -1;

	r = rtlsdr_set_sample_rate(u->dev, SAMPLE_RATE);
	if (r < 0)
		return r;
	/* rtlsdr_set_freq_correction() returns -2 when the value is unchanged
	 * (e.g. ppm 0 on a freshly opened device): that is not an error. */
	r = rtlsdr_set_freq_correction(u->dev, u->ppm);
	if (r < 0 && r != -2)
		return r;
	r = rtlsdr_set_center_freq(u->dev, freq_hz);
	if (r < 0)
		return r;
	if (gain_tenths_db <= 0) {
		r = rtlsdr_set_tuner_gain_mode(u->dev, 0);   /* auto gain */
	} else {
		r = rtlsdr_set_tuner_gain_mode(u->dev, 1);   /* manual gain */
		if (r >= 0)
			r = rtlsdr_set_tuner_gain(u->dev, gain_tenths_db);
	}
	if (r < 0)
		return r;
	r = rtlsdr_reset_buffer(u->dev);
	if (r < 0)
		return r;

	u->center_hz = freq_hz;
	u->channel_hz = freq_hz;
	tetra_engine_set_channel_offset(u->engine, 0);

#if TETRA_DEBUG
	printf("[tetra] start: centre_req=%u centre_act=%u rate=%u act=%u gain_req=%d gain_act=%d tuner=%d ppm=%d\n",
	       freq_hz, rtlsdr_get_center_freq(u->dev), SAMPLE_RATE,
	       rtlsdr_get_sample_rate(u->dev), gain_tenths_db,
	       rtlsdr_get_tuner_gain(u->dev), rtlsdr_get_tuner_type(u->dev),
	       u->ppm);
	fflush(stdout);
	diag("[tetra] start: centre_req=%u centre_act=%u rate=%u act=%u gain_req=%d gain_act=%d tuner=%d ppm=%d\n",
	     freq_hz, rtlsdr_get_center_freq(u->dev), SAMPLE_RATE,
	     rtlsdr_get_sample_rate(u->dev), gain_tenths_db,
	     rtlsdr_get_tuner_gain(u->dev), rtlsdr_get_tuner_type(u->dev),
	     u->ppm);
	{
		int ng = rtlsdr_get_tuner_gains(u->dev, NULL);
		int gains[64];
		if (ng > 64) ng = 64;
		if (ng > 0) {
			rtlsdr_get_tuner_gains(u->dev, gains);
			diag("[tetra] tuner gains(%d):", ng);
			for (int i = 0; i < ng; i++) diag(" %d", gains[i]);
			diag("\n");
		}
	}
#endif

	u->ring_head = 0;
	u->ring_tail = 0;
	u->ring_overflow = 0;
	u->stop_flag = 0;
	u->running = 1;
	if (pthread_create(&u->thread, NULL, capture_thread, u) != 0) {
		u->running = 0;
		return -1;
	}
	/* Start the async USB reader last so the consumer is already draining. */
	if (pthread_create(&u->reader_thread, NULL, reader_thread, u) != 0) {
		u->stop_flag = 1;
		pthread_join(u->thread, NULL);
		u->running = 0;
		return -1;
	}

	return 0;
}

void tetra_usb_stop(tetra_usb_t *u)
{
	if (!u)
		return;
	if (u->running) {
		u->stop_flag = 1;
		if (u->dev)
			rtlsdr_cancel_async(u->dev);
		pthread_join(u->reader_thread, NULL);
		pthread_join(u->thread, NULL);
		u->running = 0;
	}
	if (u->dev) {
		rtlsdr_close(u->dev);
		u->dev = NULL;
	}
}

size_t tetra_usb_read_pcm(tetra_usb_t *u, int16_t *out, size_t max_samples)
{
	if (!u || !u->engine)
		return 0;
	return tetra_engine_read_pcm(u->engine, out, max_samples);
}

void tetra_usb_get_netinfo(tetra_usb_t *u, int out[8])
{
	tetra_engine_status_t st;

	memset(out, 0, 8 * sizeof(int));
	if (!u || !u->engine)
		return;

	tetra_engine_get_status(u->engine, &st);
	out[0] = st.locked;
	out[1] = (int)st.mcc;
	out[2] = (int)st.mnc;
	out[3] = (int)st.colour_code;
	out[4] = (int)(st.rssi_db * 100.0f);
	out[5] = (int)st.fll_hz;
	out[6] = st.in_call;
	out[7] = (int)st.call_ssi;
}

void tetra_usb_set_ppm(tetra_usb_t *u, int ppm)
{
	if (!u)
		return;
	u->ppm = ppm;
	if (u->dev)
		rtlsdr_set_freq_correction(u->dev, ppm);
}

void tetra_usb_set_channel(tetra_usb_t *u, uint32_t channel_hz)
{
	if (!u || !u->engine)
		return;
	u->channel_hz = channel_hz;
	tetra_engine_set_channel_offset(u->engine, (int)channel_hz - (int)u->center_hz);
}

void tetra_usb_set_center(tetra_usb_t *u, uint32_t center_hz)
{
	if (!u)
		return;
	u->center_hz = center_hz;
	if (u->dev)
		rtlsdr_set_center_freq(u->dev, center_hz);
	if (u->engine)
		tetra_engine_set_channel_offset(u->engine, (int)u->channel_hz - (int)center_hz);
}

void tetra_usb_set_span(tetra_usb_t *u, uint32_t span_hz)
{
	if (u && u->engine)
		tetra_engine_set_span(u->engine, span_hz);
}

void tetra_usb_set_spectrum_enabled(tetra_usb_t *u, int on)
{
	if (u && u->engine)
		tetra_engine_set_spectrum_enabled(u->engine, on);
}

void tetra_usb_set_gain(tetra_usb_t *u, int gain_tenths)
{
	if (!u || !u->dev)
		return;
	if (gain_tenths <= 0) {
		rtlsdr_set_tuner_gain_mode(u->dev, 0);   /* auto */
		return;
	}
	rtlsdr_set_tuner_gain_mode(u->dev, 1);
	rtlsdr_set_tuner_gain(u->dev, gain_tenths);
}

int tetra_usb_get_spectrum(tetra_usb_t *u, float *out, int max_bins,
			   int *nfft, uint32_t *rate_hz)
{
	if (!u || !u->engine)
		return 0;
	return tetra_engine_get_psd(u->engine, out, max_bins, nfft, rate_hz);
}
