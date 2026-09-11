package dev.pogoroot.automation.headless

import dev.pogoroot.automation.core.model.GameLifecycleState

data class StructuredAutomationTick(
    val runtimeSessionId: String?,
    val strongIdentityVerified: Boolean = false,
    val runtimeCapabilities: Set<String> = emptySet(),
    val mutationPermissionGranted: Boolean = false,
    val lifecycleState: GameLifecycleState,
    val observationSeq: Long? = null,
    val lastAction: String? = null,
    val lastError: String? = null,
    val suspended: Boolean = false,
    val submitted: Boolean = false,
)
