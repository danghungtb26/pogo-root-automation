package dev.pogoroot.automation.runtime.observation

import dev.pogoroot.automation.core.model.GameLifecycleState

data class RuntimeObservationTick(
    val runtimeSessionId: String?,
    val strongIdentityVerified: Boolean = false,
    val runtimeCapabilities: Set<String> = emptySet(),
    val lifecycleState: GameLifecycleState = GameLifecycleState.DISCONNECTED,
    val observationSeq: Long? = null,
    val lastError: String? = null,
)
