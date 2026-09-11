package dev.pogoroot.automation.bridge

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream

/**
 * Runtime lifecycle control actions only. Feature-module control actions (e.g.
 * SCAN_MAP) are declared per-module in [ModuleControlAction], not here, so module
 * actions stay out of the shared enum. The wire carries the action as a raw number
 * ([RuntimeControlRequest.actionWire]); the native dispatcher validates it against
 * this lifecycle set plus the module owner map, like a gameplay action tag.
 */
enum class RuntimeControlAction(val wireValue: Int) {
    START(1),
    STOP(2),
    DIAGNOSTIC(3),
    // 4 (snapshot) retired; 5 (scan map) is a module-owned control action.
}

/**
 * Control-plane request carried inside the existing COMMAND frame for bridge-v2
 * compatibility. [MARKER] separates runtime lifecycle traffic from gameplay
 * [BridgeEvent.AutomationCommand] payloads without overloading an AutomationAction.
 * [actionWire] is the raw action number (a [RuntimeControlAction] or a
 * [ModuleControlAction] wire value); it is not whitelisted by the transport.
 */
data class RuntimeControlRequest(
    val runtimeSessionId: String,
    val requestId: String,
    val actionWire: Int,
    val expiresAtElapsedNs: Long,
    val pid: Int,
    val processName: String,
    val packageName: String,
    val cycleId: Long? = null,
)

object RuntimeControlPayloadCodec {
    private val codec = BridgePayloadCodecSupport

    // ASCII "RTCT". This occupies the position where gameplay commands carry
    // their AutomationAction tag, making the two sub-protocols unambiguous.
    const val MARKER: Int = 0x52544354

    fun encode(request: RuntimeControlRequest): Result<ByteArray> = runCatching {
        require(request.runtimeSessionId.isNotBlank()) { "runtime session is required" }
        require(request.requestId.isNotBlank()) { "runtime control request id is required" }
        require(request.expiresAtElapsedNs > 0L) { "runtime control expiry is required" }
        require(request.pid > 0) { "runtime pid is required" }
        require(request.processName.isNotBlank()) { "runtime process is required" }
        require(request.packageName.isNotBlank()) { "runtime package is required" }

        ByteArrayOutputStream().use { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(codec.PAYLOAD_VERSION)
                codec.writeString(output, request.runtimeSessionId)
                codec.writeString(output, request.requestId)
                output.writeInt(MARKER)
                output.writeInt(request.actionWire)
                output.writeLong(request.expiresAtElapsedNs)
                output.writeInt(request.pid)
                codec.writeString(output, request.processName)
                codec.writeString(output, request.packageName)
                // Generic trailing field on every control frame (0 = unused).
                output.writeLong(request.cycleId ?: 0L)
            }
            bytes.toByteArray().also {
                require(it.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
                    "runtime control payload exceeds hard limit"
                }
            }
        }
    }

    fun decode(payload: ByteArray): Result<RuntimeControlRequest> = runCatching {
        require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
            "runtime control payload exceeds hard limit"
        }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == codec.PAYLOAD_VERSION) {
                "unsupported runtime control payload version"
            }
            val runtimeSessionId = codec.readString(input)
            val requestId = codec.readString(input)
            require(input.readInt() == MARKER) { "invalid runtime control marker" }
            // Raw action number (not whitelisted here); the native dispatcher
            // validates it against the lifecycle set + module owner map.
            val actionWire = input.readInt()
            val request = RuntimeControlRequest(
                runtimeSessionId = runtimeSessionId,
                requestId = requestId,
                actionWire = actionWire,
                expiresAtElapsedNs = input.readLong(),
                pid = input.readInt(),
                processName = codec.readString(input),
                packageName = codec.readString(input),
                // Generic trailing field on every control frame (0 = unused).
                cycleId = input.readLong(),
            )
            require(input.available() == 0) { "trailing bytes in runtime control payload" }
            require(request.runtimeSessionId.isNotBlank()) { "runtime session is required" }
            require(request.requestId.isNotBlank()) { "runtime control request id is required" }
            require(request.expiresAtElapsedNs > 0L) { "runtime control expiry is required" }
            require(request.pid > 0) { "runtime pid is required" }
            request
        }
    }
}
