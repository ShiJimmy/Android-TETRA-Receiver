// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Shi Jimmy
package org.tetra.receiver

import android.annotation.SuppressLint
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.method.ScrollingMovementMethod
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.ProgressBar
import android.widget.SeekBar
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.widget.doAfterTextChanged
import kotlin.math.pow
import kotlin.math.roundToInt

class MainActivity : AppCompatActivity() {

    private lateinit var usbManager: UsbManager
    private lateinit var statusText: TextView
    private lateinit var gainLabel: TextView
    private lateinit var spanLabel: TextView
    private lateinit var offsetLabel: TextView
    private lateinit var freqInput: EditText
    private lateinit var channelInput: EditText
    private lateinit var ppmInput: EditText
    private lateinit var gainSeekBar: SeekBar
    private lateinit var spanSeekBar: SeekBar
    private lateinit var rssiBar: ProgressBar
    private lateinit var waterfall: WaterfallView
    private lateinit var logText: TextView
    private lateinit var waterfallCheck: CheckBox

    @Volatile private var audioThread: Thread? = null
    @Volatile private var running = false

    private var spanHz = 25000
    private var waterfallSpan = 0
    private var lastSpanApplied = 0
    private val spectrumPsd = FloatArray(SPECTRUM_NFFT)
    private val spectrumInfo = IntArray(2)

