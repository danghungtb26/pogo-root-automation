package dev.pogoroot.automation.headless.modules

import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.headless.RuntimeFeatureModuleDescriptor

/** discard: recycle inventory items. Mirrors the native modules/discard ownership. */
val DiscardModuleDescriptor = RuntimeFeatureModuleDescriptor(
    module = RuntimeFeatureModule.DISCARD,
    gameplayActionTags = setOf(5),
    controlActions = emptySet(),
    isDesired = { it.autoDiscard },
)
