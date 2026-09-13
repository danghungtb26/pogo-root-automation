package dev.pogoroot.automation.runtime

import dev.pogoroot.automation.bridge.RuntimeAutomationEventPayloadCodec
import dev.pogoroot.automation.bridge.RuntimeAutomationEventType
import dev.pogoroot.automation.events.AutomationEventType
import dev.pogoroot.automation.runtime.observation.toAutomationEvent
import java.nio.ByteBuffer
import java.io.ByteArrayOutputStream
import java.io.DataOutputStream
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class RuntimeAutomationEventMapperTest {
    @Test
    fun nativeStartAndTerminalEventsDecodeToToastTypes() {
        val expected = mapOf(
            1 to AutomationEventType.INFO, 2 to AutomationEventType.CAUGHT,
            3 to AutomationEventType.RAN_AWAY, 4 to AutomationEventType.TRANSFERRED,
            5 to AutomationEventType.INFO, 6 to AutomationEventType.ERROR,
            7 to AutomationEventType.INFO, 8 to AutomationEventType.DISCARDED,
            9 to AutomationEventType.ERROR, 10 to AutomationEventType.INFO,
            11 to AutomationEventType.SPUN, 12 to AutomationEventType.ERROR,
            13 to AutomationEventType.ERROR, 14 to AutomationEventType.ERROR,
        )
        for ((wire, toastType) in expected) {
            val decoded = RuntimeAutomationEventPayloadCodec.decode(payload(wire)).getOrThrow()
            assertEquals(toastType, assertNotNull(decoded.toAutomationEvent()).type)
        }
    }

    @Test
    fun unknownAndMalformedEventsNeverProduceSuccessToasts() {
        val unknown = RuntimeAutomationEventPayloadCodec.decode(payload(99)).getOrThrow()
        assertEquals(RuntimeAutomationEventType.UNKNOWN, unknown.type)
        assertNull(unknown.toAutomationEvent())
        assertTrue(RuntimeAutomationEventPayloadCodec.decode(payload(11).dropLast(1).toByteArray()).isFailure)
    }

    @Test
    fun namesSurviveStartSuccessAndFailureWithoutDisplayingStorageIds() {
        for (wire in listOf(1, 2, 3, 4, 5, 6, 13)) {
            val event = RuntimeAutomationEventPayloadCodec.decode(payload(wire, "Pikachu")).getOrThrow()
                .toAutomationEvent()!!
            assertTrue(event.message.contains("Pikachu"))
            assertTrue(!event.message.contains("#") && !event.message.contains("18446744073709551615"))
        }
        for (wire in listOf(7, 8, 9)) {
            val event = RuntimeAutomationEventPayloadCodec.decode(payload(wire, "Great Ball")).getOrThrow()
                .toAutomationEvent()!!
            assertTrue(event.message.contains("Great Ball ×1"))
        }
        val failure = RuntimeAutomationEventPayloadCodec.decode(
            payload(9, "Poké Ball", "Inventory unavailable"),
        ).getOrThrow().toAutomationEvent()!!
        assertEquals("Discard failed: Poké Ball ×1 — Inventory unavailable", failure.message)
    }

    @Test
    fun legacyEventsUseGenericLabelsAndBadNamesAreRejected() {
        val legacy = ByteBuffer.allocate(24).putInt(0x504F4745).putInt(2).putLong(-1).putLong(-2).array()
        assertEquals("Catch success: Pokémon",
            RuntimeAutomationEventPayloadCodec.decode(legacy, 1).getOrThrow().toAutomationEvent()!!.message)
        assertTrue(RuntimeAutomationEventPayloadCodec.decode(legacy, 2).isFailure)
        assertTrue(RuntimeAutomationEventPayloadCodec.decode(payload(2), 3).isFailure)
        assertTrue(RuntimeAutomationEventPayloadCodec.decode(payload(2, "x".repeat(257))).isFailure)
        assertTrue(RuntimeAutomationEventPayloadCodec.decode(payload(9, "Ball", "x".repeat(513))).isFailure)
        assertTrue(RuntimeAutomationEventPayloadCodec.decode(payload(2) + byteArrayOf(0)).isFailure)
    }

    private fun payload(type: Int, name: String = "Pikachu", detail: String = ""): ByteArray =
        ByteArrayOutputStream().also { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(0x504F4745)
                output.writeInt(type)
                output.writeLong(-1L)
                output.writeLong(1L)
                for (text in listOf(name, detail)) {
                    val utf8 = text.toByteArray(Charsets.UTF_8)
                    output.writeInt(utf8.size)
                    output.write(utf8)
                }
            }
        }.toByteArray()
}
