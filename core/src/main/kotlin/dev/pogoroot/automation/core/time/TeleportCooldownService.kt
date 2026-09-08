package dev.pogoroot.automation.core.time

import dev.pogoroot.automation.core.location.GeoMath
import dev.pogoroot.automation.core.model.GeoPoint

data class TeleportCooldown(
    val distanceMeters: Double,
    val startedAtEpochMs: Long,
    val readyAtEpochMs: Long,
) {
    val cooldownMillis: Long
        get() = (readyAtEpochMs - startedAtEpochMs).coerceAtLeast(0L)

    fun remainingMillis(nowEpochMs: Long): Long =
        (readyAtEpochMs - nowEpochMs).coerceAtLeast(0L)

    fun isReady(nowEpochMs: Long): Boolean = remainingMillis(nowEpochMs) == 0L
}

enum class TeleportCooldownMode {
    CURRENT_POSITION,
    LAST_ACTIVE,
}

/**
 * Conservative, community-reported teleport safety brackets.
 *
 * This is not an official Niantic API or a server-status detector. The app
 * must present the result as an estimate because the real cooldown also
 * depends on location-marking actions that this controller cannot observe.
 */
class TeleportCooldownService(
    private val nowEpochMs: () -> Long = System::currentTimeMillis,
) {
    fun forTeleport(
        previousPoint: GeoPoint?,
        destination: GeoPoint,
        startedAtEpochMs: Long = nowEpochMs(),
    ): TeleportCooldown? {
        previousPoint ?: return null
        val distanceMeters = GeoMath.distanceMeters(previousPoint, destination)
        val durationMillis = cooldownMillisForDistance(distanceMeters / METERS_PER_KILOMETER)
        return TeleportCooldown(
            distanceMeters = distanceMeters,
            startedAtEpochMs = startedAtEpochMs,
            readyAtEpochMs = startedAtEpochMs + durationMillis,
        )
    }

    fun forLastActive(
        lastActivePoint: GeoPoint?,
        lastActiveAtEpochMs: Long?,
        destination: GeoPoint,
    ): TeleportCooldown? {
        if (lastActivePoint == null || lastActiveAtEpochMs == null) return null
        val distanceMeters = GeoMath.distanceMeters(lastActivePoint, destination)
        val durationMillis = cooldownMillisForDistance(distanceMeters / METERS_PER_KILOMETER)
        return TeleportCooldown(
            distanceMeters = distanceMeters,
            startedAtEpochMs = lastActiveAtEpochMs,
            readyAtEpochMs = lastActiveAtEpochMs + durationMillis,
        )
    }

    fun cooldownMillisForDistance(distanceKm: Double): Long {
        require(distanceKm.isFinite() && distanceKm >= 0.0) {
            "distanceKm must be finite and non-negative"
        }
        if (distanceKm <= ZERO_DISTANCE_KM) return 0L

        return COOLDOWN_BRACKETS.firstOrNull { distanceKm <= it.maxDistanceKm }?.durationMillis
            ?: MAX_COOLDOWN_MILLIS
    }

    private data class CooldownBracket(
        val maxDistanceKm: Double,
        val durationMillis: Long,
    )

    private companion object {
        const val METERS_PER_KILOMETER = 1_000.0
        const val ZERO_DISTANCE_KM = 0.001
        const val MAX_COOLDOWN_MILLIS = 120L * 60L * 1_000L

        // Round up to the next bracket to keep the UI conservative.
        val COOLDOWN_BRACKETS = listOf(
            CooldownBracket(1.0, 1L * 60L * 1_000L),
            CooldownBracket(2.0, 2L * 60L * 1_000L),
            CooldownBracket(3.0, 2L * 60L * 1_000L),
            CooldownBracket(4.0, 6L * 60L * 1_000L),
            CooldownBracket(5.0, 6L * 60L * 1_000L),
            CooldownBracket(6.0, 7L * 60L * 1_000L),
            CooldownBracket(8.0, 8L * 60L * 1_000L),
            CooldownBracket(10.0, 8L * 60L * 1_000L),
            CooldownBracket(12.0, 9L * 60L * 1_000L),
            CooldownBracket(15.0, 11L * 60L * 1_000L),
            CooldownBracket(20.0, 13L * 60L * 1_000L),
            CooldownBracket(25.0, 15L * 60L * 1_000L),
            CooldownBracket(30.0, 18L * 60L * 1_000L),
            CooldownBracket(40.0, 22L * 60L * 1_000L),
            CooldownBracket(50.0, 24L * 60L * 1_000L),
            CooldownBracket(60.0, 25L * 60L * 1_000L),
            CooldownBracket(70.0, 26L * 60L * 1_000L),
            CooldownBracket(80.0, 27L * 60L * 1_000L),
            CooldownBracket(90.0, 28L * 60L * 1_000L),
            CooldownBracket(100.0, 30L * 60L * 1_000L),
            CooldownBracket(125.0, 33L * 60L * 1_000L),
            CooldownBracket(150.0, 36L * 60L * 1_000L),
            CooldownBracket(200.0, 42L * 60L * 1_000L),
            CooldownBracket(250.0, 46L * 60L * 1_000L),
            CooldownBracket(300.0, 50L * 60L * 1_000L),
            CooldownBracket(400.0, 56L * 60L * 1_000L),
            CooldownBracket(500.0, 64L * 60L * 1_000L),
            CooldownBracket(600.0, 72L * 60L * 1_000L),
            CooldownBracket(700.0, 79L * 60L * 1_000L),
            CooldownBracket(750.0, 82L * 60L * 1_000L),
            CooldownBracket(800.0, 86L * 60L * 1_000L),
            CooldownBracket(900.0, 93L * 60L * 1_000L),
            CooldownBracket(1_000.0, 100L * 60L * 1_000L),
            CooldownBracket(1_100.0, 108L * 60L * 1_000L),
            CooldownBracket(1_200.0, 115L * 60L * 1_000L),
            CooldownBracket(1_300.0, MAX_COOLDOWN_MILLIS),
        )
    }
}
