package dev.pogoroot.automation.pogo

import dev.pogoroot.automation.adapter.GameCapability
import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.BridgeMessageType
import dev.pogoroot.automation.bridge.BridgePayloadCodec
import dev.pogoroot.automation.bridge.RuntimeBridge
import dev.pogoroot.automation.core.automation.ActionRequest
import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.CatchReason
import dev.pogoroot.automation.core.model.GameLifecycleState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class BridgeBackedPogoActionExecutorTest {
    @Test
    fun `forwards full command only when identity and capability gates pass`() {
        val bridge = CapturingBridge(ready(strongIdentityVerified = true))
        val executor = BridgeBackedPogoActionExecutor(
            bridge = bridge,
            runtimeReady = { bridge.ready },
            allowedBuildFingerprints = setOf(BUILD_FINGERPRINT),
        )
        val request = request()

        assertTrue(executor.submit(request).isSuccess)
        val command = bridge.sent.single()
        val decoded = BridgePayloadCodec.decode(
            BridgeMessageType.COMMAND,
            BridgePayloadCodec.encode(command).getOrThrow(),
        ).getOrThrow()

        assertEquals(command, decoded)
        assertEquals(9L, command.basedOnObservationSeq)
    }

    @Test
    fun `allowlisted weak identity cannot submit mutation`() {
        val bridge = CapturingBridge(ready(strongIdentityVerified = false))
        val executor = BridgeBackedPogoActionExecutor(
            bridge = bridge,
            runtimeReady = { bridge.ready },
            allowedBuildFingerprints = setOf(BUILD_FINGERPRINT),
        )

        assertTrue(executor.submit(request()).isFailure)
        assertTrue(bridge.sent.isEmpty())
    }

    private fun request() = ActionRequest.create(
        runtimeSessionId = SESSION_ID,
        action = AutomationAction.Catch("encounter-1", CatchReason.CATCH_ALL),
        basedOnObservationSeq = 9L,
        expectedLifecycle = GameLifecycleState.ENCOUNTER,
        createdAtEpochMs = 1_000L,
        createdAtElapsedNs = 2_000L,
        timeoutNs = 15_000L,
        pid = PID,
        processName = PACKAGE_NAME,
        packageName = PACKAGE_NAME,
        buildFingerprint = BUILD_FINGERPRINT,
        commandId = "command-1",
    )

    private fun ready(strongIdentityVerified: Boolean) = BridgeEvent.RuntimeReady(
        runtimeSessionId = SESSION_ID,
        messageSeq = 8L,
        pid = PID,
        processName = PACKAGE_NAME,
        packageName = PACKAGE_NAME,
        buildFingerprint = BUILD_FINGERPRINT,
        strongIdentityVerified = strongIdentityVerified,
        capabilities = setOf(GameCapability.CATCH.name),
    )

    private class CapturingBridge(
        val ready: BridgeEvent.RuntimeReady,
    ) : RuntimeBridge {
        override val connected: Boolean = true
        val sent = mutableListOf<BridgeEvent.AutomationCommand>()

        override fun connect(): Result<BridgeEvent.RuntimeReady> = Result.success(ready)

        override fun receiveEvents(): Result<List<BridgeEvent>> = Result.success(emptyList())

        override fun send(command: BridgeEvent.AutomationCommand): Result<Unit> {
            sent += command
            return Result.success(Unit)
        }

        override fun disconnect() = Unit
    }

    companion object {
        private const val SESSION_ID = "session-a"
        private const val PID = 1234
        private const val PACKAGE_NAME = "com.nianticlabs.pokemongo"
        private const val BUILD_FINGERPRINT = "verified-build"
    }
}
