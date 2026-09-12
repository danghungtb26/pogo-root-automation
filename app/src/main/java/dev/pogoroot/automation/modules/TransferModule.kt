package dev.pogoroot.automation.modules

import dev.pogoroot.automation.bridge.ModuleControlAction
import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.runtime.ExecutionMode
import dev.pogoroot.automation.runtime.ModuleTriggerType
import dev.pogoroot.automation.runtime.RuntimeFeatureModuleDescriptor

/** Native transfer owns post-catch metadata, filtering, release and Promise polling. */
val TransferModuleDescriptor = RuntimeFeatureModuleDescriptor(
    module = RuntimeFeatureModule.TRANSFER,
    gameplayActionTags = setOf(6),
    controlActions = setOf(ModuleControlAction.TRANSFER_CONFIG_SET),
    // Reactive: native is triggered by the authoritative catch Promise.
    triggerType = ModuleTriggerType.REACTIVE,
    // Concurrent at the controller level; native transfer serializes its own
    // Promise and temporarily gates catch-spin while release is unresolved.
    executionMode = ExecutionMode.CONCURRENT,
    // Ignores the master arm.
    isActive = { it.config.autoTransfer },
)
