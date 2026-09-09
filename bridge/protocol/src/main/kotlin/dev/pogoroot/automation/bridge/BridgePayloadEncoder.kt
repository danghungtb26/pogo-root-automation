package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.model.NearbySnapshot
import java.io.ByteArrayOutputStream
import java.io.DataOutputStream

internal object BridgePayloadEncoder {
    private val codec = BridgePayloadCodecSupport

    fun encode(event: BridgeEvent): Result<ByteArray> = runCatching {
        require(event.protocolVersion == BridgeProtocol.VERSION) { "bridge protocol mismatch" }
        ByteArrayOutputStream().use { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(codec.PAYLOAD_VERSION)
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
                require(it.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
                    "payload exceeds hard limit"
                }
            }
        }
    }

    private fun writeRuntimeStatus(
        output: DataOutputStream,
        value: BridgeEvent.RuntimeStatus,
    ) {
        codec.writeString(output, value.processName)
        codec.writeNullableString(output, value.gameVersion)
        output.writeInt(codec.lifecycleWireValue(value.lifecycleState))
        codec.writeNullableString(output, value.runtimeSessionId)
        codec.writeNullableLong(output, value.messageSeq)
    }

    private fun writeRuntimeReady(
        output: DataOutputStream,
        value: BridgeEvent.RuntimeReady,
    ) {
        codec.writeString(output, value.runtimeSessionId)
        output.writeLong(value.messageSeq)
        output.writeInt(value.pid)
        codec.writeString(output, value.processName)
        codec.writeString(output, value.packageName)
        codec.writeString(output, value.buildFingerprint)
        codec.writeStringSet(output, value.capabilities)
        codec.writeNullableString(output, value.gameVersionName)
        codec.writeNullableLong(output, value.gameVersionCode)
        output.writeLong(value.observedAtEpochMs)
        output.writeLong(value.observedAtElapsedNs)
        output.writeBoolean(value.strongIdentityVerified)
    }

    private fun writeObservation(
        output: DataOutputStream,
        value: BridgeEvent.ObservationEvent,
    ) {
        codec.writeString(output, value.runtimeSessionId)
        output.writeLong(value.messageSeq)
        output.writeInt(value.observationType.wireValue)
        output.writeInt(value.payloadVersion)
        codec.writeBytes(output, value.payload)
        output.writeLong(value.observedAtEpochMs)
        output.writeLong(value.observedAtElapsedNs)
        output.writeInt(value.pid)
        codec.writeString(output, value.processName)
        codec.writeString(output, value.packageName)
        codec.writeString(output, value.buildFingerprint)
        codec.writeNullableDouble(output, value.playerLatitude)
        codec.writeNullableDouble(output, value.playerLongitude)
        codec.writeNullableLifecycle(output, value.lifecycleState)
    }

    private fun writeCommand(
        output: DataOutputStream,
        value: BridgeEvent.AutomationCommand,
    ) {
        codec.writeString(output, value.runtimeSessionId)
        codec.writeString(output, value.commandId)
        BridgeActionCodec.write(output, value.action)
        output.writeLong(value.basedOnObservationSeq)
        codec.writeNullableLifecycle(output, value.expectedLifecycle)
        output.writeLong(value.expiresAtElapsedNs)
        output.writeInt(value.pid)
        codec.writeString(output, value.processName)
        codec.writeString(output, value.packageName)
        codec.writeString(output, value.buildFingerprint)
    }

    private fun writeCommandResult(
        output: DataOutputStream,
        value: BridgeEvent.AutomationCommandResult,
    ) {
        codec.writeString(output, value.runtimeSessionId)
        output.writeLong(value.messageSeq)
        codec.writeString(output, value.commandId)
        output.writeInt(value.phase.wireValue)
        codec.writeNullableString(output, value.errorCode)
        codec.writeNullableString(output, value.message)
        output.writeLong(value.observedAtEpochMs)
        output.writeLong(value.observedAtElapsedNs)
        codec.writeNullableCatchOutcome(output, value.catchOutcome)
        if (value.throwOutcome != null || value.snapshotResult != null) {
            output.writeBoolean(true)
            codec.writeNullableThrowOutcome(output, value.throwOutcome)
            codec.writeNullableSnapshotResult(output, value.snapshotResult)
        }
    }

    private fun writeBindingLost(
        output: DataOutputStream,
        value: BridgeEvent.BindingLost,
    ) {
        codec.writeString(output, value.runtimeSessionId)
        output.writeLong(value.messageSeq)
        output.writeInt(value.pid)
        codec.writeString(output, value.processName)
        codec.writeString(output, value.reason)
        output.writeBoolean(value.canReconnect)
    }

    private fun writeRuntimeError(
        output: DataOutputStream,
        value: BridgeEvent.RuntimeError,
    ) {
        codec.writeNullableString(output, value.runtimeSessionId)
        codec.writeNullableLong(output, value.messageSeq)
        codec.writeString(output, value.code)
        codec.writeString(output, value.message)
        codec.writeNullableInt(output, value.pid)
        codec.writeNullableString(output, value.processName)
    }

    private fun writeNearbyUpdated(
        output: DataOutputStream,
        value: BridgeEvent.NearbyUpdated,
    ) {
        codec.writeNullableString(output, value.runtimeSessionId)
        codec.writeNullableLong(output, value.messageSeq)
        codec.writeNullableLong(output, value.basedOnObservationSeq)
        writeNearbySnapshot(output, value.snapshot)
    }

    private fun writeNearbySnapshot(output: DataOutputStream, snapshot: NearbySnapshot) {
        output.writeLong(snapshot.observedAtEpochMs)
        codec.writeNullablePoint(output, snapshot.playerPosition)
        output.writeInt(snapshot.spawns.size)
        snapshot.spawns.forEach { spawn ->
            codec.writeString(output, spawn.spawnId)
            output.writeInt(spawn.speciesId)
            codec.writeString(output, spawn.speciesName)
            output.writeDouble(spawn.position.latitude)
            output.writeDouble(spawn.position.longitude)
            output.writeLong(spawn.firstSeenAtEpochMs)
            codec.writeNullableLong(output, spawn.expiresAtEpochMs)
            output.writeInt(codec.spawnExpiryWireValue(spawn.expiryConfidence))
        }
        output.writeBoolean(snapshot.isComplete)
    }
}
