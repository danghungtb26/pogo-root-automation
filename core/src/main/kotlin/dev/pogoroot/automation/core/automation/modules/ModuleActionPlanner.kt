package dev.pogoroot.automation.core.automation.modules

import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.AutomationPolicy
import dev.pogoroot.automation.core.automation.AutomationSnapshot
import dev.pogoroot.automation.core.automation.CatchDecision

/**
 * One planning cycle's inputs, shared across module planners. The coordinator
 * computes cross-module derived values once — e.g. the active-encounter catch
 * decision, needed by both the encounter berry assist and the catch throw — so the
 * per-module planners stay independent and never duplicate a decision.
 */
data class PlanningContext(
    val snapshot: AutomationSnapshot,
    val policy: AutomationPolicy,
    /** Catch decision for the active encounter; null in overworld or when not catching. */
    val encounterCatchDecision: CatchDecision? = null,
)

/**
 * Produces the actions a single feature module wants this cycle. The coordinator
 * runs the planners in a fixed priority order and concatenates the results;
 * cross-module arbitration (ordering here, and one-mutation-at-a-time downstream in
 * AutomationRunner) stays with the coordinator/runner, not the planners.
 *
 * This mirrors the native per-module ownership on the planning side: each module
 * owns what it wants to do; the shared coordinator arbitrates.
 */
interface ModuleActionPlanner {
    fun plan(context: PlanningContext): List<AutomationAction>
}
