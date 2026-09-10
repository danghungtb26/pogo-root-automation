package dev.pogoroot.automation.headless

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
import dev.pogoroot.automation.core.automation.CatchOutcome
import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.MapTargetObservation
import dev.pogoroot.automation.core.model.PokemonIv
import dev.pogoroot.automation.core.model.PokemonStorageSnapshot
import dev.pogoroot.automation.core.model.StoredPokemon
import dev.pogoroot.automation.bridge.MapTargetPayloadCodec
import dev.pogoroot.automation.pogo.BridgeBackedPogoActionExecutor
import dev.pogoroot.automation.pogo.BridgePogoRuntimeSource
import dev.pogoroot.automation.pogo.PogoGameAdapter
import dev.pogoroot.automation.pogo.RuntimeThrowDiagnosticPayloadCodec

data class StructuredAutomationTick(
    val runtimeSessionId: String?,
    val strongIdentityVerified: Boolean = false,
    val runtimeCapabilities: Set<String> = emptySet(),
    val mutationPermissionGranted: Boolean = false,
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
    private var lastAction: String? = null
    private var lastError: String? = null
    // Set when the native catch executor reports out_of_balls; drives the
    // coordinator to skip catching and force a spin. Cleared once a spin runs.
    private var outOfBalls = false
    private var latestPlayerPosition: GeoPoint? = null
    private val recordedGameActionCommands = mutableSetOf<String>()
    private val catchLabelsByCommand = mutableMapOf<String, String>()
    private val publishedCatchOutcomeCommands = mutableSetOf<String>()
    // Wild-caught Pokémon known at catch-dispatch time, keyed by command. On a
    // confirmed CAUGHT the matching entry is promoted to a StoredPokemon (using
    // the runtime-reported captured id) and fed into snapshot.storage so the
    // shared transfer path (policy.autoTransfer) decides whether to release it.
    private val caughtWildTemplateByCommand = linkedMapOf<String, WildCatchTemplate>()
    private val pendingWildTransfers = linkedMapOf<String, StoredPokemon>()

    private data class WildCatchTemplate(
        val speciesId: Int,
        val speciesName: String,
        val iv: PokemonIv?,
        val shiny: Boolean,
    )

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
                        val current = source.runtimeMetadata ?: error("runtime session disappeared")
                        val snapshot = readSnapshot()
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
                            rememberCatchLabel(it, snapshot)
                            rememberWildCatchTemplate(it, snapshot)
                            submitted = true
                            lastAction = it.action::class.simpleName
                        }
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
        latestPlayerPosition = null
        recordedGameActionCommands.clear()
        catchLabelsByCommand.clear()
        publishedCatchOutcomeCommands.clear()
        caughtWildTemplateByCommand.clear()
        pendingWildTransfers.clear()
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
        val runtimeStorage = if (GameCapability.READ_POKEMON_STORAGE in adapter.capabilities) {
            adapter.readPokemonStorage().getOrNull()
        } else {
            null
        }
        val storage = mergePendingWildTransfers(runtimeStorage)
        return AutomationSnapshot(
            lifecycleState = lifecycle,
            nearby = nearby,
            encounter = encounter,
            forts = forts,
            inventory = inventory,
            storage = storage,
            outOfBalls = outOfBalls,
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
        if (resultStatus.isSuccess && phase.mayHaveRun && phase != ActionExecutionPhase.ACCEPTED) {
            recordGameActionIfNeeded(request, result)
        }
        if (resultStatus.isSuccess && phase.isTerminal) {
            lastAction = request.action::class.simpleName
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
            registerWildTransferIfCaught(request, result, phase)
            resolveWildTransferResult(request, phase)
        }
    }

    /**
     * Promotes a freshly caught wild Pokémon into the pending-transfer set so the
     * shared transfer path considers it. Runs only for a definitive catch result;
     * the per-command template is always cleared to bound memory.
     */
    private fun registerWildTransferIfCaught(
        request: dev.pogoroot.automation.core.automation.ActionRequest,
        result: BridgeEvent.AutomationCommandResult,
        phase: ActionExecutionPhase,
    ) {
        val action = request.action as? AutomationAction.Catch
        val template = caughtWildTemplateByCommand.remove(request.commandId) ?: return
        if (action == null || phase != ActionExecutionPhase.COMPLETED) return
        if (result.catchOutcome != CatchOutcome.CAUGHT) return
        // The transfer target is the storage id assigned on capture; without it
        // (native catch-outcome observer not yet reporting the id) there is
        // nothing to release, so skip until the runtime provides it.
        val pokemonId = result.capturedPokemonId ?: return
        pendingWildTransfers[pokemonId] = StoredPokemon(
            pokemonId = pokemonId,
            speciesId = template.speciesId,
            speciesName = template.speciesName,
            iv = template.iv,
            shiny = template.shiny,
        )
        while (pendingWildTransfers.size > MAX_PENDING_WILD_TRANSFERS) {
            val eldest = pendingWildTransfers.keys.firstOrNull() ?: break
            pendingWildTransfers.remove(eldest)
        }
    }

    private fun resolveWildTransferResult(
        request: dev.pogoroot.automation.core.automation.ActionRequest,
        phase: ActionExecutionPhase,
    ) {
        val action = request.action as? AutomationAction.TransferPokemon ?: return
        // Drop on any definitive result: COMPLETED means released; a rejection or
        // failure (e.g. TRANSFER_POKEMON not advertised yet) is not retried.
        val released = pendingWildTransfers.remove(action.pokemonId)
        if (phase == ActionExecutionPhase.COMPLETED) {
            val label = released?.speciesName?.takeIf { it.isNotBlank() } ?: action.pokemonId
            eventSink.publish(AutomationEvent(AutomationEventType.TRANSFERRED, "Transferred $label"))
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

    /**
     * Captures the wild Pokémon being caught at dispatch time. Encounter catches
     * carry IV/shiny; direct-map (TryCapture) catches expose only species, so IV
     * stays null and TransferPolicy.keepUnknownIv governs whether it is released.
     */
    private fun rememberWildCatchTemplate(
        request: dev.pogoroot.automation.core.automation.ActionRequest,
        snapshot: AutomationSnapshot,
    ) {
        val action = request.action as? AutomationAction.Catch ?: return
        val template = snapshot.encounter
            ?.takeIf { it.encounterId == action.encounterId }
            ?.let { WildCatchTemplate(it.speciesId, it.speciesName, it.iv, it.shiny == true) }
            ?: snapshot.nearby?.spawns
                ?.firstOrNull { it.spawnId == action.encounterId }
                ?.let { WildCatchTemplate(it.speciesId, it.speciesName, iv = null, shiny = false) }
            ?: return
        caughtWildTemplateByCommand[request.commandId] = template
        while (caughtWildTemplateByCommand.size > MAX_PENDING_WILD_TRANSFERS) {
            val eldest = caughtWildTemplateByCommand.keys.firstOrNull() ?: break
            caughtWildTemplateByCommand.remove(eldest)
        }
    }

    /**
     * Folds the freshly caught wild Pokémon awaiting transfer into the runtime
     * storage snapshot (deduped by id) so the shared transfer planner considers
     * them. Once a real box reader lands it becomes just another storage source.
     */
    private fun mergePendingWildTransfers(base: PokemonStorageSnapshot?): PokemonStorageSnapshot? {
        if (pendingWildTransfers.isEmpty()) return base
        val pending = pendingWildTransfers.values.toList()
        if (base == null) {
            return PokemonStorageSnapshot(
                observedAtEpochMs = System.currentTimeMillis(),
                usedSlots = pending.size,
                capacity = pending.size,
                pokemon = pending,
            )
        }
        val merged = (base.pokemon + pending).distinctBy { it.pokemonId }
        return base.copy(pokemon = merged)
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
        const val MAX_PENDING_WILD_TRANSFERS = 64
    }
}
