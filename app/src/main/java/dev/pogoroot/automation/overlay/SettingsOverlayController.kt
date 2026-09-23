package dev.pogoroot.automation.overlay

import android.content.Context
import android.view.WindowManager
import dev.pogoroot.automation.config.AutomationConfigRepository

internal class SettingsOverlayController(
    context: Context,
    windowManager: WindowManager,
    repository: AutomationConfigRepository,
    positionStore: OverlayPositionStore,
    private val isForeground: () -> Boolean,
    private val onHideMain: () -> Unit,
    private val onShowMain: () -> Unit,
    private val onSaved: () -> Unit,
) {
    private val overlay = AutomationSettingsOverlay(
        context = context,
        windowManager = windowManager,
        repository = repository,
        positionStore = positionStore,
        onSaved = onSaved,
        onClosed = ::restoreMainOverlay,
    )

    fun ensure() = overlay.ensure()

    fun open() {
        if (!isForeground()) return
        onHideMain()
        overlay.open()
    }

    fun dismiss() = overlay.dismiss()

    fun onConfigurationChanged() = overlay.onConfigurationChanged()

    fun dispose() = overlay.dispose()

    private fun restoreMainOverlay() {
        if (!isForeground()) return
        onShowMain()
    }
}
