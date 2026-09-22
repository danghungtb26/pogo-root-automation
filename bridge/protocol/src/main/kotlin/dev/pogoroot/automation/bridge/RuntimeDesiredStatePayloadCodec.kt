package dev.pogoroot.automation.bridge

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream

data class RuntimeDesiredState(
    val configRevision: Long,
    val enabled: Boolean,
    val catchSpinArmed: Boolean,
    val autoCatch: Boolean,
    val autoSpin: Boolean,
    val autoEncounter: Boolean,
    val catchAll: Boolean,
    val autoWalkToFort: Boolean,
    val spinSettleDelayMs: Long,
    val catchSettleDelayMs: Long,
    val autoDiscard: Boolean,
    val discardLimits: Map<Int, Int>,
    val autoTransfer: Boolean,
    val minimumIvPercentToKeep: Double,
    val keepUnknownIv: Boolean = true,
    val keepShiny: Boolean = true,
    val keepHundo: Boolean = true,
    val keepSpecialBackground: Boolean = true,
    val keepFavorite: Boolean = true,
    val keepLegendary: Boolean = true,
    val keepMythical: Boolean = true,
) {
    init {
        require(configRevision > 0L) { "desired state revision must be positive" }
        require(spinSettleDelayMs in 0L..MAX_SETTLE_DELAY_MS) {
            "desired spin settle delay is outside the supported range"
        }
        require(catchSettleDelayMs in 0L..MAX_SETTLE_DELAY_MS) {
            "desired catch settle delay is outside the supported range"
        }
        require(discardLimits.size <= MAX_DISCARD_LIMITS) {
            "desired state contains too many discard limits"
        }
        require(discardLimits.keys.all { it > 0 }) {
            "desired discard item ids must be positive"
        }
        require(discardLimits.values.all { it >= 0 }) {
            "desired discard counts must be non-negative"
        }
        require(minimumIvPercentToKeep.isFinite() && minimumIvPercentToKeep in 0.0..100.0) {
            "desired transfer IV threshold is outside the supported range"
        }
    }

    companion object {
        const val SCHEMA_VERSION = 1
        const val MARKER = 0x52445354
        const val MAX_DISCARD_LIMITS = 64
        const val MAX_SETTLE_DELAY_MS = 60_000L
        const val MAX_LOGICAL_ID_BYTES = 256
        const val MAX_BUILD_FINGERPRINT_BYTES = 4 * 1024
        const val MAX_EXPIRY_NS = 30_000_000_000L
    }
}

data class RuntimeDesiredStateRequest(
    val runtimeSessionId: String,
    val requestId: String,
    val state: RuntimeDesiredState,
    val expiresAtElapsedNs: Long,
    val pid: Int,
    val processName: String,
    val packageName: String,
    val buildFingerprint: String,
) {
    init {
        require(runtimeSessionId.isNotBlank()) { "desired state session is required" }
        require(requestId.isNotBlank()) { "desired state request id is required" }
        require(expiresAtElapsedNs > 0L) { "desired state expiry is required" }
        require(pid > 0) { "desired state pid must be positive" }
        require(processName.isNotBlank()) { "desired state process is required" }
        require(packageName.isNotBlank()) { "desired state package is required" }
        require(buildFingerprint.isNotBlank()) { "desired state build fingerprint is required" }
        requireDesiredStateUtf8Size(runtimeSessionId, RuntimeDesiredState.MAX_LOGICAL_ID_BYTES, "session")
        requireDesiredStateUtf8Size(requestId, RuntimeDesiredState.MAX_LOGICAL_ID_BYTES, "request id")
        requireDesiredStateUtf8Size(processName, RuntimeDesiredState.MAX_LOGICAL_ID_BYTES, "process")
        requireDesiredStateUtf8Size(packageName, RuntimeDesiredState.MAX_LOGICAL_ID_BYTES, "package")
        requireDesiredStateUtf8Size(
            buildFingerprint,
            RuntimeDesiredState.MAX_BUILD_FINGERPRINT_BYTES,
            "build fingerprint",
        )
    }
}

private fun requireDesiredStateUtf8Size(value: String, maxBytes: Int, label: String) {
    require(value.toByteArray(Charsets.UTF_8).size <= maxBytes) {
        "desired state $label exceeds logical limit"
    }
}

object RuntimeDesiredStatePayloadCodec {
    private val codec = BridgePayloadCodecSupport

