package dev.pogoroot.automation.modules

import dev.pogoroot.automation.bridge.ModuleControlAction
import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.runtime.RuntimeFeatureModuleDescriptor

/**
 * catch_spin: map automation only — direct-map catch (TRY_CATCH, 12) and spin
 * (TRY_SPIN, 4), plus the SCAN_MAP world-snapshot pull. The encounter-based flow
 * (open/throw/snapshot/berry) belongs to the encounter module. Mirrors the native
 * modules/catch_spin ownership.
 */
val CatchSpinModuleDescriptor = RuntimeFeatureModuleDescriptor(
    module = RuntimeFeatureModule.CATCH_SPIN,
    gameplayActionTags = setOf(4, 12),
    controlActions = setOf(ModuleControlAction.SCAN_MAP),
    isDesired = { config ->
        config.autoCatch || config.autoSpin
    },
)
