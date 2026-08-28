package dev.pogoroot.automation.pogo

enum class RawExpiryConfidence {
    EXACT,
    ESTIMATED,
    UNKNOWN,
}

data class RawNearbySpawn(
    val spawnId: String,
    val speciesId: Int,
    val latitude: Double,
    val longitude: Double,
    val firstSeenAtEpochMs: Long?,
    val expiresAtEpochMs: Long?,
    val expiryConfidence: RawExpiryConfidence,
)

data class RawNearbyObservation(
    val observedAtEpochMs: Long,
    val playerLatitude: Double?,
    val playerLongitude: Double?,
    val spawns: List<RawNearbySpawn>,
)
