package dev.pogoroot.automation.pogo

import java.io.ByteArrayOutputStream
import java.io.DataOutputStream
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RuntimeFortsPayloadCodecTest {
    @Test
    fun `decodes structured pokestop state`() {
        val payload = ByteArrayOutputStream().also { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(0x504F4746)
                output.writeInt(1)
                output.writeInt(6)
                output.write("fort-1".toByteArray())
                output.writeInt(0)
                output.writeDouble(21.0)
                output.writeDouble(105.0)
                output.writeByte(1)
            }
        }.toByteArray()

        val result = RuntimeFortsPayloadCodec.decode(payload, 1234L).getOrThrow()

        assertEquals(1234L, result.observedAtEpochMs)
        assertEquals("fort-1", result.forts.single().fortId)
        assertTrue(result.forts.single().spinAvailable)
    }

    @Test
    fun `rejects invalid fort spin state`() {
        val payload = ByteArrayOutputStream().also { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(0x504F4746)
                output.writeInt(1)
                output.writeInt(1)
                output.writeByte('x'.code)
                output.writeInt(0)
                output.writeDouble(0.0)
                output.writeDouble(0.0)
                output.writeByte(2)
            }
        }.toByteArray()

        assertTrue(RuntimeFortsPayloadCodec.decode(payload, 1L).isFailure)
    }
}
