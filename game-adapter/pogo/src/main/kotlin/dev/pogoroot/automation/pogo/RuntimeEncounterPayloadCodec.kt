package dev.pogoroot.automation.pogo

import java.io.ByteArrayInputStream
import java.io.DataInputStream

/** Decodes the versioned, structured encounter payload emitted by IL2CPP. */
object RuntimeEncounterPayloadCodec {
    private const val MAGIC = 0x504F4745
    private const val MAX_ENCOUNTER_ID = 20

    fun decode(payload: ByteArray, observedAtEpochMs: Long): Result<RawEncounterObservation> = runCatching {
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == MAGIC) { "invalid runtime encounter payload" }
            val encounterId = input.readLong().toULong().toString()
            require(encounterId.length in 1..MAX_ENCOUNTER_ID && encounterId != "0") {
                "invalid runtime encounter id"
            }
            val speciesId = input.readInt()
            val attack = input.readInt()
            val defense = input.readInt()
            val stamina = input.readInt()
            require(speciesId > 0) { "invalid runtime species id" }
            require(attack in 0..15 && defense in 0..15 && stamina in 0..15) {
                "invalid runtime IV data"
            }
            val hasShiny = readStrictBoolean(input)
            val shiny = if (hasShiny) readStrictBoolean(input) else null
            val latitude = input.readDouble()
            val longitude = input.readDouble()
            require(latitude in -90.0..90.0 && longitude in -180.0..180.0) {
                "invalid runtime encounter position"
            }
            require(input.available() == 0) { "trailing runtime encounter payload" }
            RawEncounterObservation(
                encounterId = encounterId,
                speciesId = speciesId,
                observedAtEpochMs = observedAtEpochMs,
                individualAttack = attack,
                individualDefense = defense,
                individualStamina = stamina,
                shiny = shiny,
                latitude = latitude,
                longitude = longitude,
            )
        }
    }

    private fun readStrictBoolean(input: DataInputStream): Boolean = when (input.readUnsignedByte()) {
        0 -> false
        1 -> true
        else -> error("invalid runtime encounter boolean")
    }
}
