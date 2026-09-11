package dev.pogoroot.automation.bridge

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream

/** Independently activatable native feature groups hosted by the Zygisk runtime. */
enum class RuntimeFeatureModule(val wireValue: Int) {
    CATCH_SPIN(1),
    DISCARD(2),
    TRANSFER(3),
    ENCOUNTER(4),
}

enum class RuntimeModuleControlAction(val wireValue: Int) {
    ENABLE(1),
    DISABLE(2),
}

data class RuntimeModuleControlRequest(
    val runtimeSessionId: String,
    val requestId: String,
    val module: RuntimeFeatureModule,
    val action: RuntimeModuleControlAction,
    val expiresAtElapsedNs: Long,
    val pid: Int,
    val processName: String,
    val packageName: String,
)

/**
 * Module-control sub-protocol carried by the existing bridge COMMAND frame.
 * The runtime host is injected once; each feature module can then be activated
 * or deactivated independently by the foreground service.
 */
object RuntimeModuleControlPayloadCodec {
    private val codec = BridgePayloadCodecSupport

    // ASCII "RTMD".
    const val MARKER: Int = 0x52544D44

    fun encode(request: RuntimeModuleControlRequest): Result<ByteArray> = runCatching {
        require(request.runtimeSessionId.isNotBlank()) { "runtime session is required" }
        require(request.requestId.isNotBlank()) { "runtime module request id is required" }
        require(request.expiresAtElapsedNs > 0L) { "runtime module expiry is required" }
        require(request.pid > 0) { "runtime pid is required" }
        require(request.processName.isNotBlank()) { "runtime process is required" }
        require(request.packageName.isNotBlank()) { "runtime package is required" }

        ByteArrayOutputStream().use { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(codec.PAYLOAD_VERSION)
                codec.writeString(output, request.runtimeSessionId)
                codec.writeString(output, request.requestId)
                output.writeInt(MARKER)
                output.writeInt(request.module.wireValue)
                output.writeInt(request.action.wireValue)
                output.writeLong(request.expiresAtElapsedNs)
                output.writeInt(request.pid)
                codec.writeString(output, request.processName)
                codec.writeString(output, request.packageName)
            }
            bytes.toByteArray().also {
                require(it.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
                    "runtime module payload exceeds hard limit"
                }
            }
        }
    }

    fun decode(payload: ByteArray): Result<RuntimeModuleControlRequest> = runCatching {
        require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
            "runtime module payload exceeds hard limit"
        }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == codec.PAYLOAD_VERSION) {
                "unsupported runtime module payload version"
            }
            val runtimeSessionId = codec.readString(input)
            val requestId = codec.readString(input)
            require(input.readInt() == MARKER) { "invalid runtime module marker" }
            val moduleWire = input.readInt()
            val module = RuntimeFeatureModule.entries.firstOrNull { it.wireValue == moduleWire }
                ?: error("invalid runtime feature module: $moduleWire")
            val actionWire = input.readInt()
            val action = RuntimeModuleControlAction.entries.firstOrNull { it.wireValue == actionWire }
                ?: error("invalid runtime module action: $actionWire")
            val request = RuntimeModuleControlRequest(
                runtimeSessionId = runtimeSessionId,
                requestId = requestId,
                module = module,
                action = action,
                expiresAtElapsedNs = input.readLong(),
                pid = input.readInt(),
                processName = codec.readString(input),
                packageName = codec.readString(input),
            )
            require(input.available() == 0) { "trailing bytes in runtime module payload" }
            require(request.runtimeSessionId.isNotBlank()) { "runtime session is required" }
            require(request.requestId.isNotBlank()) { "runtime module request id is required" }
            require(request.expiresAtElapsedNs > 0L) { "runtime module expiry is required" }
            require(request.pid > 0) { "runtime pid is required" }
            request
        }
    }
}
