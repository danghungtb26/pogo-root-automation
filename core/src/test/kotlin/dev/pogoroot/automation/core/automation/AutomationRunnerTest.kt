package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.Fort
import dev.pogoroot.automation.core.model.FortSnapshot
import dev.pogoroot.automation.core.model.FortType
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
    fun `definitive rejection is not retried when only observation time changes`() {
        val submitted = mutableListOf<ActionRequest>()
        val identity = RuntimeIdentity(
            runtimeSessionId = "session-rejected",
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
            commandIdFactory = { "rejected-command-${submitted.size + 1}" },
        )
        runner.attach(identity).getOrThrow()

        fun observation(seq: Long, observedAtEpochMs: Long) = AutomationObservation(
            identity = identity,
            messageSeq = seq,
            observedAtEpochMs = observedAtEpochMs,
            observedAtElapsedNs = 1_000_000L,
            snapshot = AutomationSnapshot(
                lifecycleState = GameLifecycleState.OVERWORLD,
                nearby = NearbySnapshot(
                    observedAtEpochMs,
                    null,
                    listOf(
                        NearbySpawn(
                            "spawn-1",
                            25,
                            "Pikachu",
                            GeoPoint(1.0, 2.0),
                            observedAtEpochMs,
                            null,
                            SpawnExpiryConfidence.UNKNOWN,
                        ),
                    ),
                ),
            ),
        )

        val first = runner.onObservation(observation(1L, 1_000L), AutomationPolicy(autoEncounter = true))
            .getOrThrow()
        val request = first.request!!
        runner.onResult(
            ActionExecution(
                request,
                ActionExecutionPhase.REJECTED,
                message = "target unavailable",
                runtimeMessageSeq = 2L,
            ),
        ).getOrThrow()

        val duplicate = runner.onObservation(
            observation(3L, 1_001L),
            AutomationPolicy(autoEncounter = true),
        ).getOrThrow()
        assertNull(duplicate.request)
        assertTrue(duplicate.reason!!.contains("duplicate"))
        assertEquals(1, submitted.size)
    }

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
    fun `spin settle delay blocks the next mutation until it expires`() {
        var nowElapsedNs = 1_000_000L
        val submitted = mutableListOf<ActionRequest>()
        val identity = RuntimeIdentity(
            runtimeSessionId = "session-spin",
            pid = 99,
            processName = "pogo",
            packageName = "com.nianticlabs.pokemongo",
            buildFingerprint = "verified",
            capabilities = setOf("SPIN"),
            mutationsAllowed = true,
        )
        val runner = AutomationRunner(
            executor = ActionRequestExecutor { request -> submitted += request; Result.success(Unit) },
            nowEpochMs = { 1_000L },
            nowElapsedNs = { nowElapsedNs },
            commandIdFactory = { "spin-command-${submitted.size + 1}" },
        )
        runner.attach(identity).getOrThrow()
        val policy = AutomationPolicy(
            autoSpin = true,
            timing = AutomationTimingPolicy(
                spinSettleDelayMs = 1_000L,
                catchSettleDelayMs = 3_500L,
            ),
        )
        fun observation(seq: Long, observedAtEpochMs: Long) = AutomationObservation(
            identity = identity,
            messageSeq = seq,
            observedAtEpochMs = observedAtEpochMs,
            observedAtElapsedNs = nowElapsedNs,
            snapshot = AutomationSnapshot(
                lifecycleState = GameLifecycleState.OVERWORLD,
                forts = FortSnapshot(
                    observedAtEpochMs = observedAtEpochMs,
                    forts = listOf(
                        Fort(
                            fortId = "fort-1",
                            type = FortType.POKESTOP,
                            position = GeoPoint(1.0, 2.0),
                            spinAvailable = true,
                        ),
                    ),
                ),
            ),
        )

        val first = runner.onObservation(observation(1L, 1_000L), policy).getOrThrow()
        val request = first.request!!
        assertEquals(1_000_000_000L, request.settleDelayNs)
        runner.onResult(
            ActionExecution(request, ActionExecutionPhase.COMPLETED, runtimeMessageSeq = 2L),
        ).getOrThrow()

        nowElapsedNs += 999_000_000L
        assertNull(runner.onObservation(observation(3L, 1_001L), policy).getOrThrow().request)
        assertEquals(1, submitted.size)

        nowElapsedNs += 1_000_000L
        assertNotNull(runner.onObservation(observation(4L, 1_002L), policy).getOrThrow().request)
        assertEquals(2, submitted.size)
    }

    @Test
    fun `catch uses its longer configurable settle delay`() {
        var nowElapsedNs = 1_000_000L
        val submitted = mutableListOf<ActionRequest>()
        val identity = RuntimeIdentity(
            runtimeSessionId = "session-catch",
            pid = 99,
            processName = "pogo",
            packageName = "com.nianticlabs.pokemongo",
            buildFingerprint = "verified",
            capabilities = setOf("CATCH"),
            mutationsAllowed = true,
        )
        val runner = AutomationRunner(
            executor = ActionRequestExecutor { request -> submitted += request; Result.success(Unit) },
            nowEpochMs = { 1_000L },
            nowElapsedNs = { nowElapsedNs },
            commandIdFactory = { "catch-command-${submitted.size + 1}" },
        )
        runner.attach(identity).getOrThrow()
        val policy = AutomationPolicy(
            autoCatch = true,
            timing = AutomationTimingPolicy(
                spinSettleDelayMs = 1_000L,
                catchSettleDelayMs = 3_500L,
            ),
        )
        fun observation(seq: Long, observedAtEpochMs: Long) = AutomationObservation(
            identity = identity,
            messageSeq = seq,
            observedAtEpochMs = observedAtEpochMs,
            observedAtElapsedNs = nowElapsedNs,
            snapshot = AutomationSnapshot(
                lifecycleState = GameLifecycleState.ENCOUNTER,
                encounter = dev.pogoroot.automation.core.model.EncounterSnapshot(
                    encounterId = "encounter-1",
                    speciesId = 25,
                    speciesName = "Pikachu",
                    observedAtEpochMs = observedAtEpochMs,
                ),
            ),
        )

        val first = runner.onObservation(observation(1L, 1_000L), policy).getOrThrow()
        val request = first.request!!
        assertEquals(3_500_000_000L, request.settleDelayNs)
        runner.onResult(
            ActionExecution(request, ActionExecutionPhase.COMPLETED, runtimeMessageSeq = 2L),
        ).getOrThrow()

        nowElapsedNs += 3_499_000_000L
        assertNull(runner.onObservation(observation(3L, 1_001L), policy).getOrThrow().request)
        assertEquals(1, submitted.size)

        nowElapsedNs += 1_000_000L
        assertNotNull(runner.onObservation(observation(4L, 1_002L), policy).getOrThrow().request)
        assertEquals(2, submitted.size)
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

    @Test
    fun `direct catch resumes after fresh map observation removes target`() {
        val submitted = mutableListOf<ActionRequest>()
        val identity = RuntimeIdentity(
            runtimeSessionId = "session-direct-catch",
            pid = 99,
            processName = "pogo",
            packageName = "com.nianticlabs.pokemongo",
            buildFingerprint = "verified",
            capabilities = setOf("DIRECT_CATCH"),
            mutationsAllowed = true,
        )
        val runner = AutomationRunner(
            executor = ActionRequestExecutor { request -> submitted += request; Result.success(Unit) },
            nowEpochMs = { 1_000L },
            nowElapsedNs = { 1_000_000L },
            commandIdFactory = { "direct-catch-command" },
        )
        runner.attach(identity).getOrThrow()
        val position = GeoPoint(1.0, 2.0)
        fun observation(seq: Long, spawns: List<NearbySpawn>) = AutomationObservation(
            identity = identity,
            messageSeq = seq,
            observedAtEpochMs = 1_000L,
            observedAtElapsedNs = 1_000_000L,
            snapshot = AutomationSnapshot(
                lifecycleState = GameLifecycleState.OVERWORLD,
                nearby = NearbySnapshot(1_000L, position, spawns),
            ),
        )
        val spawn = NearbySpawn(
            spawnId = "spawn-1",
            speciesId = 25,
            speciesName = "Pikachu",
            position = position,
            firstSeenAtEpochMs = 1_000L,
            expiresAtEpochMs = null,
            expiryConfidence = SpawnExpiryConfidence.UNKNOWN,
        )

        val request = runner.onObservation(
            observation(1L, listOf(spawn)),
            AutomationPolicy(autoCatch = true, catchPolicy = CatchPolicy(catchAll = true)),
        ).getOrThrow().request!!
        runner.onResult(
            ActionExecution(
                request = request,
                phase = ActionExecutionPhase.INDETERMINATE,
                errorCode = "direct_catch_outcome_unavailable",
                runtimeMessageSeq = 2L,
            ),
        ).getOrThrow()
        assertTrue(runner.snapshot().suspended)

        runner.onObservation(
            observation(3L, emptyList()),
            AutomationPolicy(autoCatch = true, catchPolicy = CatchPolicy(catchAll = true)),
        ).getOrThrow()

        assertEquals(1, submitted.size)
        assertTrue(!runner.snapshot().suspended)
        assertNull(runner.snapshot().activeExecution)
    }
}
