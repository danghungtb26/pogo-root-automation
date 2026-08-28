package dev.pogoroot.automation.core.model

enum class SpawnExpiryConfidence {
    EXACT,
    ESTIMATED,
    UNKNOWN,
}

data class NearbySpawn(
    val spawnId: String,
    val speciesId: Int,
    val speciesName: String,
    val position: GeoPoint,
    val firstSeenAtEpochMs: Long,
    val expiresAtEpochMs: Long?,
    val expiryConfidence: SpawnExpiryConfidence,
)
