package dev.pogoroot.automation.runtime

import dev.pogoroot.automation.bridge.RuntimeCatchSpinConfig
import dev.pogoroot.automation.config.HeadlessAutomationConfig
import dev.pogoroot.automation.config.toRuntimeCatchSpinConfig
import dev.pogoroot.automation.root.RuntimeControlBridge

/**
 * Synchronizes the complete Kotlin config snapshot with the native catch_spin
 * session. The Android repository remains durable; this class owns only the
 * edge-triggered runtime mirror and its session/revision cache.
 */
class CatchSpinConfigDispatcher(
    private val bridge: RuntimeControlBridge,
) {
    private var appliedSessionId: String? = null
    private var appliedConfig: RuntimeCatchSpinConfig? = null

    @Synchronized
    fun sync(
        config: HeadlessAutomationConfig,
        armed: Boolean,
    ): Result<Unit> = runCatching {
        val ready = bridge.currentRuntimeReady() ?: error("runtime disconnected before catch-spin config")
        val desired = config.toRuntimeCatchSpinConfig(armed)
        if (appliedSessionId == ready.runtimeSessionId && appliedConfig == desired) {
            return@runCatching
        }

        bridge.setCatchSpinConfig(desired).getOrThrow()
        appliedSessionId = ready.runtimeSessionId
        appliedConfig = desired
    }

    @Synchronized
    fun reset() {
        appliedSessionId = null
        appliedConfig = null
    }
}
