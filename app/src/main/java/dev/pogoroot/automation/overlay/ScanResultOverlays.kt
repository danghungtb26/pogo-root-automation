package dev.pogoroot.automation.overlay

import android.content.Context
import android.view.Gravity
import android.view.WindowManager
import dev.pogoroot.automation.core.scan.ScanMatchType
import dev.pogoroot.automation.scan.ScanResultRepository

/** Owns the draggable hundo and shiny result widgets. */
internal class ScanResultOverlays(
    private val context: Context,
    private val windowManager: WindowManager,
    private val positionStore: OverlayPositionStore,
    private val scanResultRepository: ScanResultRepository,
    private val onOpenResults: (ScanMatchType) -> Unit,
) {
    private val widgetWidthPx = context.dp(WIDGET_WIDTH_DP)
    private val edgeMarginPx = context.dp(EDGE_MARGIN_DP)

    private lateinit var hundoView: ScanResultOverlayView
    private lateinit var shinyView: ScanResultOverlayView
    private lateinit var hundoParams: WindowManager.LayoutParams
    private lateinit var shinyParams: WindowManager.LayoutParams

    fun ensure() {
        if (::hundoView.isInitialized || ::shinyView.isInitialized) return

        hundoView = ScanResultOverlayView(context, ScanMatchType.HUNDO) {
            onOpenResults(ScanMatchType.HUNDO)
        }
        shinyView = ScanResultOverlayView(context, ScanMatchType.SHINY) {
            onOpenResults(ScanMatchType.SHINY)
        }

        val defaultY = context.dp(DEFAULT_Y_DP)
        val hundoDefault = positionStore.loadHundoPosition(
            OverlayPosition(edgeMarginPx, defaultY),
        )
        val shinyDefault = positionStore.loadShinyPosition(
            OverlayPosition(edgeMarginPx + widgetWidthPx + context.dp(WIDGET_GAP_DP), defaultY),
        )
        hundoParams = newOverlayParams(
            widgetWidthPx,
            context.dp(INITIAL_HEIGHT_DP),
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            x = hundoDefault.x
            y = hundoDefault.y
        }
        shinyParams = newOverlayParams(
            widgetWidthPx,
            context.dp(INITIAL_HEIGHT_DP),
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            x = shinyDefault.x
            y = shinyDefault.y
        }
        clampWidget(hundoParams, hundoView)
        clampWidget(shinyParams, shinyView)
        windowManager.addView(hundoView, hundoParams)
        windowManager.addView(shinyView, shinyParams)

        val dragHandler = ScanWidgetDragHandler(context)
        dragHandler.attachTo(hundoView, widgetCallbacks(
            view = hundoView,
            params = hundoParams,
            persist = { positionStore.persistHundoPosition(it) },
        ))
        dragHandler.attachTo(shinyView, widgetCallbacks(
            view = shinyView,
            params = shinyParams,
            persist = { positionStore.persistShinyPosition(it) },
        ))
    }

    fun render() {
        if (!::hundoView.isInitialized || !::shinyView.isInitialized) return
        hundoView.render(scanResultRepository.read(ScanMatchType.HUNDO))
        shinyView.render(scanResultRepository.read(ScanMatchType.SHINY))
        resizeWidget(hundoView, hundoParams)
        resizeWidget(shinyView, shinyParams)
    }

    fun onConfigurationChanged() {
        if (!::hundoView.isInitialized || !::shinyView.isInitialized) return
        clampWidget(hundoParams, hundoView)
        clampWidget(shinyParams, shinyView)
        positionStore.persistHundoPosition(OverlayPosition(hundoParams.x, hundoParams.y))
        positionStore.persistShinyPosition(OverlayPosition(shinyParams.x, shinyParams.y))
        runCatching {
            windowManager.updateViewLayout(hundoView, hundoParams)
            windowManager.updateViewLayout(shinyView, shinyParams)
        }
    }

    fun dispose() {
        if (::hundoView.isInitialized) runCatching { windowManager.removeView(hundoView) }
        if (::shinyView.isInitialized) runCatching { windowManager.removeView(shinyView) }
    }

    private fun widgetCallbacks(
        view: ScanResultOverlayView,
        params: WindowManager.LayoutParams,
        persist: (OverlayPosition) -> Unit,
    ): ScanWidgetDragHandler.Callbacks = object : ScanWidgetDragHandler.Callbacks {
        override fun readPosition(): OverlayPosition = OverlayPosition(params.x, params.y)

        override fun writePosition(position: OverlayPosition) {
            params.x = position.x
            params.y = position.y
            clampWidget(params, view)
        }

        override fun onMove() {
            runCatching { windowManager.updateViewLayout(view, params) }
        }

        override fun onDrop() = persist(OverlayPosition(params.x, params.y))
    }

    private fun resizeWidget(
        view: ScanResultOverlayView,
        params: WindowManager.LayoutParams,
    ) {
        params.width = widgetWidthPx
        params.height = view.desiredHeightPx().coerceAtLeast(context.dp(INITIAL_HEIGHT_DP))
        clampWidget(params, view)
        runCatching { windowManager.updateViewLayout(view, params) }
    }

    private fun clampWidget(
        params: WindowManager.LayoutParams,
        view: ScanResultOverlayView,
    ) {
        params.width = widgetWidthPx
        params.height = view.desiredHeightPx().coerceAtLeast(context.dp(INITIAL_HEIGHT_DP))
        params.x = params.x.coerceIn(0, (displayWidth() - params.width).coerceAtLeast(0))
        params.y = params.y.coerceIn(0, (displayHeight() - params.height).coerceAtLeast(0))
    }

    private fun displayWidth(): Int = context.resources.displayMetrics.widthPixels

    private fun displayHeight(): Int = context.resources.displayMetrics.heightPixels

    private companion object {
        const val WIDGET_WIDTH_DP = 50
        const val INITIAL_HEIGHT_DP = 72
        const val WIDGET_GAP_DP = 8
        const val EDGE_MARGIN_DP = 16
        const val DEFAULT_Y_DP = 120
    }
}
