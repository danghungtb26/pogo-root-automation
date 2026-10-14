package dev.pogoroot.automation.bridge

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream
import java.nio.ByteBuffer
import java.nio.charset.CharacterCodingException
import java.nio.charset.CodingErrorAction
import java.nio.charset.StandardCharsets

enum class RuntimeBootstrapPhase(val wireValue: Int) {
    CREATED(1),
    CORE_LOADED(2),
    LOADER_OBSERVER_ACTIVE(3),
    IL2CPP_LIBRARY_SEEN(4),
    IL2CPP_INITIALIZED(5),
    BUILD_VERIFIED(6),
    MAIN_THREAD_READY(7),
    BINDINGS_READY(8),
    LIVE_READY(9),
    FAILED(10),
    ;

    companion object {
        fun fromWireValue(value: Int): RuntimeBootstrapPhase =
            entries.firstOrNull { it.wireValue == value }
                ?: error("invalid bootstrap phase: $value")
    }
}

data class RuntimeBootstrapStatus(
    val sessionId: String,
    val pid: Int,
    val processName: String,
    val sequence: Long,
    val phase: RuntimeBootstrapPhase,
    val gameBuild: String? = null,
    val abi: String? = null,
    val translationLayer: String? = null,
    val recoverable: Boolean = false,
    val errorCode: String? = null,
    val message: String? = null,
    val strongIdentityVerified: Boolean = false,
    val capabilities: Set<String> = emptySet(),
    val observedAtEpochMs: Long,
    val observedAtElapsedNs: Long,
) {
    init {
        require(pid > 0 && sequence > 0L) { "invalid bootstrap identity or sequence" }
        requireBounded(sessionId, MAX_ID_BYTES, "sessionId")
        requireBounded(processName, MAX_ID_BYTES, "processName")
        gameBuild?.let { requireBounded(it, MAX_BUILD_BYTES, "gameBuild") }
        abi?.let { requireBounded(it, MAX_ABI_BYTES, "abi") }
        translationLayer?.let { requireBounded(it, MAX_ABI_BYTES, "translationLayer") }
        errorCode?.let { requireBounded(it, MAX_ERROR_BYTES, "errorCode") }
        message?.let { requireBounded(it, MAX_MESSAGE_BYTES, "message") }
        require(capabilities.size <= MAX_CAPABILITIES) { "too many bootstrap capabilities" }
        capabilities.forEach { requireBounded(it, MAX_ERROR_BYTES, "capability") }
        require(observedAtEpochMs >= 0L && observedAtElapsedNs >= 0L) {
            "invalid bootstrap timestamp"
        }
        if (phase == RuntimeBootstrapPhase.LIVE_READY) {
            require(strongIdentityVerified && !gameBuild.isNullOrBlank() && capabilities.isNotEmpty()) {
                "live bootstrap requires verified identity and capability"
            }
        }
        if (phase == RuntimeBootstrapPhase.FAILED) {
            require(!errorCode.isNullOrBlank() && !strongIdentityVerified && capabilities.isEmpty()) {
                "failed bootstrap requires error code and closed capabilities"
            }
        }
        if (phase != RuntimeBootstrapPhase.LIVE_READY) {
            require(capabilities.isEmpty()) { "pre-live bootstrap cannot expose capabilities" }
        }
        if (phase.wireValue < RuntimeBootstrapPhase.BUILD_VERIFIED.wireValue) {
            require(gameBuild == null && !strongIdentityVerified) {
                "pre-build bootstrap cannot expose game identity"
            }
        }
    }

    companion object {
        const val MARKER = 0x52425354
        const val SCHEMA_VERSION = 1
        const val MAX_ID_BYTES = 256
        const val MAX_BUILD_BYTES = 4096
        const val MAX_ABI_BYTES = 64
        const val MAX_ERROR_BYTES = 256
        const val MAX_MESSAGE_BYTES = 4096
        const val MAX_CAPABILITIES = 256

        private fun requireBounded(value: String, maximum: Int, field: String) {
            require(value.isNotBlank() && value.toByteArray(StandardCharsets.UTF_8).size <= maximum) {
                "invalid bootstrap $field"
            }
        }
    }
}

