// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Shi Jimmy
package org.tetra.receiver

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.util.AttributeSet
import android.view.View

/**
 * Scrolling spectrum waterfall.
 *
 * Each PSD row is pushed from the UI thread; the bitmap is scrolled down by one
 * pixel and the new row is drawn at the top.  The dB range is auto-scaled per
 * row (peak - 60 dB .. peak) so weak signals stay visible.
 */
class WaterfallView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0
) : View(context, attrs, defStyle) {

    private var fg: Bitmap? = null
    private var bg: Bitmap? = null
    private var fgCanvas: Canvas? = null
    private var bgCanvas: Canvas? = null
    private var rowBm: Bitmap? = null
    private var rowPixels: IntArray = IntArray(0)

    private val blitPaint = Paint()
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 24f
        setShadowLayer(3f, 0f, 0f, Color.BLACK)
    }
    private val srcRect = Rect()
    private val dstRect = Rect()

    private var centerHz = 0L
    private var spanHz = 25000
    private var channelOffsetHz = 0

    private var hasData = false

    private val palette = IntArray(256).also { p ->
        for (i in 0..255) p[i] = gradientColor(i / 255f)
    }

    private val markerPaint = Paint().apply {
        color = Color.rgb(255, 80, 80)
        strokeWidth = 3f
        style = Paint.Style.STROKE
    }

    fun setAxis(centerHz: Long, spanHz: Int) {
        this.centerHz = centerHz
        this.spanHz = spanHz
        invalidate()
    }

    /** Channel frequency relative to the centre, in Hz. */
    fun setChannelOffset(offsetHz: Int) {
        this.channelOffsetHz = offsetHz
    }

    /** Push one PSD row (fft-shifted dB values). */
    fun push(psd: FloatArray, count: Int) {
        if (count <= 0) return
        val w = width
        if (w <= 0) return
        ensureBitmaps(w, height)
        val bc = bgCanvas ?: return

        // find peak for auto-scaling
        var peak = -1000f
        for (i in 0 until count) if (psd[i] > peak) peak = psd[i]
        if (peak < -200f) peak = -200f
        val floor = peak - 60f
        val inv = 255f / (peak - floor)

        if (rowPixels.size != count) rowPixels = IntArray(count)
        for (i in 0 until count) {
            var t = (psd[i] - floor) * inv
            if (t < 0f) t = 0f
            if (t > 255f) t = 255f
            rowPixels[i] = palette[t.toInt()]
        }
        if (rowBm == null || rowBm!!.width != count) {
            rowBm = Bitmap.createBitmap(count, 1, Bitmap.Config.ARGB_8888)
        }
        rowBm!!.setPixels(rowPixels, 0, count, 0, 0, count, 1)

        // scroll previous frame down by 1 px, draw new row at the top
        bc.drawColor(Color.BLACK)
        fg?.let { bc.drawBitmap(it, 0f, 1f, null) }
        rowBm?.let {
            srcRect.set(0, 0, count, 1)
            dstRect.set(0, 0, w, 1)
            bc.drawBitmap(it, srcRect, dstRect, blitPaint)
        }

        val tb = fg; fg = bg; bg = tb
        val tc = fgCanvas; fgCanvas = bgCanvas; bgCanvas = tc
        hasData = true
        invalidate()
    }

    fun clear() {
        fgCanvas?.drawColor(Color.BLACK)
        bgCanvas?.drawColor(Color.BLACK)
        hasData = false
        invalidate()
    }

    private fun ensureBitmaps(w: Int, h: Int) {
        if (w <= 0 || h <= 0) return
        val cur = fg
        if (cur != null && cur.width == w && cur.height == h) return
        fg = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        bg = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        fgCanvas = Canvas(fg!!)
        bgCanvas = Canvas(bg!!)
        fgCanvas!!.drawColor(Color.BLACK)
        bgCanvas!!.drawColor(Color.BLACK)
        hasData = false
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        ensureBitmaps(w, h)
    }

    override fun onDraw(canvas: Canvas) {
        ensureBitmaps(width, height)
        if (hasData) fg?.let { canvas.drawBitmap(it, 0f, 0f, null) }
        drawChannelMarker(canvas)
        drawAxis(canvas)
    }

    private fun drawChannelMarker(canvas: Canvas) {
        val w = width
        val h = height
        if (w <= 0 || spanHz <= 0) return
        val half = spanHz / 2
        if (channelOffsetHz < -half || channelOffsetHz > half) return
        val x = (channelOffsetHz + half).toFloat() / spanHz * w
        canvas.drawLine(x, 0f, x, h.toFloat(), markerPaint)
    }

    private fun drawAxis(canvas: Canvas) {
        val w = width
        val h = height
        if (w <= 0) return
        val half = spanHz / 2
        /* lower bound, centre, upper bound — only three labels fit at 1 kHz
         * resolution on a phone-width display */
        val ticks = 2
        for (i in 0..ticks) {
            val x = w.toFloat() * i / ticks
            val f = centerHz - half + (spanHz.toLong() * i / ticks)
            val label = formatHz(f)
            textPaint.textAlign = when (i) {
                0 -> Paint.Align.LEFT
                ticks -> Paint.Align.RIGHT
                else -> Paint.Align.CENTER
            }
            canvas.drawText(label, x.coerceIn(0f, w.toFloat()), h - 6f, textPaint)
        }
    }

    private fun formatHz(hz: Long): String {
        /* MHz to 3 decimals = 1 kHz resolution */
        return "%.3f".format(hz / 1_000_000.0)
    }

    private fun gradientColor(t: Float): Int {
        // 5 stops: black -> blue -> cyan -> green -> yellow -> red
        val stops = arrayOf(
            intArrayOf(0, 0, 0),
            intArrayOf(0, 0, 160),
            intArrayOf(0, 180, 200),
            intArrayOf(0, 200, 0),
            intArrayOf(230, 220, 0),
            intArrayOf(255, 40, 0)
        )
        val s = (t * (stops.size - 1)).coerceIn(0f, stops.size - 1.001f)
        val i = s.toInt()
        val f = s - i
        val a = stops[i]
        val b = stops[i + 1]
        return Color.rgb(
            (a[0] + (b[0] - a[0]) * f).toInt(),
            (a[1] + (b[1] - a[1]) * f).toInt(),
            (a[2] + (b[2] - a[2]) * f).toInt()
        )
    }
}
