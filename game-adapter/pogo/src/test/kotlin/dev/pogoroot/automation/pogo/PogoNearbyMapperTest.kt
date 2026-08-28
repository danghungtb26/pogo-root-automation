package dev.pogoroot.automation.pogo

import dev.pogoroot.automation.core.model.SpawnExpiryConfidence
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class PogoNearbyMapperTest {
    private val now = 100_000L

    @Test
    fun `maps structured raw nearby state`() {
        val mapper = PogoNearbyMapper(SpeciesNameResolver { id -> if (id == 25) "Pikachu" else null })

        val result = mapper.map(
            observation(
                RawNearbySpawn(
                    spawnId = "abc",
                    speciesId = 25,
                    latitude = 35.0,
                    longitude = 139.0,
                    firstSeenAtEpochMs = null,
                    expiresAtEpochMs = now + 60_000L,
                    expiryConfidence = RawExpiryConfidence.EXACT,
                ),
            ),
        )

        val spawn = result.snapshot.spawns.single()
        assertEquals("Pikachu", spawn.speciesName)
        assertEquals(now, spawn.firstSeenAtEpochMs)
        assertEquals(SpawnExpiryConfidence.EXACT, spawn.expiryConfidence)
        assertEquals(emptyList<NearbyMappingIssue>(), result.issues)
    }

    @Test
    fun `unknown expiry never fabricates an exact countdown`() {
        val result = PogoNearbyMapper().map(
            observation(
                RawNearbySpawn(
                    spawnId = "abc",
                    speciesId = 1,
                    latitude = 0.0,
                    longitude = 0.0,
                    firstSeenAtEpochMs = now,
                    expiresAtEpochMs = null,
                    expiryConfidence = RawExpiryConfidence.EXACT,
                ),
            ),
        )

        val spawn = result.snapshot.spawns.single()
        assertNull(spawn.expiresAtEpochMs)
        assertEquals(SpawnExpiryConfidence.UNKNOWN, spawn.expiryConfidence)
    }

    @Test
    fun `timestamp with unknown source is conservatively estimated`() {
        val result = PogoNearbyMapper().map(
            observation(
                RawNearbySpawn(
                    spawnId = "abc",
                    speciesId = 1,
                    latitude = 0.0,
                    longitude = 0.0,
                    firstSeenAtEpochMs = now,
                    expiresAtEpochMs = now + 1_000L,
                    expiryConfidence = RawExpiryConfidence.UNKNOWN,
                ),
            ),
        )

        assertEquals(
            SpawnExpiryConfidence.ESTIMATED,
            result.snapshot.spawns.single().expiryConfidence,
        )
    }

    @Test
    fun `rejects malformed raw entries without dropping valid entries`() {
        val result = PogoNearbyMapper().map(
            observation(
                raw("good", 1, 10.0, 10.0),
                raw("", 2, 10.0, 10.0),
                raw("bad-lat", 3, 100.0, 10.0),
            ),
        )

        assertEquals(listOf("good"), result.snapshot.spawns.map { it.spawnId })
        assertEquals(2, result.issues.size)
    }

    private fun observation(vararg spawns: RawNearbySpawn) = RawNearbyObservation(
        observedAtEpochMs = now,
        playerLatitude = 35.0,
        playerLongitude = 139.0,
        spawns = spawns.toList(),
    )

    private fun raw(
        id: String,
        speciesId: Int,
        latitude: Double,
        longitude: Double,
    ) = RawNearbySpawn(
        spawnId = id,
        speciesId = speciesId,
        latitude = latitude,
        longitude = longitude,
        firstSeenAtEpochMs = now,
        expiresAtEpochMs = now + 60_000L,
        expiryConfidence = RawExpiryConfidence.EXACT,
    )
}
