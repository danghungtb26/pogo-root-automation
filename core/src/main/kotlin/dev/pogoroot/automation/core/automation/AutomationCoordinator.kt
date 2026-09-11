package dev.pogoroot.automation.core.automation

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

class AutomationCoordinator(
    private val catchPlanner: CatchPlanner = CatchPlanner(),
    private val inventoryPlanner: InventoryPlanner = InventoryPlanner(),
    private val transferPlanner: TransferPlanner = TransferPlanner(),
) {
    fun plan(
        snapshot: AutomationSnapshot,
        policy: AutomationPolicy,
    ): List<AutomationAction> {
        // The ball check is on-demand in the native catch executor (DIRECT_MAP
        // throws a Poké Ball and reports out_of_balls when none remain). The
        // controller reflects that back as snapshot.outOfBalls; while set, skip
        // catching and force a spin to farm balls.
        val outOfBalls = snapshot.outOfBalls

        if (snapshot.lifecycleState == GameLifecycleState.ENCOUNTER) {
            return planEncounter(snapshot.encounter, policy, outOfBalls)
        }

        if (snapshot.lifecycleState != GameLifecycleState.OVERWORLD) {
            return emptyList()
        }

        val actions = mutableListOf<AutomationAction>()

        if (policy.autoDiscard) {
            snapshot.inventory?.let {
                actions += inventoryPlanner.plan(it, policy.inventoryPolicy)
            }
        }

        if (policy.autoTransfer) {
            snapshot.storage?.let {
                actions += transferPlanner.plan(it, policy.transferPolicy)
            }
        }

        val catchAllTarget = if (policy.autoCatch && policy.catchPolicy.catchAll) {
            soonestExpiringSpawn(snapshot)
        } else {
            null
        }

        val encounterTarget =
            if (policy.autoEncounter && !(policy.autoCatch && policy.catchPolicy.catchAll)) {
                soonestExpiringSpawn(snapshot)
            } else {
                null
            }

        // A catch was intended this cycle; used to decide whether an empty ball
        // pouch should force a spin to farm more balls.
        val catchIntended = catchAllTarget != null || encounterTarget != null

        if (catchAllTarget != null && !outOfBalls) {
            // Nearby map state has no IV/shiny metadata. Direct catch is
            // therefore limited to catch-all until encounter metadata is
            // intentionally requested by a separate policy path.
            actions += AutomationAction.Catch(
                encounterId = catchAllTarget.spawnId,
                reason = CatchReason.CATCH_ALL,
                mode = CatchMode.DIRECT_MAP,
            )
        }

        if (encounterTarget != null && !outOfBalls) {
            actions += AutomationAction.OpenEncounter(encounterTarget.spawnId)
        }

        // Spin on the normal toggle, or force one when a catch was suppressed
        // because the pouch is out of catch balls.
        val forceSpinForBalls = outOfBalls && catchIntended
        if (policy.autoSpin || forceSpinForBalls) {
            snapshot.forts?.forts
                ?.asSequence()
                ?.filter { it.spinAvailable }
                ?.forEach { actions += AutomationAction.Spin(it.fortId) }
        }

        return actions
    }

    private fun soonestExpiringSpawn(
        snapshot: AutomationSnapshot,
    ): NearbySpawn? {
        val nearby = snapshot.nearby ?: return null
        return nearby.spawns
            .asSequence()
            .filter { spawn ->
                val expiresAt = spawn.expiresAtEpochMs
                expiresAt == null || expiresAt > nearby.observedAtEpochMs
            }
            .minByOrNull { it.expiresAtEpochMs ?: Long.MAX_VALUE }
    }

    private fun planEncounter(
        encounter: EncounterSnapshot?,
        policy: AutomationPolicy,
        outOfBalls: Boolean,
    ): List<AutomationAction> {
        if (encounter == null) {
            return emptyList()
        }

        val actions = mutableListOf<AutomationAction>()
        if (policy.autoSnapshotDuringEncounter) {
            actions += AutomationAction.TakeEncounterSnapshot(
                encounterId = encounter.encounterId,
                encounterMode = policy.snapshotEncounterMode,
            )
        }

        // No catch balls left (Master Ball excluded): do not throw. A spin
        // cannot run inside an active encounter, so there is nothing else to do.
        if (!policy.autoCatch || outOfBalls) return actions

        val decision = catchPlanner.decide(encounter, policy.catchPolicy)
        if (!decision.shouldCatch || decision.reason == null) return actions

        actions += buildList {
            policy.berryType?.let { berryType ->
                add(
                    AutomationAction.UseBerry(
                        encounterId = encounter.encounterId,
                        berryType = berryType,
                    ),
                )
            }
            if (encounter.isShundo) {
                add(
                    AutomationAction.Alert(
                        kind = AlertKind.SHUNDO,
                        message = "Shundo detected: ${encounter.speciesName} (${encounter.encounterId})",
                    ),
                )
            }
            add(
                AutomationAction.Catch(
                    encounterId = encounter.encounterId,
                    reason = decision.reason,
                    closePreviewAfterCaught = policy.autoCloseCatchPreview,
                    throwProfile = policy.catchPolicy.throwProfile,
                ),
            )
        }
        return actions
    }
}
