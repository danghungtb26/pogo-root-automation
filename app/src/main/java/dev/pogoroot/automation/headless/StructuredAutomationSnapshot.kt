package dev.pogoroot.automation.headless

import android.util.Log
import dev.pogoroot.automation.adapter.GameCapability
import dev.pogoroot.automation.core.automation.AutomationSnapshot
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.PokemonStorageSnapshot
import dev.pogoroot.automation.pogo.PogoGameAdapter

internal fun PogoGameAdapter.readStructuredSnapshot(
    outOfBalls: Boolean,
    catchSpinArmed: Boolean,
    mergeStorage: (PokemonStorageSnapshot?) -> PokemonStorageSnapshot?,
    excludedSpawnIds: Set<String> = emptySet(),
): AutomationSnapshot {
    val lifecycle = lifecycleState()
    val nearby = readNearby().getOrElse { error ->
        Log.w(LOG_TAG, "automation nearby unavailable: ${error.message}")
        null
    }?.let { snapshot ->
        if (excludedSpawnIds.isEmpty()) snapshot else snapshot.copy(
            spawns = snapshot.spawns.filterNot { it.spawnId in excludedSpawnIds },
        )
    }
    val encounter = if (lifecycle == GameLifecycleState.ENCOUNTER) {
        readEncounter().getOrNull()
    } else {
        null
    }
    val forts = if (GameCapability.READ_FORTS in capabilities) {
        readForts().getOrElse { error ->
            Log.w(LOG_TAG, "automation forts unavailable: ${error.message}")
            null
        }
    } else {
        null
    }
    val inventory = if (GameCapability.READ_INVENTORY in capabilities) {
        readInventory().getOrElse { error ->
            Log.w(LOG_TAG, "automation inventory unavailable: ${error.message}")
            null
        }
    } else {
        null
    }
    val runtimeStorage = if (GameCapability.READ_POKEMON_STORAGE in capabilities) {
        readPokemonStorage().getOrNull()
    } else {
        null
    }
    return AutomationSnapshot(
        lifecycleState = lifecycle,
        nearby = nearby,
        encounter = encounter,
        forts = forts,
        inventory = inventory,
        storage = mergeStorage(runtimeStorage),
        outOfBalls = outOfBalls,
        catchSpinArmed = catchSpinArmed,
    )
}

private const val LOG_TAG = "PogoRootAutomation"
