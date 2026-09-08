package dev.pogoroot.automation.overlay

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.view.WindowManager

internal fun Context.dp(value: Int): Int =
    (value * resources.displayMetrics.density).toInt()

internal fun Context.roundedBackground(color: Int, radiusDp: Int): GradientDrawable =
    GradientDrawable().apply {
        setColor(color)
        cornerRadius = dp(radiusDp).toFloat()
    }

internal fun overlayWindowType(): Int = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
    WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
} else {
    @Suppress("DEPRECATION")
        WindowManager.LayoutParams.TYPE_PHONE
}

internal fun newOverlayParams(width: Int, height: Int): WindowManager.LayoutParams =
    WindowManager.LayoutParams(
        width,
        height,
        overlayWindowType(),
        WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
            WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL or
            WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or
            WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH,
        android.graphics.PixelFormat.TRANSLUCENT,
    )
