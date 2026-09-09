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
            require(input.available() == 0) { "trailing runtime nearby payload" }
            RawNearbyObservation(
                observedAtEpochMs = observedAtEpochMs,
                playerLatitude = null,
                playerLongitude = null,
                spawns = spawns,
                isComplete = isComplete,
            )
        }
    }
}
