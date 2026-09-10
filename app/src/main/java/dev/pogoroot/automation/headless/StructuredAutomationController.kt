package dev.pogoroot.automation.headless

import android.util.Log
import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.RuntimeBridge
import dev.pogoroot.automation.bridge.RuntimeSessionManager
import dev.pogoroot.automation.bridge.toRuntimeIdentity
import dev.pogoroot.automation.adapter.GameCapability
import dev.pogoroot.automation.core.automation.ActionExecution
import dev.pogoroot.automation.core.automation.ActionExecutionPhase
import dev.pogoroot.automation.core.automation.ActionRequestExecutor
import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.AutomationObservation
import dev.pogoroot.automation.core.automation.AutomationRunner
import dev.pogoroot.automation.core.automation.AutomationRunnerStatus
import dev.pogoroot.automation.core.automation.AutomationSnapshot
import dev.pogoroot.automation.core.automation.CatchMode
import dev.pogoroot.automation.core.automation.CatchOutcome
import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.MapTargetObservation
import dev.pogoroot.automation.bridge.MapTargetPayloadCodec
import dev.pogoroot.automation.pogo.BridgeBackedPogoActionExecutor
import dev.pogoroot.automation.pogo.BridgePogoRuntimeSource
import dev.pogoroot.automation.pogo.PogoGameAdapter
import dev.pogoroot.automation.pogo.RuntimeThrowDiagnosticPayloadCodec
/** Structured policy loop; all state comes from the runtime bridge. */
class StructuredAutomationController(
    private val bridge: RuntimeBridge,
    private val eventSink: AutomationEventSink = AutomationEventSink { },
    allowedBuildFingerprints: Set<String> = emptySet(),
    private val allowedBuildFingerprintsProvider: (() -> Set<String>)? = null,
    private val onGameAction: (LastActiveGameAction) -> Unit = {},
    private val onEncounterSnapshot: (EncounterSnapshot) -> Unit = {},
    private val onMapTarget: (MapTargetObservation) -> Unit = {},
) {
    private val configuredAllowedBuildFingerprints = allowedBuildFingerprints.toSet()
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
    private var processedScanCycleId = 0L
    private var lastAction: String? = null
    private var lastError: String? = null
    private var outOfBalls = false
    private var latestPlayerPosition: GeoPoint? = null
    private val recordedGameActionCommands = mutableSetOf<String>()
    private val catchLabelsByCommand = mutableMapOf<String, String>()
    private val publishedCatchOutcomeCommands = mutableSetOf<String>()
    private val requestedCatchPokemonIds = linkedSetOf<String>()
    private val wildState = StructuredAutomationWildState(eventSink)
    private var resetRunnerOnNextAttach = false
    fun tick(config: HeadlessAutomationConfig): Result<StructuredAutomationTick> = runCatching {
        syncSafetyConfig()
        ensureConnected()
        source.refresh().getOrThrow()
        val runtimeMetadata = source.runtimeMetadata ?: error("runtime session disappeared")
        syncRuntimeIdentity(runtimeMetadata.ready)

        var submitted = false
        var observationSeq: Long? = null
        val events = source.drainEvents().getOrThrow()
        for (event in events) {
            when (event) {
                is BridgeEvent.AutomationCommandResult -> consumeResult(event)
                is BridgeEvent.ObservationEvent -> {
                    if (event.messageSeq <= processedObservationSeq) continue
                    source.selectObservation(event.messageSeq).getOrThrow()
                    try {
                        if (event.observationType == dev.pogoroot.automation.bridge.ObservationType.MAP_TARGET) {
                            if (GameCapability.READ_MAP_TARGET !in adapter.capabilities) {
                                lastError = "map target ignored: runtime did not advertise READ_MAP_TARGET"
                            } else {
                                MapTargetPayloadCodec.decode(event.payload)
                                    .onSuccess(onMapTarget)
                                    .onFailure { error -> lastError = "map target decode: ${error.message}" }
                            }
                            processedObservationSeq = event.messageSeq
                            observationSeq = event.messageSeq
                            continue
                        }
                        if (event.observationType == dev.pogoroot.automation.bridge.ObservationType.THROW_DIAGNOSTIC) {
                            RuntimeThrowDiagnosticPayloadCodec.decode(event.payload)
                                .onSuccess { diagnostic ->
                                    eventSink.publish(
                                        AutomationEvent(
                                            AutomationEventType.INFO,
                                            diagnostic.message(),
                                        ),
                                    )
                                }
                                .onFailure { error ->
                                    lastError = "throw diagnostic decode: ${error.message}"
                                    eventSink.publish(
                                        AutomationEvent(AutomationEventType.ERROR, lastError!!),
                                    )
                                }
                            processedObservationSeq = event.messageSeq
                            observationSeq = event.messageSeq
                            continue
                        }
                        if (event.observationType !in setOf(
                                dev.pogoroot.automation.bridge.ObservationType.REQUEST_CATCH_SPIN,
                                dev.pogoroot.automation.bridge.ObservationType.ENCOUNTER,
                            )) {
                            Log.i(
                                LOG_TAG,
                                "automation observation consumed without filter type=" +
                                    "${event.observationType} seq=${event.messageSeq}",
                            )
                            processedObservationSeq = event.messageSeq
                            observationSeq = event.messageSeq
                            continue
                        }
                        val scanCycleId = source.selectedScanCycleId()
                        if (event.observationType ==
                            dev.pogoroot.automation.bridge.ObservationType.REQUEST_CATCH_SPIN
                        ) {
                            if (scanCycleId == null || scanCycleId <= processedScanCycleId) {
                                Log.w(
                                    LOG_TAG,
                                    "automation REQUEST_CATCH_SPIN ignored stale cycle=$scanCycleId " +
                                        "last=$processedScanCycleId seq=${event.messageSeq}",
                                )
                                processedObservationSeq = event.messageSeq
                                observationSeq = event.messageSeq
                                continue
                            }
                            Log.i(
                                LOG_TAG,
                                "automation REQUEST_CATCH_SPIN received cycle=$scanCycleId " +
                                    "seq=${event.messageSeq}",
                            )
                        }
                        val current = source.runtimeMetadata ?: error("runtime session disappeared")
                        val snapshot = adapter.readStructuredSnapshot(
                            outOfBalls = outOfBalls,
                            mergeStorage = wildState::mergePendingWildTransfers,
                            excludedSpawnIds = requestedCatchPokemonIds,
                        )
                        Log.i(
                            LOG_TAG,
                            "automation filter input seq=${event.messageSeq} " +
                                "type=${event.observationType} cycle=${scanCycleId ?: "none"} " +
                                "lifecycle=${snapshot.lifecycleState} " +
                                "nearby=${snapshot.nearby?.spawns?.size ?: "unavailable"} " +
                                "forts=${snapshot.forts?.forts?.size ?: "unavailable"} " +
                                "inventory=${snapshot.inventory?.items?.size ?: "unavailable"}",
                        )
                        snapshot.encounter?.let(onEncounterSnapshot)
                        (snapshot.nearby?.playerPosition ?: snapshot.encounter?.position)
                            ?.let { latestPlayerPosition = it }
                        val automationObservation = AutomationObservation(
                            identity = current.identity(),
                            messageSeq = event.messageSeq,
                            observedAtEpochMs = event.observedAtEpochMs,
                            observedAtElapsedNs = event.observedAtElapsedNs,
                            snapshot = snapshot,
                        )
                        val policy = config.toCorePolicy().let { configured ->
                            if (configured.autoCloseCatchPreview &&
                                GameCapability.CATCH_AND_CLOSE_PREVIEW !in adapter.capabilities
                            ) {
                                lastError = "catch preview close unavailable; using normal catch"
                                configured.copy(autoCloseCatchPreview = false)
                            } else {
                                configured
                            }
                        }
                        val dispatch = runner.onObservation(
                            automationObservation,
                            policy,
                        ).getOrThrow()
                        Log.i(
                            LOG_TAG,
                            "automation filter result seq=${event.messageSeq} " +
                                "action=${dispatch.request?.action ?: "none"} " +
                                "reason=${dispatch.reason ?: "none"}",
                        )
                        dispatch.alerts.forEach { alert ->
                            eventSink.publish(AutomationEvent(AutomationEventType.INFO, alert.message))
                        }
                        dispatch.reason?.let {
                            lastError = it
                            if (it != "runner suspended; resync required" &&
                                it != "mutation active" &&
                                !it.startsWith("mutation blocked: missing capability")
                            ) {
                                eventSink.publish(AutomationEvent(AutomationEventType.ERROR, it))
                            }
                        }
                        dispatch.request?.let {
                            (it.action as? AutomationAction.Catch)?.encounterId?.let {
                                requestedCatchPokemonIds += it
                            }
                            rememberCatchLabel(it, snapshot)
                            wildState.rememberCatchTemplate(it, snapshot)
                            submitted = true
                            lastAction = it.action::class.simpleName
                            Log.i(
                                LOG_TAG,
                                "automation action dispatched semantic=${it.action.structuredSemanticName()} " +
                                    "command=${it.commandId} " +
                                    "action=${it.action}",
                            )
                        }
                        if (scanCycleId != null) processedScanCycleId = scanCycleId
                        processedObservationSeq = event.messageSeq
                        observationSeq = event.messageSeq
                    } finally {
                        source.clearObservationSelection()
                    }
                }
                is BridgeEvent.BindingLost -> {
                    val reason = event.reason.ifBlank { "runtime binding lost" }
                    runner.disconnect(reason)
                    connected = false
                    lastError = reason
                    break
                }
                is BridgeEvent.RuntimeError -> {
                    lastError = "${event.code}: ${event.message}"
                    eventSink.publish(AutomationEvent(AutomationEventType.ERROR, lastError!!))
                }
                else -> Unit
            }
        }
        runner.checkTimeout()

        tickStatus(
            observationSeq = observationSeq ?: source.runtimeMetadata?.lastObservationSeq?.takeIf { it > 0L },
            submitted = submitted,
        )
    }.onFailure { error ->
        runner.disconnect("runtime bridge failure: ${error.message ?: error::class.java.simpleName}")
        connected = false
        lastError = error.message
    }

    fun snapshot(): AutomationRunnerStatus = runner.snapshot()
    fun awaitingActionResult(): Boolean = runner.snapshot().activeExecution != null
    fun resetForAutomationDisable() {
        if (connected) source.disconnect()
        connected = false
        resetRunnerOnNextAttach = true
        processedObservationSeq = 0L
        processedScanCycleId = 0L
        requestedCatchPokemonIds.clear()
        outOfBalls = false
        latestPlayerPosition = null
        recordedGameActionCommands.clear()
        catchLabelsByCommand.clear()
        publishedCatchOutcomeCommands.clear()
        wildState.clear()
    }

    fun stop() = resetForAutomationDisable()
    private fun ensureConnected() {
        if (connected) return
        source.connect().getOrThrow()
        val ready = source.runtimeMetadata?.ready ?: error("runtime did not provide readiness")
        sessionManager.accept(ready).getOrThrow()
        val identity = ready.toRuntimeIdentity(sessionManager.mutationsAllowed)
        runner.attach(identity, preserveRecoveryState = !resetRunnerOnNextAttach).getOrThrow()
        resetRunnerOnNextAttach = false
        processedObservationSeq = 0L
        processedScanCycleId = 0L
        latestPlayerPosition = null
        recordedGameActionCommands.clear()
        catchLabelsByCommand.clear()
        publishedCatchOutcomeCommands.clear()
        wildState.clear()
        connected = true
        lastError = null
    }

    private fun syncSafetyConfig() {
        val allowlist = currentAllowedBuildFingerprints()
        sessionManager.updateAllowedBuildFingerprints(allowlist)
        actionExecutor.updateAllowedBuildFingerprints(allowlist)
        runner.updateMutationPermission(sessionManager.mutationsAllowed)
    }

    private fun syncRuntimeIdentity(ready: BridgeEvent.RuntimeReady) {
        if (sessionManager.current != ready) {
            sessionManager.accept(ready).getOrThrow()
            runner.updateRuntimeIdentity(ready.toRuntimeIdentity(sessionManager.mutationsAllowed)).getOrThrow()
        } else {
            runner.updateMutationPermission(sessionManager.mutationsAllowed)
        }
    }

    private fun currentAllowedBuildFingerprints(): Set<String> =
        allowedBuildFingerprintsProvider?.invoke()?.toSet() ?: configuredAllowedBuildFingerprints
    private fun consumeResult(result: BridgeEvent.AutomationCommandResult) {
        Log.i(
            LOG_TAG,
            "automation action result command=${result.commandId} phase=${result.phase} " +
                "error=${result.errorCode} message=${result.message}",
        )
        val request = runner.snapshot().activeExecution?.request ?: run {
            Log.w(LOG_TAG, "automation action result has no active request command=${result.commandId}")
            return
        }
        if (request.commandId != result.commandId) {
            Log.w(
                LOG_TAG,
                "automation action result ignored expected=${request.commandId} " +
                    "actual=${result.commandId}",
            )
            return
        }
        val phase = when (result.phase) {
            dev.pogoroot.automation.bridge.CommandPhase.ACCEPTED -> ActionExecutionPhase.ACCEPTED
            dev.pogoroot.automation.bridge.CommandPhase.STARTED -> ActionExecutionPhase.STARTED
            dev.pogoroot.automation.bridge.CommandPhase.COMPLETED -> ActionExecutionPhase.COMPLETED
            dev.pogoroot.automation.bridge.CommandPhase.REJECTED -> ActionExecutionPhase.REJECTED
            dev.pogoroot.automation.bridge.CommandPhase.FAILED -> ActionExecutionPhase.FAILED
            dev.pogoroot.automation.bridge.CommandPhase.SAFE_TIMEOUT -> ActionExecutionPhase.SAFE_TIMEOUT
            dev.pogoroot.automation.bridge.CommandPhase.INDETERMINATE -> ActionExecutionPhase.INDETERMINATE
        }
        val resultStatus = runner.onResult(
            ActionExecution(
                request = request,
                phase = phase,
                message = result.message,
                errorCode = result.errorCode,
                catchOutcome = result.catchOutcome,
                throwOutcome = result.throwOutcome,
                snapshotResult = result.snapshotResult,
                runtimeMessageSeq = result.messageSeq,
                observedAtEpochMs = result.observedAtEpochMs,
                observedAtElapsedNs = result.observedAtElapsedNs,
            ),
        ).onFailure { lastError = it.message }
        val catchAction = request.action as? AutomationAction.Catch
        val holdDirectCatchResult = resultStatus.isSuccess &&
            phase == ActionExecutionPhase.INDETERMINATE &&
            catchAction?.mode == CatchMode.DIRECT_MAP
        if (holdDirectCatchResult) {
            Log.w(
                LOG_TAG,
                "automation direct catch held command=${request.commandId} " +
                    "encounter=${catchAction.encounterId}; outcome observer unavailable; " +
                    "no further catch will be dispatched",
            )
            eventSink.publish(
                AutomationEvent(
                    AutomationEventType.ERROR,
                    "Direct catch submitted for ${catchAction.encounterId}; " +
                        "waiting for authoritative outcome before continuing",
                ),
            )
        }
        if (resultStatus.isSuccess && phase.mayHaveRun && phase != ActionExecutionPhase.ACCEPTED) {
            recordGameActionIfNeeded(request, result)
        }
        if (resultStatus.isSuccess && phase.isTerminal && !holdDirectCatchResult) {
            lastAction = request.action::class.simpleName
            val label = if (request.action is AutomationAction.Catch) {
                catchLabelsByCommand[request.commandId] ?: request.action.encounterId()
            } else {
                request.action::class.simpleName ?: "action"
            }
            if (!isAuthoritativeCatchResult(request, result, phase)) {
                publishTerminalActionResult(eventSink, request, result, phase, label)
            }
        }
        // Track the native on-demand ball check: a catch rejected with
        // out_of_balls forces the next cycle to spin; a completed spin clears it.
        if (result.errorCode == "out_of_balls") {
            outOfBalls = true
        } else if (request.action is AutomationAction.Spin && phase.isTerminal) {
            outOfBalls = false
        }
        if (resultStatus.isSuccess && isAuthoritativeCatchResult(request, result, phase)) {
            publishCatchOutcome(request, result)
        }
        if (resultStatus.isSuccess && phase.isDefinitive) {
            wildState.registerIfCaught(request, result, phase)
            wildState.resolveTransfer(request, phase)
        }
    }

    private fun rememberCatchLabel(
        request: dev.pogoroot.automation.core.automation.ActionRequest,
        snapshot: AutomationSnapshot,
    ) {
        val action = request.action as? AutomationAction.Catch ?: return
        val label = snapshot.nearby?.spawns
            ?.firstOrNull { it.spawnId == action.encounterId }
            ?.speciesName
            ?.takeIf { it.isNotBlank() }
            ?: snapshot.encounter
                ?.takeIf { it.encounterId == action.encounterId }
                ?.speciesName
                ?.takeIf { it.isNotBlank() }
            ?: action.encounterId
        catchLabelsByCommand[request.commandId] = label
    }

    private fun isAuthoritativeCatchResult(
        request: dev.pogoroot.automation.core.automation.ActionRequest,
        result: BridgeEvent.AutomationCommandResult,
        phase: ActionExecutionPhase,
    ): Boolean {
        if (phase != ActionExecutionPhase.COMPLETED) return false
        val action = request.action as? AutomationAction.Catch ?: return false
        val outcome = result.catchOutcome ?: return false
        if (outcome == CatchOutcome.INDETERMINATE) return false
        return !action.throwProfile.requiresStructuredOutcome || result.throwOutcome?.isAttempt == true
    }

    private fun publishCatchOutcome(
        request: dev.pogoroot.automation.core.automation.ActionRequest,
        result: BridgeEvent.AutomationCommandResult,
    ) {
        if (!publishedCatchOutcomeCommands.add(request.commandId)) return
        val label = catchLabelsByCommand.remove(request.commandId) ?: request.action.encounterId()
        val outcome = result.catchOutcome ?: return
        val event = when (outcome) {
            CatchOutcome.CAUGHT -> AutomationEvent(AutomationEventType.CAUGHT, "Caught $label")
            CatchOutcome.FLED -> AutomationEvent(AutomationEventType.RAN_AWAY, "$label ran away")
            CatchOutcome.MISSED -> AutomationEvent(AutomationEventType.INFO, "Missed $label")
            CatchOutcome.BREAKOUT -> AutomationEvent(AutomationEventType.INFO, "$label broke out")
            CatchOutcome.NO_BALL -> AutomationEvent(AutomationEventType.ERROR, "No Poké Balls for $label")
            CatchOutcome.INDETERMINATE -> return
        }
        eventSink.publish(event)
    }

    private fun AutomationAction.encounterId(): String = when (this) {
        is AutomationAction.Catch -> encounterId
        else -> "target"
    }

    private fun recordGameActionIfNeeded(
        request: dev.pogoroot.automation.core.automation.ActionRequest,
        result: BridgeEvent.AutomationCommandResult,
    ) {
        val actionName = when (request.action) {
            is AutomationAction.Catch -> "catch"
            is AutomationAction.Spin -> "spin"
            is AutomationAction.UseBerry -> "berry"
            else -> return
        }
        val point = latestPlayerPosition ?: return
        if (!recordedGameActionCommands.add(request.commandId)) return
        onGameAction(
            LastActiveGameAction(
                point = point,
                action = actionName,
                activeAtEpochMs = result.observedAtEpochMs,
            ),
        )
    }

    private fun tickStatus(
        observationSeq: Long? = source.runtimeMetadata?.lastObservationSeq?.takeIf { it > 0L },
        submitted: Boolean = false,
    ): StructuredAutomationTick {
        val metadata = source.runtimeMetadata
        return StructuredAutomationTick(
            runtimeSessionId = metadata?.ready?.runtimeSessionId,
            strongIdentityVerified = metadata?.ready?.strongIdentityVerified == true,
            runtimeCapabilities = adapter.capabilities.map(GameCapability::name).toSortedSet(),
            mutationPermissionGranted = sessionManager.mutationsAllowed,
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

    private companion object {
        const val LOG_TAG = "PogoRootAutomation"
    }
}
