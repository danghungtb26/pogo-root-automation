package dev.pogoroot.automation.pogo

import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.NearbySpawn
import dev.pogoroot.automation.core.model.SpawnExpiryConfidence

fun interface SpeciesNameResolver {
    fun resolve(speciesId: Int): String?
}

data class NearbyMappingIssue(
    val index: Int,
    val reason: String,
)

data class NearbyMappingResult(
    val snapshot: NearbySnapshot,
    val issues: List<NearbyMappingIssue>,
)

class PogoNearbyMapper(
    private val speciesNameResolver: SpeciesNameResolver = SpeciesNameResolver { null },
) {
    fun map(observation: RawNearbyObservation): NearbyMappingResult {
        val issues = mutableListOf<NearbyMappingIssue>()
        val mapped = observation.spawns.mapIndexedNotNull { index, raw ->
            val reason = validationError(raw)
            if (reason != null) {
                issues += NearbyMappingIssue(index, reason)
                return@mapIndexedNotNull null
            }

            NearbySpawn(
                spawnId = raw.spawnId,
                speciesId = raw.speciesId,
                speciesName = speciesNameResolver.resolve(raw.speciesId) ?: "#${raw.speciesId}",
                position = GeoPoint(raw.latitude, raw.longitude),
                firstSeenAtEpochMs = raw.firstSeenAtEpochMs ?: observation.observedAtEpochMs,
                expiresAtEpochMs = raw.expiresAtEpochMs,
                expiryConfidence = mapConfidence(raw),
            )
        }

        return NearbyMappingResult(
            snapshot = NearbySnapshot(
                observedAtEpochMs = observation.observedAtEpochMs,
                playerPosition = playerPosition(observation),
                spawns = mapped,
            ),
            issues = issues,
        )
    }

    private fun validationError(raw: RawNearbySpawn): String? = when {
        raw.spawnId.isBlank() -> "blank spawn id"
        raw.speciesId <= 0 -> "invalid species id"
        raw.latitude !in -90.0..90.0 -> "invalid latitude"
        raw.longitude !in -180.0..180.0 -> "invalid longitude"
        else -> null
    }

    private fun mapConfidence(raw: RawNearbySpawn): SpawnExpiryConfidence {
        if (raw.expiresAtEpochMs == null) {
            return SpawnExpiryConfidence.UNKNOWN
        }

        return when (raw.expiryConfidence) {
            RawExpiryConfidence.EXACT -> SpawnExpiryConfidence.EXACT
            RawExpiryConfidence.ESTIMATED,
            RawExpiryConfidence.UNKNOWN,
            -> SpawnExpiryConfidence.ESTIMATED
        }
    }

    private fun playerPosition(observation: RawNearbyObservation): GeoPoint? {
        val latitude = observation.playerLatitude ?: return null
        val longitude = observation.playerLongitude ?: return null

        if (latitude !in -90.0..90.0 || longitude !in -180.0..180.0) {
            return null
        }

        return GeoPoint(latitude, longitude)
    }
}
