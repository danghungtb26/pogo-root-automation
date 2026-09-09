package dev.pogoroot.automation.pogo

import java.io.ByteArrayOutputStream
import java.io.DataOutputStream
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RuntimeThrowDiagnosticPayloadCodecTest {
    @Test
    fun `decodes AttemptCapture diagnostic`() {
        val payload = ByteArrayOutputStream().also { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(0x504F4754)
                output.writeInt(RuntimeThrowDiagnosticStage.CAPTURE_PROMISE_CREATED.wireValue)
                output.writeLong(123456789L)
                output.writeInt(4)
                output.writeInt((1 shl 0) or (1 shl 1) or (1 shl 7) or (1 shl 8))
            }
        }.toByteArray()

        val diagnostic = RuntimeThrowDiagnosticPayloadCodec.decode(payload).getOrThrow()

        assertEquals("123456789", diagnostic.encounterId)
        assertEquals(RuntimeThrowDiagnosticStage.CAPTURE_PROMISE_CREATED, diagnostic.stage)
        assertEquals(4, diagnostic.ballType)
        assertTrue(diagnostic.interactionInPlay)
        assertTrue(diagnostic.ballInPlay)
        assertTrue(diagnostic.hitCollision)
        assertTrue(diagnostic.capturePromise)
        assertTrue(diagnostic.message().contains("AttemptCapture"))
    }

    @Test
    fun `rejects invalid payload`() {
        val payload = ByteArrayOutputStream().also { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(0x504F4754)
                output.writeInt(RuntimeThrowDiagnosticStage.BALL_STOPPED.wireValue)
                output.writeLong(0L)
                output.writeInt(-1)
                output.writeInt(0)
            }
        }.toByteArray()

        val result = RuntimeThrowDiagnosticPayloadCodec.decode(payload)

        assertFalse(result.isSuccess)
    }
}
