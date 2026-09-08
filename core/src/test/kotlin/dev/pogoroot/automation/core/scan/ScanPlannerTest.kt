package dev.pogoroot.automation.core.scan

import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.NearbySpawn
import dev.pogoroot.automation.core.model.SpawnExpiryConfidence
import org.junit.Assert.assertEquals
import org.junit.Test

class ScanPlannerTest {
    private val planner = ScanPlanner()
    private val now = 100_000L
    private val origin = GeoPoint(21.0285, 105.8542)

    @Test
    fun `filters by distance species and remaining time`() {
        val plan = planner.plan(
            snapshot = snapshot(
                spawn("near", 25, origin, now + 60_000L),
                spawn("far", 25, GeoPoint(21.0385, 105.8542), now + 60_000L),
                spawn("wrong-species", 4, origin, now + 60_000L),
                spawn("too-soon", 25, origin, now + 1_000L),
            ),
            criteria = ScanCriteria(
                mode = ScanMode.HUNDO,
                maxDistanceMeters = 500.0,
                minimumRemainingMillis = 10_000L,
                speciesIds = setOf(25),
            ),
        )

        assertEquals(listOf("near"), plan.candidates.map { it.spawn.spawnId })
        assertEquals(1, plan.eligibleCount)
    }

    @Test
    fun `orders known expiry before unknown and caps at one hundred`() {
        val spawns = (1..101).map { index ->
            spawn(
                id = "spawn-$index",
                speciesId = 25,
                position = origin,
                expiresAt = now + index * 1_000L,
            )
        } + spawn("unknown", 25, origin, null)

        val plan = planner.plan(
            snapshot = snapshot(*spawns.toTypedArray()),
            criteria = ScanCriteria(mode = ScanMode.SHINY),
        )

        assertEquals(100, plan.candidates.size)
        assertEquals("spawn-1", plan.candidates.first().spawn.spawnId)
        assertEquals(102, plan.eligibleCount)
        assertEquals(false, plan.candidates.any { it.spawn.spawnId == "unknown" })
    }

    @Test
    fun `does not include unknown expiry when minimum time is requested`() {
        val plan = planner.plan(
            snapshot = snapshot(spawn("unknown", 25, origin, null)),
            criteria = ScanCriteria(
                mode = ScanMode.HUNDO,
                minimumRemainingMillis = 1_000L,
            ),
        )

        assertEquals(emptyList<ScanCandidate>(), plan.candidates)
    }

    private fun snapshot(vararg spawns: NearbySpawn) = NearbySnapshot(
        observedAtEpochMs = now,
        playerPosition = origin,
        spawns = spawns.toList(),
    )

    private fun spawn(
        id: String,
        speciesId: Int,
        position: GeoPoint,
        expiresAt: Long?,
    ) = NearbySpawn(
        spawnId = id,
        speciesId = speciesId,
        speciesName = "#$speciesId",
        position = position,
        firstSeenAtEpochMs = now,
        expiresAtEpochMs = expiresAt,
        expiryConfidence = if (expiresAt == null) {
            SpawnExpiryConfidence.UNKNOWN
        } else {
            SpawnExpiryConfidence.EXACT
        },
    )
}
