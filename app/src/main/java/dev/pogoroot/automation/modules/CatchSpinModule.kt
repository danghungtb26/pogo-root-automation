package dev.pogoroot.automation.modules

import dev.pogoroot.automation.bridge.ModuleControlAction
import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.runtime.ExecutionMode
import dev.pogoroot.automation.runtime.ModuleTriggerType
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
    // Periodic: pulls its own SCAN_MAP world snapshot on a cadence.
    triggerType = ModuleTriggerType.PERIODIC,
    // Exclusive: TRY_CATCH/TRY_SPIN drive the world UI; block other modules
    // for the action window (see docs/automation-flow.md — Phần 3.2c).
    executionMode = ExecutionMode.EXCLUSIVE,
    // catch_spin is the ONLY module that depends on the master arm: it activates
    // only while armed AND a catch/spin behaviour is on. This rule lives here, not
    // in any central reducer.
    isActive = { it.armed && (it.config.autoCatch || it.config.autoSpin) },
)
