package dev.pogoroot.automation.core.model

data class NearbySnapshot(
    val observedAtEpochMs: Long,
    val playerPosition: GeoPoint?,
    val spawns: List<NearbySpawn>,
)
