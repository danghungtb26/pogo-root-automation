package dev.pogoroot.automation.overlay

import android.content.Context
import android.graphics.Color
import android.view.Gravity
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView
import io.github.controlwear.virtual.joystick.android.JoystickView

internal class JoystickPadView(
    context: Context,
    onMove: (angle: Int, strength: Int) -> Unit,
    onClose: () -> Unit,
) : LinearLayout(context) {
    init {
        orientation = VERTICAL
        val padding = context.dp(8)
        setPadding(padding, padding, padding, padding)
        background = context.roundedBackground(0xE6202124.toInt(), 14)

        addView(TextView(context).apply {
            text = "×"
            textSize = 20f
            setTextColor(Color.WHITE)
            gravity = Gravity.END or Gravity.CENTER_VERTICAL
            minHeight = context.dp(40)
            contentDescription = "Close joystick pad"
            setOnClickListener { onClose() }
        }, LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, context.dp(40)))

        addView(JoystickView(context).apply {
            layoutParams = LayoutParams(context.dp(190), context.dp(190)).apply {
                gravity = Gravity.CENTER_HORIZONTAL
            }
            contentDescription = "Joystick pad"
            setOnMoveListener(object : JoystickView.OnMoveListener {
                override fun onMove(angle: Int, strength: Int) {
                    onMove(angle, strength)
                }
            })
        })
    }
}
