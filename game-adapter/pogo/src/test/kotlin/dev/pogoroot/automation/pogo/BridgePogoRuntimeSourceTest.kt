package dev.pogoroot.automation.pogo

import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.BridgeProtocol
import dev.pogoroot.automation.bridge.CommandPhase
import dev.pogoroot.automation.bridge.ObservationType
import dev.pogoroot.automation.bridge.RuntimeBridge
import dev.pogoroot.automation.core.model.GameLifecycleState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class BridgePogoRuntimeSourceTest {
    @Test
    fun `orders result and observations by message sequence and reads selected state`() {
        val bridge = FakeRuntimeBridge(
            incoming = listOf(
                lifecycleObservation(11L, GameLifecycleState.ENCOUNTER),
                BridgeEvent.AutomationCommandResult(
                    runtimeSessionId = SESSION_ID,
                    messageSeq = 10L,
                    commandId = "command-1",
                    phase = CommandPhase.REJECTED,
                ),
                lifecycleObservation(9L, GameLifecycleState.OVERWORLD),
            ),
        )
        val source = BridgePogoRuntimeSource(bridge)

        source.connect().getOrThrow()
        source.refresh().getOrThrow()
        val events = source.drainEvents().getOrThrow()

        assertEquals(listOf(9L, 10L, 11L), events.mapNotNull { it.messageSeq })

        source.selectObservation(9L).getOrThrow()
        assertEquals(GameLifecycleState.OVERWORLD, source.lifecycleState())
        source.clearObservationSelection()
        source.selectObservation(11L).getOrThrow()
        assertEquals(GameLifecycleState.ENCOUNTER, source.lifecycleState())
    }

    @Test
    fun `accepts post-init runtime capability update for the same session`() {
        val bridge = FakeRuntimeBridge(
            incoming = listOf(
                BridgeEvent.RuntimeReady(
                    runtimeSessionId = SESSION_ID,
                    messageSeq = 2L,
                    pid = PID,
                    processName = PROCESS_NAME,
                    packageName = PACKAGE_NAME,
                    buildFingerprint = "verified-runtime-build",
                    strongIdentityVerified = true,
                    capabilities = setOf("USE_BERRY"),
                ),
            ),
        )
        val source = BridgePogoRuntimeSource(bridge)

        source.connect().getOrThrow()
        source.refresh().getOrThrow()

        val ready = source.runtimeMetadata?.ready
        assertEquals("verified-runtime-build", ready?.buildFingerprint)
        assertTrue(ready?.strongIdentityVerified == true)
        assertEquals(setOf("USE_BERRY"), ready?.capabilities)
        assertTrue(source.drainEvents().getOrThrow().isEmpty())
    }

    private fun lifecycleObservation(seq: Long, state: GameLifecycleState) =
        BridgeEvent.ObservationEvent(
            runtimeSessionId = SESSION_ID,
            messageSeq = seq,
            observationType = ObservationType.LIFECYCLE,
            payloadVersion = BridgeProtocol.OBSERVATION_PAYLOAD_VERSION,
            payload = byteArrayOf(),
            observedAtEpochMs = seq * 1_000L,
            observedAtElapsedNs = seq * 1_000_000L,
            pid = PID,
            processName = PROCESS_NAME,
            packageName = PACKAGE_NAME,
            buildFingerprint = BUILD_FINGERPRINT,
            lifecycleState = state,
        )

    private class FakeRuntimeBridge(
        private var incoming: List<BridgeEvent>,
    ) : RuntimeBridge {
        override var connected: Boolean = false
            private set

        override fun connect(): Result<BridgeEvent.RuntimeReady> {
            connected = true
            return Result.success(
                BridgeEvent.RuntimeReady(
                    runtimeSessionId = SESSION_ID,
                    messageSeq = 1L,
                    pid = PID,
                    processName = PROCESS_NAME,
                    packageName = PACKAGE_NAME,
                    buildFingerprint = BUILD_FINGERPRINT,
                    capabilities = emptySet(),
                ),
            )
        }

        override fun receiveEvents(): Result<List<BridgeEvent>> = Result.success(
            incoming.also { incoming = emptyList() },
        )

        override fun send(command: BridgeEvent.AutomationCommand): Result<Unit> = Result.success(Unit)

        override fun disconnect() {
            connected = false
        }
    }

    companion object {
        private const val SESSION_ID = "session-a"
        private const val PID = 1234
        private const val PACKAGE_NAME = "com.nianticlabs.pokemongo"
        private const val PROCESS_NAME = "com.nianticlabs.pokemongo"
        private const val BUILD_FINGERPRINT = "verified-build"
    }
}
