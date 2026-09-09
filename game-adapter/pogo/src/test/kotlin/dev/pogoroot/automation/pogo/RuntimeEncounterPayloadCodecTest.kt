package dev.pogoroot.automation.pogo

import java.nio.ByteBuffer
import java.nio.ByteOrder
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RuntimeEncounterPayloadCodecTest {
    @Test
    fun `decodes structured runtime encounter`() {
        val payload = ByteBuffer.allocate(4 + 8 + (4 * 4) + 2 + 16)
            .order(ByteOrder.BIG_ENDIAN)
            .putInt(0x504F4745)
            .putLong(12682638754462502383UL.toLong())
            .putInt(427)
            .putInt(10)
            .putInt(11)
            .putInt(12)
            .put(1.toByte())
            .put(0.toByte())
            .putDouble(10.123)
            .putDouble(106.456)
            .array()

        val result = RuntimeEncounterPayloadCodec.decode(payload, 1234L).getOrThrow()
        assertEquals("12682638754462502383", result.encounterId)
        assertEquals(427, result.speciesId)
        assertEquals(10, result.individualAttack)
        assertEquals(false, result.shiny)
        assertEquals(10.123, result.latitude!!, 0.0)
    }

    @Test
    fun `rejects trailing structured runtime encounter bytes`() {
        val payload = ByteBuffer.allocate(4 + 8 + (4 * 4) + 2 + 16 + 1)
            .order(ByteOrder.BIG_ENDIAN)
            .putInt(0x504F4745)
            .putLong(1L)
            .putInt(1)
            .putInt(0)
            .putInt(0)
            .putInt(0)
            .put(0.toByte())
            .putDouble(0.0)
            .putDouble(0.0)
            .put(1.toByte())
            .array()

        assertTrue(RuntimeEncounterPayloadCodec.decode(payload, 1L).isFailure)
    }
}
