package dev.pogoroot.automation.headless.modules

import dev.pogoroot.automation.bridge.ModuleControlAction
import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.headless.RuntimeFeatureModuleDescriptor

/**
 * catch_spin: map targeting — encounter open, catch, spin, direct-map catch, and
 * the SCAN_MAP world-snapshot pull. Mirrors the native modules/catch_spin ownership.
 */
val CatchSpinModuleDescriptor = RuntimeFeatureModuleDescriptor(
    module = RuntimeFeatureModule.CATCH_SPIN,
    gameplayActionTags = setOf(2, 3, 4, 9, 10, 11, 12),
    controlActions = setOf(ModuleControlAction.SCAN_MAP),
    isDesired = { config ->
        config.autoEncounter && (config.autoCatch || config.autoSpin)
    },
)
