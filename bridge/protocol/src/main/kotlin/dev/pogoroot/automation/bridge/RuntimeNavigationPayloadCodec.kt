package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.model.GeoPoint
import java.io.ByteArrayInputStream
import java.io.DataInputStream

enum class RuntimeNavigationKind { STOP, WALK, ARRIVED }

data class RuntimeNavigationPayload(
    val kind: RuntimeNavigationKind,
    val fortId: String,
    val target: GeoPoint,
    val reason: String,
)

/** Native-owned decisions; decoding performs validation, never target selection. */
object RuntimeNavigationPayloadCodec {
    const val VERSION = 1
    fun decode(payload: ByteArray): Result<RuntimeNavigationPayload> = runCatching {
        require(payload.size <= BridgeProtocol.NORMAL_MESSAGE_BYTES)
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == 0x504f4757) { "invalid navigation marker" }
            val kind = RuntimeNavigationKind.entries.getOrNull(input.readInt())
                ?: error("unknown navigation instruction")
            val id = input.readBoundedString()
            val point = GeoPoint(input.readDouble(), input.readDouble())
            require(point.latitude.isFinite() && point.longitude.isFinite() &&
                point.latitude in -90.0..90.0 && point.longitude in -180.0..180.0) {
                "invalid navigation coordinate"
            }
            val reason = input.readBoundedString()
            require(kind == RuntimeNavigationKind.STOP || id.isNotBlank())
            require(input.available() == 0) { "trailing navigation bytes" }
            RuntimeNavigationPayload(kind, id, point, reason)
        }
    }

    private fun DataInputStream.readBoundedString(): String {
        val length = readInt()
        require(length in 0..4096 && length <= available()) { "invalid navigation string" }
        val bytes = ByteArray(length)
        readFully(bytes)
        return bytes.toString(Charsets.UTF_8)
    }
}
