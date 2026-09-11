package dev.pogoroot.automation.modules

import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.core.automation.CurvePreference
import dev.pogoroot.automation.core.automation.ThrowQualityTarget
import dev.pogoroot.automation.config.BerryMode
import dev.pogoroot.automation.runtime.ExecutionMode
import dev.pogoroot.automation.runtime.ModuleTriggerType
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
    // Periodic: drives the encounter open/throw flow on its own cadence
    // (coupled with catch_spin on the shared overworld target — see Phần 4).
    triggerType = ModuleTriggerType.PERIODIC,
    // Exclusive: encounter open/throw drives the world/encounter UI.
    executionMode = ExecutionMode.EXCLUSIVE,
    // Ignores the master arm — driven purely by encounter-shaping config.
    isActive = {
        it.config.berryMode != BerryMode.NONE ||
            it.config.catchThrowQuality != ThrowQualityTarget.ANY ||
            it.config.catchCurvePreference != CurvePreference.ANY
    },
)
