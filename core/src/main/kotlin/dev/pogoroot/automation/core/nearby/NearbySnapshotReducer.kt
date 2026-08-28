package dev.pogoroot.automation.core.nearby

import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.NearbySpawn

data class NearbySnapshotDiff(
    val current: NearbySnapshot,
    val added: List<NearbySpawn>,
    val updated: List<NearbySpawn>,
    val removed: List<NearbySpawn>,
)

class NearbySnapshotReducer {
    fun reduce(
        previous: NearbySnapshot?,
        incoming: NearbySnapshot,
    ): NearbySnapshotDiff {
        val previousById = previous
            ?.spawns
            .orEmpty()
            .associateBy(NearbySpawn::spawnId)

        val currentById = incoming.spawns
            .asSequence()
            .filterNot { spawn ->
                val expiry = spawn.expiresAtEpochMs
                expiry != null && expiry <= incoming.observedAtEpochMs
            }
            .associateBy(NearbySpawn::spawnId)

        val added = currentById
            .filterKeys { it !in previousById }
            .values
            .sortedBy(NearbySpawn::spawnId)

        val updated = currentById
            .mapNotNull { (spawnId, spawn) ->
                val old = previousById[spawnId]
                spawn.takeIf { old != null && old != spawn }
            }
            .sortedBy(NearbySpawn::spawnId)

        val removed = previousById
            .filterKeys { it !in currentById }
            .values
            .sortedBy(NearbySpawn::spawnId)

        val normalized = incoming.copy(
            spawns = currentById.values.sortedBy(NearbySpawn::spawnId),
        )

        return NearbySnapshotDiff(
            current = normalized,
            added = added,
            updated = updated,
            removed = removed,
        )
    }
}
