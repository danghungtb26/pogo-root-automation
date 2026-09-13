package dev.pogoroot.automation.runtime

import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.RuntimeCatchSpinConfig
import dev.pogoroot.automation.bridge.RuntimeDiscardConfig
import dev.pogoroot.automation.bridge.RuntimeTransferConfig
import dev.pogoroot.automation.config.HeadlessAutomationConfig
import dev.pogoroot.automation.root.RuntimeControlBridge
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class RuntimeLifecycleCoordinatorTest {
    private val config = HeadlessAutomationConfig(autoTransfer = true, autoDiscard = true)

    @Test
    fun readinessArrivesFromNativeWithoutKotlinSchedulingDiagnostic() {
        val bridge = FakeControlBridge()
        val coordinator = RuntimeLifecycleCoordinator(bridge)
        repeat(4) { coordinator.ensureRunning(config).getOrThrow() }
        assertEquals(0, bridge.diagnostics)
        assertTrue(bridge.applied.isEmpty())
        bridge.ready = bridge.ready.copy(messageSeq = 2, strongIdentityVerified = true,
            capabilities = setOf("READ_NEARBY"))
        assertTrue(coordinator.ensureRunning(config).getOrThrow().strongIdentityVerified)
        assertEquals(3, bridge.applied.size)
        assertEquals(0, bridge.diagnostics)
    }

    @Test
    fun waitsForVerifiedSessionThenAppliesSavedConfigOnce() {
        val bridge = FakeControlBridge()
        val coordinator = RuntimeLifecycleCoordinator(bridge)
        assertFalse(coordinator.ensureRunning(config).getOrThrow().strongIdentityVerified)
        assertTrue(bridge.applied.isEmpty())
        coordinator.runDiagnostic(config).getOrThrow()
        assertTrue(coordinator.configsAppliedToNative)
        assertTrue(bridge.catchConfig!!.armed)
        assertEquals(listOf("catch:session-1", "transfer:session-1", "discard:session-1"), bridge.applied)
        coordinator.ensureRunning(config).getOrThrow()
        assertEquals(3, bridge.applied.size)
    }

    @Test
    fun processDeathClearsStateAndNewProcessReceivesAllConfigs() {
        val bridge = FakeControlBridge()
        val coordinator = RuntimeLifecycleCoordinator(bridge)
        coordinator.ensureRunning(config).getOrThrow()
        coordinator.runDiagnostic(config).getOrThrow()
        bridge.available = false
        bridge.disconnect()
        assertTrue(coordinator.ensureRunning(config).isFailure)
        assertFalse(coordinator.configsAppliedToNative)
        assertEquals(RuntimeControlState.DETACHED, coordinator.snapshot().state)
        bridge.available = true
        bridge.ready = bridge.ready.copy(runtimeSessionId = "session-2", messageSeq = 1,
            strongIdentityVerified = false, capabilities = emptySet())
        coordinator.ensureRunning(config).getOrThrow()
        assertEquals(3, bridge.applied.size)
        coordinator.runDiagnostic(config).getOrThrow()
        assertEquals(listOf("catch:session-2", "transfer:session-2", "discard:session-2"), bridge.applied.takeLast(3))
        assertEquals(2, bridge.starts)
    }

    @Test
    fun stopAndResumeSameProcessRequireFreshReadinessAndPreserveDisarmedChoice() {
        val bridge = FakeControlBridge()
        val coordinator = RuntimeLifecycleCoordinator(bridge)
        val disarmed = config.copy(catchSpinArmed = false)
        coordinator.ensureRunning(disarmed).getOrThrow()
        coordinator.runDiagnostic(disarmed).getOrThrow()
        assertFalse(bridge.catchConfig!!.armed)
        coordinator.ensureIdle().getOrThrow()
        assertEquals(1, bridge.stops)
        assertFalse(coordinator.configsAppliedToNative)
        coordinator.ensureRunning(disarmed).getOrThrow()
        assertEquals(3, bridge.applied.size)
        coordinator.runDiagnostic(disarmed).getOrThrow()
        assertEquals(6, bridge.applied.size)
        assertFalse(bridge.catchConfig!!.armed)
    }

    @Test
    fun failedConfigIsReportedAndNotMarkedApplied() {
        val bridge = FakeControlBridge()
        val coordinator = RuntimeLifecycleCoordinator(bridge)
        coordinator.ensureRunning(config).getOrThrow()
        bridge.rejectDiscard = true
        assertTrue(coordinator.runDiagnostic(config).isFailure)
        assertFalse(coordinator.configsAppliedToNative)
        bridge.rejectDiscard = false
        coordinator.ensureRunning(config).getOrThrow()
        coordinator.runDiagnostic(config).getOrThrow()
        assertTrue(coordinator.configsAppliedToNative)
    }

    private class FakeControlBridge : RuntimeControlBridge {
        override var connected = false
        var available = true
        var rejectDiscard = false
        var starts = 0
        var stops = 0
        var diagnostics = 0
        var catchConfig: RuntimeCatchSpinConfig? = null
        val applied = mutableListOf<String>()
        var ready = BridgeEvent.RuntimeReady(
            runtimeSessionId = "session-1", messageSeq = 1, pid = 42,
            processName = "com.nianticlabs.pokemongo", packageName = "com.nianticlabs.pokemongo",
            buildFingerprint = "test-build", capabilities = emptySet(),
        )
        override fun connect() = runCatching {
            check(available) { "game process unavailable" }
            connected = true
            ready
        }
        override fun disconnect() { connected = false }
        override fun currentRuntimeReady() = ready.takeIf { connected }
        override fun startRuntime() = runCatching {
            starts++
            ready = ready.copy(strongIdentityVerified = false, capabilities = emptySet())
        }
        override fun stopRuntime() = runCatching { stops++; Unit }
        override fun requestRuntimeDiagnostic() = runCatching {
            diagnostics++
            ready = ready.copy(messageSeq = ready.messageSeq + 1,
                strongIdentityVerified = true, capabilities = setOf("READ_NEARBY"))
        }
        override fun setCatchSpinConfig(config: RuntimeCatchSpinConfig) = runCatching {
            catchConfig = config
            applied.add("catch:${ready.runtimeSessionId}")
            Unit
        }
        override fun setTransferConfig(config: RuntimeTransferConfig) = runCatching {
            applied.add("transfer:${ready.runtimeSessionId}")
            Unit
        }
        override fun setDiscardConfig(config: RuntimeDiscardConfig) = runCatching {
            check(!rejectDiscard) { "discard config rejected" }
            applied.add("discard:${ready.runtimeSessionId}")
            Unit
        }
    }
}
