package dev.pogoroot.automation.overlay

import android.content.Context
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import kotlin.math.abs

internal class OverlayDragHandler(
    context: Context,
    private val readPosition: () -> OverlayPosition,
    private val writePosition: (OverlayPosition) -> Unit,
    private val onMove: () -> Unit,
    private val onDrop: () -> Unit,
) {
    private val touchSlop = ViewConfiguration.get(context).scaledTouchSlop

    fun attachTo(view: View) {
        var downX = 0f
        var downY = 0f
        var initialPosition = OverlayPosition(0, 0)
        var dragging = false

        view.setOnTouchListener { touchedView, event ->
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    downX = event.rawX
                    downY = event.rawY
                    initialPosition = readPosition()
                    dragging = false
                    true
                }

                MotionEvent.ACTION_MOVE -> {
                    val dx = event.rawX - downX
                    val dy = event.rawY - downY
                    if (!dragging && (abs(dx) > touchSlop || abs(dy) > touchSlop)) {
                        dragging = true
                    }
                    if (dragging) {
                        writePosition(
                            OverlayPosition(
                                x = initialPosition.x + dx.toInt(),
                                y = initialPosition.y + dy.toInt(),
                            ),
                        )
                        onMove()
                    }
                    true
                }

                MotionEvent.ACTION_UP -> {
                    if (dragging) {
                        onDrop()
                    } else {
                        touchedView.performClick()
                    }
                    true
                }

                MotionEvent.ACTION_CANCEL -> {
                    dragging = false
                    true
                }

                else -> false
            }
        }
    }
}
