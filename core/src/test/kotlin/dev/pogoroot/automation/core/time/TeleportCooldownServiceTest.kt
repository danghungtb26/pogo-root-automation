package dev.pogoroot.automation.core.time

import dev.pogoroot.automation.core.location.GeoMath
import dev.pogoroot.automation.core.model.GeoPoint
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class TeleportCooldownServiceTest {
    private val service = TeleportCooldownService { 1_000_000L }

    @Test
    fun `no previous point means no cooldown candidate`() {
        assertNull(
            service.forTeleport(
                previousPoint = null,
                destination = GeoPoint(1.0, 2.0),
            ),
        )
    }

    @Test
    fun `same point is immediately ready`() {
        val point = GeoPoint(1.0, 2.0)
        val cooldown = service.forTeleport(point, point, startedAtEpochMs = 1_000_000L)

        assertNotNull(cooldown)
        assertEquals(0L, cooldown?.cooldownMillis)
        assertEquals(0L, cooldown?.remainingMillis(1_000_000L))
    }

    @Test
    fun `distance rounds up to the next safety bracket`() {
        assertEquals(1L * 60L * 1_000L, service.cooldownMillisForDistance(0.5))
        assertEquals(2L * 60L * 1_000L, service.cooldownMillisForDistance(1.1))
        assertEquals(6L * 60L * 1_000L, service.cooldownMillisForDistance(4.1))
        assertEquals(8L * 60L * 1_000L, service.cooldownMillisForDistance(9.1))
    }

    @Test
    fun `long distance is capped at two hours`() {
        assertEquals(120L * 60L * 1_000L, service.cooldownMillisForDistance(2_000.0))
    }

    @Test
    fun `teleport stores ready timestamp from distance`() {
        val destination = GeoMath.destination(
            start = GeoPoint(0.0, 0.0),
            bearingDegrees = 90.0,
            distanceMeters = 1_100.0,
        )

        val cooldown = service.forTeleport(
            previousPoint = GeoPoint(0.0, 0.0),
            destination = destination,
            startedAtEpochMs = 1_000_000L,
        )

        assertEquals(2L * 60L * 1_000L, cooldown?.cooldownMillis)
        assertEquals(1_120_000L, cooldown?.readyAtEpochMs)
    }

    @Test
    fun `last active cooldown starts at the game action time, not teleport time`() {
        val destination = GeoMath.destination(
            start = GeoPoint(0.0, 0.0),
            bearingDegrees = 90.0,
            distanceMeters = 1_100.0,
        )

        val cooldown = service.forLastActive(
            lastActivePoint = GeoPoint(0.0, 0.0),
            lastActiveAtEpochMs = 1_000_000L,
            destination = destination,
        )

        assertEquals(2L * 60L * 1_000L, cooldown?.cooldownMillis)
        assertEquals(1_120_000L, cooldown?.readyAtEpochMs)
        assertEquals(20_000L, cooldown?.remainingMillis(1_100_000L))
    }

    @Test
    fun `missing last active means no last active cooldown`() {
        assertNull(
            service.forLastActive(
                lastActivePoint = null,
                lastActiveAtEpochMs = null,
                destination = GeoPoint(1.0, 2.0),
            ),
        )
    }
}
