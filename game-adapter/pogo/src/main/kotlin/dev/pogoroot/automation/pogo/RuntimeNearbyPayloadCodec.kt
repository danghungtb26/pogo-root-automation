package dev.pogoroot.automation.pogo

import java.io.ByteArrayInputStream
import java.io.DataInputStream

/** Decodes the exact map-entity nearby payload emitted by the PoGo binding. */
object RuntimeNearbyPayloadCodec {
    private const val MAGIC = 0x504F474E
    private const val MAX_SPAWNS = 512

    fun decode(
        payload: ByteArray,
        observedAtEpochMs: Long,
    ): Result<RawNearbyObservation> = runCatching {
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == MAGIC) { "invalid runtime nearby payload" }
            val count = input.readInt()
            require(count in 0..MAX_SPAWNS) { "invalid runtime nearby count" }
            val spawns = buildList(count) {
                repeat(count) {
                    val spawnId = input.readLong().toULong().toString()
                    val speciesId = input.readInt()
                    val latitude = input.readDouble()
                    val longitude = input.readDouble()
                    require(spawnId != "0" && speciesId > 0) { "invalid runtime nearby spawn" }
                    require(latitude in -90.0..90.0 && longitude in -180.0..180.0) {
                        "invalid runtime nearby position"
                    }
                    add(
                        RawNearbySpawn(
                            spawnId = spawnId,
                            speciesId = speciesId,
                            latitude = latitude,
                            longitude = longitude,
                            firstSeenAtEpochMs = observedAtEpochMs,
                            expiresAtEpochMs = null,
                            expiryConfidence = RawExpiryConfidence.UNKNOWN,
                        ),
                    )
                }
            }
            val isComplete = if (input.available() > 0) input.readBoolean() else true
            // Optional trailing player-position block (payload version >= 2):
            // present flag, then two doubles when present. Absent on v1 payloads.
            var playerLatitude: Double? = null
            var playerLongitude: Double? = null
            if (input.available() > 0) {
                val hasPlayerPosition = input.readBoolean()
                if (hasPlayerPosition) {
                    val lat = input.readDouble()
                    val lng = input.readDouble()
                    require(lat in -90.0..90.0 && lng in -180.0..180.0) {
                        "invalid runtime player position"
                    }
                    playerLatitude = lat
                    playerLongitude = lng
                }
            }
            require(input.available() == 0) { "trailing runtime nearby payload" }
            RawNearbyObservation(
                observedAtEpochMs = observedAtEpochMs,
                playerLatitude = playerLatitude,
                playerLongitude = playerLongitude,
                spawns = spawns,
                isComplete = isComplete,
            )
        }
    }
}
