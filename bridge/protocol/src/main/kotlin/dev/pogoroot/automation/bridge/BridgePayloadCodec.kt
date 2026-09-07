package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.automation.AlertKind
import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.BerryType
import dev.pogoroot.automation.core.automation.CatchReason
import dev.pogoroot.automation.core.automation.MovementMode
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.NearbySpawn
import dev.pogoroot.automation.core.model.SpawnExpiryConfidence
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream
import java.nio.charset.StandardCharsets

/**
 * Versioned payload codec used inside [BridgeFrameCodec]. It intentionally
 * keeps the runtime observation payload opaque; only the controller-side
 * POGO adapter decodes game protobuf bytes.
 */
object BridgePayloadCodec {
    private const val PAYLOAD_VERSION = 1
    private const val MAX_STRING_BYTES = 64 * 1024

    fun encode(event: BridgeEvent): Result<ByteArray> = runCatching {
        require(event.protocolVersion == BridgeProtocol.VERSION) { "bridge protocol mismatch" }
        ByteArrayOutputStream().use { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(PAYLOAD_VERSION)
                when (event) {
                    is BridgeEvent.RuntimeStatus -> writeRuntimeStatus(output, event)
                    is BridgeEvent.RuntimeReady -> writeRuntimeReady(output, event)
                    is BridgeEvent.ObservationEvent -> writeObservation(output, event)
                    is BridgeEvent.AutomationCommand -> writeCommand(output, event)
                    is BridgeEvent.AutomationCommandResult -> writeCommandResult(output, event)
                    is BridgeEvent.BindingLost -> writeBindingLost(output, event)
                    is BridgeEvent.RuntimeError -> writeRuntimeError(output, event)
                    is BridgeEvent.NearbyUpdated -> writeNearbyUpdated(output, event)
                }
            }
            bytes.toByteArray().also {
                require(it.size <= BridgeProtocol.HARD_MESSAGE_BYTES) { "payload exceeds hard limit" }
            }
        }
    }

    fun decode(type: BridgeMessageType, payload: ByteArray): Result<BridgeEvent> = runCatching {
        require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) { "payload exceeds hard limit" }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == PAYLOAD_VERSION) { "unsupported bridge payload version" }
            val event = when (type) {
                BridgeMessageType.RUNTIME_READY -> readRuntimeReady(input)
                BridgeMessageType.RUNTIME_STATUS -> readRuntimeStatus(input)
                BridgeMessageType.OBSERVATION -> readObservation(input)
                BridgeMessageType.NEARBY_UPDATED -> readNearbyUpdated(input)
                BridgeMessageType.COMMAND -> readCommand(input)
                BridgeMessageType.COMMAND_RESULT -> readCommandResult(input)
                BridgeMessageType.BINDING_LOST -> readBindingLost(input)
                BridgeMessageType.ERROR -> readRuntimeError(input)
                else -> error("message type $type has no typed payload")
            }
            require(input.available() == 0) { "trailing bytes in bridge payload" }
            event
        }
    }

    private fun writeRuntimeStatus(output: DataOutputStream, value: BridgeEvent.RuntimeStatus) {
        writeString(output, value.processName)
        writeNullableString(output, value.gameVersion)
        output.writeInt(value.lifecycleState.ordinal)
        writeNullableString(output, value.runtimeSessionId)
        writeNullableLong(output, value.messageSeq)
    }

    private fun writeRuntimeReady(output: DataOutputStream, value: BridgeEvent.RuntimeReady) {
        writeString(output, value.runtimeSessionId)
        output.writeLong(value.messageSeq)
        output.writeInt(value.pid)
        writeString(output, value.processName)
        writeString(output, value.packageName)
        writeString(output, value.buildFingerprint)
        writeStringSet(output, value.capabilities)
        writeNullableString(output, value.gameVersionName)
        writeNullableLong(output, value.gameVersionCode)
        output.writeLong(value.observedAtEpochMs)
        output.writeLong(value.observedAtElapsedNs)
    }

    private fun writeObservation(output: DataOutputStream, value: BridgeEvent.ObservationEvent) {
        writeString(output, value.runtimeSessionId)
        output.writeLong(value.messageSeq)
        output.writeInt(value.observationType.ordinal)
        output.writeInt(value.payloadVersion)
        writeBytes(output, value.payload)
        output.writeLong(value.observedAtEpochMs)
        output.writeLong(value.observedAtElapsedNs)
        output.writeInt(value.pid)
        writeString(output, value.processName)
        writeString(output, value.packageName)
        writeString(output, value.buildFingerprint)
        writeNullableDouble(output, value.playerLatitude)
        writeNullableDouble(output, value.playerLongitude)
        writeNullableEnum(output, value.lifecycleState)
    }

    private fun writeCommand(output: DataOutputStream, value: BridgeEvent.AutomationCommand) {
        writeString(output, value.runtimeSessionId)
        writeString(output, value.commandId)
        writeAction(output, value.action)
        output.writeLong(value.basedOnObservationSeq)
        writeNullableEnum(output, value.expectedLifecycle)
        output.writeLong(value.expiresAtElapsedNs)
        output.writeInt(value.pid)
        writeString(output, value.processName)
        writeString(output, value.packageName)
        writeString(output, value.buildFingerprint)
    }

    private fun writeCommandResult(output: DataOutputStream, value: BridgeEvent.AutomationCommandResult) {
        writeString(output, value.runtimeSessionId)
        output.writeLong(value.messageSeq)
        writeString(output, value.commandId)
        output.writeInt(value.phase.ordinal)
        writeNullableString(output, value.errorCode)
        writeNullableString(output, value.message)
        output.writeLong(value.observedAtEpochMs)
        output.writeLong(value.observedAtElapsedNs)
    }

    private fun writeBindingLost(output: DataOutputStream, value: BridgeEvent.BindingLost) {
        writeString(output, value.runtimeSessionId)
        output.writeLong(value.messageSeq)
        output.writeInt(value.pid)
        writeString(output, value.processName)
        writeString(output, value.reason)
        output.writeBoolean(value.canReconnect)
    }

    private fun writeRuntimeError(output: DataOutputStream, value: BridgeEvent.RuntimeError) {
        writeNullableString(output, value.runtimeSessionId)
        writeNullableLong(output, value.messageSeq)
        writeString(output, value.code)
        writeString(output, value.message)
        writeNullableInt(output, value.pid)
        writeNullableString(output, value.processName)
    }

    private fun writeNearbyUpdated(output: DataOutputStream, value: BridgeEvent.NearbyUpdated) {
        writeNullableString(output, value.runtimeSessionId)
        writeNullableLong(output, value.messageSeq)
        writeNullableLong(output, value.basedOnObservationSeq)
        val snapshot = value.snapshot
        output.writeLong(snapshot.observedAtEpochMs)
        writeNullablePoint(output, snapshot.playerPosition)
        output.writeInt(snapshot.spawns.size)
        snapshot.spawns.forEach { spawn ->
            writeString(output, spawn.spawnId)
            output.writeInt(spawn.speciesId)
            writeString(output, spawn.speciesName)
            output.writeDouble(spawn.position.latitude)
            output.writeDouble(spawn.position.longitude)
            output.writeLong(spawn.firstSeenAtEpochMs)
            writeNullableLong(output, spawn.expiresAtEpochMs)
            output.writeInt(spawn.expiryConfidence.ordinal)
        }
    }

    private fun readRuntimeReady(input: DataInputStream): BridgeEvent.RuntimeReady = BridgeEvent.RuntimeReady(
        runtimeSessionId = readString(input),
        messageSeq = input.readLong(),
        pid = input.readInt(),
        processName = readString(input),
        packageName = readString(input),
        buildFingerprint = readString(input),
        capabilities = readStringSet(input),
        gameVersionName = readNullableString(input),
        gameVersionCode = readNullableLong(input),
        observedAtEpochMs = input.readLong(),
        observedAtElapsedNs = input.readLong(),
    )

    private fun readObservation(input: DataInputStream): BridgeEvent.ObservationEvent = BridgeEvent.ObservationEvent(
        runtimeSessionId = readString(input),
        messageSeq = input.readLong(),
        observationType = readEnum(input, ObservationType.entries),
        payloadVersion = input.readInt(),
        payload = readBytes(input),
        observedAtEpochMs = input.readLong(),
        observedAtElapsedNs = input.readLong(),
        pid = input.readInt(),
        processName = readString(input),
        packageName = readString(input),
        buildFingerprint = readString(input),
        playerLatitude = readNullableDouble(input),
        playerLongitude = readNullableDouble(input),
        lifecycleState = readNullableEnum(input, GameLifecycleState.entries),
    )

    private fun readCommand(input: DataInputStream): BridgeEvent.AutomationCommand = BridgeEvent.AutomationCommand(
        runtimeSessionId = readString(input),
        commandId = readString(input),
        action = readAction(input),
        basedOnObservationSeq = input.readLong(),
        expectedLifecycle = readNullableEnum(input, GameLifecycleState.entries),
        expiresAtElapsedNs = input.readLong(),
        pid = input.readInt(),
        processName = readString(input),
        packageName = readString(input),
        buildFingerprint = readString(input),
    )

    private fun readCommandResult(input: DataInputStream): BridgeEvent.AutomationCommandResult = BridgeEvent.AutomationCommandResult(
        runtimeSessionId = readString(input),
        messageSeq = input.readLong(),
        commandId = readString(input),
        phase = readEnum(input, CommandPhase.entries),
        errorCode = readNullableString(input),
        message = readNullableString(input),
        observedAtEpochMs = input.readLong(),
        observedAtElapsedNs = input.readLong(),
    )

    private fun readBindingLost(input: DataInputStream): BridgeEvent.BindingLost = BridgeEvent.BindingLost(
        runtimeSessionId = readString(input),
        messageSeq = input.readLong(),
        pid = input.readInt(),
        processName = readString(input),
        reason = readString(input),
        canReconnect = input.readBoolean(),
    )

    private fun readRuntimeError(input: DataInputStream): BridgeEvent.RuntimeError = BridgeEvent.RuntimeError(
        runtimeSessionId = readNullableString(input),
        messageSeq = readNullableLong(input),
        code = readString(input),
        message = readString(input),
        pid = readNullableInt(input),
        processName = readNullableString(input),
    )

    private fun writeAction(output: DataOutputStream, action: AutomationAction) {
        when (action) {
            is AutomationAction.MoveTo -> {
                output.writeInt(1)
                output.writeDouble(action.target.latitude)
                output.writeDouble(action.target.longitude)
                output.writeInt(action.mode.ordinal)
            }
            is AutomationAction.OpenEncounter -> {
                output.writeInt(2)
                writeString(output, action.spawnId)
            }
            is AutomationAction.Catch -> {
                output.writeInt(3)
                writeString(output, action.encounterId)
                output.writeInt(action.reason.ordinal)
            }
            is AutomationAction.Spin -> {
                output.writeInt(4)
                writeString(output, action.fortId)
            }
            is AutomationAction.DiscardItem -> {
                output.writeInt(5)
                output.writeInt(action.itemId)
                output.writeInt(action.amount)
            }
            is AutomationAction.TransferPokemon -> {
                output.writeInt(6)
                writeString(output, action.pokemonId)
            }
            is AutomationAction.Alert -> {
                output.writeInt(7)
                output.writeInt(action.kind.ordinal)
                writeString(output, action.message)
            }
            is AutomationAction.UseBerry -> {
                output.writeInt(8)
                writeString(output, action.encounterId)
                output.writeInt(action.berryType.ordinal)
            }
        }
    }

    private fun readAction(input: DataInputStream): AutomationAction = when (input.readInt()) {
        1 -> AutomationAction.MoveTo(
            target = GeoPoint(input.readDouble(), input.readDouble()),
            mode = readEnum(input, MovementMode.entries),
        )
        2 -> AutomationAction.OpenEncounter(readString(input))
        3 -> AutomationAction.Catch(
            encounterId = readString(input),
            reason = readEnum(input, CatchReason.entries),
        )
        4 -> AutomationAction.Spin(readString(input))
        5 -> AutomationAction.DiscardItem(input.readInt(), input.readInt())
        6 -> AutomationAction.TransferPokemon(readString(input))
        7 -> AutomationAction.Alert(
            kind = readEnum(input, AlertKind.entries),
            message = readString(input),
        )
        8 -> AutomationAction.UseBerry(
            encounterId = readString(input),
            berryType = readEnum(input, BerryType.entries),
        )
        else -> error("unknown automation action tag")
    }

    private fun readRuntimeStatus(input: DataInputStream): BridgeEvent.RuntimeStatus = BridgeEvent.RuntimeStatus(
        processName = readString(input),
        gameVersion = readNullableString(input),
        lifecycleState = readEnum(input, GameLifecycleState.entries),
        runtimeSessionId = readNullableString(input),
        messageSeq = readNullableLong(input),
    )

    private fun readNearbyUpdated(input: DataInputStream): BridgeEvent.NearbyUpdated {
        val runtimeSessionId = readNullableString(input)
        val messageSeq = readNullableLong(input)
        val basedOnObservationSeq = readNullableLong(input)
        val observedAt = input.readLong()
        val playerPosition = readNullablePoint(input)
        val count = input.readInt().also { require(it in 0..10_000) { "invalid nearby spawn count" } }
        val spawns = List(count) {
            NearbySpawn(
                spawnId = readString(input),
                speciesId = input.readInt(),
                speciesName = readString(input),
                position = GeoPoint(input.readDouble(), input.readDouble()),
                firstSeenAtEpochMs = input.readLong(),
                expiresAtEpochMs = readNullableLong(input),
                expiryConfidence = readEnum(input, SpawnExpiryConfidence.entries),
            )
        }
        return BridgeEvent.NearbyUpdated(
            snapshot = NearbySnapshot(observedAt, playerPosition, spawns),
            runtimeSessionId = runtimeSessionId,
            messageSeq = messageSeq,
            basedOnObservationSeq = basedOnObservationSeq,
        )
    }

    private fun writeString(output: DataOutputStream, value: String) {
        val bytes = value.toByteArray(StandardCharsets.UTF_8)
        require(bytes.size <= MAX_STRING_BYTES) { "string exceeds bridge limit" }
        output.writeInt(bytes.size)
        output.write(bytes)
    }

    private fun writeNullableString(output: DataOutputStream, value: String?) {
        output.writeBoolean(value != null)
        if (value != null) writeString(output, value)
    }

    private fun readString(input: DataInputStream): String {
        val size = input.readInt()
        require(size in 0..MAX_STRING_BYTES) { "invalid bridge string length" }
        val bytes = ByteArray(size)
        input.readFully(bytes)
        return String(bytes, StandardCharsets.UTF_8)
    }

    private fun readNullableString(input: DataInputStream): String? =
        if (input.readBoolean()) readString(input) else null

    private fun writeBytes(output: DataOutputStream, value: ByteArray) {
        require(value.size <= BridgeProtocol.HARD_MESSAGE_BYTES) { "bytes exceed bridge limit" }
        output.writeInt(value.size)
        output.write(value)
    }

    private fun readBytes(input: DataInputStream): ByteArray {
        val size = input.readInt()
        require(size in 0..BridgeProtocol.HARD_MESSAGE_BYTES) { "invalid bridge byte length" }
        return ByteArray(size).also { input.readFully(it) }
    }

    private fun writeStringSet(output: DataOutputStream, values: Set<String>) {
        output.writeInt(values.size)
        values.sorted().forEach { writeString(output, it) }
    }

    private fun readStringSet(input: DataInputStream): Set<String> {
        val count = input.readInt()
        require(count in 0..256) { "invalid bridge string set size" }
        return List(count) { readString(input) }.toSet()
    }

    private fun writeNullableLong(output: DataOutputStream, value: Long?) {
        output.writeBoolean(value != null)
        if (value != null) output.writeLong(value)
    }

    private fun readNullableLong(input: DataInputStream): Long? =
        if (input.readBoolean()) input.readLong() else null

    private fun writeNullableInt(output: DataOutputStream, value: Int?) {
        output.writeBoolean(value != null)
        if (value != null) output.writeInt(value)
    }

    private fun readNullableInt(input: DataInputStream): Int? =
        if (input.readBoolean()) input.readInt() else null

    private fun writeNullableDouble(output: DataOutputStream, value: Double?) {
        output.writeBoolean(value != null)
        if (value != null) output.writeDouble(value)
    }

    private fun readNullableDouble(input: DataInputStream): Double? =
        if (input.readBoolean()) input.readDouble() else null

    private fun writeNullablePoint(output: DataOutputStream, value: GeoPoint?) {
        output.writeBoolean(value != null)
        if (value != null) {
            output.writeDouble(value.latitude)
            output.writeDouble(value.longitude)
        }
    }

    private fun readNullablePoint(input: DataInputStream): GeoPoint? =
        if (input.readBoolean()) GeoPoint(input.readDouble(), input.readDouble()) else null

    private fun <T : Enum<T>> writeNullableEnum(output: DataOutputStream, value: T?) {
        output.writeBoolean(value != null)
        if (value != null) output.writeInt(value.ordinal)
    }

    private inline fun <reified T : Enum<T>> readNullableEnum(
        input: DataInputStream,
        values: List<T> = enumValues<T>().toList(),
    ): T? = if (input.readBoolean()) readEnum(input, values) else null

    private fun <T : Enum<T>> readEnum(input: DataInputStream, values: List<T>): T {
        val ordinal = input.readInt()
        require(ordinal in values.indices) { "invalid enum ordinal: $ordinal" }
        return values[ordinal]
    }
}
