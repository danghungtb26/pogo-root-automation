package dev.pogoroot.automation.core.automation

import java.util.concurrent.TimeUnit

/**
 * Controller-side execution state machine.
 *
 * It accepts observations from one runtime session, plans from the pure
 * coordinator, and submits at most one mutation. A new observation is required
 * after every definitive result, so actions are never bulk-queued from a stale
 * snapshot. Catch and spin may also hold a controller-side settle gate before
 * the next mutation is eligible.
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
    private var lastPlannedMutationPlan: List<AutomationAction>? = null
    private val terminalActionsForSnapshot = linkedSetOf<AutomationAction>()
    private val repeatableActionsAfterSettle = linkedSetOf<AutomationAction>()
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
        val preserveRecovery = suspended && needsResync && blockedActionAfterIndeterminate != null

        identity = runtime
        // A new runtime session is a new authority. Never carry an old command
        // into it, even if the PID happens to be reused.
        if (replacingSession || active?.request?.runtimeSessionId != runtime.runtimeSessionId) active = null
        lastObservation = null
        lastPlannedMutationPlan = null
        terminalActionsForSnapshot.clear()
        repeatableActionsAfterSettle.clear()
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
        if (repeatableActionsAfterSettle.isNotEmpty()) {
            terminalActionsForSnapshot.removeAll(repeatableActionsAfterSettle)
            repeatableActionsAfterSettle.clear()
        }

        val actions = coordinator.plan(observation.snapshot, policy)
        val alerts = actions.filterIsInstance<AutomationAction.Alert>()
        val plannedMutations = actions.filter { it.isMutation }
        if (plannedMutations != lastPlannedMutationPlan) {
            terminalActionsForSnapshot.clear()
            repeatableActionsAfterSettle.clear()
        }
        val mutation = plannedMutations.firstOrNull {
            it !in terminalActionsForSnapshot && it != blockedActionAfterIndeterminate
        }
        if (mutation == null) {
            lastPlannedMutationPlan = plannedMutations
            return Result.success(
                RunnerDispatch(
                    alerts = alerts,
                    reason = "duplicate mutation suppressed".takeIf { plannedMutations.isNotEmpty() },
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

        // Do not repeat a completed/definitively failed mutation when a runtime
        // emits an identical snapshot. Other actions from the same plan may
        // still proceed on a later observation (UseBerry -> Catch is one case).

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
        lastPlannedMutationPlan = plannedMutations

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
            lastError = execution.message ?: "action outcome is indeterminate"
        } else if (execution.phase.isDefinitive) {
            terminalActionsForSnapshot += execution.request.action
            if (execution.phase == ActionExecutionPhase.COMPLETED &&
                execution.request.settleDelayNs > 0L
            ) {
                repeatableActionsAfterSettle += execution.request.action
            }
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
        }
        if (current != null) lastError = reason
        identity = null
        suspended = true
        needsResync = true
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
        terminalActionsForSnapshot.clear()
        repeatableActionsAfterSettle.clear()
        // Do not automatically retry the mutation whose outcome was unknown.
        // Other intents from the same fresh snapshot may still be considered.
        if (indeterminateAction != null) terminalActionsForSnapshot += indeterminateAction
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
        terminalActionsForSnapshot += current.request.action
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

        terminalActionsForSnapshot += action
        active = null
        suspended = false
        needsResync = false
        blockedActionAfterIndeterminate = null
        lastError = null
        scheduleSettle(execution.request.settleDelayNs)
        return true
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
