package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.model.GeoPoint
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream
import java.nio.charset.CodingErrorAction

enum class PointWalkCandidateKind(val wireValue: Int) {
    WALK(1),
    STOP(2),
    ARRIVED(3),
    ;

    companion object {
        fun fromWireValue(value: Int): PointWalkCandidateKind? =
            entries.firstOrNull { it.wireValue == value }
    }
}

/** Candidate or terminal identity carried through an observation event. */
data class PointWalkCandidatePayload(
    val kind: PointWalkCandidateKind,
    val candidateId: String,
    val target: GeoPoint? = null,
    val force: Boolean = false,
)

/** Strict versioned wire codec. It validates data and never admits/selects a route. */
object PointWalkCandidatePayloadCodec {
    const val VERSION = 1
    const val MAX_CANDIDATE_ID_BYTES = 128
    private const val MAGIC = 0x504F4743 // POGC

    fun encode(value: PointWalkCandidatePayload): Result<ByteArray> = runCatching {
        val id = value.candidateId.toByteArray(Charsets.UTF_8)
        require(id.isNotEmpty() && id.size <= MAX_CANDIDATE_ID_BYTES) { "invalid candidate ID length" }
        validateKindFields(value.kind, value.target, value.force)
        ByteArrayOutputStream().use { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(MAGIC)
                output.writeInt(value.kind.wireValue)
                output.writeInt(if (value.force) 1 else 0)
                output.writeInt(id.size)
                output.write(id)
                value.target?.let { target ->
                    output.writeDouble(target.latitude)
                    output.writeDouble(target.longitude)
                }
            }
            bytes.toByteArray()
        }
    }

    fun decode(payload: ByteArray, payloadVersion: Int): Result<PointWalkCandidatePayload> = runCatching {
        require(payloadVersion == VERSION) { "unsupported point-walk payload version" }
        require(payload.size <= BridgeProtocol.NORMAL_MESSAGE_BYTES) { "point-walk payload too large" }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == MAGIC) { "invalid point-walk marker" }
            val kind = PointWalkCandidateKind.fromWireValue(input.readInt())
                ?: error("unknown point-walk instruction")
            val flags = input.readInt()
            require(flags == 0 || flags == FORCE_FLAG) { "unknown point-walk flags" }
            val candidateId = input.readBoundedUtf8String()
            require(candidateId.isNotBlank()) { "candidate ID is blank" }
            val target = if (kind == PointWalkCandidateKind.WALK) {
                GeoPoint(input.readDouble(), input.readDouble()).also(::requireValidCoordinate)
            } else {
                null
            }
            val force = flags == FORCE_FLAG
            validateKindFields(kind, target, force)
            require(input.available() == 0) { "trailing point-walk bytes" }
            PointWalkCandidatePayload(kind, candidateId, target, force)
        }
    }

    private fun validateKindFields(kind: PointWalkCandidateKind, target: GeoPoint?, force: Boolean) {
        if (kind == PointWalkCandidateKind.WALK) {
            require(target != null) { "walk candidate is missing a target" }
            requireValidCoordinate(target)
        } else {
            require(target == null && !force) { "terminal must not include target or force" }
        }
    }

    private fun requireValidCoordinate(point: GeoPoint) {
        require(point.latitude.isFinite() && point.longitude.isFinite() &&
            point.latitude in -90.0..90.0 && point.longitude in -180.0..180.0) {
            "invalid point-walk coordinate"
        }
    }

    private fun DataInputStream.readBoundedUtf8String(): String {
        val length = readInt()
        require(length in 1..MAX_CANDIDATE_ID_BYTES && length <= available()) {
            "invalid point-walk candidate ID length"
        }
        val bytes = ByteArray(length)
        readFully(bytes)
        return Charsets.UTF_8.newDecoder()
            .onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT)
            .decode(java.nio.ByteBuffer.wrap(bytes))
            .toString()
    }

    private const val FORCE_FLAG = 1
}
