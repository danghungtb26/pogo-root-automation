package dev.pogoroot.automation.runtime

import dev.pogoroot.automation.bridge.RuntimeDiscardConfig
import dev.pogoroot.automation.config.HeadlessAutomationConfig
import dev.pogoroot.automation.config.toRuntimeDiscardConfig
import dev.pogoroot.automation.root.RuntimeControlBridge

/** Mirrors the durable Kotlin discard policy into the current native session. */
class DiscardConfigDispatcher(
    private val bridge: RuntimeControlBridge,
) {
    private var appliedSessionId: String? = null
    private var appliedConfig: RuntimeDiscardConfig? = null

    @Synchronized
    fun sync(
        config: HeadlessAutomationConfig,
    ): Result<Unit> = runCatching {
        val ready = bridge.currentRuntimeReady() ?: error("runtime disconnected before discard config")
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
