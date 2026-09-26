/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* USB capture layer: RTL-SDR device + capture thread + receive engine.
 *
 * Pure C (no JNI) so it can be unit-tested on the host.  The JNI bridge is a
 * thin wrapper on top of this API.
 *
 * One tetra_usb_t handles a single RTL-SDR dongle; the capture thread reads
 * synchronous IQ blocks and feeds them to the TETRA receive engine, which
 * decodes traffic-channel voice into a PCM ring buffer.
 */
#ifndef TETRA_USB_H
#define TETRA_USB_H

#include <stdint.h>
#include <stddef.h>

typedef struct tetra_usb tetra_usb_t;

/* Create the capture context and the receive engine. */
tetra_usb_t *tetra_usb_create(void);

void tetra_usb_destroy(tetra_usb_t *u);

/*
 * Open the RTL-SDR from an already-opened file descriptor (obtained from
 * Android's UsbDeviceConnection.getFileDescriptor()).  Returns 0 on success,
 * < 0 on error.
 */
int tetra_usb_init(tetra_usb_t *u, int fd);

/*
 * Configure the dongle (sample rate 1.8 MSps, centre frequency, manual tuner
 * gain) and start the capture thread.  gain_tenths_db is in tenths of a dB
 * (0 .. ~496 for R820T).  Returns 0 on success, < 0 on error.
 */
int tetra_usb_start(tetra_usb_t *u, uint32_t freq_hz, int gain_tenths_db);

/* Stop the capture thread and close the device. */
void tetra_usb_stop(tetra_usb_t *u);

/* Drain decoded PCM.  Returns the number of samples copied (0 if none). */
size_t tetra_usb_read_pcm(tetra_usb_t *u, int16_t *out, size_t max_samples);

/*
 * Current network/cell status, as an int[8]:
 *   [0] locked       (1 = burst sync locked)
 *   [1] mcc
 *   [2] mnc
 *   [3] colour_code
 *   [4] rssi_db * 100 (as int)
 *   [5] fll_hz (as int)
 *   [6] in_call      (1 while a traffic call is active)
 *   [7] call_ssi     (SSI of the current call, 0 if none)
 */
void tetra_usb_get_netinfo(tetra_usb_t *u, int out[8]);

/* Set the crystal frequency correction in ppm (applied immediately if running,
 * otherwise remembered for the next start). */
void tetra_usb_set_ppm(tetra_usb_t *u, int ppm);

/*
 * Select the analysis (channel) frequency in Hz.  The RTL-SDR keeps its centre
 * frequency (set at start()); the demodulator selects the channel via its NCO
 * so that a carrier away from the centre (and its DC spike) can be received.
 */
void tetra_usb_set_channel(tetra_usb_t *u, uint32_t channel_hz);

/* Retune the RTL-SDR centre frequency (Hz) while running. */
void tetra_usb_set_center(tetra_usb_t *u, uint32_t center_hz);

/* Set the waterfall display span in Hz (clamped to [1 kHz, 1 MHz]). */
void tetra_usb_set_span(tetra_usb_t *u, uint32_t span_hz);

/* Enable/disable the spectrum (waterfall) computation. */
void tetra_usb_set_spectrum_enabled(tetra_usb_t *u, int on);

/* Change the tuner gain (tenths of dB) while running. */
void tetra_usb_set_gain(tetra_usb_t *u, int gain_tenths);

/*
 * Copy the latest fft-shifted PSD (dB) into out[].  Returns the number of bins
 * copied, and stores the FFT size in *nfft and the covered bandwidth (Hz) in
 * *rate_hz.  Returns 0 if no PSD is available yet.
 */
int tetra_usb_get_spectrum(tetra_usb_t *u, float *out, int max_bins,
			   int *nfft, uint32_t *rate_hz);

#endif /* TETRA_USB_H */
