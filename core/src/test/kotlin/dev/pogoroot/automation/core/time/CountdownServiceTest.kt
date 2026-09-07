package dev.pogoroot.automation.core.time

import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.NearbySpawn
import dev.pogoroot.automation.core.model.SpawnExpiryConfidence
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CountdownServiceTest {
    private val now = 1_000_000L

    @Test
    fun `returns remaining time for known expiry`() {
        val countdown = CountdownService { now }.forSpawn(spawn(expiresAt = now + 90_000L))

        assertEquals(90_000L, countdown.remainingMillis)
        assertFalse(countdown.isExpired)
        assertFalse(countdown.isEstimated)
    }

    @Test
    fun `clamps expired spawn to zero`() {
        val countdown = CountdownService { now }.forSpawn(spawn(expiresAt = now - 1L))

        assertEquals(0L, countdown.remainingMillis)
        assertTrue(countdown.isExpired)
    }

    @Test
    fun `keeps unknown expiry explicit`() {
        val countdown = CountdownService { now }.forSpawn(
            spawn(
                expiresAt = null,
                confidence = SpawnExpiryConfidence.UNKNOWN,
            ),
        )

        assertNull(countdown.remainingMillis)
        assertFalse(countdown.isExpired)
    }

    @Test
    fun `samples clock once so remaining and expiry state stay consistent`() {
        var calls = 0
        val service = CountdownService {
            calls += 1
            now
        }

        val countdown = service.forSpawn(spawn(expiresAt = now + 1L))

        assertEquals(1, calls)
        assertEquals(1L, countdown.remainingMillis)
        assertFalse(countdown.isExpired)
    }

    private fun spawn(
        expiresAt: Long?,
        confidence: SpawnExpiryConfidence = SpawnExpiryConfidence.EXACT,
    ) = NearbySpawn(
        spawnId = "spawn-1",
        speciesId = 25,
        speciesName = "Pikachu",
        position = GeoPoint(0.0, 0.0),
        firstSeenAtEpochMs = now,
        expiresAtEpochMs = expiresAt,
        expiryConfidence = confidence,
    )
}
