package dev.pogoroot.automation.headless

import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.core.automation.ActionExecutionPhase
import dev.pogoroot.automation.core.automation.ActionRequest
import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.CatchOutcome
import dev.pogoroot.automation.core.automation.AutomationSnapshot
import dev.pogoroot.automation.core.model.PokemonIv
import dev.pogoroot.automation.core.model.PokemonStorageSnapshot
import dev.pogoroot.automation.core.model.StoredPokemon

/** Tracks wild catches that may become transfer candidates after capture. */
internal class StructuredAutomationWildState(
    private val eventSink: AutomationEventSink,
) {
    private data class WildCatchTemplate(
        val speciesId: Int,
        val speciesName: String,
        val iv: PokemonIv?,
        val shiny: Boolean,
    )

    private val templatesByCommand = linkedMapOf<String, WildCatchTemplate>()
    private val pendingTransfers = linkedMapOf<String, StoredPokemon>()

    fun clear() {
        templatesByCommand.clear()
        pendingTransfers.clear()
    }

    fun rememberCatchTemplate(request: ActionRequest, snapshot: AutomationSnapshot) {
        val action = request.action as? AutomationAction.Catch ?: return
        val template = snapshot.encounter
            ?.takeIf { it.encounterId == action.encounterId }
            ?.let { WildCatchTemplate(it.speciesId, it.speciesName, it.iv, it.shiny == true) }
            ?: snapshot.nearby?.spawns
                ?.firstOrNull { it.spawnId == action.encounterId }
                ?.let { WildCatchTemplate(it.speciesId, it.speciesName, iv = null, shiny = false) }
            ?: return
        templatesByCommand[request.commandId] = template
        trim(templatesByCommand)
    }

    fun registerIfCaught(
        request: ActionRequest,
        result: BridgeEvent.AutomationCommandResult,
        phase: ActionExecutionPhase,
    ) {
        val action = request.action as? AutomationAction.Catch
        val template = templatesByCommand.remove(request.commandId) ?: return
        if (action == null || phase != ActionExecutionPhase.COMPLETED ||
            result.catchOutcome != CatchOutcome.CAUGHT
        ) return
        val pokemonId = result.capturedPokemonId ?: return
        pendingTransfers[pokemonId] = StoredPokemon(
            pokemonId = pokemonId,
            speciesId = template.speciesId,
            speciesName = template.speciesName,
            iv = template.iv,
            shiny = template.shiny,
        )
        trim(pendingTransfers)
    }

    fun resolveTransfer(request: ActionRequest, phase: ActionExecutionPhase) {
        val action = request.action as? AutomationAction.TransferPokemon ?: return
        val released = pendingTransfers.remove(action.pokemonId)
        if (phase == ActionExecutionPhase.COMPLETED) {
            val label = released?.speciesName?.takeIf { it.isNotBlank() } ?: action.pokemonId
            eventSink.publish(AutomationEvent(AutomationEventType.TRANSFERRED, "Transferred $label"))
        }
    }

    fun mergePendingWildTransfers(base: PokemonStorageSnapshot?): PokemonStorageSnapshot? {
        if (pendingTransfers.isEmpty()) return base
        val pending = pendingTransfers.values.toList()
        if (base == null) {
            return PokemonStorageSnapshot(
                observedAtEpochMs = System.currentTimeMillis(),
                usedSlots = pending.size,
                capacity = pending.size,
                pokemon = pending,
            )
        }
        return base.copy(pokemon = (base.pokemon + pending).distinctBy { it.pokemonId })
    }

    private fun <T> trim(values: LinkedHashMap<String, T>) {
        while (values.size > MAX_PENDING_WILD_TRANSFERS) {
            values.remove(values.keys.firstOrNull() ?: break)
        }
    }

    private companion object {
        const val MAX_PENDING_WILD_TRANSFERS = 64
    }
}
