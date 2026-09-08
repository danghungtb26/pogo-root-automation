package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.CatchOutcome
import dev.pogoroot.automation.core.automation.RuntimeIdentity
import dev.pogoroot.automation.core.model.GameLifecycleState
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.InputStream
import java.io.OutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

object BridgeProtocol {
    const val VERSION = 2
    const val OBSERVATION_PAYLOAD_VERSION = 1
    const val NORMAL_MESSAGE_BYTES = 1 * 1024 * 1024
    const val HARD_MESSAGE_BYTES = 4 * 1024 * 1024
    const val FRAME_HEADER_BYTES = 4 + 2 + 2 + 8
}

enum class BridgeMessageType(val wireValue: Int) {
    HELLO(1),
    RUNTIME_READY(2),
    OBSERVATION(3),
    COMMAND(4),
    COMMAND_RESULT(5),
    BINDING_LOST(6),
    ERROR(7),
    PING(8),
    PONG(9),
    RUNTIME_STATUS(10),
    NEARBY_UPDATED(11),
    ;

    companion object {
        fun fromWireValue(value: Int): BridgeMessageType = entries.firstOrNull { it.wireValue == value }
            ?: error("unknown bridge message type: $value")
    }
}

enum class ObservationType(val wireValue: Int) {
    LIFECYCLE(1),
    NEARBY(2),
    ENCOUNTER(3),
    FORTS(4),
    INVENTORY(5),
    POKEMON_STORAGE(6),
}

enum class CommandPhase(val wireValue: Int) {
    ACCEPTED(1),
    STARTED(2),
    COMPLETED(3),
    REJECTED(4),
    FAILED(5),
    SAFE_TIMEOUT(6),
    INDETERMINATE(7),
    ;

    val isTerminal: Boolean
        get() = this in setOf(COMPLETED, REJECTED, FAILED, SAFE_TIMEOUT, INDETERMINATE)

    val mayHaveRun: Boolean
        get() = this in setOf(ACCEPTED, STARTED, COMPLETED, INDETERMINATE)
}

/** Common fields are intentionally available on every runtime message. */
sealed interface BridgeEvent {
    val protocolVersion: Int
    val runtimeSessionId: String?
        get() = null
    val messageSeq: Long?
        get() = null
    val basedOnObservationSeq: Long?
        get() = null

    data class RuntimeStatus(
        override val protocolVersion: Int = BridgeProtocol.VERSION,
        val processName: String,
        val gameVersion: String?,
        val lifecycleState: GameLifecycleState,
        override val runtimeSessionId: String? = null,
        override val messageSeq: Long? = null,
    ) : BridgeEvent

    data class RuntimeReady(
        override val protocolVersion: Int = BridgeProtocol.VERSION,
        override val runtimeSessionId: String,
        override val messageSeq: Long,
        val pid: Int,
        val processName: String,
        val packageName: String,
        val buildFingerprint: String,
        /** True only when the runtime proved a strong, version-scoped identity. */
        val strongIdentityVerified: Boolean = false,
        val capabilities: Set<String>,
        val gameVersionName: String? = null,
        val gameVersionCode: Long? = null,
        val observedAtEpochMs: Long = System.currentTimeMillis(),
        val observedAtElapsedNs: Long = System.nanoTime(),
    ) : BridgeEvent {
        init {
            require(runtimeSessionId.isNotBlank()) { "runtimeSessionId must not be blank" }
            require(messageSeq > 0L) { "messageSeq must be positive" }
            require(pid > 0) { "pid must be positive" }
            require(processName.isNotBlank()) { "processName must not be blank" }
            require(packageName.isNotBlank()) { "packageName must not be blank" }
            require(buildFingerprint.isNotBlank()) { "buildFingerprint must not be blank" }
        }
    }

    data class ObservationEvent(
        override val protocolVersion: Int = BridgeProtocol.VERSION,
        override val runtimeSessionId: String,
        override val messageSeq: Long,
        val observationType: ObservationType,
        val payloadVersion: Int,
        val payload: ByteArray,
        val observedAtEpochMs: Long,
        val observedAtElapsedNs: Long,
        val pid: Int,
        val processName: String,
        val packageName: String,
        val buildFingerprint: String,
        val playerLatitude: Double? = null,
        val playerLongitude: Double? = null,
        val lifecycleState: GameLifecycleState? = null,
    ) : BridgeEvent {
        init {
            require(runtimeSessionId.isNotBlank()) { "runtimeSessionId must not be blank" }
            require(messageSeq > 0L) { "messageSeq must be positive" }
            require(payloadVersion > 0) { "payloadVersion must be positive" }
            require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) { "observation payload exceeds hard limit" }
            require(pid > 0) { "pid must be positive" }
            require(processName.isNotBlank()) { "processName must not be blank" }
            require(packageName.isNotBlank()) { "packageName must not be blank" }
            require(buildFingerprint.isNotBlank()) { "buildFingerprint must not be blank" }
        }

