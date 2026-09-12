package dev.pogoroot.automation.bridge

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream

/** Runtime mirror of the Kotlin-owned per-item auto-discard limits. */
data class RuntimeDiscardConfig(
    val configRevision: Long,
    val autoDiscard: Boolean,
    val maxCountByItemId: Map<Int, Int>,
) {
    init {
        require(configRevision > 0L) { "discard config revision must be positive" }
        require(maxCountByItemId.size <= MAX_LIMITS) {
            "discard config contains too many item limits"
        }
        require(maxCountByItemId.keys.all { it > 0 }) {
            "discard item ids must be positive"
        }
        require(maxCountByItemId.values.all { it >= 0 }) {
            "discard max counts must be non-negative"
        }
    }

    companion object {
        const val SCHEMA_VERSION: Int = 1
        const val MAX_LIMITS: Int = 64
    }
}

data class RuntimeDiscardConfigRequest(
    val runtimeSessionId: String,
    val requestId: String,
    val config: RuntimeDiscardConfig,
    val expiresAtElapsedNs: Long,
    val pid: Int,
    val processName: String,
    val packageName: String,
    val buildFingerprint: String,
)

/** Dedicated discard policy protocol; it is independent from gameplay commands. */
object RuntimeDiscardConfigPayloadCodec {
    private val codec = BridgePayloadCodecSupport

    // ASCII "DSCF".
    const val MARKER: Int = 0x44534346

    fun encode(request: RuntimeDiscardConfigRequest): Result<ByteArray> = runCatching {
        require(request.runtimeSessionId.isNotBlank()) { "runtime session is required" }
        require(request.requestId.isNotBlank()) { "runtime discard config request id is required" }
        require(request.expiresAtElapsedNs > 0L) { "runtime discard config expiry is required" }
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
                output.writeInt(RuntimeFeatureModule.DISCARD.wireValue)
                output.writeInt(RuntimeDiscardConfig.SCHEMA_VERSION)
                output.writeLong(request.config.configRevision)
                output.writeBoolean(request.config.autoDiscard)
                val limits = request.config.maxCountByItemId.entries.sortedBy { it.key }
                output.writeInt(limits.size)
                limits.forEach { (itemId, maxCount) ->
                    output.writeInt(itemId)
                    output.writeInt(maxCount)
                }
                output.writeLong(request.expiresAtElapsedNs)
                output.writeInt(request.pid)
                codec.writeString(output, request.processName)
                codec.writeString(output, request.packageName)
                codec.writeString(output, request.buildFingerprint)
            }
            bytes.toByteArray().also {
                require(it.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
                    "runtime discard config exceeds hard limit"
                }
            }
        }
    }

    fun decode(payload: ByteArray): Result<RuntimeDiscardConfigRequest> = runCatching {
        require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
            "runtime discard config exceeds hard limit"
        }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == codec.PAYLOAD_VERSION) {
                "unsupported runtime discard config payload version"
            }
            val runtimeSessionId = codec.readString(input)
            val requestId = codec.readString(input)
            require(input.readInt() == MARKER) { "invalid runtime discard config marker" }
            require(input.readInt() == RuntimeFeatureModule.DISCARD.wireValue) {
                "invalid runtime discard config module"
            }
            require(input.readInt() == RuntimeDiscardConfig.SCHEMA_VERSION) {
                "unsupported runtime discard config schema"
            }
            val revision = input.readLong()
            val autoDiscard = input.readBoolean()
            val limitCount = input.readInt()
            require(limitCount in 0..RuntimeDiscardConfig.MAX_LIMITS) {
                "invalid runtime discard limit count"
            }
            val limits = linkedMapOf<Int, Int>()
            repeat(limitCount) {
                val itemId = input.readInt()
                val maxCount = input.readInt()
                require(itemId > 0) { "discard item ids must be positive" }
                require(maxCount >= 0) { "discard max counts must be non-negative" }
                require(limits.put(itemId, maxCount) == null) {
                    "duplicate runtime discard item id"
                }
            }
            val request = RuntimeDiscardConfigRequest(
                runtimeSessionId = runtimeSessionId,
                requestId = requestId,
                config = RuntimeDiscardConfig(revision, autoDiscard, limits),
                expiresAtElapsedNs = input.readLong(),
                pid = input.readInt(),
                processName = codec.readString(input),
                packageName = codec.readString(input),
                buildFingerprint = codec.readString(input),
            )
            require(input.available() == 0) { "trailing bytes in runtime discard config" }
            require(request.runtimeSessionId.isNotBlank()) { "runtime session is required" }
            require(request.requestId.isNotBlank()) { "runtime discard config request id is required" }
            require(request.expiresAtElapsedNs > 0L) { "runtime discard config expiry is required" }
            require(request.pid > 0) { "runtime pid is required" }
            require(request.processName.isNotBlank()) { "runtime process is required" }
            require(request.packageName.isNotBlank()) { "runtime package is required" }
            require(request.buildFingerprint.isNotBlank()) { "runtime build fingerprint is required" }
            request
        }
    }
}
