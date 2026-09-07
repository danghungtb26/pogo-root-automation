package dev.pogoroot.automation.headless

import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.RuntimeBridge
import dev.pogoroot.automation.bridge.RuntimeSessionManager
import dev.pogoroot.automation.bridge.toRuntimeIdentity
import dev.pogoroot.automation.adapter.GameCapability
import dev.pogoroot.automation.core.automation.ActionExecution
import dev.pogoroot.automation.core.automation.ActionExecutionPhase
import dev.pogoroot.automation.core.automation.ActionRequestExecutor
import dev.pogoroot.automation.core.automation.AutomationObservation
import dev.pogoroot.automation.core.automation.AutomationRunner
import dev.pogoroot.automation.core.automation.AutomationRunnerStatus
import dev.pogoroot.automation.core.automation.AutomationSnapshot
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.pogo.BridgeBackedPogoActionExecutor
import dev.pogoroot.automation.pogo.BridgePogoRuntimeSource
import dev.pogoroot.automation.pogo.PogoGameAdapter

data class StructuredAutomationTick(
    val runtimeSessionId: String?,
    val lifecycleState: GameLifecycleState,
    val observationSeq: Long? = null,
    val lastAction: String? = null,
    val lastError: String? = null,
    val suspended: Boolean = false,
    val submitted: Boolean = false,
)

/**
 * Structured policy loop used by the foreground service. It has no screen
 * capture or input-driver dependency; all state comes from the runtime bridge.
 */
