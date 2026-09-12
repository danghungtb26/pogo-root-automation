package dev.pogoroot.automation.bridge

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream

/** Runtime mirror of the keep policy consumed by the native transfer module. */
data class RuntimeTransferConfig(
    val configRevision: Long,
    val autoTransfer: Boolean,
    val keepUnknownIv: Boolean = true,
    val minimumIvPercentToKeep: Double,
    val keepShiny: Boolean,
    val keepHundo: Boolean,
    val keepSpecialBackground: Boolean,
    val keepFavorite: Boolean,
    val keepLegendary: Boolean = true,
    val keepMythical: Boolean = true,
) {
    init {
        require(configRevision > 0L) { "transfer config revision must be positive" }
        require(minimumIvPercentToKeep in 0.0..100.0) {
            "transfer IV threshold is outside the supported range"
        }
    }

    companion object {
        const val SCHEMA_VERSION: Int = 1
    }
}

data class RuntimeTransferConfigRequest(
    val runtimeSessionId: String,
    val requestId: String,
    val config: RuntimeTransferConfig,
    val expiresAtElapsedNs: Long,
    val pid: Int,
    val processName: String,
    val packageName: String,
    val buildFingerprint: String,
)

/** Dedicated transfer policy protocol; it is not coupled to catch_spin config. */
object RuntimeTransferConfigPayloadCodec {
    private val codec = BridgePayloadCodecSupport

    // ASCII "TRCF".
    const val MARKER: Int = 0x54524346

    fun encode(request: RuntimeTransferConfigRequest): Result<ByteArray> = runCatching {
        require(request.runtimeSessionId.isNotBlank()) { "runtime session is required" }
        require(request.requestId.isNotBlank()) { "runtime transfer config request id is required" }
        require(request.expiresAtElapsedNs > 0L) { "runtime transfer config expiry is required" }
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
                output.writeInt(RuntimeFeatureModule.TRANSFER.wireValue)
                output.writeInt(RuntimeTransferConfig.SCHEMA_VERSION)
                output.writeLong(request.config.configRevision)
                output.writeBoolean(request.config.autoTransfer)
                output.writeBoolean(request.config.keepUnknownIv)
                output.writeDouble(request.config.minimumIvPercentToKeep)
                output.writeBoolean(request.config.keepShiny)
                output.writeBoolean(request.config.keepHundo)
                output.writeBoolean(request.config.keepSpecialBackground)
                output.writeBoolean(request.config.keepFavorite)
                output.writeBoolean(request.config.keepLegendary)
                output.writeBoolean(request.config.keepMythical)
                output.writeLong(request.expiresAtElapsedNs)
                output.writeInt(request.pid)
                codec.writeString(output, request.processName)
                codec.writeString(output, request.packageName)
                codec.writeString(output, request.buildFingerprint)
            }
            bytes.toByteArray().also {
                require(it.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
                    "runtime transfer config exceeds hard limit"
                }
            }
        }
    }

    fun decode(payload: ByteArray): Result<RuntimeTransferConfigRequest> = runCatching {
        require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
            "runtime transfer config exceeds hard limit"
        }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == codec.PAYLOAD_VERSION) {
                "unsupported runtime transfer config payload version"
            }
            val runtimeSessionId = codec.readString(input)
            val requestId = codec.readString(input)
            require(input.readInt() == MARKER) { "invalid runtime transfer config marker" }
            require(input.readInt() == RuntimeFeatureModule.TRANSFER.wireValue) {
                "invalid runtime transfer config module"
            }
            require(input.readInt() == RuntimeTransferConfig.SCHEMA_VERSION) {
                "unsupported runtime transfer config schema"
            }
            val config = RuntimeTransferConfig(
                configRevision = input.readLong(),
                autoTransfer = input.readBoolean(),
                keepUnknownIv = input.readBoolean(),
                minimumIvPercentToKeep = input.readDouble(),
                keepShiny = input.readBoolean(),
                keepHundo = input.readBoolean(),
                keepSpecialBackground = input.readBoolean(),
                keepFavorite = input.readBoolean(),
                keepLegendary = input.readBoolean(),
                keepMythical = input.readBoolean(),
            )
            val request = RuntimeTransferConfigRequest(
                runtimeSessionId = runtimeSessionId,
                requestId = requestId,
                config = config,
                expiresAtElapsedNs = input.readLong(),
                pid = input.readInt(),
                processName = codec.readString(input),
                packageName = codec.readString(input),
                buildFingerprint = codec.readString(input),
            )
            require(input.available() == 0) { "trailing bytes in runtime transfer config" }
            require(request.runtimeSessionId.isNotBlank()) { "runtime session is required" }
            require(request.requestId.isNotBlank()) { "runtime transfer config request id is required" }
            require(request.expiresAtElapsedNs > 0L) { "runtime transfer config expiry is required" }
            require(request.pid > 0) { "runtime pid is required" }
            request
        }
    }
}
