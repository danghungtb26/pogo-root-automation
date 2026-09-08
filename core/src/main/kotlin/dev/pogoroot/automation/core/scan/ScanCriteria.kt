package dev.pogoroot.automation.core.scan

import dev.pogoroot.automation.core.location.GeoMath
import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.NearbySpawn

enum class ScanMode {
    HUNDO,
    SHINY,
    BOTH,
}

data class ScanCriteria(
    val mode: ScanMode,
    val maxCandidates: Int = 100,
    val maxDistanceMeters: Double? = null,
    val minimumRemainingMillis: Long? = null,
    val speciesIds: Set<Int> = emptySet(),
    val includeUnknownExpiry: Boolean = false,
) {
    init {
        require(maxCandidates in 1..100) { "maxCandidates must be between 1 and 100" }
        require(maxDistanceMeters == null || maxDistanceMeters.isFinite()) {
            "maxDistanceMeters must be finite when present"
        }
        require(maxDistanceMeters == null || maxDistanceMeters >= 0.0) {
            "maxDistanceMeters must not be negative"
        }
        require(minimumRemainingMillis == null || minimumRemainingMillis >= 0L) {
            "minimumRemainingMillis must not be negative"
        }
        require(speciesIds.all { it > 0 }) { "species ids must be positive" }
    }
}

data class ScanCandidate(
    val spawn: NearbySpawn,
    val distanceMeters: Double?,
    val remainingMillis: Long?,
)

data class ScanPlan(
    val candidates: List<ScanCandidate>,
    val eligibleCount: Int,
)

/** Pure nearby filtering and deterministic queue preparation for a scan session. */
class ScanPlanner {
    fun plan(
        snapshot: NearbySnapshot,
        criteria: ScanCriteria,
        alreadyProbedSpawnIds: Set<String> = emptySet(),
    ): ScanPlan {
        val eligible = snapshot.spawns.asSequence()
            .filter { it.spawnId !in alreadyProbedSpawnIds }
            .filter { criteria.speciesIds.isEmpty() || it.speciesId in criteria.speciesIds }
            .mapNotNull { spawn -> candidate(snapshot, spawn, criteria) }
            .sortedWith(compareBy<ScanCandidate> {
                // Known expiry is more actionable than unknown expiry.
                it.remainingMillis == null
            }.thenBy { it.remainingMillis ?: Long.MAX_VALUE }
                .thenBy { it.distanceMeters ?: Double.MAX_VALUE }
                .thenBy { it.spawn.spawnId })
            .toList()

        return ScanPlan(
            candidates = eligible.take(criteria.maxCandidates),
            eligibleCount = eligible.size,
        )
    }

    private fun candidate(
        snapshot: NearbySnapshot,
        spawn: NearbySpawn,
        criteria: ScanCriteria,
    ): ScanCandidate? {
        val distanceMeters = snapshot.playerPosition?.let { player ->
            GeoMath.distanceMeters(player, spawn.position)
        }
        if (criteria.maxDistanceMeters != null &&
            (distanceMeters == null || distanceMeters > criteria.maxDistanceMeters)
        ) {
            return null
        }

        val remainingMillis = spawn.expiresAtEpochMs?.minus(snapshot.observedAtEpochMs)
        if (remainingMillis != null && remainingMillis <= 0L) return null

        val minimumRemaining = criteria.minimumRemainingMillis
        if (minimumRemaining != null) {
            if (remainingMillis == null && !criteria.includeUnknownExpiry) return null
            if (remainingMillis != null && remainingMillis < minimumRemaining) return null
        }

        return ScanCandidate(
            spawn = spawn,
            distanceMeters = distanceMeters,
            remainingMillis = remainingMillis,
        )
    }
}
