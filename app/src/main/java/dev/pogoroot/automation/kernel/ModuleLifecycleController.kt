package dev.pogoroot.automation.kernel

import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.config.HeadlessAutomationConfig
import dev.pogoroot.automation.runtime.RuntimeFeatureModuleCatalog

/**
 * The two independent control axes that gate module activation
 * (see docs/automation-flow.md — Phần 1).
 *
 * - [gameReady]: Pokémon GO is foreground AND the runtime is managed-ready.
 *   Gates EVERY module. When false, all modules stop (GO-absent overrides).
 * - [automationOn]: the master arm (overlay Automation button / AutomationState).
 *   Gates ONLY catch_spin — every other module ignores it.
 */
data class ModuleLifecycleInputs(
    val gameReady: Boolean = false,
    val automationOn: Boolean = false,
)

/** What changed since the last [ModuleLifecycleController.reduce]. */
data class ModuleTransition(
    val toStart: Set<RuntimeFeatureModule>,
    val toStop: Set<RuntimeFeatureModule>,
    val active: Set<RuntimeFeatureModule>,
) {
    val changed: Boolean get() = toStart.isNotEmpty() || toStop.isNotEmpty()
}

/**
 * Pure 2-axis reducer that decides which feature modules should be active, then
 * diffs against the currently-active set to yield start/stop transitions. Holds
 * no runtime resources — the caller performs the actual start/stop.
 *
 * Rule (docs/automation-flow.md — Phần 1):
 *
 *     desiredActive(m) = gameReady && isDesired(config, m) && (m != CATCH_SPIN || automationOn)
 *
 * catch_spin is the only module that also requires [ModuleLifecycleInputs.automationOn];
 * it auto-starts as soon as the arm flips on while GO is present. GO-absent
 * (`gameReady == false`) forces the desired set empty, so it always wins.
 */
class ModuleLifecycleController(
    private val catalog: RuntimeFeatureModuleCatalog = RuntimeFeatureModuleCatalog,
) {
    private var active: Set<RuntimeFeatureModule> = emptySet()

    fun current(): Set<RuntimeFeatureModule> = active

    fun desiredActive(
        inputs: ModuleLifecycleInputs,
        config: HeadlessAutomationConfig,
    ): Set<RuntimeFeatureModule> {
        if (!inputs.gameReady) return emptySet()
        return catalog.descriptors
            .asSequence()
            .filter { it.isDesired(config) }
            .filter { it.module != RuntimeFeatureModule.CATCH_SPIN || inputs.automationOn }
            .mapTo(linkedSetOf()) { it.module }
    }

    fun reduce(
        inputs: ModuleLifecycleInputs,
        config: HeadlessAutomationConfig,
    ): ModuleTransition {
        val desired = desiredActive(inputs, config)
        val transition = ModuleTransition(
            toStart = desired - active,
            toStop = active - desired,
            active = desired,
        )
        active = desired
        return transition
    }

    /** Reset to no-active (e.g. on runtime session loss / teardown). */
    fun reset() {
        active = emptySet()
    }
}
