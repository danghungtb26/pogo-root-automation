package dev.pogoroot.automation.core.nearby

import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.NearbySpawn
import dev.pogoroot.automation.core.model.SpawnExpiryConfidence
import org.junit.Assert.assertEquals
import org.junit.Test

class NearbySnapshotReducerTest {
    private val reducer = NearbySnapshotReducer()
    private val now = 10_000L

    @Test
    fun `reports add update and remove deterministically`() {
        val previous = snapshot(
            spawn("a", 1),
            spawn("b", 2),
        )
        val incoming = snapshot(
            spawn("b", 2, latitude = 1.0),
            spawn("c", 3),
        )

        val diff = reducer.reduce(previous, incoming)

        assertEquals(listOf("c"), diff.added.map(NearbySpawn::spawnId))
        assertEquals(listOf("b"), diff.updated.map(NearbySpawn::spawnId))
        assertEquals(listOf("a"), diff.removed.map(NearbySpawn::spawnId))
        assertEquals(listOf("b", "c"), diff.current.spawns.map(NearbySpawn::spawnId))
    }

    @Test
    fun `deduplicates incoming spawn ids with latest value`() {
        val incoming = snapshot(
            spawn("same", 1, latitude = 1.0),
            spawn("same", 1, latitude = 2.0),
        )

        val diff = reducer.reduce(previous = null, incoming = incoming)

        assertEquals(1, diff.current.spawns.size)
        assertEquals(2.0, diff.current.spawns.single().position.latitude, 0.0)
    }

    @Test
    fun `expired incoming spawn is removed from current snapshot`() {
        val previous = snapshot(spawn("expired", 1))
        val expired = spawn("expired", 1).copy(expiresAtEpochMs = now)

        val diff = reducer.reduce(previous, snapshot(expired))

        assertEquals(emptyList<NearbySpawn>(), diff.current.spawns)
        assertEquals(listOf("expired"), diff.removed.map(NearbySpawn::spawnId))
    }

    private fun snapshot(vararg spawns: NearbySpawn) = NearbySnapshot(
        observedAtEpochMs = now,
        playerPosition = GeoPoint(0.0, 0.0),
        spawns = spawns.toList(),
    )

    private fun spawn(
        id: String,
        speciesId: Int,
        latitude: Double = 0.0,
    ) = NearbySpawn(
        spawnId = id,
        speciesId = speciesId,
        speciesName = "#$speciesId",
        position = GeoPoint(latitude, 0.0),
        firstSeenAtEpochMs = 1_000L,
        expiresAtEpochMs = now + 60_000L,
        expiryConfidence = SpawnExpiryConfidence.EXACT,
    )
}
