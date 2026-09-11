package dev.pogoroot.automation.modules

import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.runtime.ExecutionMode
import dev.pogoroot.automation.runtime.ModuleTriggerType
import dev.pogoroot.automation.runtime.RuntimeFeatureModuleDescriptor

/** discard: recycle inventory items. Mirrors the native modules/discard ownership. */
val DiscardModuleDescriptor = RuntimeFeatureModuleDescriptor(
    module = RuntimeFeatureModule.DISCARD,
    gameplayActionTags = setOf(5),
    controlActions = emptySet(),
    // Reactive: triggered by native inventory lifecycle events.
    triggerType = ModuleTriggerType.REACTIVE,
    // Concurrent: operates on inventory, not the world UI — runs alongside transfer.
    executionMode = ExecutionMode.CONCURRENT,
    isDesired = { it.autoDiscard },
)
