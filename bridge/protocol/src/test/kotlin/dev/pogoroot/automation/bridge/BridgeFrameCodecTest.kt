package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.BerryType
import dev.pogoroot.automation.core.model.GameLifecycleState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

class BridgeFrameCodecTest {
    @Test
    fun `round trips runtime ready through binary frame and payload`() {
        val ready = BridgeEvent.RuntimeReady(
            runtimeSessionId = "session-a",
            messageSeq = 1L,
            pid = 1234,
            processName = "com.nianticlabs.pokemongo",
            packageName = "com.nianticlabs.pokemongo",
            buildFingerprint = "pogo|123|il2cpp|binding|arm64-v8a|aarch64|native|il2cpp=abc",
            strongIdentityVerified = true,
            capabilities = setOf("READ_LIFECYCLE", "SPIN"),
            gameVersionName = "1.0",
            gameVersionCode = 123L,
            observedAtEpochMs = 10L,
            observedAtElapsedNs = 20L,
        )
        val frame = BridgeFrame(
            protocolVersion = BridgeProtocol.VERSION,
            messageType = BridgeMessageType.RUNTIME_READY,
            messageSeq = ready.messageSeq,
            payload = BridgePayloadCodec.encode(ready).getOrThrow(),
        )

        val decodedFrame = BridgeFrameCodec.decode(BridgeFrameCodec.encode(frame).getOrThrow()).getOrThrow()
        val decoded = BridgePayloadCodec.decode(decodedFrame.messageType, decodedFrame.payload)
            .getOrThrow() as BridgeEvent.RuntimeReady

        assertEquals(ready, decoded)
        assertEquals(ready.messageSeq, decodedFrame.messageSeq)
    }

    @Test
    fun `rejects protocol mismatch and oversized payload`() {
        val frame = BridgeFrame(
            protocolVersion = BridgeProtocol.VERSION,
            messageType = BridgeMessageType.PING,
            messageSeq = 1L,
            payload = byteArrayOf(1, 2, 3),
        )
        assertTrue(BridgeFrameCodec.decode(
            BridgeFrameCodec.encode(frame).getOrThrow(),
            expectedProtocolVersion = BridgeProtocol.VERSION + 1,
        ).isFailure)

        val oversizedFrame = ByteBuffer.allocate(BridgeProtocol.FRAME_HEADER_BYTES)
            .order(ByteOrder.BIG_ENDIAN)
            .putInt(BridgeProtocol.HARD_MESSAGE_BYTES + 1)
            .putShort(BridgeProtocol.VERSION.toShort())
            .putShort(BridgeMessageType.PING.wireValue.toShort())
            .putLong(1L)
            .array()
        assertTrue(BridgeFrameCodec.decode(oversizedFrame).isFailure)
    }

    @Test
    fun `round trips command lifecycle payload including berry intent`() {
        val command = BridgeEvent.AutomationCommand(
            runtimeSessionId = "session-a",
            commandId = "command-1",
            action = AutomationAction.UseBerry("encounter-1", BerryType.GOLDEN_RAZZ),
            basedOnObservationSeq = 7L,
            expectedLifecycle = GameLifecycleState.ENCOUNTER,
            expiresAtElapsedNs = 99L,
            pid = 1234,
            processName = "com.nianticlabs.pokemongo",
            packageName = "com.nianticlabs.pokemongo",
            buildFingerprint = "verified-build",
        )
        val decoded = BridgePayloadCodec.decode(
            BridgeMessageType.COMMAND,
            BridgePayloadCodec.encode(command).getOrThrow(),
        ).getOrThrow()

        assertEquals(command, decoded)
    }

    @Test
    fun `wire enum values are explicit and stable`() {
        assertEquals(1, ObservationType.LIFECYCLE.wireValue)
        assertEquals(4, CommandPhase.REJECTED.wireValue)
        assertEquals(7, CommandPhase.INDETERMINATE.wireValue)
    }
}
