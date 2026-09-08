package dev.pogoroot.automation.overlay

import android.content.Context
import android.graphics.Typeface
import android.view.Gravity
import android.view.View
import android.view.WindowManager
import android.widget.TextView

/** Owns the draggable cooldown badge shown independently from the main menu. */
internal class CooldownOverlayView(
    private val context: Context,
    private val windowManager: WindowManager,
    private val positionStore: OverlayPositionStore,
) {
    private val widthPx = context.dp(WIDTH_DP)
    private val heightPx = context.dp(HEIGHT_DP)
    private val edgeMarginPx = context.dp(EDGE_MARGIN_DP)

    private lateinit var view: TextView
    private lateinit var windowParams: WindowManager.LayoutParams
    private var gameVisible = false

    fun ensure() {
        if (::view.isInitialized) return

        view = TextView(context).apply {
            gravity = Gravity.CENTER
            textSize = 16f
            typeface = Typeface.MONOSPACE
            setTextColor(0xFFFFE082.toInt())
            background = context.roundedBackground(0xE6202124.toInt(), 12)
            setPadding(context.dp(6), 0, context.dp(6), 0)
            contentDescription = "Cooldown"
            visibility = View.INVISIBLE
        }
        val defaultX = (displayWidth() - widthPx - edgeMarginPx).coerceAtLeast(0)
        val savedPosition = positionStore.loadCooldownPosition(
            OverlayPosition(defaultX, edgeMarginPx),
        )
        windowParams = newOverlayParams(widthPx, heightPx).apply {
            gravity = Gravity.TOP or Gravity.START
            x = savedPosition.x
            y = savedPosition.y
        }
        clampPosition()
        windowManager.addView(view, windowParams)
        OverlayDragHandler(
            context = context,
            readPosition = { OverlayPosition(windowParams.x, windowParams.y) },
            writePosition = { position ->
                windowParams.x = position.x
                windowParams.y = position.y
                clampPosition()
            },
            onMove = { runCatching { windowManager.updateViewLayout(view, windowParams) } },
            onDrop = {
                positionStore.persistCooldownPosition(
                    OverlayPosition(windowParams.x, windowParams.y),
                )
            },
        ).attachTo(view)
    }

    fun render(remainingMillis: Long) {
        if (!::view.isInitialized) return
        if (!gameVisible || remainingMillis <= 0L) {
            view.visibility = View.INVISIBLE
            return
        }
        val totalMinutes = (remainingMillis + MILLIS_PER_MINUTE - 1L) / MILLIS_PER_MINUTE
        val hours = totalMinutes / MINUTES_PER_HOUR
        val minutes = totalMinutes % MINUTES_PER_HOUR
        val text = String.format(java.util.Locale.US, "%02d:%02d", hours, minutes)
        view.text = text
        view.contentDescription = "Cooldown $text"
        view.visibility = View.VISIBLE
    }

    fun setVisible(visible: Boolean) {
        gameVisible = visible
        if (!::view.isInitialized) return
        view.visibility = if (visible) view.visibility else View.INVISIBLE
        windowParams.flags = if (visible) {
            windowParams.flags and WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE.inv()
        } else {
            windowParams.flags or WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
        }
        runCatching { windowManager.updateViewLayout(view, windowParams) }
    }

    fun onConfigurationChanged() {
        if (!::view.isInitialized) return
        clampPosition()
        positionStore.persistCooldownPosition(OverlayPosition(windowParams.x, windowParams.y))
        runCatching { windowManager.updateViewLayout(view, windowParams) }
    }

    fun dispose() {
        if (!::view.isInitialized) return
        runCatching { windowManager.removeView(view) }
    }

    private fun clampPosition() {
        windowParams.x = windowParams.x.coerceIn(0, (displayWidth() - widthPx).coerceAtLeast(0))
        windowParams.y = windowParams.y.coerceIn(0, (displayHeight() - heightPx).coerceAtLeast(0))
    }

    private fun displayWidth(): Int = context.resources.displayMetrics.widthPixels

    private fun displayHeight(): Int = context.resources.displayMetrics.heightPixels

    private companion object {
        const val WIDTH_DP = 82
        const val HEIGHT_DP = 44
        const val EDGE_MARGIN_DP = 16
        const val MILLIS_PER_MINUTE = 60_000L
        const val MINUTES_PER_HOUR = 60L
    }
}
