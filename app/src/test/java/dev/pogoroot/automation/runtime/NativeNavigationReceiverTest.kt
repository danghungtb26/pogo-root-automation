package dev.pogoroot.automation.runtime

import dev.pogoroot.automation.bridge.RuntimeNavigationKind
import dev.pogoroot.automation.bridge.RuntimeNavigationPayload
import dev.pogoroot.automation.bridge.RuntimeNavigationPayloadCodec
import dev.pogoroot.automation.core.automation.AutoFortNavigationCommand
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.events.AutomationEvent
import dev.pogoroot.automation.events.AutomationEventSink
import dev.pogoroot.automation.location.NativeNavigationReceiver
import java.io.ByteArrayOutputStream
import java.io.DataOutputStream
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

class NativeNavigationReceiverTest {
    private val now = 10_000_000_000L
    private val walk = RuntimeNavigationPayload(RuntimeNavigationKind.WALK, "first", GeoPoint(10.0, 106.001), "")

    @Test
    fun forwardsNativeDestinationAndRenewsWithoutRepeatingToast() {
        val events = mutableListOf<AutomationEvent>()
        val commands = mutableListOf<Pair<AutoFortNavigationCommand, Long>>()
        val receiver = NativeNavigationReceiver(AutomationEventSink(events::add), { command, expiry ->
            commands.add(command to expiry)
        }, { now })
        receiver.receive(walk, now - 1_000_000_000L)
        receiver.receive(walk, now)
        assertEquals(2, commands.size)
        assertEquals(AutoFortNavigationCommand.WalkTo("first", walk.target), commands.last().first)
        assertTrue(commands.last().second > commands.first().second)
        assertEquals(1, events.size)
    }

    @Test
    fun staleAndFutureNativeInstructionsStopTheLocationService() {
        val commands = mutableListOf<AutoFortNavigationCommand>()
        val receiver = NativeNavigationReceiver(AutomationEventSink { }, { command, _ -> commands.add(command) }, { now })
        receiver.receive(walk, 1L)
        receiver.receive(walk, now + 1L)
        assertTrue(commands.all { it is AutoFortNavigationCommand.Stop })
        assertEquals(2, commands.size)
    }

    @Test
    fun disconnectStopsAndArrivalOnlyProducesUiFeedback() {
        val events = mutableListOf<AutomationEvent>()
        val commands = mutableListOf<AutoFortNavigationCommand>()
        val receiver = NativeNavigationReceiver(AutomationEventSink(events::add), { command, _ -> commands.add(command) }, { now })
        receiver.receive(walk, now)
        receiver.receive(walk.copy(kind = RuntimeNavigationKind.ARRIVED), now)
        assertEquals(1, commands.size) // Kotlin does not select the next fort.
        assertTrue(events.last().message.startsWith("Arrived"))
        receiver.reset()
        assertIs<AutoFortNavigationCommand.Stop>(commands.last())
    }

    @Test
    fun decodesNativeBigEndianNavigationPayload() {
        val decoded = RuntimeNavigationPayloadCodec.decode(payload(1)).getOrThrow()
        assertEquals(walk, decoded)
        assertEquals(RuntimeNavigationKind.STOP, RuntimeNavigationPayloadCodec.decode(payload(0)).getOrThrow().kind)
        assertEquals(RuntimeNavigationKind.ARRIVED, RuntimeNavigationPayloadCodec.decode(payload(2)).getOrThrow().kind)
    }

    @Test
    fun invalidInstructionsCannotReachLocationExecution() {
        assertTrue(RuntimeNavigationPayloadCodec.decode(payload(3)).isFailure)
        assertTrue(RuntimeNavigationPayloadCodec.decode(payload(1, Double.NaN)).isFailure)
        assertTrue(RuntimeNavigationPayloadCodec.decode(payload(1) + byteArrayOf(0)).isFailure)
        assertTrue(RuntimeNavigationPayloadCodec.decode(payload(1).dropLast(2).toByteArray()).isFailure)
    }

    private fun payload(kind: Int, latitude: Double = 10.0): ByteArray {
        val bytes = ByteArrayOutputStream()
        DataOutputStream(bytes).use {
            it.writeInt(0x504f4757)
            it.writeInt(kind)
            it.writeInt(5)
            it.writeBytes("first")
            it.writeDouble(latitude)
            it.writeDouble(106.001)
            it.writeInt(0)
        }
        return bytes.toByteArray()
    }
}
