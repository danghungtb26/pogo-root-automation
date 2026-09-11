package dev.pogoroot.automation.modules

import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.core.automation.CurvePreference
import dev.pogoroot.automation.core.automation.ThrowQualityTarget
import dev.pogoroot.automation.config.BerryMode
import dev.pogoroot.automation.runtime.RuntimeFeatureModuleDescriptor

/**
 * encounter: the encounter-based flow — open encounter (2), catch throw (3 default,
 * 9 close-preview, 10 structured throw), berry assist (8), GO snapshot (11).
 * Does not represent a guaranteed server-side capture result. Mirrors the native
 * modules/encounter ownership.
 */
val EncounterModuleDescriptor = RuntimeFeatureModuleDescriptor(
    module = RuntimeFeatureModule.ENCOUNTER,
    gameplayActionTags = setOf(2, 3, 8, 9, 10, 11),
    controlActions = emptySet(),
    isDesired = { config ->
        config.berryMode != BerryMode.NONE ||
            config.catchThrowQuality != ThrowQualityTarget.ANY ||
            config.catchCurvePreference != CurvePreference.ANY
    },
)
