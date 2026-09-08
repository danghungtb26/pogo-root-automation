package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.MapTargetObservation
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream
import java.nio.charset.StandardCharsets

/**
 * Payload emitted by a verified, version-scoped Pokémon GO map binding.
 *
 * This codec deliberately carries the resolved GeoPoint. The controller must
 * not try to reconstruct a projection from screen pixels without a matching
 * camera snapshot supplied by the runtime binding.
 */
object MapTargetPayloadCodec {
    const val VERSION = 1
    private const val MAX_STRING_BYTES = 4 * 1024

    fun encode(value: MapTargetObservation): Result<ByteArray> = runCatching {
        ByteArrayOutputStream().use { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(VERSION)
                writeString(output, value.tapId)
                output.writeDouble(value.target.latitude)
                output.writeDouble(value.target.longitude)
                output.writeFloat(value.screenX)
                output.writeFloat(value.screenY)
                output.writeInt(value.viewportWidth)
                output.writeInt(value.viewportHeight)
                writeNullableString(output, value.cameraSnapshotId)
            }
            bytes.toByteArray().also {
                require(it.size <= BridgeProtocol.NORMAL_MESSAGE_BYTES) {
                    "map target payload exceeds normal limit"
                }
            }
        }
    }

    fun decode(payload: ByteArray): Result<MapTargetObservation> = runCatching {
        require(payload.size <= BridgeProtocol.NORMAL_MESSAGE_BYTES) {
            "map target payload exceeds normal limit"
        }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == VERSION) { "unsupported map target payload version" }
            val value = MapTargetObservation(
                tapId = readString(input),
                target = GeoPoint(input.readDouble(), input.readDouble()),
                screenX = input.readFloat(),
                screenY = input.readFloat(),
                viewportWidth = input.readInt(),
                viewportHeight = input.readInt(),
                cameraSnapshotId = readNullableString(input),
            )
            require(input.available() == 0) { "trailing map target payload bytes" }
            value
        }
    }

    private fun writeString(output: DataOutputStream, value: String) {
        val bytes = value.toByteArray(StandardCharsets.UTF_8)
        require(bytes.size <= MAX_STRING_BYTES) { "map target string exceeds limit" }
        output.writeInt(bytes.size)
        output.write(bytes)
    }

    private fun writeNullableString(output: DataOutputStream, value: String?) {
        output.writeBoolean(value != null)
        if (value != null) writeString(output, value)
    }

    private fun readString(input: DataInputStream): String {
        val size = input.readInt()
        require(size in 0..MAX_STRING_BYTES) { "invalid map target string length" }
        val bytes = ByteArray(size)
        input.readFully(bytes)
        return String(bytes, StandardCharsets.UTF_8)
    }

    private fun readNullableString(input: DataInputStream): String? =
        if (input.readBoolean()) readString(input) else null
}