        override fun equals(other: Any?): Boolean = other is ObservationEvent &&
            protocolVersion == other.protocolVersion &&
            runtimeSessionId == other.runtimeSessionId &&
            messageSeq == other.messageSeq &&
            observationType == other.observationType &&
            payloadVersion == other.payloadVersion &&
            payload.contentEquals(other.payload) &&
            observedAtEpochMs == other.observedAtEpochMs &&
            observedAtElapsedNs == other.observedAtElapsedNs &&
            pid == other.pid &&
            processName == other.processName &&
            packageName == other.packageName &&
            buildFingerprint == other.buildFingerprint &&
            playerLatitude == other.playerLatitude &&
            playerLongitude == other.playerLongitude &&
            lifecycleState == other.lifecycleState

        override fun hashCode(): Int {
            var result = protocolVersion
            result = 31 * result + runtimeSessionId.hashCode()
            result = 31 * result + messageSeq.hashCode()
            result = 31 * result + observationType.hashCode()
            result = 31 * result + payloadVersion
            result = 31 * result + payload.contentHashCode()
            result = 31 * result + observedAtEpochMs.hashCode()
            result = 31 * result + observedAtElapsedNs.hashCode()
            result = 31 * result + pid
            result = 31 * result + processName.hashCode()
            result = 31 * result + packageName.hashCode()
            result = 31 * result + buildFingerprint.hashCode()
            result = 31 * result + (playerLatitude?.hashCode() ?: 0)
            result = 31 * result + (playerLongitude?.hashCode() ?: 0)
            result = 31 * result + (lifecycleState?.hashCode() ?: 0)
            return result
        }
    }

    data class AutomationCommand(
        override val protocolVersion: Int = BridgeProtocol.VERSION,
        override val runtimeSessionId: String,
        val commandId: String,
        val action: AutomationAction,
        override val basedOnObservationSeq: Long,
        val expectedLifecycle: GameLifecycleState?,
        val expiresAtElapsedNs: Long,
        val pid: Int,
        val processName: String,
        val packageName: String,
        val buildFingerprint: String,
    ) : BridgeEvent {
        init {
            require(runtimeSessionId.isNotBlank()) { "runtimeSessionId must not be blank" }
            require(commandId.isNotBlank()) { "commandId must not be blank" }
            require(basedOnObservationSeq > 0L) { "basedOnObservationSeq must be positive" }
            require(expiresAtElapsedNs >= 0L) { "expiresAtElapsedNs must not be negative" }
            require(pid > 0) { "pid must be positive" }
            require(processName.isNotBlank()) { "processName must not be blank" }
            require(packageName.isNotBlank()) { "packageName must not be blank" }
            require(buildFingerprint.isNotBlank()) { "buildFingerprint must not be blank" }
        }
    }

    data class AutomationCommandResult(
        override val protocolVersion: Int = BridgeProtocol.VERSION,
        override val runtimeSessionId: String,
        override val messageSeq: Long,
        val commandId: String,
        val phase: CommandPhase,
        val errorCode: String? = null,
        val message: String? = null,
        /** Optional semantic result for a Catch command. */
        val catchOutcome: CatchOutcome? = null,
        val observedAtEpochMs: Long = System.currentTimeMillis(),
        val observedAtElapsedNs: Long = System.nanoTime(),
    ) : BridgeEvent {
        init {
            require(runtimeSessionId.isNotBlank()) { "runtimeSessionId must not be blank" }
            require(messageSeq > 0L) { "messageSeq must be positive" }
            require(commandId.isNotBlank()) { "commandId must not be blank" }
        }
    }

    data class BindingLost(
        override val protocolVersion: Int = BridgeProtocol.VERSION,
        override val runtimeSessionId: String,
        override val messageSeq: Long,
        val pid: Int,
        val processName: String,
        val reason: String,
        val canReconnect: Boolean = true,
    ) : BridgeEvent

    data class NearbyUpdated(
        override val protocolVersion: Int = BridgeProtocol.VERSION,
        val snapshot: dev.pogoroot.automation.core.model.NearbySnapshot,
        override val runtimeSessionId: String? = null,
        override val messageSeq: Long? = null,
        override val basedOnObservationSeq: Long? = null,
    ) : BridgeEvent

    data class RuntimeError(
        override val protocolVersion: Int = BridgeProtocol.VERSION,
        val code: String,
        val message: String,
        override val runtimeSessionId: String? = null,
        override val messageSeq: Long? = null,
        val pid: Int? = null,
        val processName: String? = null,
    ) : BridgeEvent
}

