package dev.pogoroot.automation.modules

import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.runtime.ExecutionMode
import dev.pogoroot.automation.runtime.ModuleTriggerType
import dev.pogoroot.automation.runtime.RuntimeFeatureModuleDescriptor

/** transfer: release stored Pokémon. Mirrors the native modules/transfer ownership. */
val TransferModuleDescriptor = RuntimeFeatureModuleDescriptor(
    module = RuntimeFeatureModule.TRANSFER,
    gameplayActionTags = setOf(6),
    controlActions = emptySet(),
    // Reactive: triggered by native storage/inventory lifecycle events.
    triggerType = ModuleTriggerType.REACTIVE,
    // Concurrent: operates on storage, not the world UI — runs alongside discard.
    executionMode = ExecutionMode.CONCURRENT,
    isDesired = { it.autoTransfer },
)
