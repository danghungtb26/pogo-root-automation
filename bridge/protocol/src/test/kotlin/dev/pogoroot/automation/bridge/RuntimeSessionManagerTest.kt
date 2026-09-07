package dev.pogoroot.automation.bridge

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RuntimeSessionManagerTest {
    private val ready = BridgeEvent.RuntimeReady(
        runtimeSessionId = "session-a",
        messageSeq = 1L,
        pid = 55,
        processName = "com.nianticlabs.pokemongo",
        packageName = "com.nianticlabs.pokemongo",
        buildFingerprint = "verified-build",
        capabilities = setOf("CATCH"),
    )

    @Test
    fun `only matching session and increasing sequence are accepted`() {
        val manager = RuntimeSessionManager(
            expectedPackageName = ready.packageName,
            allowedBuildFingerprints = setOf(ready.buildFingerprint),
        )
        assertTrue(manager.accept(ready).isSuccess)
        val event = BridgeEvent.AutomationCommandResult(
            runtimeSessionId = ready.runtimeSessionId,
            messageSeq = 2L,
            commandId = "command-1",
            phase = CommandPhase.REJECTED,
        )
        assertTrue(manager.accepts(event))
        assertFalse(manager.accepts(event))
        assertTrue(manager.mutationsAllowed)
    }

    @Test
    fun `unallowlisted build remains read only`() {
        val manager = RuntimeSessionManager(expectedPackageName = ready.packageName)
        assertTrue(manager.accept(ready).isSuccess)
        assertFalse(manager.mutationsAllowed)
    }
}
