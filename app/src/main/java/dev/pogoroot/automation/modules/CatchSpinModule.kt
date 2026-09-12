package dev.pogoroot.automation.modules

import dev.pogoroot.automation.bridge.ModuleControlAction
import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.runtime.ExecutionMode
import dev.pogoroot.automation.runtime.ModuleTriggerType
import dev.pogoroot.automation.runtime.RuntimeFeatureModuleDescriptor

/**
 * catch_spin is fully native-owned: the native observer reads map/inventory,
 * filters targets, and invokes direct catch/spin. Kotlin only enables the module
 * and sends its revisioned configuration snapshot.
 */
val CatchSpinModuleDescriptor = RuntimeFeatureModuleDescriptor(
    module = RuntimeFeatureModule.CATCH_SPIN,
    gameplayActionTags = emptySet(),
    controlActions = setOf(ModuleControlAction.CATCH_SPIN_CONFIG_SET),
    // Native observer ticks own the trigger and action window.
    triggerType = ModuleTriggerType.REACTIVE,
    // Native direct catch/spin drive the world UI and are exclusive with other
    // gameplay mutations.
    executionMode = ExecutionMode.EXCLUSIVE,
    // catch_spin is the ONLY module that depends on the master arm: it activates
    // only while armed AND a catch/spin behaviour is on. This rule lives here, not
    // in any central reducer.
    isActive = {
        it.armed && (
            (it.config.autoCatch && it.config.catchAll) ||
                it.config.autoSpin ||
                it.config.autoWalkToFort
            )
    },
)
