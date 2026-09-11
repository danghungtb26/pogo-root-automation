package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.automation.modules.CatchSpinActionPlanner
import dev.pogoroot.automation.core.automation.modules.DiscardActionPlanner
import dev.pogoroot.automation.core.automation.modules.EncounterActionPlanner
import dev.pogoroot.automation.core.automation.modules.ModuleActionPlanner
import dev.pogoroot.automation.core.automation.modules.PlanningContext
import dev.pogoroot.automation.core.automation.modules.TransferActionPlanner
import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.FortSnapshot
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.InventorySnapshot
import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.NearbySpawn
import dev.pogoroot.automation.core.model.PokemonStorageSnapshot

data class AutomationSnapshot(
    val lifecycleState: GameLifecycleState,
    val nearby: NearbySnapshot? = null,
    val encounter: EncounterSnapshot? = null,
    val forts: FortSnapshot? = null,
    val inventory: InventorySnapshot? = null,
    val storage: PokemonStorageSnapshot? = null,
    /**
     * Set by the controller after a catch was rejected with `out_of_balls` by the
     * native on-demand ball check. While set, catching is skipped and a spin is
     * forced to farm balls. Cleared once a spin completes.
     */
    val outOfBalls: Boolean = false,
)

/**
 * Plans one action set per cycle by running each feature module's planner in a
 * fixed priority order and concatenating the results. The per-module planning
 * lives in [ModuleActionPlanner]s (mirroring native per-module ownership); this
 * coordinator owns only the cross-module arbitration: the planner order, and the
 * shared active-encounter catch decision used by both the encounter berry assist
 * and the catch throw. One-mutation-at-a-time is enforced downstream by the runner.
 *
 * Order (discard, transfer, encounter, catch_spin) preserves the previous dispatch
 * priority: overworld maintenance (discard/transfer) before catch/open/spin, and an
 * in-encounter berry before the throw.
 */
class AutomationCoordinator(
    private val planners: List<ModuleActionPlanner> = listOf(
        DiscardActionPlanner(),
        TransferActionPlanner(),
        EncounterActionPlanner(),
        CatchSpinActionPlanner(),
    ),
    private val catchPlanner: CatchPlanner = CatchPlanner(),
) {
    fun plan(
        snapshot: AutomationSnapshot,
        policy: AutomationPolicy,
    ): List<AutomationAction> {
        val target = overworldTargetSpawn(snapshot)
        val context = PlanningContext(
            snapshot = snapshot,
            policy = policy,
            encounterCatchDecision = encounterCatchDecision(snapshot, policy),
            overworldTargetSpawn = target,
            // A catch (direct-map) or an open-encounter was intended this cycle;
            // catch_spin uses this to force a spin when the pouch is out of balls.
            overworldCatchIntended = target != null &&
                ((policy.autoCatch && policy.catchPolicy.catchAll) || policy.autoEncounter),
        )
        return planners.flatMap { it.plan(context) }
    }

    /**
     * The catch decision for the active encounter, shared across module planners so
     * the encounter berry and the catch throw agree without recomputing it. Null
     * unless we are in an encounter, catching is enabled, and balls remain (the
     * native executor reports out_of_balls; while set, do not throw).
     */
    private fun encounterCatchDecision(
        snapshot: AutomationSnapshot,
        policy: AutomationPolicy,
    ): CatchDecision? {
        if (snapshot.lifecycleState != GameLifecycleState.ENCOUNTER) return null
        val encounter = snapshot.encounter ?: return null
        if (!policy.autoCatch || snapshot.outOfBalls) return null
        return catchPlanner.decide(encounter, policy.catchPolicy)
    }

    /** The soonest-expiring nearby spawn to target this overworld cycle, if any. */
    private fun overworldTargetSpawn(snapshot: AutomationSnapshot): NearbySpawn? {
        if (snapshot.lifecycleState != GameLifecycleState.OVERWORLD) return null
        val nearby = snapshot.nearby ?: return null
        return nearby.spawns
            .asSequence()
            .filter { spawn ->
                val expiresAt = spawn.expiresAtEpochMs
                expiresAt == null || expiresAt > nearby.observedAtEpochMs
            }
            .minByOrNull { it.expiresAtEpochMs ?: Long.MAX_VALUE }
    }
}