    fun encode(request: RuntimeDesiredStateRequest): Result<ByteArray> = runCatching {
        ByteArrayOutputStream().use { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(codec.PAYLOAD_VERSION)
                codec.writeString(output, request.runtimeSessionId)
                codec.writeString(output, request.requestId)
                output.writeInt(RuntimeDesiredState.MARKER)
                output.writeInt(RuntimeDesiredState.SCHEMA_VERSION)
                val state = request.state
                output.writeLong(state.configRevision)
                output.writeBoolean(state.enabled)
                output.writeBoolean(state.catchSpinArmed)
                output.writeBoolean(state.autoCatch)
                output.writeBoolean(state.autoSpin)
                output.writeBoolean(state.autoEncounter)
                output.writeBoolean(state.catchAll)
                output.writeBoolean(state.autoWalkToFort)
                output.writeLong(state.spinSettleDelayMs)
                output.writeLong(state.catchSettleDelayMs)
                output.writeBoolean(state.autoDiscard)
                val limits = state.discardLimits.entries.sortedBy { it.key }
                output.writeInt(limits.size)
                limits.forEach { (itemId, maxCount) ->
                    output.writeInt(itemId)
                    output.writeInt(maxCount)
                }
                output.writeBoolean(state.autoTransfer)
                output.writeDouble(state.minimumIvPercentToKeep)
                output.writeBoolean(state.keepUnknownIv)
                output.writeBoolean(state.keepShiny)
                output.writeBoolean(state.keepHundo)
                output.writeBoolean(state.keepSpecialBackground)
                output.writeBoolean(state.keepFavorite)
                output.writeBoolean(state.keepLegendary)
                output.writeBoolean(state.keepMythical)
                output.writeLong(request.expiresAtElapsedNs)
                output.writeInt(request.pid)
                codec.writeString(output, request.processName)
                codec.writeString(output, request.packageName)
                codec.writeString(output, request.buildFingerprint)
            }
            bytes.toByteArray().also {
                require(it.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
                    "desired state payload exceeds hard limit"
                }
            }
        }
    }

    fun decode(payload: ByteArray): Result<RuntimeDesiredStateRequest> = runCatching {
        require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
            "desired state payload exceeds hard limit"
        }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == codec.PAYLOAD_VERSION) {
                "unsupported desired state payload version"
            }
            val runtimeSessionId = codec.readString(input)
            val requestId = codec.readString(input)
            require(input.readInt() == RuntimeDesiredState.MARKER) {
                "invalid desired state marker"
            }
            require(input.readInt() == RuntimeDesiredState.SCHEMA_VERSION) {
                "unsupported desired state schema"
            }
            val configRevision = input.readLong()
            val enabled = input.readStrictBoolean()
            val catchSpinArmed = input.readStrictBoolean()
            val autoCatch = input.readStrictBoolean()
            val autoSpin = input.readStrictBoolean()
            val autoEncounter = input.readStrictBoolean()
            val catchAll = input.readStrictBoolean()
            val autoWalkToFort = input.readStrictBoolean()
            val spinSettleDelayMs = input.readLong()
            val catchSettleDelayMs = input.readLong()
            val autoDiscard = input.readStrictBoolean()
            val limitCount = input.readInt()
            require(limitCount in 0..RuntimeDesiredState.MAX_DISCARD_LIMITS) {
                "invalid desired discard limit count"
            }
            val discardLimits = linkedMapOf<Int, Int>()
            var previousItemId = 0
            repeat(limitCount) {
                val itemId = input.readInt()
                val maxCount = input.readInt()
                require(itemId > previousItemId) {
                    "desired discard item ids must be sorted and unique"
                }
                require(maxCount >= 0) { "desired discard count must be non-negative" }
                previousItemId = itemId
                discardLimits[itemId] = maxCount
            }
            val state = RuntimeDesiredState(
                configRevision = configRevision,
                enabled = enabled,
                catchSpinArmed = catchSpinArmed,
                autoCatch = autoCatch,
                autoSpin = autoSpin,
                autoEncounter = autoEncounter,
                catchAll = catchAll,
                autoWalkToFort = autoWalkToFort,
                spinSettleDelayMs = spinSettleDelayMs,
                catchSettleDelayMs = catchSettleDelayMs,
                autoDiscard = autoDiscard,
                discardLimits = discardLimits,
                autoTransfer = input.readStrictBoolean(),
                minimumIvPercentToKeep = input.readDouble(),
                keepUnknownIv = input.readStrictBoolean(),
                keepShiny = input.readStrictBoolean(),
                keepHundo = input.readStrictBoolean(),
                keepSpecialBackground = input.readStrictBoolean(),
                keepFavorite = input.readStrictBoolean(),
                keepLegendary = input.readStrictBoolean(),
                keepMythical = input.readStrictBoolean(),
            )
            val request = RuntimeDesiredStateRequest(
                runtimeSessionId = runtimeSessionId,
                requestId = requestId,
                state = state,
                expiresAtElapsedNs = input.readLong(),
                pid = input.readInt(),
                processName = codec.readString(input),
                packageName = codec.readString(input),
                buildFingerprint = codec.readString(input),
            )
            require(input.available() == 0) { "trailing bytes in desired state payload" }
            request
        }
    }

    private fun requireUtf8Size(value: String, maxBytes: Int, label: String) {
        require(value.toByteArray(Charsets.UTF_8).size <= maxBytes) {
            "desired state $label exceeds logical limit"
        }
    }

    private fun DataInputStream.readStrictBoolean(): Boolean = when (readUnsignedByte()) {
        0 -> false
        1 -> true
        else -> error("invalid boolean wire value")
    }
}
