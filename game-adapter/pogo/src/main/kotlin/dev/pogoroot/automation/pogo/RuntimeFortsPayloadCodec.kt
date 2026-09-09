package dev.pogoroot.automation.pogo

import java.io.ByteArrayInputStream
import java.io.DataInputStream

/** Decodes the exact fort payload emitted by the PoGo map binding. */
object RuntimeFortsPayloadCodec {
    private const val MAGIC = 0x504F4746
    private const val MAX_FORTS = 512

    fun decode(
        payload: ByteArray,
        observedAtEpochMs: Long,
    ): Result<RawFortObservation> = runCatching {
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == MAGIC) { "invalid runtime forts payload" }
            val count = input.readInt()
            require(count in 0..MAX_FORTS) { "invalid runtime forts count" }
            val forts = buildList(count) {
                repeat(count) {
                    val fortId = readString(input)
                    val type = when (input.readInt()) {
                        0 -> RawFortType.POKESTOP
                        1 -> RawFortType.GYM
                        else -> error("invalid runtime fort type")
                    }
                    val latitude = input.readDouble()
                    val longitude = input.readDouble()
                    val spinAvailable = input.readUnsignedByte().also { value ->
                        require(value in 0..1) { "invalid runtime fort spin state" }
                    } == 1
                    require(fortId.isNotBlank()) { "blank runtime fort id" }
                    require(latitude in -90.0..90.0 && longitude in -180.0..180.0) {
                        "invalid runtime fort position"
                    }
                    add(
                        RawFort(
                            fortId = fortId,
                            type = type,
                            latitude = latitude,
                            longitude = longitude,
                            spinAvailable = spinAvailable,
                        ),
                    )
                }
            }
            require(input.available() == 0) { "trailing runtime forts payload" }
            RawFortObservation(observedAtEpochMs, forts)
        }
    }

    private fun readString(input: DataInputStream): String {
        val length = input.readInt()
        require(length in 0..65536) { "invalid runtime fort string length" }
        return ByteArray(length).also(input::readFully).toString(Charsets.UTF_8)
    }
}
