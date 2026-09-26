// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Shi Jimmy
package org.tetra.receiver

/**
 * Native method declarations matching tetra_jni.c.
 *
 * The native library (libtetra.so) is loaded once; all methods are static and
 * operate on a single global RTL-SDR capture context.
 */
object TetraNative {
    init {
        System.loadLibrary("tetra")
    }

    /** Open the RTL-SDR from an already-opened USB file descriptor. */
    external fun nativeInit(usbFd: Int): Int

    /** Configure (1.8 MSps / centre freq / manual gain) and start capture. */
    external fun nativeStart(freqHz: Long, gainTenths: Int): Int

    /** Stop capture and close the device. */
    external fun nativeStop()

    /** Drain decoded 8 kHz PCM into [outBuf]; returns samples copied. */
    external fun nativeReadPcm(outBuf: ShortArray, maxSamples: Int): Int

    /**
     * [locked, mcc, mnc, colourCode, rssiDb*100, fllHz, inCall, callSsi]
     */
    external fun nativeGetNetworkInfo(): IntArray

    /** Crystal frequency correction in ppm. */
    external fun nativeSetPpm(ppm: Int)

    /**
     * Analysis (channel) frequency in Hz.  The dongle keeps its centre
     * frequency; the demodulator selects this carrier via its NCO.
     */
    external fun nativeSetChannel(channelHz: Long)

    /** Retune the RTL-SDR centre frequency (Hz) while running. */
    external fun nativeSetCenter(centerHz: Long)

    /** Waterfall display span in Hz (clamped to [1 kHz, 1 MHz]). */
    external fun nativeSetSpectrumSpan(spanHz: Int)

    /** Enable/disable the spectrum (waterfall) computation (saves CPU). */
    external fun nativeSetSpectrumEnabled(on: Int)

    /** Change the tuner gain (tenths of dB) while running. */
    external fun nativeSetGain(gainTenths: Int)

    /**
     * Copy the latest PSD (dB, fft-shifted) into [outBuf].
     * [infoBuf] receives [nfft, rateHz].  Returns the number of bins.
     */
    external fun nativeGetSpectrum(outBuf: FloatArray, infoBuf: IntArray): Int

    /** Rolling native debug log (stdout/stderr of the decoder). */
    external fun nativeGetLog(): String

    external fun nativeClearLog()
}
