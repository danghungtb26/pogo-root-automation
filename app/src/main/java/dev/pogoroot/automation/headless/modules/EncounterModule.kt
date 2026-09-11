package dev.pogoroot.automation.headless.modules

import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.core.automation.CurvePreference
import dev.pogoroot.automation.core.automation.ThrowQualityTarget
import dev.pogoroot.automation.headless.BerryMode
import dev.pogoroot.automation.headless.RuntimeFeatureModuleDescriptor

/**
 * encounter: in-encounter assistance — berry, throw profile/excellent (future AR).
 * Does not request or represent a guaranteed server-side capture result. Mirrors
 * the native modules/encounter ownership.
 */
val EncounterModuleDescriptor = RuntimeFeatureModuleDescriptor(
    module = RuntimeFeatureModule.ENCOUNTER,
    gameplayActionTags = setOf(8),
    controlActions = emptySet(),
    isDesired = { config ->
        config.berryMode != BerryMode.NONE ||
            config.catchThrowQuality != ThrowQualityTarget.ANY ||
            config.catchCurvePreference != CurvePreference.ANY
    },
)
