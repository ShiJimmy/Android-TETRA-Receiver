/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* JNI bridge between the Kotlin/Java layer and the native TETRA receiver. */

#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>

#include "tetra_usb.h"

#define TAG "tetra-native"

/* Debug facilities default to OFF; the Gradle debug build defines it to 1. */
#ifndef TETRA_DEBUG
#define TETRA_DEBUG 0
#endif

static tetra_usb_t *g_usb;

#if TETRA_DEBUG
/* debug bit dump */
extern void (*tetra_bitdump_cb)(const uint8_t *bits, size_t n);
static FILE *g_bitdump;
static size_t g_bitdump_len;
#define BITDUMP_MAX (8u * 1024u * 1024u)

static void bitdump_write(const uint8_t *bits, size_t n)
{
	if (!g_bitdump || g_bitdump_len >= BITDUMP_MAX)
		return;
	if (g_bitdump_len + n > BITDUMP_MAX)
		n = BITDUMP_MAX - g_bitdump_len;
	fwrite(bits, 1, n, g_bitdump);
	g_bitdump_len += n;
}

/* -------------------------------------------------------------------------
 * Native stdout/stderr capture.
 *
 * The vendored osmo-tetra decoder reports its progress with printf().  On
 * Android that output is lost, and the phone's single USB port is used by the
 * RTL-SDR so `adb logcat` is unavailable.  We redirect stdout/stderr into a
 * pipe and keep a rolling text buffer that the UI can display via
 * nativeGetLog().
 * ---------------------------------------------------------------------- */

#define LOG_BUF_SIZE 65536
static char g_log[LOG_BUF_SIZE];
static volatile int g_log_len;
static pthread_mutex_t g_log_mtx = PTHREAD_MUTEX_INITIALIZER;
static int g_log_pipe[2] = { -1, -1 };

static void *log_reader_thread(void *arg)
{
	char tmp[1024];
	ssize_t n;
	ssize_t i;

	(void)arg;
	for (;;) {
		n = read(g_log_pipe[0], tmp, sizeof(tmp));
		if (n <= 0)
			break;

		pthread_mutex_lock(&g_log_mtx);
		for (i = 0; i < n; i++) {
			unsigned char c = (unsigned char)tmp[i];
			if (c != '\n' && c != '\t' && (c < 0x20 || c >= 0x7f))
				c = '.';
			if (g_log_len >= LOG_BUF_SIZE - 1) {
				memmove(g_log, g_log + LOG_BUF_SIZE / 2, LOG_BUF_SIZE / 2);
				g_log_len = LOG_BUF_SIZE / 2;
			}
			g_log[g_log_len++] = (char)c;
		}
		g_log[g_log_len] = 0;
		pthread_mutex_unlock(&g_log_mtx);
	}
	return NULL;
}
#endif /* TETRA_DEBUG */

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
#if TETRA_DEBUG
	pthread_t th;
#endif
	(void)vm;
	(void)reserved;

#if TETRA_DEBUG
	if (pipe(g_log_pipe) == 0) {
		dup2(g_log_pipe[1], STDOUT_FILENO);
		dup2(g_log_pipe[1], STDERR_FILENO);
		close(g_log_pipe[1]);
		g_log_pipe[1] = -1;
		setvbuf(stdout, NULL, _IOLBF, 0);
		setvbuf(stderr, NULL, _IOLBF, 0);
		if (pthread_create(&th, NULL, log_reader_thread, NULL) == 0)
			pthread_detach(th);
	}
	printf("[tetra] native loaded, log capture active\n");
	fflush(stdout);

	/* debug: dump the demodulated bit stream for offline analysis */
	mkdir("/data/data/org.tetra.receiver/files", 0700);
	g_bitdump = fopen("/data/data/org.tetra.receiver/files/bits.bin", "wb");
	if (g_bitdump)
		setvbuf(g_bitdump, NULL, _IONBF, 0);
	tetra_bitdump_cb = bitdump_write;
#else
	/* Release: discard the vendored decoder's printf() output entirely so no
	 * diagnostics leave the process. */
	if (!freopen("/dev/null", "w", stdout))
		(void)0;
	if (!freopen("/dev/null", "w", stderr))
		(void)0;
#endif

	return JNI_VERSION_1_6;
}

JNIEXPORT jstring JNICALL
Java_org_tetra_receiver_TetraNative_nativeGetLog(JNIEnv *env, jobject thiz)
{
	(void)env;
	(void)thiz;
#if TETRA_DEBUG
	{
		jstring s;
		pthread_mutex_lock(&g_log_mtx);
		g_log[g_log_len] = 0;
		s = (*env)->NewStringUTF(env, g_log);
		pthread_mutex_unlock(&g_log_mtx);
		return s;
	}
#else
	return (*env)->NewStringUTF(env, "");
#endif
}

JNIEXPORT void JNICALL
Java_org_tetra_receiver_TetraNative_nativeClearLog(JNIEnv *env, jobject thiz)
{
	(void)env;
	(void)thiz;
#if TETRA_DEBUG
	pthread_mutex_lock(&g_log_mtx);
	g_log_len = 0;
	g_log[0] = 0;
	pthread_mutex_unlock(&g_log_mtx);
#endif
}