data class BridgeFrame(
    val protocolVersion: Int,
    val messageType: BridgeMessageType,
    val messageSeq: Long,
    val payload: ByteArray,
) {
    init {
        require(protocolVersion in 0..0xffff) { "protocolVersion must fit uint16" }
        require(messageSeq >= 0L) { "messageSeq must not be negative" }
        require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) { "payload exceeds hard limit" }
    }
}

/** Binary framing for the persistent local IPC channel. Payload schemas are separate. */
object BridgeFrameCodec {
    fun encode(frame: BridgeFrame): Result<ByteArray> = runCatching {
        require(frame.payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) { "payload exceeds hard limit" }
        ByteBuffer.allocate(BridgeProtocol.FRAME_HEADER_BYTES + frame.payload.size)
            .order(ByteOrder.BIG_ENDIAN)
            .putInt(frame.payload.size)
            .putShort(frame.protocolVersion.toShort())
            .putShort(frame.messageType.wireValue.toShort())
            .putLong(frame.messageSeq)
            .put(frame.payload)
            .array()
    }

    fun decode(
        bytes: ByteArray,
        expectedProtocolVersion: Int = BridgeProtocol.VERSION,
    ): Result<BridgeFrame> = runCatching {
        require(bytes.size >= BridgeProtocol.FRAME_HEADER_BYTES) { "truncated bridge frame" }
        val buffer = ByteBuffer.wrap(bytes).order(ByteOrder.BIG_ENDIAN)
        val payloadLength = buffer.int
        require(payloadLength >= 0) { "negative bridge payload length" }
        require(payloadLength <= BridgeProtocol.HARD_MESSAGE_BYTES) { "payload exceeds hard limit" }
        require(payloadLength == bytes.size - BridgeProtocol.FRAME_HEADER_BYTES) {
            "bridge frame length does not match payload"
        }
        val protocolVersion = buffer.short.toInt() and 0xffff
        require(protocolVersion == expectedProtocolVersion) {
            "bridge protocol mismatch: expected $expectedProtocolVersion, got $protocolVersion"
        }
        val messageType = BridgeMessageType.fromWireValue(buffer.short.toInt() and 0xffff)
        val messageSeq = buffer.long
        val payload = ByteArray(payloadLength)
        buffer.get(payload)
        BridgeFrame(protocolVersion, messageType, messageSeq, payload)
    }

    fun read(
        input: InputStream,
        expectedProtocolVersion: Int = BridgeProtocol.VERSION,
    ): Result<BridgeFrame> = runCatching {
        val data = DataInputStream(input)
        val payloadLength = data.readInt()
        require(payloadLength in 0..BridgeProtocol.HARD_MESSAGE_BYTES) {
            "payload exceeds hard limit"
        }
        val headerAndPayload = ByteArray(BridgeProtocol.FRAME_HEADER_BYTES - 4 + payloadLength)
        data.readFully(headerAndPayload)
        val bytes = ByteBuffer.allocate(BridgeProtocol.FRAME_HEADER_BYTES + payloadLength)
            .putInt(payloadLength)
            .put(headerAndPayload)
            .array()
        decode(bytes, expectedProtocolVersion).getOrThrow()
    }

    fun write(frame: BridgeFrame, output: OutputStream): Result<Unit> = runCatching {
        val bytes = encode(frame).getOrThrow()
        DataOutputStream(output).apply {
            write(bytes)
            flush()
        }
    }
}

/** Persistent bidirectional bridge abstraction used by the controller and adapters. */
interface RuntimeBridge {
    val connected: Boolean

    fun connect(): Result<BridgeEvent.RuntimeReady>

    fun receiveEvents(): Result<List<BridgeEvent>>

    fun send(command: BridgeEvent.AutomationCommand): Result<Unit>

    fun disconnect()
}

fun BridgeEvent.messageType(): BridgeMessageType = when (this) {
    is BridgeEvent.RuntimeStatus -> BridgeMessageType.RUNTIME_STATUS
    is BridgeEvent.RuntimeReady -> BridgeMessageType.RUNTIME_READY
    is BridgeEvent.ObservationEvent -> BridgeMessageType.OBSERVATION
    is BridgeEvent.AutomationCommand -> BridgeMessageType.COMMAND
    is BridgeEvent.AutomationCommandResult -> BridgeMessageType.COMMAND_RESULT
    is BridgeEvent.BindingLost -> BridgeMessageType.BINDING_LOST
    is BridgeEvent.RuntimeError -> BridgeMessageType.ERROR
    is BridgeEvent.NearbyUpdated -> BridgeMessageType.NEARBY_UPDATED
}

fun BridgeEvent.RuntimeReady.toRuntimeIdentity(mutationsAllowed: Boolean): RuntimeIdentity = RuntimeIdentity(
    runtimeSessionId = runtimeSessionId,
    pid = pid,
    processName = processName,
    packageName = packageName,
    buildFingerprint = buildFingerprint,
    capabilities = capabilities,
    mutationsAllowed = mutationsAllowed,
)
