package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.NearbySpawn
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.SpawnExpiryConfidence
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class AutomationRunnerTest {
    @Test
    fun `submits one mutation and waits for terminal outcome before replanning`() {
        val submitted = mutableListOf<ActionRequest>()
        val identity = RuntimeIdentity(
            runtimeSessionId = "session-a",
            pid = 99,
            processName = "pogo",
            packageName = "com.nianticlabs.pokemongo",
            buildFingerprint = "verified",
            capabilities = setOf("OPEN_ENCOUNTER"),
            mutationsAllowed = true,
        )
        val runner = AutomationRunner(
            executor = ActionRequestExecutor { request -> submitted += request; Result.success(Unit) },
            nowEpochMs = { 1_000L },
            nowElapsedNs = { 1_000_000L },
            commandIdFactory = { "command-${submitted.size + 1}" },
        )
        runner.attach(identity).getOrThrow()

        val observation = AutomationObservation(
            identity = identity,
            messageSeq = 1L,
            observedAtEpochMs = 1_000L,
            observedAtElapsedNs = 1_000_000L,
            snapshot = AutomationSnapshot(
                lifecycleState = GameLifecycleState.OVERWORLD,
                nearby = NearbySnapshot(
                    1_000L,
                    null,
                    listOf(
                        NearbySpawn(
                            "spawn-1",
                            25,
                            "Pikachu",
                            GeoPoint(1.0, 2.0),
                            1_000L,
                            null,
                            SpawnExpiryConfidence.UNKNOWN,
                        ),
                    ),
                ),
            ),
        )
        val policy = AutomationPolicy(autoEncounter = true)
        val first = runner.onObservation(observation, policy).getOrThrow()
        assertNotNull(first.request)
        assertEquals(1, submitted.size)

        val whileActive = runner.onObservation(observation.copy(messageSeq = 2L), policy).getOrThrow()
        assertNull(whileActive.request)
        assertEquals(1, submitted.size)

        val request = submitted.single()
        runner.onResult(
            ActionExecution(request, ActionExecutionPhase.ACCEPTED, runtimeMessageSeq = 3L),
        ).getOrThrow()
        runner.onResult(
            ActionExecution(request, ActionExecutionPhase.COMPLETED, runtimeMessageSeq = 4L),
        ).getOrThrow()
        assertNull(runner.snapshot().activeExecution)

        val duplicate = runner.onObservation(observation.copy(messageSeq = 5L), policy).getOrThrow()
        assertNull(duplicate.request)
        assertTrue(duplicate.reason!!.contains("duplicate"))
        assertEquals(1, submitted.size)
    }

    @Test
    fun `disconnect turns active command indeterminate and suspends runner`() {
        val identity = RuntimeIdentity("session-a", 1, "pogo", "pkg", "verified", setOf("OPEN_ENCOUNTER"), true)
        val runner = AutomationRunner(
            executor = ActionRequestExecutor { Result.success(Unit) },
            nowEpochMs = { 1L },
            nowElapsedNs = { 1_000L },
            commandIdFactory = { "command-1" },
        )
        runner.attach(identity).getOrThrow()
        runner.onObservation(
            AutomationObservation(
                identity,
                1L,
                1L,
                1_000L,
                AutomationSnapshot(
                    GameLifecycleState.OVERWORLD,
                    nearby = NearbySnapshot(
                        1L,
                        null,
                        listOf(NearbySpawn("spawn-1", 25, "Pikachu", GeoPoint(1.0, 2.0), 1L, null, SpawnExpiryConfidence.UNKNOWN)),
                    ),
                ),
            ),
            AutomationPolicy(autoEncounter = true),
        ).getOrThrow()

        val lost = runner.disconnect("socket closed")
        assertEquals(ActionExecutionPhase.INDETERMINATE, lost!!.phase)
        assertTrue(runner.snapshot().suspended)
        assertTrue(runner.snapshot().needsResync)
    }
}
