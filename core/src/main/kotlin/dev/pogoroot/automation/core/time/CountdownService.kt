package dev.pogoroot.automation.core.time

import dev.pogoroot.automation.core.model.NearbySpawn
import dev.pogoroot.automation.core.model.SpawnExpiryConfidence

data class SpawnCountdown(
    val remainingMillis: Long?,
    val isExpired: Boolean,
    val isEstimated: Boolean,
)

class CountdownService(
    private val nowEpochMs: () -> Long = System::currentTimeMillis,
) {
    fun forSpawn(spawn: NearbySpawn): SpawnCountdown {
        val expiresAt = spawn.expiresAtEpochMs
            ?: return SpawnCountdown(
                remainingMillis = null,
                isExpired = false,
                isEstimated = spawn.expiryConfidence == SpawnExpiryConfidence.ESTIMATED,
            )

        val remaining = (expiresAt - nowEpochMs()).coerceAtLeast(0L)

        return SpawnCountdown(
            remainingMillis = remaining,
            isExpired = expiresAt <= nowEpochMs(),
            isEstimated = spawn.expiryConfidence == SpawnExpiryConfidence.ESTIMATED,
        )
    }
}
