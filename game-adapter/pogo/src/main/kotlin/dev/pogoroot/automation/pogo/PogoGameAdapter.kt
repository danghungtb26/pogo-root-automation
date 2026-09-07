package dev.pogoroot.automation.pogo

import dev.pogoroot.automation.adapter.GameAdapter
import dev.pogoroot.automation.adapter.GameCapability
import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.AutomationActionResult
import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.FortSnapshot
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.InventorySnapshot
import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.PokemonStorageSnapshot

interface PogoRuntimeSource {
    val capabilities: Set<GameCapability>

    fun connect(): Result<Unit>

    fun disconnect()

    fun lifecycleState(): GameLifecycleState

    fun readNearby(): Result<RawNearbyObservation>

    fun readEncounter(): Result<RawEncounterObservation?>

    fun readForts(): Result<FortSnapshot> = unsupported(GameCapability.READ_FORTS)

    fun readInventory(): Result<InventorySnapshot> = unsupported(GameCapability.READ_INVENTORY)

    fun readPokemonStorage(): Result<PokemonStorageSnapshot> =
        unsupported(GameCapability.READ_POKEMON_STORAGE)
}

interface PogoActionExecutor {
    val capabilities: Set<GameCapability>

    fun execute(action: AutomationAction): Result<AutomationActionResult>
}

class PogoGameAdapter(
    private val runtimeSource: PogoRuntimeSource,
    private val actionExecutor: PogoActionExecutor? = null,
    private val nearbyMapper: PogoNearbyMapper = PogoNearbyMapper(),
    private val encounterMapper: PogoEncounterMapper = PogoEncounterMapper(),
) : GameAdapter {
    override val id: String = "pogo-runtime-v1"

    override val capabilities: Set<GameCapability>
        get() = runtimeSource.capabilities + (actionExecutor?.capabilities ?: emptySet())

    override fun connect(): Result<Unit> = runtimeSource.connect()

    override fun disconnect() = runtimeSource.disconnect()

    override fun lifecycleState(): GameLifecycleState = runtimeSource.lifecycleState()

    override fun readNearby(): Result<NearbySnapshot> = runtimeSource.readNearby().mapCatching { raw ->
        val mapped = nearbyMapper.map(raw)
        if (mapped.issues.isNotEmpty()) {
            throw IllegalStateException(
                "Nearby mapping failed: ${mapped.issues.joinToString { it.reason }}",
            )
        }
        mapped.snapshot
    }

    override fun readEncounter(): Result<EncounterSnapshot?> = runtimeSource.readEncounter().mapCatching { raw ->
        if (raw == null) {
            return@mapCatching null
        }

        val mapped = encounterMapper.map(raw)
        mapped.snapshot ?: throw IllegalStateException(
            "Encounter mapping failed: ${mapped.issues.joinToString()}",
        )
    }

    override fun readForts(): Result<FortSnapshot> = runtimeSource.readForts()

    override fun readInventory(): Result<InventorySnapshot> = runtimeSource.readInventory()

    override fun readPokemonStorage(): Result<PokemonStorageSnapshot> = runtimeSource.readPokemonStorage()

    override fun execute(action: AutomationAction): Result<AutomationActionResult> {
        val executor = actionExecutor ?: return Result.failure(
            UnsupportedOperationException("No Pogo action executor configured"),
        )
        return executor.execute(action)
    }
}

private fun <T> unsupported(capability: GameCapability): Result<T> = Result.failure(
    UnsupportedOperationException("Pogo runtime capability is not implemented: $capability"),
)
