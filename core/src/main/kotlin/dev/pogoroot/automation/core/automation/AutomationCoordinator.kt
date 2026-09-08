package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.FortSnapshot
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.InventorySnapshot
import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.PokemonStorageSnapshot

data class AutomationSnapshot(
    val lifecycleState: GameLifecycleState,
    val nearby: NearbySnapshot? = null,
    val encounter: EncounterSnapshot? = null,
    val forts: FortSnapshot? = null,
    val inventory: InventorySnapshot? = null,
    val storage: PokemonStorageSnapshot? = null,
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
        if (snapshot.lifecycleState == GameLifecycleState.ENCOUNTER) {
            return planEncounter(snapshot.encounter, policy)
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

        if (policy.autoSpin) {
            snapshot.forts?.forts
                ?.asSequence()
                ?.filter { it.spinAvailable }
                ?.forEach { actions += AutomationAction.Spin(it.fortId) }
        }

        if (policy.autoEncounter) {
            val nearby = snapshot.nearby
            val target = nearby?.spawns
                ?.asSequence()
                ?.filter { spawn ->
                    val expiresAt = spawn.expiresAtEpochMs
                    expiresAt == null || expiresAt > nearby.observedAtEpochMs
                }
                ?.minByOrNull { it.expiresAtEpochMs ?: Long.MAX_VALUE }

            if (target != null) {
                actions += AutomationAction.OpenEncounter(target.spawnId)
            }
        }

        return actions
    }

    private fun planEncounter(
        encounter: EncounterSnapshot?,
        policy: AutomationPolicy,
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

        if (!policy.autoCatch) return actions

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
