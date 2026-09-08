package dev.pogoroot.automation.overlay

import android.content.Context
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import dev.pogoroot.automation.R

/** Drag handler for independent scan result overlay windows. */
internal class ScanWidgetDragHandler(context: Context) {
    interface Callbacks {
        fun readPosition(): OverlayPosition
        fun writePosition(position: OverlayPosition)
        fun onMove()
        fun onDrop()
    }

    private val touchSlop = ViewConfiguration.get(context).scaledTouchSlop

    fun attachTo(view: View, callbacks: Callbacks) {
        view.setOnTouchListener { touchedView, event ->
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    touchedView.setTag(R.id.scan_drag_down_x, event.rawX)
                    touchedView.setTag(R.id.scan_drag_down_y, event.rawY)
                    touchedView.setTag(
                        R.id.scan_drag_initial_position,
                        callbacks.readPosition(),
                    )
                    touchedView.setTag(R.id.scan_dragging, false)
                    true
                }

                MotionEvent.ACTION_MOVE -> {
                    val downX = touchedView.getTag(R.id.scan_drag_down_x) as? Float
                    val downY = touchedView.getTag(R.id.scan_drag_down_y) as? Float
                    if (downX == null || downY == null) {
                        false
                    } else {
                        val dx = event.rawX - downX
                        val dy = event.rawY - downY
                        var dragging = touchedView.getTag(R.id.scan_dragging) == true
                        if (!dragging && (kotlin.math.abs(dx) > touchSlop ||
                                kotlin.math.abs(dy) > touchSlop)) {
                            dragging = true
                            touchedView.setTag(R.id.scan_dragging, true)
                        }
                        if (dragging) {
                            val initial = touchedView.getTag(
                                R.id.scan_drag_initial_position,
                            ) as? OverlayPosition
                            if (initial != null) {
                                callbacks.writePosition(
                                    OverlayPosition(initial.x + dx.toInt(), initial.y + dy.toInt()),
                                )
                                callbacks.onMove()
                            }
                        }
                        true
                    }
                }

                MotionEvent.ACTION_UP -> {
                    if (touchedView.getTag(R.id.scan_dragging) == true) {
                        callbacks.onDrop()
                    } else {
                        touchedView.performClick()
                    }
                    true
                }

                MotionEvent.ACTION_CANCEL -> {
                    touchedView.setTag(R.id.scan_dragging, false)
                    true
                }

                else -> false
            }
        }
    }
}
