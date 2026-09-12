package dev.pogoroot.automation.modules

import dev.pogoroot.automation.bridge.ModuleControlAction
import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.runtime.ExecutionMode
import dev.pogoroot.automation.runtime.ModuleTriggerType
import dev.pogoroot.automation.runtime.RuntimeFeatureModuleDescriptor

/** discard: recycle inventory items. Mirrors the native modules/discard ownership. */
val DiscardModuleDescriptor = RuntimeFeatureModuleDescriptor(
    module = RuntimeFeatureModule.DISCARD,
    gameplayActionTags = setOf(5),
    controlActions = setOf(ModuleControlAction.DISCARD_CONFIG_SET),
    // Periodic: native reads inventory and decides on its own observer cadence.
    triggerType = ModuleTriggerType.PERIODIC,
    // Concurrent: operates on inventory, not the world UI — runs alongside transfer.
    executionMode = ExecutionMode.CONCURRENT,
    // Ignores the master arm.
    isActive = { it.config.autoDiscard },
)
