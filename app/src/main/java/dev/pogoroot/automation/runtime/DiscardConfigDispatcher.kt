package dev.pogoroot.automation.runtime

import dev.pogoroot.automation.bridge.RuntimeDiscardConfig
import dev.pogoroot.automation.config.HeadlessAutomationConfig
import dev.pogoroot.automation.config.toRuntimeDiscardConfig
import dev.pogoroot.automation.root.RuntimeBridgeClient

/** Mirrors the durable Kotlin discard policy into the current native session. */
class DiscardConfigDispatcher(
    private val bridge: RuntimeBridgeClient,
) {
    private var appliedSessionId: String? = null
    private var appliedConfig: RuntimeDiscardConfig? = null

    @Synchronized
    fun sync(
        config: HeadlessAutomationConfig,
        moduleEnabled: Boolean,
    ): Result<Unit> = runCatching {
        if (!moduleEnabled) return@runCatching
        val ready = bridge.currentRuntimeReady() ?: return@runCatching
        val desired = config.toRuntimeDiscardConfig()
        if (appliedSessionId == ready.runtimeSessionId && appliedConfig == desired) {
            return@runCatching
        }

        bridge.setDiscardConfig(desired).getOrThrow()
        appliedSessionId = ready.runtimeSessionId
        appliedConfig = desired
    }

    @Synchronized
    fun reset() {
        appliedSessionId = null
        appliedConfig = null
    }
}