class StructuredAutomationController(
    private val bridge: RuntimeBridge,
    private val eventSink: AutomationEventSink = AutomationEventSink { },
    allowedBuildFingerprints: Set<String> = emptySet(),
) {
    private val source = BridgePogoRuntimeSource(bridge)
    private val sessionManager = RuntimeSessionManager(
        expectedPackageNames = setOf(
            "com.nianticlabs.pokemongo",
            "com.nianticlabs.pokemongo.ares",
        ),
        allowedBuildFingerprints = allowedBuildFingerprints,
    )
    private val actionExecutor = BridgeBackedPogoActionExecutor(
        bridge = bridge,
        runtimeReady = { source.runtimeMetadata?.ready },
        allowedBuildFingerprints = allowedBuildFingerprints,
    )
    private val adapter = PogoGameAdapter(source, actionExecutor)
    private val runner = AutomationRunner(
        executor = ActionRequestExecutor { request -> adapter.submit(request) },
    )
    private var connected = false
    private var processedObservationSeq = 0L
    private var lastAction: String? = null
    private var lastError: String? = null

    fun tick(config: HeadlessAutomationConfig): Result<StructuredAutomationTick> = runCatching {
        ensureConnected()
        source.refresh().getOrThrow()
        source.runtimeMetadata ?: error("runtime session disappeared")

        val results = source.drainCommandResults().getOrThrow()
        results.forEach(::consumeResult)
        runner.checkTimeout()

        val current = source.runtimeMetadata ?: error("runtime session disappeared")
        if (source.lifecycleState() == GameLifecycleState.DISCONNECTED) {
            val reason = "runtime binding lost"
            runner.disconnect(reason)
            connected = false
            lastError = reason
            return@runCatching tickStatus()
        }

        var submitted = false
        val observationSeq = current.lastObservationSeq
        val observationEpochMs = current.lastObservationEpochMs
        if (observationSeq > processedObservationSeq && observationEpochMs != null) {
            val snapshot = readSnapshot()
            val observation = AutomationObservation(
                identity = current.identity(),
                messageSeq = observationSeq,
                observedAtEpochMs = observationEpochMs,
                observedAtElapsedNs = current.lastObservationElapsedNs ?: System.nanoTime(),
                snapshot = snapshot,
            )
            val resyncing = runner.snapshot().needsResync
            if (resyncing) {
                runner.acceptResync(observation).getOrThrow()
                runner.resumeAfterResync().getOrThrow()
                processedObservationSeq = observationSeq
                return@runCatching tickStatus(
                    observationSeq = observationSeq,
                    submitted = false,
                )
            }
            val dispatch = runner.onObservation(observation, config.toCorePolicy()).getOrThrow()
            processedObservationSeq = observationSeq
            dispatch.alerts.forEach { alert ->
                eventSink.publish(AutomationEvent(AutomationEventType.INFO, alert.message))
            }
            dispatch.reason?.let {
                lastError = it
                eventSink.publish(AutomationEvent(AutomationEventType.ERROR, it))
            }
            dispatch.request?.let {
                submitted = true
                lastAction = it.action::class.simpleName
            }
        }

        tickStatus(
            observationSeq = observationSeq.takeIf { it > 0L },
            submitted = submitted,
        )
    }.onFailure { error ->
        runner.disconnect("runtime bridge failure: ${error.message ?: error::class.java.simpleName}")
        connected = false
        lastError = error.message
    }

    fun snapshot(): AutomationRunnerStatus = runner.snapshot()

    fun stop() {
        if (connected) {
            runner.disconnect("structured runner stopped")
            source.disconnect()
        }
        connected = false
    }

    private fun ensureConnected() {
        if (connected) return
        source.connect().getOrThrow()
        val ready = source.runtimeMetadata?.ready ?: error("runtime did not provide readiness")
        sessionManager.accept(ready).getOrThrow()
        val identity = ready.toRuntimeIdentity(sessionManager.mutationsAllowed)
        runner.attach(identity).getOrThrow()
        processedObservationSeq = 0L
        connected = true
        lastError = null
    }

    private fun readSnapshot(): AutomationSnapshot {
        val lifecycle = adapter.lifecycleState()
        val nearby = adapter.readNearby().getOrNull()
        val encounter = if (lifecycle == GameLifecycleState.ENCOUNTER) {
            adapter.readEncounter().getOrNull()
        } else {
            null
        }
        val forts = if (GameCapability.READ_FORTS in adapter.capabilities) {
            adapter.readForts().getOrNull()
        } else {
            null
        }
        val inventory = if (GameCapability.READ_INVENTORY in adapter.capabilities) {
            adapter.readInventory().getOrNull()
        } else {
            null
        }
        val storage = if (GameCapability.READ_POKEMON_STORAGE in adapter.capabilities) {
            adapter.readPokemonStorage().getOrNull()
        } else {
            null
        }
        return AutomationSnapshot(
            lifecycleState = lifecycle,
            nearby = nearby,
            encounter = encounter,
            forts = forts,
            inventory = inventory,
            storage = storage,
        )
    }

    private fun consumeResult(result: BridgeEvent.AutomationCommandResult) {
        val request = runner.snapshot().activeExecution?.request ?: return
        if (request.commandId != result.commandId) return
        val phase = when (result.phase) {
            dev.pogoroot.automation.bridge.CommandPhase.ACCEPTED -> ActionExecutionPhase.ACCEPTED
            dev.pogoroot.automation.bridge.CommandPhase.STARTED -> ActionExecutionPhase.STARTED
            dev.pogoroot.automation.bridge.CommandPhase.COMPLETED -> ActionExecutionPhase.COMPLETED
            dev.pogoroot.automation.bridge.CommandPhase.REJECTED -> ActionExecutionPhase.REJECTED
            dev.pogoroot.automation.bridge.CommandPhase.FAILED -> ActionExecutionPhase.FAILED
            dev.pogoroot.automation.bridge.CommandPhase.SAFE_TIMEOUT -> ActionExecutionPhase.SAFE_TIMEOUT
            dev.pogoroot.automation.bridge.CommandPhase.INDETERMINATE -> ActionExecutionPhase.INDETERMINATE
        }
        runner.onResult(
            ActionExecution(
                request = request,
                phase = phase,
                message = result.message,
                errorCode = result.errorCode,
                runtimeMessageSeq = result.messageSeq,
                observedAtEpochMs = result.observedAtEpochMs,
                observedAtElapsedNs = result.observedAtElapsedNs,
            ),
        ).onFailure { lastError = it.message }
        if (phase.isTerminal) {
            lastAction = request.action::class.simpleName
        }
    }

    private fun tickStatus(
        observationSeq: Long? = source.runtimeMetadata?.lastObservationSeq?.takeIf { it > 0L },
        submitted: Boolean = false,
    ): StructuredAutomationTick {
        val metadata = source.runtimeMetadata
        return StructuredAutomationTick(
            runtimeSessionId = metadata?.ready?.runtimeSessionId,
            lifecycleState = source.lifecycleState(),
            observationSeq = observationSeq,
            lastAction = lastAction,
            lastError = lastError ?: runner.snapshot().lastError,
            suspended = runner.snapshot().suspended,
            submitted = submitted,
        )
    }

    private fun dev.pogoroot.automation.pogo.PogoRuntimeMetadata.identity() =
        ready.toRuntimeIdentity(sessionManager.mutationsAllowed)
}
