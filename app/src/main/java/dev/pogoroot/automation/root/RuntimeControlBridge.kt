package dev.pogoroot.automation.root

import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.RuntimeCatchSpinConfig
import dev.pogoroot.automation.bridge.RuntimeDiscardConfig
import dev.pogoroot.automation.bridge.RuntimeTransferConfig

/** Session control contract shared by the socket transport and lifecycle tests. */
interface RuntimeControlBridge {
    val connected: Boolean
    fun connect(): Result<BridgeEvent.RuntimeReady>
    fun disconnect()
    fun currentRuntimeReady(): BridgeEvent.RuntimeReady?
    fun startRuntime(): Result<Unit>
    fun stopRuntime(): Result<Unit>
    fun requestRuntimeDiagnostic(): Result<Unit>
    fun setCatchSpinConfig(config: RuntimeCatchSpinConfig): Result<Unit>
    fun setTransferConfig(config: RuntimeTransferConfig): Result<Unit>
    fun setDiscardConfig(config: RuntimeDiscardConfig): Result<Unit>
}
