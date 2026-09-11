package dev.pogoroot.automation.modules

import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.runtime.RuntimeFeatureModuleDescriptor

/** transfer: release stored Pokémon. Mirrors the native modules/transfer ownership. */
val TransferModuleDescriptor = RuntimeFeatureModuleDescriptor(
    module = RuntimeFeatureModule.TRANSFER,
    gameplayActionTags = setOf(6),
    controlActions = emptySet(),
    isDesired = { it.autoTransfer },
)