JNIEXPORT jint JNICALL
Java_org_tetra_receiver_TetraNative_nativeInit(JNIEnv *env, jobject thiz, jint usbFd)
{
	(void)env;
	(void)thiz;

	if (g_usb)
		return -1;
	g_usb = tetra_usb_create();
	if (!g_usb)
		return -2;

	if (tetra_usb_init(g_usb, (int)usbFd) < 0) {
		tetra_usb_destroy(g_usb);
		g_usb = NULL;
		return -3;
	}
	return 0;
}

JNIEXPORT jint JNICALL
Java_org_tetra_receiver_TetraNative_nativeStart(JNIEnv *env, jobject thiz, jlong freqHz, jint gainTenths)
{
	(void)env;
	(void)thiz;

	if (!g_usb)
		return -1;
	return tetra_usb_start(g_usb, (uint32_t)freqHz, (int)gainTenths);
}

JNIEXPORT void JNICALL
Java_org_tetra_receiver_TetraNative_nativeStop(JNIEnv *env, jobject thiz)
{
	(void)env;
	(void)thiz;

	if (g_usb) {
		tetra_usb_destroy(g_usb);
		g_usb = NULL;
	}
}

JNIEXPORT jint JNICALL
Java_org_tetra_receiver_TetraNative_nativeReadPcm(JNIEnv *env, jobject thiz, jshortArray outBuf, jint maxSamples)
{
	jshort *elems;
	size_t n;

	(void)thiz;

	if (!g_usb || maxSamples <= 0)
		return 0;

	elems = (*env)->GetShortArrayElements(env, outBuf, NULL);
	if (!elems)
		return 0;

	n = tetra_usb_read_pcm(g_usb, (int16_t *)elems, (size_t)maxSamples);

	(*env)->ReleaseShortArrayElements(env, outBuf, elems, 0);
	return (jint)n;
}

JNIEXPORT jintArray JNICALL
Java_org_tetra_receiver_TetraNative_nativeGetNetworkInfo(JNIEnv *env, jobject thiz)
{
	int info[8];
	jintArray arr;
	jint tmp[8];
	int i;

	(void)thiz;

	tetra_usb_get_netinfo(g_usb, info);
	for (i = 0; i < 8; i++)
		tmp[i] = (jint)info[i];

	arr = (*env)->NewIntArray(env, 8);
	if (!arr)
		return NULL;
	(*env)->SetIntArrayRegion(env, arr, 0, 8, tmp);
	return arr;
}

JNIEXPORT void JNICALL
Java_org_tetra_receiver_TetraNative_nativeSetPpm(JNIEnv *env, jobject thiz, jint ppm)
{
	(void)env;
	(void)thiz;
	tetra_usb_set_ppm(g_usb, (int)ppm);
}

JNIEXPORT void JNICALL
Java_org_tetra_receiver_TetraNative_nativeSetChannel(JNIEnv *env, jobject thiz, jlong channelHz)
{
	(void)env;
	(void)thiz;
	tetra_usb_set_channel(g_usb, (uint32_t)channelHz);
}

JNIEXPORT void JNICALL
Java_org_tetra_receiver_TetraNative_nativeSetCenter(JNIEnv *env, jobject thiz, jlong centerHz)
{
	(void)env;
	(void)thiz;
	tetra_usb_set_center(g_usb, (uint32_t)centerHz);
}

JNIEXPORT void JNICALL
Java_org_tetra_receiver_TetraNative_nativeSetSpectrumSpan(JNIEnv *env, jobject thiz, jint spanHz)
{
	(void)env;
	(void)thiz;
	tetra_usb_set_span(g_usb, (uint32_t)spanHz);
}

JNIEXPORT void JNICALL
Java_org_tetra_receiver_TetraNative_nativeSetSpectrumEnabled(JNIEnv *env, jobject thiz, jint on)
{
	(void)env;
	(void)thiz;
	tetra_usb_set_spectrum_enabled(g_usb, (int)on);
}

JNIEXPORT void JNICALL
Java_org_tetra_receiver_TetraNative_nativeSetGain(JNIEnv *env, jobject thiz, jint gainTenths)
{
	(void)env;
	(void)thiz;
	tetra_usb_set_gain(g_usb, (int)gainTenths);
}

JNIEXPORT jint JNICALL
Java_org_tetra_receiver_TetraNative_nativeGetSpectrum(JNIEnv *env, jobject thiz,
						      jfloatArray outBuf, jintArray infoBuf)
{
	jfloat *out;
	int nfft = 0;
	uint32_t rate = 0;
	int n;

	(void)thiz;

	if (!g_usb)
		return 0;

	out = (*env)->GetFloatArrayElements(env, outBuf, NULL);
	if (!out)
		return 0;

	n = tetra_usb_get_spectrum(g_usb, (float *)out,
				   (*env)->GetArrayLength(env, outBuf), &nfft, &rate);

	(*env)->ReleaseFloatArrayElements(env, outBuf, out, 0);

	if (infoBuf) {
		jint tmp[2];
		jsize len = (*env)->GetArrayLength(env, infoBuf);
		tmp[0] = (jint)nfft;
		tmp[1] = (jint)rate;
		if (len > 2)
			len = 2;
		if (len > 0)
			(*env)->SetIntArrayRegion(env, infoBuf, 0, len, tmp);
	}

	return (jint)n;
}
