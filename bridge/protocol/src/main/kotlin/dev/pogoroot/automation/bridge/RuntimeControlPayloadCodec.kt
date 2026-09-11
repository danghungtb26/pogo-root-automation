package dev.pogoroot.automation.bridge

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream

/**
 * Control-plane request carried inside the existing COMMAND frame for bridge-v2
 * compatibility. [MARKER] separates runtime lifecycle traffic from gameplay
 * [BridgeEvent.AutomationCommand] payloads without overloading an AutomationAction.
 */
enum class RuntimeControlAction(val wireValue: Int) {
    START(1),
    STOP(2),
    DIAGNOSTIC(3),
    SNAPSHOT(4),
    SCAN_MAP(5),
}

data class RuntimeControlRequest(
    val runtimeSessionId: String,
    val requestId: String,
    val action: RuntimeControlAction,
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
        if (request.action == RuntimeControlAction.SCAN_MAP) {
            require(request.cycleId != null && request.cycleId > 0L) {
                "scan map cycle is required"
            }
        }

        ByteArrayOutputStream().use { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(codec.PAYLOAD_VERSION)
                codec.writeString(output, request.runtimeSessionId)
                codec.writeString(output, request.requestId)
                output.writeInt(MARKER)
                output.writeInt(request.action.wireValue)
                output.writeLong(request.expiresAtElapsedNs)
                output.writeInt(request.pid)
                codec.writeString(output, request.processName)
                codec.writeString(output, request.packageName)
                request.cycleId?.let(output::writeLong)
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
            val actionWire = input.readInt()
            val action = RuntimeControlAction.entries.firstOrNull { it.wireValue == actionWire }
                ?: error("invalid runtime control action: $actionWire")
            val request = RuntimeControlRequest(
                runtimeSessionId = runtimeSessionId,
                requestId = requestId,
                action = action,
                expiresAtElapsedNs = input.readLong(),
                pid = input.readInt(),
                processName = codec.readString(input),
                packageName = codec.readString(input),
                cycleId = if (input.available() > 0) input.readLong() else null,
            )
            require(input.available() == 0) { "trailing bytes in runtime control payload" }
            require(request.runtimeSessionId.isNotBlank()) { "runtime session is required" }
            require(request.requestId.isNotBlank()) { "runtime control request id is required" }
            require(request.expiresAtElapsedNs > 0L) { "runtime control expiry is required" }
            require(request.pid > 0) { "runtime pid is required" }
            if (request.action == RuntimeControlAction.SCAN_MAP) {
                require(request.cycleId != null && request.cycleId > 0L) {
                    "scan map cycle is required"
                }
            }
            request
        }
    }
}
