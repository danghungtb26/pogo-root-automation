package dev.pogoroot.automation.bridge

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream

/** Runtime mirror of the settings consumed by the native catch_spin module. */
data class RuntimeCatchSpinConfig(
    val configRevision: Long,
    val armed: Boolean,
    val autoCatch: Boolean,
    val autoSpin: Boolean,
    val autoEncounter: Boolean,
    val catchAll: Boolean,
    val spinSettleDelayMs: Long,
    val catchSettleDelayMs: Long,
) {
    init {
        require(configRevision > 0L) { "catch_spin config revision must be positive" }
        require(spinSettleDelayMs in 0L..MAX_SETTLE_DELAY_MS) {
            "spin settle delay is outside the supported range"
        }
        require(catchSettleDelayMs in 0L..MAX_SETTLE_DELAY_MS) {
            "catch settle delay is outside the supported range"
        }
    }

    companion object {
        const val SCHEMA_VERSION: Int = 1
        const val MAX_SETTLE_DELAY_MS: Long = 60_000L
    }
}

data class RuntimeCatchSpinConfigRequest(
    val runtimeSessionId: String,
    val requestId: String,
    val config: RuntimeCatchSpinConfig,
    val expiresAtElapsedNs: Long,
    val pid: Int,
    val processName: String,
    val packageName: String,
    val buildFingerprint: String,
)

/**
 * Config sub-protocol carried inside the existing bridge COMMAND frame. It is
 * deliberately separate from world-snapshot telemetry so a config update is idempotent and
 * cannot be mistaken for a gameplay action.
 */
object RuntimeCatchSpinConfigPayloadCodec {
    private val codec = BridgePayloadCodecSupport

    // ASCII "CSCF".
    const val MARKER: Int = 0x43534346

    fun encode(request: RuntimeCatchSpinConfigRequest): Result<ByteArray> = runCatching {
        require(request.runtimeSessionId.isNotBlank()) { "runtime session is required" }
        require(request.requestId.isNotBlank()) { "runtime config request id is required" }
        require(request.expiresAtElapsedNs > 0L) { "runtime config expiry is required" }
        require(request.pid > 0) { "runtime pid is required" }
        require(request.processName.isNotBlank()) { "runtime process is required" }
        require(request.packageName.isNotBlank()) { "runtime package is required" }
        require(request.buildFingerprint.isNotBlank()) { "runtime build fingerprint is required" }

        ByteArrayOutputStream().use { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(codec.PAYLOAD_VERSION)
                codec.writeString(output, request.runtimeSessionId)
                codec.writeString(output, request.requestId)
                output.writeInt(MARKER)
                output.writeInt(RuntimeFeatureModule.CATCH_SPIN.wireValue)
                output.writeInt(RuntimeCatchSpinConfig.SCHEMA_VERSION)
                output.writeLong(request.config.configRevision)
                output.writeBoolean(request.config.armed)
                output.writeBoolean(request.config.autoCatch)
                output.writeBoolean(request.config.autoSpin)
                output.writeBoolean(request.config.autoEncounter)
                output.writeBoolean(request.config.catchAll)
                output.writeLong(request.config.spinSettleDelayMs)
                output.writeLong(request.config.catchSettleDelayMs)
                output.writeLong(request.expiresAtElapsedNs)
                output.writeInt(request.pid)
                codec.writeString(output, request.processName)
                codec.writeString(output, request.packageName)
                codec.writeString(output, request.buildFingerprint)
            }
            bytes.toByteArray().also {
                require(it.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
                    "runtime catch_spin config exceeds hard limit"
                }
            }
        }
    }

    fun decode(payload: ByteArray): Result<RuntimeCatchSpinConfigRequest> = runCatching {
        require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
            "runtime catch_spin config exceeds hard limit"
        }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == codec.PAYLOAD_VERSION) {
                "unsupported runtime catch_spin config payload version"
            }
            val runtimeSessionId = codec.readString(input)
            val requestId = codec.readString(input)
            require(input.readInt() == MARKER) { "invalid runtime catch_spin config marker" }
            require(input.readInt() == RuntimeFeatureModule.CATCH_SPIN.wireValue) {
                "invalid runtime catch_spin config module"
            }
            val schemaVersion = input.readInt()
            require(schemaVersion == RuntimeCatchSpinConfig.SCHEMA_VERSION) {
                "unsupported runtime catch_spin config schema"
            }
            val config = RuntimeCatchSpinConfig(
                configRevision = input.readLong(),
                armed = input.readBoolean(),
                autoCatch = input.readBoolean(),
                autoSpin = input.readBoolean(),
                autoEncounter = input.readBoolean(),
                catchAll = input.readBoolean(),
                spinSettleDelayMs = input.readLong(),
                catchSettleDelayMs = input.readLong(),
            )
            val request = RuntimeCatchSpinConfigRequest(
                runtimeSessionId = runtimeSessionId,
                requestId = requestId,
                config = config,
                expiresAtElapsedNs = input.readLong(),
                pid = input.readInt(),
                processName = codec.readString(input),
                packageName = codec.readString(input),
                buildFingerprint = codec.readString(input),
            )
            require(input.available() == 0) { "trailing bytes in runtime catch_spin config" }
            require(request.runtimeSessionId.isNotBlank()) { "runtime session is required" }
            require(request.requestId.isNotBlank()) { "runtime config request id is required" }
            require(request.expiresAtElapsedNs > 0L) { "runtime config expiry is required" }
            require(request.pid > 0) { "runtime pid is required" }
            request
        }
    }
}
