package dev.pogoroot.automation.headless

import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.core.automation.CurvePreference
import dev.pogoroot.automation.core.automation.ThrowQualityTarget

enum class RuntimeFeatureModuleState {
    DISABLED,
    ENABLING,
    ENABLED,
    DISABLING,
    UNAVAILABLE,
    ERROR,
}

data class RuntimeFeatureModuleSnapshot(
    val module: RuntimeFeatureModule,
    val desired: Boolean,
    val state: RuntimeFeatureModuleState,
    val lastError: String? = null,
)

/**
 * Convert user-facing switches into native feature-group intent.
 *
 * CATCH_SPIN owns map targeting: encounter open, catch, and spin. ENCOUNTER owns
 * in-encounter assistance (berry, throw profile/excellent; future guaranteed/AR).
 * It does not request or represent a guaranteed server-side capture result.
 */
fun HeadlessAutomationConfig.desiredRuntimeFeatureModules(): Set<RuntimeFeatureModule> = buildSet {
    if (autoCatch || autoSpin || autoEncounter || autoSnapshotDuringEncounter) {
        add(RuntimeFeatureModule.CATCH_SPIN)
    }
    if (autoDiscard) add(RuntimeFeatureModule.DISCARD)
    if (autoTransfer) add(RuntimeFeatureModule.TRANSFER)
    if (
        berryMode != BerryMode.NONE ||
        catchThrowQuality != ThrowQualityTarget.ANY ||
        catchCurvePreference != CurvePreference.ANY
    ) {
        add(RuntimeFeatureModule.ENCOUNTER)
    }
}
