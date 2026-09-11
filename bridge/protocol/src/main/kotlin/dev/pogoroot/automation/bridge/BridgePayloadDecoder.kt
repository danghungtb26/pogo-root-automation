package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.NearbySpawn
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.SpawnExpiryConfidence
import java.io.ByteArrayInputStream
import java.io.DataInputStream

internal object BridgePayloadDecoder {
    private val codec = BridgePayloadCodecSupport

    fun decode(type: BridgeMessageType, payload: ByteArray): Result<BridgeEvent> = runCatching {
        require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
            "payload exceeds hard limit"
        }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == codec.PAYLOAD_VERSION) {
                "unsupported bridge payload version"
            }
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

    private fun readRuntimeReady(input: DataInputStream): BridgeEvent.RuntimeReady =
        BridgeEvent.RuntimeReady(
            runtimeSessionId = codec.readString(input),
            messageSeq = input.readLong(),
            pid = input.readInt(),
            processName = codec.readString(input),
            packageName = codec.readString(input),
            buildFingerprint = codec.readString(input),
            capabilities = codec.readStringSet(input),
            gameVersionName = codec.readNullableString(input),
            gameVersionCode = codec.readNullableLong(input),
            observedAtEpochMs = input.readLong(),
            observedAtElapsedNs = input.readLong(),
            strongIdentityVerified = if (input.available() > 0) input.readBoolean() else false,
        )

    private fun readObservation(input: DataInputStream): BridgeEvent.ObservationEvent =
        BridgeEvent.ObservationEvent(
            runtimeSessionId = codec.readString(input),
            messageSeq = input.readLong(),
            observationType = codec.readEnum(input, ObservationType.entries) { it.wireValue },
            payloadVersion = input.readInt(),
            payload = codec.readBytes(input),
            observedAtEpochMs = input.readLong(),
            observedAtElapsedNs = input.readLong(),
            pid = input.readInt(),
            processName = codec.readString(input),
            packageName = codec.readString(input),
            buildFingerprint = codec.readString(input),
            playerLatitude = codec.readNullableDouble(input),
            playerLongitude = codec.readNullableDouble(input),
            lifecycleState = codec.readNullableLifecycle(input),
        )

    private fun readCommand(input: DataInputStream): BridgeEvent.AutomationCommand =
        BridgeEvent.AutomationCommand(
            runtimeSessionId = codec.readString(input),
            commandId = codec.readString(input),
            action = BridgeActionCodec.read(input),
            basedOnObservationSeq = input.readLong(),
            expectedLifecycle = codec.readNullableLifecycle(input),
            expiresAtElapsedNs = input.readLong(),
            pid = input.readInt(),
            processName = codec.readString(input),
            packageName = codec.readString(input),
            buildFingerprint = codec.readString(input),
        )

    private fun readCommandResult(
        input: DataInputStream,
    ): BridgeEvent.AutomationCommandResult {
        val base = BridgeEvent.AutomationCommandResult(
            runtimeSessionId = codec.readString(input),
            messageSeq = input.readLong(),
            commandId = codec.readString(input),
            phase = codec.readEnum(input, CommandPhase.entries) { it.wireValue },
            errorCode = codec.readNullableString(input),
            message = codec.readNullableString(input),
            observedAtEpochMs = input.readLong(),
            observedAtElapsedNs = input.readLong(),
            catchOutcome = if (input.available() > 0) {
                codec.readNullableCatchOutcome(input)
            } else {
                null
            },
        )
        return if (input.available() == 0) {
            base
        } else {
            // Marker tells whether a throw/snapshot outcome is present; the
            // captured pokemon id is a trailing optional read whenever bytes
            // remain (absent in older frames that predate it).
            val hasThrowOrSnapshot = input.readBoolean()
            val throwOutcome = if (hasThrowOrSnapshot) codec.readNullableThrowOutcome(input) else null
            val snapshotResult = if (hasThrowOrSnapshot) codec.readNullableSnapshotResult(input) else null
            base.copy(
                throwOutcome = throwOutcome,
                snapshotResult = snapshotResult,
                capturedPokemonId = if (input.available() > 0) {
                    codec.readNullableString(input)
                } else {
                    null
                },
            )
        }
    }

    private fun readBindingLost(input: DataInputStream): BridgeEvent.BindingLost =
        BridgeEvent.BindingLost(
            runtimeSessionId = codec.readString(input),
            messageSeq = input.readLong(),
            pid = input.readInt(),
            processName = codec.readString(input),
            reason = codec.readString(input),
            canReconnect = input.readBoolean(),
        )

    private fun readRuntimeError(input: DataInputStream): BridgeEvent.RuntimeError =
        BridgeEvent.RuntimeError(
            runtimeSessionId = codec.readNullableString(input),
            messageSeq = codec.readNullableLong(input),
            code = codec.readString(input),
            message = codec.readString(input),
            pid = codec.readNullableInt(input),
            processName = codec.readNullableString(input),
        )

    private fun readRuntimeStatus(input: DataInputStream): BridgeEvent.RuntimeStatus =
        BridgeEvent.RuntimeStatus(
            processName = codec.readString(input),
            gameVersion = codec.readNullableString(input),
            lifecycleState = codec.readEnum(
                input,
                GameLifecycleState.entries,
                codec::lifecycleWireValue,
            ),
            runtimeSessionId = codec.readNullableString(input),
            messageSeq = codec.readNullableLong(input),
        )

    private fun readNearbyUpdated(input: DataInputStream): BridgeEvent.NearbyUpdated {
        val runtimeSessionId = codec.readNullableString(input)
        val messageSeq = codec.readNullableLong(input)
        val basedOnObservationSeq = codec.readNullableLong(input)
        val snapshot = readNearbySnapshot(input)
        return BridgeEvent.NearbyUpdated(
            snapshot = snapshot,
            runtimeSessionId = runtimeSessionId,
            messageSeq = messageSeq,
            basedOnObservationSeq = basedOnObservationSeq,
        )
    }

    private fun readNearbySnapshot(input: DataInputStream): NearbySnapshot {
        val observedAt = input.readLong()
        val playerPosition = codec.readNullablePoint(input)
        val count = input.readInt().also {
            require(it in 0..10_000) { "invalid nearby spawn count" }
        }
        val spawns = List(count) {
            NearbySpawn(
                spawnId = codec.readString(input),
                speciesId = input.readInt(),
                speciesName = codec.readString(input),
                position = GeoPoint(
                    input.readDouble(),
                    input.readDouble(),
                ),
                firstSeenAtEpochMs = input.readLong(),
                expiresAtEpochMs = codec.readNullableLong(input),
                expiryConfidence = codec.readEnum(
                    input,
                    SpawnExpiryConfidence.entries,
                    codec::spawnExpiryWireValue,
                ),
            )
        }
        return NearbySnapshot(
            observedAtEpochMs = observedAt,
            playerPosition = playerPosition,
            spawns = spawns,
            isComplete = if (input.available() > 0) input.readBoolean() else true,
        )
    }
}
