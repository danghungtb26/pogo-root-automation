package dev.pogoroot.automation.pogo

import java.io.ByteArrayInputStream
import java.io.DataInputStream

/** Decodes the single correlated SCAN_MAP response used by catch/spin automation. */
object RuntimeCatchSpinPayloadCodec {
    private const val MAGIC = 0x504F4743
    private const val NEARBY_PRESENT = 1
    private const val FORTS_PRESENT = 1 shl 1
    private const val INVENTORY_PRESENT = 1 shl 2
    private const val PLAYER_POSITION_PRESENT = 1 shl 3
    private const val MAX_PAYLOAD_BYTES = 4 * 1024 * 1024

    fun decode(
        payload: ByteArray,
        observedAtEpochMs: Long,
    ): Result<RawCatchSpinObservation> = runCatching {
        require(payload.size <= MAX_PAYLOAD_BYTES) { "runtime scan payload exceeds hard limit" }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == MAGIC) { "invalid runtime catch/spin payload" }
            val cycleId = input.readLong()
            require(cycleId > 0L) { "invalid runtime scan cycle" }
            val flags = input.readInt()
            require(flags and (NEARBY_PRESENT or FORTS_PRESENT or INVENTORY_PRESENT or PLAYER_POSITION_PRESENT)
                .inv() == 0) { "unknown runtime scan flags" }
            val playerPosition = if (flags and PLAYER_POSITION_PRESENT != 0) {
                val latitude = input.readDouble()
                val longitude = input.readDouble()
                require(latitude in -90.0..90.0 && longitude in -180.0..180.0) {
                    "invalid runtime player position"
                }
                latitude to longitude
            } else {
                null
            }
            val nearbyPayload = readBlob(input)
            val fortsPayload = readBlob(input)
            val inventoryPayload = readBlob(input)
            require(input.available() == 0) { "trailing runtime catch/spin payload" }
            val nearby = if (flags and NEARBY_PRESENT != 0) {
                RuntimeNearbyPayloadCodec.decode(nearbyPayload, observedAtEpochMs).getOrThrow()
            } else {
                require(nearbyPayload.isEmpty()) { "unexpected unavailable nearby payload" }
                null
            }
            val forts = if (flags and FORTS_PRESENT != 0) {
                RuntimeFortsPayloadCodec.decode(fortsPayload, observedAtEpochMs).getOrThrow()
            } else {
                require(fortsPayload.isEmpty()) { "unexpected unavailable forts payload" }
                null
            }
            val inventory = if (flags and INVENTORY_PRESENT != 0) {
                RuntimeInventoryPayloadCodec.decode(inventoryPayload, observedAtEpochMs).getOrThrow()
            } else {
                require(inventoryPayload.isEmpty()) { "unexpected unavailable inventory payload" }
                null
            }
            val playerLatitude = playerPosition?.first ?: nearby?.playerLatitude
            val playerLongitude = playerPosition?.second ?: nearby?.playerLongitude
            RawCatchSpinObservation(
                cycleId = cycleId,
                observedAtEpochMs = observedAtEpochMs,
                nearby = nearby,
                forts = forts,
                inventory = inventory,
                playerLatitude = playerLatitude,
                playerLongitude = playerLongitude,
            )
        }
    }

    private fun readBlob(input: DataInputStream): ByteArray {
        val length = input.readInt()
        require(length in 0..MAX_PAYLOAD_BYTES) { "invalid runtime scan section length" }
        return ByteArray(length).also(input::readFully)
    }
}
