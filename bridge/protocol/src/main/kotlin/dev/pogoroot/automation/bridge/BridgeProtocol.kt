package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.NearbySnapshot

object BridgeProtocol {
    const val VERSION = 1
}

sealed interface BridgeEvent {
    val protocolVersion: Int

    data class RuntimeStatus(
        override val protocolVersion: Int = BridgeProtocol.VERSION,
        val processName: String,
        val gameVersion: String?,
        val lifecycleState: GameLifecycleState,
    ) : BridgeEvent

    data class NearbyUpdated(
        override val protocolVersion: Int = BridgeProtocol.VERSION,
        val snapshot: NearbySnapshot,
    ) : BridgeEvent

    data class RuntimeError(
        override val protocolVersion: Int = BridgeProtocol.VERSION,
        val code: String,
        val message: String,
    ) : BridgeEvent
}