    private val permissionReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                UsbManager.ACTION_USB_DEVICE_ATTACHED -> {
                    val device = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)
                    if (device != null) requestPermission(device)
                }
                ACTION_USB_PERMISSION -> {
                    val device = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)
                    val granted = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)
                    if (granted && device != null) startReceiver(device)
                    else setStatus("USB permission denied")
                }
            }
        }
    }

    private val uiHandler = Handler(Looper.getMainLooper())

    private val applyRunnable = Runnable { applyFrequencies() }

    private val spanApplyRunnable = Runnable { applySpanNow() }

    private val statusPoller = object : Runnable {
        override fun run() {
            if (running) {
                updateStatus()
                uiHandler.postDelayed(this, 1000L)
            }
        }
    }

    private val waterfallPoller = object : Runnable {
        override fun run() {
            if (running) {
                val n = TetraNative.nativeGetSpectrum(spectrumPsd, spectrumInfo)
                if (n > 0) {
                    /* Rows are only meaningful at one span; clear when it
                     * changes so we don't mix pixels from different scales. */
                    if (spanHz != waterfallSpan) {
                        waterfallSpan = spanHz
                        waterfall.clear()
                    }
                    waterfall.setAxis(channelHz(), spanHz)
                    waterfall.setChannelOffset(0)
                    waterfall.push(spectrumPsd, n)
                }
                uiHandler.postDelayed(this, 40L)
            }
        }
    }

    private val logPoller = object : Runnable {
        override fun run() {
            if (running) {
                logText.text = TetraNative.nativeGetLog()
                uiHandler.postDelayed(this, 1000L)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        usbManager = getSystemService(Context.USB_SERVICE) as UsbManager

        statusText = findViewById(R.id.statusText)
        freqInput = findViewById(R.id.freqInput)
        channelInput = findViewById(R.id.channelInput)
        ppmInput = findViewById(R.id.ppmInput)
        gainSeekBar = findViewById(R.id.gainSeekBar)
        spanSeekBar = findViewById(R.id.spanSeekBar)
        gainLabel = findViewById(R.id.gainLabel)
        spanLabel = findViewById(R.id.spanLabel)
        offsetLabel = findViewById(R.id.offsetLabel)
        rssiBar = findViewById(R.id.rssiBar)
        waterfall = findViewById(R.id.waterfall)
        logText = findViewById(R.id.logText)
        logText.movementMethod = ScrollingMovementMethod()
        if (!BuildConfig.DEBUG) {
            /* The native log capture is compiled out of release builds. */
            logText.visibility = View.GONE
            findViewById<TextView>(R.id.logLabel).visibility = View.GONE
        }
        waterfallCheck = findViewById(R.id.waterfallCheck)
        waterfallCheck.setOnCheckedChangeListener { _, checked ->
            if (running) TetraNative.nativeSetSpectrumEnabled(if (checked) 1 else 0)
        }
        val startButton = findViewById<Button>(R.id.startButton)
        val stopButton = findViewById<Button>(R.id.stopButton)

        gainSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                gainLabel.text = if (progress <= 0) "Gain: Auto" else "Gain: %.1f dB".format(progress / 10.0f)
                if (running) TetraNative.nativeSetGain(progress)
            }
            override fun onStartTrackingTouch(seekBar: SeekBar) {}
            override fun onStopTrackingTouch(seekBar: SeekBar) {}
        })

        spanSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                spanHz = progressToSpan(progress)
                spanLabel.text = "Span: " + formatSpan(spanHz)
                scheduleSpanApply()
            }
            override fun onStartTrackingTouch(seekBar: SeekBar) {}
            override fun onStopTrackingTouch(seekBar: SeekBar) { applySpanNow() }
        })
        spanHz = progressToSpan(spanSeekBar.progress)
        spanLabel.text = "Span: " + formatSpan(spanHz)

        freqInput.doAfterTextChanged { updateOffsetLabel(); scheduleApply() }
        channelInput.doAfterTextChanged { updateOffsetLabel(); scheduleApply() }
        updateOffsetLabel()

        startButton.setOnClickListener { begin() }
        stopButton.setOnClickListener { teardown() }

        registerReceiver(permissionReceiver, IntentFilter().apply {
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(ACTION_USB_PERMISSION)
        })
    }

    override fun onDestroy() {
        unregisterReceiver(permissionReceiver)
        teardown()
        super.onDestroy()
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.menu_main, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == R.id.action_about) {
            AlertDialog.Builder(this)
                .setTitle(R.string.about_title)
                .setMessage(R.string.about_text)
                .setPositiveButton(android.R.string.ok, null)
                .show()
            return true
        }
        return super.onOptionsItemSelected(item)
    }

    private fun begin() {
        val device = findDevice()
        if (device == null) {
            setStatus("No RTL-SDR found")
            return
        }
        if (usbManager.hasPermission(device)) {
            startReceiver(device)
        } else {
            setStatus("Requesting USB permission...")
            requestPermission(device)
        }
    }

    private fun requestPermission(device: UsbDevice) {
        val pi = PendingIntent.getBroadcast(this, 0, Intent(ACTION_USB_PERMISSION),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        usbManager.requestPermission(device, pi)
    }

    private fun findDevice(): UsbDevice? =
        usbManager.deviceList.values.firstOrNull { isRtlSdr(it) }

    private fun isRtlSdr(device: UsbDevice): Boolean =
        device.vendorId == 0x0bda && (device.productId == 0x2832 || device.productId == 0x2838)

    private fun parseMhz(v: String, def: Double): Long {
        val mhz = v.toDoubleOrNull() ?: def
        return (mhz * 1_000_000.0).toLong()
    }

    private fun centerHz(): Long = parseMhz(freqInput.text.toString(), 410.000)
    private fun channelHz(): Long = parseMhz(channelInput.text.toString(), 409.500)

    private fun updateOffsetLabel() {
        val off = channelHz() - centerHz()
        offsetLabel.text = "Offset: %+.1f kHz".format(off / 1000.0)
    }

    /** Debounced live apply of centre/channel while running. */
    private fun scheduleApply() {
        uiHandler.removeCallbacks(applyRunnable)
        uiHandler.postDelayed(applyRunnable, 600L)
    }

    private fun applyFrequencies() {
        if (!running) return
        TetraNative.nativeSetCenter(centerHz())
        TetraNative.nativeSetChannel(channelHz())
    }

    /** Debounced so a drag doesn't trigger a spectrum rebuild per pixel. */
    private fun scheduleSpanApply() {
        uiHandler.removeCallbacks(spanApplyRunnable)
        uiHandler.postDelayed(spanApplyRunnable, 200L)
    }

    private fun applySpanNow() {
        if (!running) return
        if (spanHz == lastSpanApplied) return
        lastSpanApplied = spanHz
        TetraNative.nativeSetSpectrumSpan(spanHz)
    }

    private fun startReceiver(device: UsbDevice) {
        teardown()

        val connection = usbManager.openDevice(device)
        if (connection == null) {
            setStatus("Failed to open USB device")
            return
        }

        val fd = connection.fileDescriptor
        val center = centerHz()
        val channel = channelHz()
        val gainTenths = gainSeekBar.progress
        val ppm = ppmInput.text.toString().toIntOrNull() ?: 0

        if (TetraNative.nativeInit(fd) != 0) {
            connection.close()
            setStatus("nativeInit failed")
            return
        }
        TetraNative.nativeSetPpm(ppm)
        TetraNative.nativeSetSpectrumSpan(spanHz)
        lastSpanApplied = spanHz
        waterfallSpan = 0
        TetraNative.nativeSetSpectrumEnabled(if (waterfallCheck.isChecked) 1 else 0)
        TetraNative.nativeClearLog()
        val rc = TetraNative.nativeStart(center, gainTenths)
        if (rc != 0) {
            TetraNative.nativeStop()
            connection.close()
            setStatus("nativeStart failed (code $rc)")
            return
        }
        TetraNative.nativeSetChannel(channel)

        running = true
        waterfall.clear()
        startAudioThread()
        uiHandler.post(statusPoller)
        uiHandler.post(waterfallPoller)
        if (BuildConfig.DEBUG) uiHandler.post(logPoller)
    }

    private fun startAudioThread() {
        val minBuf = AudioTrack.getMinBufferSize(8000,
            AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT)
        val track = AudioTrack(AudioManager.STREAM_MUSIC, 8000,
            AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT,
            minBuf, AudioTrack.MODE_STREAM)

        audioThread = Thread {
            track.play()
            val buf = ShortArray(480)
            while (running) {
                val n = TetraNative.nativeReadPcm(buf, buf.size)
                if (n > 0) track.write(buf, 0, n)
                else Thread.sleep(10)
            }
            track.stop()
            track.release()
        }.apply { start() }
    }

    @SuppressLint("SetTextI18n")
    private fun updateStatus() {
        val info = TetraNative.nativeGetNetworkInfo()
        if (info.size < 8) return

        val locked = info[0]
        val mcc = info[1]
        val mnc = info[2]
        val cc = info[3]
        val rssiDb = info[4] / 100.0f
        val inCall = info[6]
        val ssi = info[7]

        val call = if (inCall == 1)
            (if (ssi > 0) "Calling (SSI $ssi)" else "Call active")
        else "Idle"

        statusText.text = "Sync: %s   MCC=%d  MNC=%d  CC=%d\nCentre=%.3f MHz  Chan=%.3f MHz\nCall: %s   RSSI=%.1f dB"
            .format(
                if (locked == 1) "LOCKED" else "searching",
                mcc, mnc, cc,
                centerHz() / 1_000_000.0, channelHz() / 1_000_000.0,
                call, rssiDb
            )

        val bar = ((rssiDb + 60.0f) / 60.0f * 100.0f).coerceIn(0.0f, 100.0f).toInt()
        rssiBar.progress = bar
    }

    private fun teardown() {
        running = false
        audioThread?.join(1000)
        audioThread = null
        TetraNative.nativeStop()
        rssiBar.progress = 0
    }

    private fun setStatus(text: String) {
        statusText.text = text
    }

    /** SeekBar 0..1000 -> span 1 kHz .. 1 MHz (logarithmic). */
    private fun progressToSpan(p: Int): Int {
        val hz = 10.0.pow(3.0 + 3.0 * p / 1000.0)
        return hz.roundToInt().coerceIn(1000, 1_000_000)
    }

    private fun formatSpan(hz: Int): String =
        if (hz >= 1_000_000) "%.3f MHz".format(hz / 1_000_000.0)
        else if (hz >= 1000) "%.1f kHz".format(hz / 1000.0)
        else "$hz Hz"

    companion object {
        private const val ACTION_USB_PERMISSION = "org.tetra.receiver.USB_PERMISSION"
        private const val SPECTRUM_NFFT = 2048
    }
}
