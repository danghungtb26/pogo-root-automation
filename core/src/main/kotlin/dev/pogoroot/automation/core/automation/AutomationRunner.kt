package dev.pogoroot.automation.core.automation

import java.util.ArrayDeque
import java.util.concurrent.TimeUnit

/**
 * Controller-side execution state machine.
 *
 * It accepts observations from one runtime session, plans from the pure
 * coordinator, and submits at most one mutation from a FIFO queue. A new
 * observation is required after every result, so queued actions are never
 * dispatched concurrently or without a fresh state check. Catch and spin may
 * also hold a controller-side settle gate before the next mutation is eligible.
 */
class AutomationRunner(
    private val executor: ActionRequestExecutor,
    private val coordinator: AutomationCoordinator = AutomationCoordinator(),
    private val nowEpochMs: () -> Long = System::currentTimeMillis,
    private val nowElapsedNs: () -> Long = System::nanoTime,
    private val maxObservationAgeNs: Long = 5_000_000_000L,
    private val commandTimeoutNs: Long = 15_000_000_000L,
    private val commandIdFactory: () -> String = { java.util.UUID.randomUUID().toString() },
) {
    private var identity: RuntimeIdentity? = null
    private var lastObservation: AutomationObservation? = null
    private var queuedMutationPlan: List<AutomationAction>? = null
    private val pendingMutationQueue = ArrayDeque<AutomationAction>()
    private val completedQueuedActions = linkedSetOf<AutomationAction>()
    private var requeueSamePlanAfterSettle = false
    private var active: ActionExecution? = null
    private var suspended = false
    private var needsResync = false
    private var blockedActionAfterIndeterminate: AutomationAction? = null
    private var lastError: String? = null
    private var lastMessageSeq = 0L
    private var nextMutationAllowedAtElapsedNs = 0L

    @Synchronized
    fun attach(runtime: RuntimeIdentity): Result<Unit> {
        val replacingSession = identity?.runtimeSessionId != runtime.runtimeSessionId
        if (identity != null && replacingSession) disconnect("runtime session replaced")
        // Recovery state belongs to the session that produced the unknown
        // result. A newly attached runtime session has a new authority and
        // must not inherit a stale blocked action from the previous session.
        val preserveRecovery = !replacingSession &&
            suspended &&
            needsResync &&
            blockedActionAfterIndeterminate != null

        identity = runtime
        // A new runtime session is a new authority. Never carry an old command
        // into it, even if the PID happens to be reused.
        if (replacingSession || active?.request?.runtimeSessionId != runtime.runtimeSessionId) active = null
        lastObservation = null
        clearMutationQueue()
        lastMessageSeq = 0L
        nextMutationAllowedAtElapsedNs = 0L
        suspended = preserveRecovery
        needsResync = preserveRecovery
        if (!preserveRecovery) {
            blockedActionAfterIndeterminate = null
            lastError = null
        }
        return Result.success(Unit)
    }

    @Synchronized
    fun updateMutationPermission(allowed: Boolean) {
        identity = identity?.copy(mutationsAllowed = allowed)
    }

    /**
     * Applies a stronger runtime announcement for the same process session
     * without clearing observation/recovery state.
     */
    @Synchronized
    fun updateRuntimeIdentity(runtime: RuntimeIdentity): Result<Unit> = runCatching {
        val current = identity ?: error("runner is not attached")
        require(current.runtimeSessionId == runtime.runtimeSessionId) {
            "runtime identity update belongs to another session"
        }
        require(current.pid == runtime.pid) { "runtime identity update PID mismatch" }
        require(current.processName == runtime.processName) {
            "runtime identity update process mismatch"
        }
        require(current.packageName == runtime.packageName) {
            "runtime identity update package mismatch"
        }
        identity = runtime
    }

    @Synchronized
    fun onObservation(
        observation: AutomationObservation,
        policy: AutomationPolicy,
    ): Result<RunnerDispatch> {
        val validation = validateObservation(observation)
        if (validation != null) return Result.failure(IllegalStateException(validation))

        lastObservation = observation
        lastMessageSeq = observation.messageSeq

        if (suspended && resolveIndeterminateMapAction(observation)) {
            // The client-side invocation is now corroborated by a fresh map
            // state. Continue planning from this observation without retrying
            // the command that had an unknown server response.
        }
        if (suspended || active != null) {
            return Result.success(
                RunnerDispatch(
                    reason = if (suspended) "runner suspended; resync required" else "mutation active",
                ),
            )
        }

        if (nowElapsedNs() < nextMutationAllowedAtElapsedNs) {
            return Result.success(RunnerDispatch())
        }

        val actions = coordinator.plan(observation.snapshot, policy)
        val alerts = actions.filterIsInstance<AutomationAction.Alert>()
        val plannedMutations = actions.filter { it.isMutation }
        syncMutationQueue(plannedMutations)
        val mutation = pendingMutationQueue.peekFirst()
        if (mutation == null) {
            return Result.success(
                RunnerDispatch(
                    alerts = alerts,
                    reason = "duplicate mutation suppressed; mutation queue drained"
                        .takeIf { plannedMutations.isNotEmpty() },
                ),
            )
        }

        val runtime = identity!!
        if (!runtime.mutationsAllowed) {
            lastError = "mutation blocked: build fingerprint is not allowlisted"
            return Result.success(RunnerDispatch(alerts = alerts, reason = lastError))
        }

        if (mutation.expectedLifecycle != null &&
            mutation.expectedLifecycle != observation.snapshot.lifecycleState
        ) {
            lastError = "mutation blocked: lifecycle does not match expected state"
            return Result.success(RunnerDispatch(alerts = alerts, reason = lastError))
        }

        val missingCapabilities = mutation.requiredCapabilities.filterNot { it in runtime.capabilities }
        if (missingCapabilities.isNotEmpty()) {
            lastError = "mutation blocked: missing capability ${missingCapabilities.joinToString(",")}"
            return Result.success(RunnerDispatch(alerts = alerts, reason = lastError))
        }

        val createdAtElapsedNs = nowElapsedNs()
        val settleDelayNs = TimeUnit.MILLISECONDS.toNanos(
            policy.timing.settleDelayMsFor(mutation),
        )
        val request = ActionRequest(
            commandId = commandIdFactory(),
            runtimeSessionId = runtime.runtimeSessionId,
            action = mutation,
            basedOnObservationSeq = observation.messageSeq,
            expectedLifecycle = mutation.expectedLifecycle,
            createdAtEpochMs = nowEpochMs(),
            createdAtElapsedNs = createdAtElapsedNs,
            expiresAtElapsedNs = createdAtElapsedNs + commandTimeoutNs,
            settleDelayNs = settleDelayNs,
            pid = runtime.pid,
            processName = runtime.processName,
            packageName = runtime.packageName,
            buildFingerprint = runtime.buildFingerprint,
        )
        val submission = runCatching { executor.submit(request) }
        if (submission.isFailure) {
            // A failed write does not prove that the broker did not receive the
            // command. Treat delivery as unknown and suspend mutation.
            val error = submission.exceptionOrNull()
            val execution = ActionExecution(
                request = request,
                phase = ActionExecutionPhase.INDETERMINATE,
                message = error?.message ?: "command submission failed",
            )
            active = execution
            suspended = true
            needsResync = true
            blockedActionAfterIndeterminate = request.action
            consumeQueuedAction(request.action)
            lastError = execution.message
            return Result.success(RunnerDispatch(alerts = alerts, reason = lastError))
        }

        active = ActionExecution(request, ActionExecutionPhase.SUBMITTED)
        lastError = null
        return Result.success(RunnerDispatch(alerts = alerts, request = request))
    }

    @Synchronized
    fun onResult(execution: ActionExecution): Result<AutomationRunnerStatus> {
        val current = active ?: return Result.failure(IllegalStateException("no active mutation"))
        if (execution.request.commandId != current.request.commandId) {
            return Result.failure(IllegalStateException("result command does not match active mutation"))
        }
        if (execution.request.runtimeSessionId != identity?.runtimeSessionId) {
            return Result.failure(IllegalStateException("result belongs to an old runtime session"))
        }
        val resultSeq = execution.runtimeMessageSeq
        if (resultSeq != null && resultSeq <= lastMessageSeq) {
            return Result.failure(IllegalStateException("stale runtime result sequence"))
        }
        if (resultSeq != null) lastMessageSeq = resultSeq
        if (!isValidTransition(current.phase, execution.phase)) {
            return Result.failure(
                IllegalStateException("invalid action transition ${current.phase} -> ${execution.phase}"),
            )
        }

        active = execution
        if (execution.phase == ActionExecutionPhase.INDETERMINATE) {
            suspended = true
            needsResync = true
            blockedActionAfterIndeterminate = execution.request.action
            consumeQueuedAction(execution.request.action)
            lastError = execution.message ?: "action outcome is indeterminate"
        } else if (execution.phase.isDefinitive) {
            consumeQueuedAction(execution.request.action)
            requeueSamePlanAfterSettle = execution.phase == ActionExecutionPhase.COMPLETED &&
                execution.request.settleDelayNs > 0L
            active = null
            lastError = execution.message.takeUnless { execution.phase == ActionExecutionPhase.COMPLETED }
            scheduleSettle(execution.request.settleDelayNs)
        }
        return Result.success(snapshot())
    }

    /** Called when the broker reports a binding/process/socket loss. */
    @Synchronized
    fun disconnect(reason: String): ActionExecution? {
        val current = active
        if (current != null) {
            active = if (current.phase == ActionExecutionPhase.INDETERMINATE) {
                current
            } else {
                current.copy(
                    phase = ActionExecutionPhase.INDETERMINATE,
                    message = reason,
                )
            }
            blockedActionAfterIndeterminate = active?.request?.action
            active?.request?.action?.let(::consumeQueuedAction)
        }
        if (current != null) lastError = reason
        identity = null
        suspended = true
        needsResync = true
        clearMutationQueue()
        nextMutationAllowedAtElapsedNs = 0L
        return active
    }

    /** Accept a fresh observation without planning while recovering. */
    @Synchronized
    fun acceptResync(observation: AutomationObservation): Result<Unit> {
        val validation = validateObservation(observation, requireAttached = false)
        if (validation != null) return Result.failure(IllegalStateException(validation))
        val currentIdentity = identity
        if (currentIdentity == null || currentIdentity != observation.identity) {
            return Result.failure(IllegalStateException("resync belongs to an inactive runtime session"))
        }
        lastObservation = observation
        lastMessageSeq = observation.messageSeq
        return Result.success(Unit)
    }

    /** Explicit operator/runtime resumption after a fresh resync. */
    @Synchronized
    fun resumeAfterResync(): Result<Unit> {
        if (!suspended || !needsResync) return Result.failure(IllegalStateException("resync is not pending"))
        if (lastObservation == null) return Result.failure(IllegalStateException("fresh observation required"))
        val indeterminateAction = active
            ?.takeIf { it.phase == ActionExecutionPhase.INDETERMINATE }
            ?.request
            ?.action
            ?: blockedActionAfterIndeterminate
        active = null
        suspended = false
        needsResync = false
        nextMutationAllowedAtElapsedNs = 0L
        lastError = null
        // Do not automatically retry the mutation whose outcome was unknown.
        // Other intents from the same fresh snapshot may still be considered.
        if (indeterminateAction != null) consumeQueuedAction(indeterminateAction)
        blockedActionAfterIndeterminate = null
        return Result.success(Unit)
    }

    @Synchronized
    fun markSafeTimeout(
        commandId: String,
        message: String = "command was not accepted",
    ): Result<AutomationRunnerStatus> {
        val current = active ?: return Result.failure(IllegalStateException("no active mutation"))
        if (current.request.commandId != commandId) {
            return Result.failure(IllegalStateException("timeout command does not match active mutation"))
        }
        if (current.phase != ActionExecutionPhase.SUBMITTED) {
            return Result.failure(IllegalStateException("safe timeout is only valid before acceptance"))
        }
        active = null
        consumeQueuedAction(current.request.action)
        requeueSamePlanAfterSettle = false
        lastError = message
        scheduleSettle(current.request.settleDelayNs)
        return Result.success(snapshot())
    }

    /**
     * Expires a command conservatively. Without an explicit broker guarantee
     * that delivery did not happen, an elapsed deadline is indeterminate.
     */
    @Synchronized
    fun checkTimeout(): Result<AutomationRunnerStatus>? {
        val current = active ?: return null
        if (nowElapsedNs() < current.request.expiresAtElapsedNs) return null
        if (current.phase == ActionExecutionPhase.INDETERMINATE) return Result.success(snapshot())
        active = current.copy(
            phase = ActionExecutionPhase.INDETERMINATE,
            message = "command outcome timed out before it could be confirmed",
        )
        suspended = true
        needsResync = true
        blockedActionAfterIndeterminate = active?.request?.action
        active?.request?.action?.let(::consumeQueuedAction)
        lastError = active?.message
        return Result.success(snapshot())
    }

    @Synchronized
    fun snapshot(): AutomationRunnerStatus = AutomationRunnerStatus(
        runtimeSessionId = identity?.runtimeSessionId,
        activeExecution = active,
        suspended = suspended,
        needsResync = needsResync,
        lastObservationSeq = lastObservation?.messageSeq,
        lastError = lastError,
    )

    private fun scheduleSettle(delayNs: Long) {
        if (delayNs <= 0L) {
            nextMutationAllowedAtElapsedNs = 0L
            return
        }
        val now = nowElapsedNs()
        nextMutationAllowedAtElapsedNs = if (Long.MAX_VALUE - now < delayNs) {
            Long.MAX_VALUE
        } else {
            now + delayNs
        }
    }

    private fun resolveIndeterminateMapAction(observation: AutomationObservation): Boolean {
        val execution = active?.takeIf { it.phase == ActionExecutionPhase.INDETERMINATE } ?: return false
        val action = execution.request.action
        val postconditionObserved = when (action) {
            is AutomationAction.Catch -> action.mode == CatchMode.DIRECT_MAP &&
                execution.errorCode == "direct_catch_outcome_unavailable" &&
                observation.snapshot.nearby?.spawns?.none { it.spawnId == action.encounterId } == true
            else -> false
        }
        if (!postconditionObserved) return false

        consumeQueuedAction(action)
        active = null
        suspended = false
        needsResync = false
        blockedActionAfterIndeterminate = null
        lastError = null
        scheduleSettle(execution.request.settleDelayNs)
        return true
    }

    /**
     * Reconciles the pending FIFO with the latest plan. Already consumed
     * actions are not re-added merely because another observation arrived;
     * they become eligible again only after a completed action's settle gate.
     * Actions that disappeared from the fresh plan are discarded as stale.
     */
    private fun syncMutationQueue(plannedMutations: List<AutomationAction>) {
        if (plannedMutations.isEmpty()) {
            clearMutationQueue()
            queuedMutationPlan = emptyList()
            return
        }

        val planChanged = queuedMutationPlan != plannedMutations
        if (planChanged) {
            queuedMutationPlan = plannedMutations
            val plannedSet = plannedMutations.toSet()
            pendingMutationQueue.removeIf { it !in plannedSet }
            completedQueuedActions.retainAll(plannedSet)
            plannedMutations.forEach { action ->
                if (action !in pendingMutationQueue && action !in completedQueuedActions) {
                    pendingMutationQueue.addLast(action)
                }
            }
            requeueSamePlanAfterSettle = false
        }

        if (pendingMutationQueue.isEmpty() && requeueSamePlanAfterSettle) {
            pendingMutationQueue.addAll(plannedMutations)
            completedQueuedActions.clear()
            requeueSamePlanAfterSettle = false
        }
    }

    private fun consumeQueuedAction(action: AutomationAction) {
        pendingMutationQueue.removeFirstOccurrence(action)
        completedQueuedActions += action
    }

    private fun clearMutationQueue() {
        queuedMutationPlan = null
        pendingMutationQueue.clear()
        completedQueuedActions.clear()
        requeueSamePlanAfterSettle = false
    }

    private fun validateObservation(
        observation: AutomationObservation,
        requireAttached: Boolean = true,
    ): String? {
        val current = identity
        if (requireAttached && current == null) return "no active runtime session"
        if (current != null && observation.identity != current) return "observation identity mismatch"
        if (observation.messageSeq <= lastMessageSeq) return "stale observation sequence"
        val age = nowElapsedNs() - observation.observedAtElapsedNs
        if (age < -maxObservationAgeNs || age > maxObservationAgeNs) return "observation is stale"
        val epochAge = nowEpochMs() - observation.observedAtEpochMs
        val maxObservationAgeMs = maxObservationAgeNs / 1_000_000L
        if (epochAge < -maxObservationAgeMs || epochAge > maxObservationAgeMs) {
            return "observation wall-clock timestamp is stale"
        }
        return null
    }

    private fun isValidTransition(
        from: ActionExecutionPhase,
        to: ActionExecutionPhase,
    ): Boolean = when (from) {
        ActionExecutionPhase.SUBMITTED -> to in setOf(
            ActionExecutionPhase.ACCEPTED,
            ActionExecutionPhase.STARTED,
            ActionExecutionPhase.COMPLETED,
            ActionExecutionPhase.REJECTED,
            ActionExecutionPhase.FAILED,
            ActionExecutionPhase.SAFE_TIMEOUT,
            ActionExecutionPhase.INDETERMINATE,
        )

        ActionExecutionPhase.ACCEPTED -> to in setOf(
            ActionExecutionPhase.STARTED,
            ActionExecutionPhase.COMPLETED,
            ActionExecutionPhase.REJECTED,
            ActionExecutionPhase.FAILED,
            ActionExecutionPhase.INDETERMINATE,
        )

        ActionExecutionPhase.STARTED -> to in setOf(
            ActionExecutionPhase.COMPLETED,
            ActionExecutionPhase.REJECTED,
            ActionExecutionPhase.FAILED,
            ActionExecutionPhase.INDETERMINATE,
        )

        else -> false
    }
}
