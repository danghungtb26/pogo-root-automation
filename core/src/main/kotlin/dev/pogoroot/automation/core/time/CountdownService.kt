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

        val now = nowEpochMs()
        val remaining = (expiresAt - now).coerceAtLeast(0L)

        return SpawnCountdown(
            remainingMillis = remaining,
            isExpired = expiresAt <= now,
            isEstimated = spawn.expiryConfidence == SpawnExpiryConfidence.ESTIMATED,
        )
    }
}
