package dev.pogoroot.automation.runtime

import dev.pogoroot.automation.bridge.RuntimeTransferConfig
import dev.pogoroot.automation.config.HeadlessAutomationConfig
import dev.pogoroot.automation.config.toRuntimeTransferConfig
import dev.pogoroot.automation.root.RuntimeControlBridge

/** Mirrors the durable Kotlin transfer policy into the current native session. */
class TransferConfigDispatcher(
    private val bridge: RuntimeControlBridge,
) {
    private var appliedSessionId: String? = null
    private var appliedConfig: RuntimeTransferConfig? = null

    @Synchronized
    fun sync(
        config: HeadlessAutomationConfig,
    ): Result<Unit> = runCatching {
        val ready = bridge.currentRuntimeReady() ?: error("runtime disconnected before transfer config")
        val desired = config.toRuntimeTransferConfig()
        if (appliedSessionId == ready.runtimeSessionId && appliedConfig == desired) {
            return@runCatching
        }

        bridge.setTransferConfig(desired).getOrThrow()
        appliedSessionId = ready.runtimeSessionId
        appliedConfig = desired
    }

    @Synchronized
    fun reset() {
        appliedSessionId = null
        appliedConfig = null
    }
}
