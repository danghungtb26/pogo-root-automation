package dev.pogoroot.automation.adapter

import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.AutomationActionResult
import dev.pogoroot.automation.core.automation.ActionRequest
import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.FortSnapshot
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.InventorySnapshot
import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.PokemonStorageSnapshot

interface GameAdapter {
    val id: String
    val capabilities: Set<GameCapability>

    fun connect(): Result<Unit>

    fun disconnect()

    fun lifecycleState(): GameLifecycleState

    fun readNearby(): Result<NearbySnapshot>

    fun readEncounter(): Result<EncounterSnapshot?> = unsupported(GameCapability.ENCOUNTER)

    fun readForts(): Result<FortSnapshot> = unsupported(GameCapability.READ_FORTS)

    fun readInventory(): Result<InventorySnapshot> = unsupported(GameCapability.READ_INVENTORY)

    fun readPokemonStorage(): Result<PokemonStorageSnapshot> =
        unsupported(GameCapability.READ_POKEMON_STORAGE)

    fun execute(action: AutomationAction): Result<AutomationActionResult> = Result.failure(
        UnsupportedOperationException("Adapter $id does not execute ${action::class.simpleName}"),
    )

    /** Asynchronous execution boundary. Legacy adapters may fall back to execute(action). */
    fun submit(request: ActionRequest): Result<Unit> = execute(request.action).map { Unit }
}

private fun <T> unsupported(capability: GameCapability): Result<T> = Result.failure(
    UnsupportedOperationException("Adapter capability is not implemented: $capability"),
)