object RuntimeBootstrapStatusPayloadCodec {
    fun encode(status: RuntimeBootstrapStatus): Result<ByteArray> = runCatching {
        ByteArrayOutputStream().use { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(BridgePayloadCodecSupport.PAYLOAD_VERSION)
                output.writeInt(RuntimeBootstrapStatus.MARKER)
                output.writeInt(RuntimeBootstrapStatus.SCHEMA_VERSION)
                writeString(output, status.sessionId)
                output.writeInt(status.pid)
                writeString(output, status.processName)
                output.writeLong(status.sequence)
                output.writeInt(status.phase.wireValue)
                writeOptional(output, status.gameBuild)
                writeOptional(output, status.abi)
                writeOptional(output, status.translationLayer)
                output.writeBoolean(status.recoverable)
                writeOptional(output, status.errorCode)
                writeOptional(output, status.message)
                output.writeBoolean(status.strongIdentityVerified)
                output.writeInt(status.capabilities.size)
                status.capabilities.sorted().forEach { writeString(output, it) }
                output.writeLong(status.observedAtEpochMs)
                output.writeLong(status.observedAtElapsedNs)
            }
            bytes.toByteArray().also {
                require(it.size <= BridgeProtocol.HARD_MESSAGE_BYTES) { "bootstrap payload exceeds limit" }
            }
        }
    }

    fun decode(payload: ByteArray): Result<RuntimeBootstrapStatus> = runCatching {
        require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) { "bootstrap payload exceeds limit" }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == BridgePayloadCodecSupport.PAYLOAD_VERSION)
            require(input.readInt() == RuntimeBootstrapStatus.MARKER)
            require(input.readInt() == RuntimeBootstrapStatus.SCHEMA_VERSION)
            val sessionId = readString(input, RuntimeBootstrapStatus.MAX_ID_BYTES)
            val pid = input.readInt()
            val processName = readString(input, RuntimeBootstrapStatus.MAX_ID_BYTES)
            val sequence = input.readLong()
            val phase = RuntimeBootstrapPhase.fromWireValue(input.readInt())
            val gameBuild = readOptional(input, RuntimeBootstrapStatus.MAX_BUILD_BYTES)
            val abi = readOptional(input, RuntimeBootstrapStatus.MAX_ABI_BYTES)
            val translationLayer = readOptional(input, RuntimeBootstrapStatus.MAX_ABI_BYTES)
            val recoverable = input.readBoolean()
            val errorCode = readOptional(input, RuntimeBootstrapStatus.MAX_ERROR_BYTES)
            val message = readOptional(input, RuntimeBootstrapStatus.MAX_MESSAGE_BYTES)
            val strongIdentityVerified = input.readBoolean()
            val count = input.readInt()
            require(count in 0..RuntimeBootstrapStatus.MAX_CAPABILITIES) {
                "invalid bootstrap capability count"
            }
            val capabilities = List(count) {
                readString(input, RuntimeBootstrapStatus.MAX_ERROR_BYTES)
            }
            require(capabilities == capabilities.distinct().sorted()) {
                "bootstrap capabilities must be sorted and unique"
            }
            val status = RuntimeBootstrapStatus(
                sessionId, pid, processName, sequence, phase, gameBuild, abi,
                translationLayer, recoverable, errorCode, message, strongIdentityVerified,
                capabilities.toSet(), input.readLong(), input.readLong(),
            )
            require(input.available() == 0) { "trailing bootstrap payload" }
            status
        }
    }

    private fun writeString(output: DataOutputStream, value: String) {
        val bytes = value.toByteArray(StandardCharsets.UTF_8)
        output.writeInt(bytes.size)
        output.write(bytes)
    }

    private fun writeOptional(output: DataOutputStream, value: String?) {
        output.writeBoolean(value != null)
        if (value != null) writeString(output, value)
    }

    private fun readOptional(input: DataInputStream, maximum: Int): String? =
        if (input.readBoolean()) readString(input, maximum) else null

    private fun readString(input: DataInputStream, maximum: Int): String {
        val size = input.readInt()
        require(size in 1..maximum) { "invalid bootstrap string length" }
        val bytes = ByteArray(size).also(input::readFully)
        return try {
            StandardCharsets.UTF_8.newDecoder()
                .onMalformedInput(CodingErrorAction.REPORT)
                .onUnmappableCharacter(CodingErrorAction.REPORT)
                .decode(ByteBuffer.wrap(bytes)).toString()
        } catch (error: CharacterCodingException) {
            throw IllegalArgumentException("invalid bootstrap UTF-8", error)
        }
    }
}
