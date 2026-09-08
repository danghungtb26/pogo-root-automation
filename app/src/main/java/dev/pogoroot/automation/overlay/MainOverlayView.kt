package dev.pogoroot.automation.overlay

import android.content.Context
import android.graphics.Color
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.TextView
import dev.pogoroot.automation.headless.HeadlessAutomationConfig
import dev.pogoroot.automation.location.JoystickLocationController
import kotlin.math.max

/** Owns the floating button, shortcut grid and joystick panel. */
internal class MainOverlayView(
    private val context: Context,
    private val windowManager: WindowManager,
    private val positionStore: OverlayPositionStore,
    private val controller: JoystickLocationController,
    private val speedPresets: DoubleArray,
    private val onToggle: (String) -> Unit,
    private val onTeleport: () -> Unit,
    private val onFavorites: () -> Unit,
    private val onSpeed: () -> Unit,
    private val onSettings: () -> Unit,
    private val onClose: () -> Unit,
) {
    private enum class Mode {
        COLLAPSED,
        SHORTCUTS,
        JOYSTICK,
    }

    private val mainHandler = Handler(Looper.getMainLooper())
    private var mode = Mode.COLLAPSED
    private var mainAnchorX = 0
    private var mainAnchorY = 0
    private var iconSizePx = 0
    private var edgeMarginPx = 0
    private var bottomMarginPx = 0

    private lateinit var shortcutMenu: ShortcutMenuView
    private lateinit var joystickPad: JoystickPadView
    private lateinit var rootView: FrameLayout
    private lateinit var windowParams: WindowManager.LayoutParams
    private lateinit var floatButton: TextView

    fun ensure() {
        if (::rootView.isInitialized) return

        iconSizePx = context.dp(56)
        edgeMarginPx = context.dp(DEFAULT_EDGE_MARGIN_DP)
        bottomMarginPx = context.dp(DEFAULT_BOTTOM_MARGIN_DP)

        floatButton = TextView(context).apply {
            gravity = Gravity.CENTER
            text = "✣"
            textSize = 24f
            setTextColor(Color.WHITE)
            isClickable = true
            isFocusable = true
            contentDescription = "PoGo Tools menu"
            setOnClickListener {
                setMode(
                    when (mode) {
                        Mode.COLLAPSED -> Mode.SHORTCUTS
                        Mode.SHORTCUTS -> Mode.COLLAPSED
                        Mode.JOYSTICK -> Mode.SHORTCUTS
                    },
                )
            }
        }
        OverlayDragHandler(
            context = context,
            readPosition = { OverlayPosition(mainAnchorX, mainAnchorY) },
            writePosition = { position ->
                mainAnchorX = position.x
                mainAnchorY = position.y
                clampMainAnchor()
            },
            onMove = ::relayoutMainOverlay,
            onDrop = ::persistMainPosition,
        ).attachTo(floatButton)

        shortcutMenu = ShortcutMenuView(
            context = context,
            speedPresets = speedPresets,
            onToggle = onToggle,
            onJoystick = { setMode(Mode.JOYSTICK) },
            onTeleport = onTeleport,
            onFavorites = onFavorites,
            onSpeed = onSpeed,
            onSettings = onSettings,
            onClose = onClose,
        )
        joystickPad = JoystickPadView(
            context = context,
            onMove = controller::setJoystick,
            onClose = { setMode(Mode.SHORTCUTS) },
        )

        rootView = FrameLayout(context).apply {
            setBackgroundColor(Color.TRANSPARENT)
            setOnTouchListener { _, event ->
                if (event.actionMasked == MotionEvent.ACTION_OUTSIDE &&
                    mode != Mode.COLLAPSED
                ) {
                    setMode(Mode.COLLAPSED)
                    true
                } else {
                    false
                }
            }
            addView(
                shortcutMenu.view,
                FrameLayout.LayoutParams(context.dp(244), ViewGroup.LayoutParams.WRAP_CONTENT),
            )
            addView(
                joystickPad,
                FrameLayout.LayoutParams(context.dp(214), ViewGroup.LayoutParams.WRAP_CONTENT),
            )
            addView(floatButton, FrameLayout.LayoutParams(iconSizePx, iconSizePx))
        }

        val defaultX = edgeMarginPx
        val defaultY = (displayHeight() - iconSizePx - bottomMarginPx).coerceAtLeast(0)
        val savedPosition = positionStore.loadMainPosition(OverlayPosition(defaultX, defaultY))
        mainAnchorX = savedPosition.x
        mainAnchorY = savedPosition.y
        clampMainAnchor()

        windowParams = newOverlayParams(iconSizePx, iconSizePx).apply {
            gravity = Gravity.TOP or Gravity.START
            x = mainAnchorX
            y = mainAnchorY
        }
        windowManager.addView(rootView, windowParams)
        setMode(Mode.COLLAPSED)
    }

    private fun setMode(next: Mode) {
        mode = next
        if (!::rootView.isInitialized) return

        shortcutMenu.view.visibility = if (mode == Mode.SHORTCUTS) View.VISIBLE else View.GONE
        joystickPad.visibility = if (mode == Mode.JOYSTICK) View.VISIBLE else View.GONE
        floatButton.visibility = View.VISIBLE
        rootView.post(::relayoutMainOverlay)
    }

    fun collapse() {
        setMode(Mode.COLLAPSED)
    }

    fun setVisible(visible: Boolean) {
        if (!::rootView.isInitialized) return
        if (!visible) collapse()
        rootView.visibility = if (visible) View.VISIBLE else View.INVISIBLE
        windowParams.flags = if (visible) {
            windowParams.flags and WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE.inv()
        } else {
            windowParams.flags or WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
        }
        runCatching { windowManager.updateViewLayout(rootView, windowParams) }
    }

    fun render(config: HeadlessAutomationConfig, speedPresetIndex: Int) {
        if (!::rootView.isInitialized) return
        shortcutMenu.render(config, speedPresetIndex)
        floatButton.background = context.roundedBackground(
            if (config.enabled) 0xE62E7D32.toInt() else 0xE6202124.toInt(),
            28,
        )
        floatButton.contentDescription = if (config.enabled) {
            "PoGo Tools menu, automation on"
        } else {
            "PoGo Tools menu, automation off"
        }
    }

    fun onConfigurationChanged() {
        if (!::rootView.isInitialized) return
        mainHandler.post {
            clampMainAnchor()
            persistMainPosition()
            relayoutMainOverlay()
        }
    }

    fun dispose() {
        if (!::rootView.isInitialized) return
        runCatching { windowManager.removeView(rootView) }
    }

    private fun relayoutMainOverlay() {
        if (!::rootView.isInitialized || !::windowParams.isInitialized) return

        if (mode == Mode.COLLAPSED) {
            floatButton.layoutParams = FrameLayout.LayoutParams(iconSizePx, iconSizePx)
            windowParams.width = iconSizePx
            windowParams.height = iconSizePx
            windowParams.x = clamp(mainAnchorX, displayWidth() - iconSizePx)
            windowParams.y = clamp(mainAnchorY, displayHeight() - iconSizePx)
            runCatching { windowManager.updateViewLayout(rootView, windowParams) }
            return
        }

        val panel = if (mode == Mode.SHORTCUTS) shortcutMenu.view else joystickPad
        val panelWidth = panel.layoutParams.width.takeIf { it > 0 } ?: context.dp(214)
        val panelMeasureSpec = View.MeasureSpec.makeMeasureSpec(panelWidth, View.MeasureSpec.EXACTLY)
        panel.measure(panelMeasureSpec, View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
        val panelHeight = panel.measuredHeight
        val rootWidth = panelWidth + context.dp(8) + iconSizePx
        val rootHeight = max(panelHeight, iconSizePx)
        val opensLeft = mainAnchorX > displayWidth() - iconSizePx - context.dp(8) - panelWidth
        val panelLeft = if (opensLeft) 0 else iconSizePx + context.dp(8)
        val floatLeft = if (opensLeft) panelWidth + context.dp(8) else 0
        val rootLeft = if (opensLeft) mainAnchorX - panelWidth - context.dp(8) else mainAnchorX
        val rootTop = mainAnchorY - (rootHeight - iconSizePx) / 2

        panel.layoutParams = FrameLayout.LayoutParams(panelWidth, panelHeight).apply {
            leftMargin = panelLeft
        }
        floatButton.layoutParams = FrameLayout.LayoutParams(iconSizePx, iconSizePx).apply {
            leftMargin = floatLeft
            topMargin = (rootHeight - iconSizePx) / 2
        }
        windowParams.width = rootWidth
        windowParams.height = rootHeight
        windowParams.x = clamp(rootLeft, displayWidth() - rootWidth)
        windowParams.y = clamp(rootTop, displayHeight() - rootHeight)
        runCatching { windowManager.updateViewLayout(rootView, windowParams) }
    }

    private fun persistMainPosition() {
        positionStore.persistMainPosition(OverlayPosition(mainAnchorX, mainAnchorY))
    }

    private fun clampMainAnchor() {
        mainAnchorX = clamp(mainAnchorX, displayWidth() - iconSizePx)
        mainAnchorY = clamp(mainAnchorY, displayHeight() - iconSizePx)
    }

    private fun displayWidth(): Int = context.resources.displayMetrics.widthPixels

    private fun displayHeight(): Int = context.resources.displayMetrics.heightPixels

    private fun clamp(value: Int, maxValue: Int): Int = value.coerceIn(0, max(0, maxValue))

    private companion object {
        const val DEFAULT_EDGE_MARGIN_DP = 16
        const val DEFAULT_BOTTOM_MARGIN_DP = 24
    }
}
