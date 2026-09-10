package dev.pogoroot.automation.pogo

import dev.pogoroot.automation.adapter.GameCapability
import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.BridgeProtocol
import dev.pogoroot.automation.bridge.ObservationType
import dev.pogoroot.automation.bridge.RuntimeBridge
import dev.pogoroot.automation.core.automation.ActionRequest
import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.AutomationActionResult
import dev.pogoroot.automation.core.automation.BerryType
import dev.pogoroot.automation.core.automation.CatchMode
import dev.pogoroot.automation.core.model.GameLifecycleState

data class PogoRuntimeMetadata(
    val ready: BridgeEvent.RuntimeReady,
    val lastObservationSeq: Long,
    val lastMessageSeq: Long = lastObservationSeq,
    val lastObservationEpochMs: Long? = null,
    val lastObservationElapsedNs: Long? = null,
)

private data class CachedRuntimeState(
    val lifecycle: GameLifecycleState,
    val nearby: RawNearbyObservation?,
    val encounter: RawEncounterObservation?,
    val forts: RawFortObservation?,
    val inventory: RawInventoryObservation?,
)

/**
 * Controller-side POGO source. The runtime only tags/copies payloads; this
 * class is the sole owner of protobuf decoding and stable model mapping.
 */
class BridgePogoRuntimeSource(
    private val bridge: RuntimeBridge,
    private val decoder: PogoProtoDecoder = PogoProtoDecoder(),
) : PogoRuntimeSource {
    private var ready: BridgeEvent.RuntimeReady? = null
    private var lastMessageSeq = 0L
    private var lastObservationSeq = 0L
    private var lastObservationEpochMs: Long? = null
    private var lastObservationElapsedNs: Long? = null
    private var lifecycle = GameLifecycleState.DISCONNECTED
    private var nearby: RawNearbyObservation? = null
    private var encounter: RawEncounterObservation? = null
    private var forts: RawFortObservation? = null
    private var inventory: RawInventoryObservation? = null
    private var lastError: String? = null
    private val pendingEvents = ArrayDeque<BridgeEvent>()
    private val observationStates = LinkedHashMap<Long, CachedRuntimeState>()
    private var selectedObservationState: CachedRuntimeState? = null

    override val capabilities: Set<GameCapability>
        get() = ready?.capabilities.orEmpty().mapNotNull { raw ->
            runCatching { GameCapability.valueOf(raw) }.getOrNull()
        }.toSet()

    override val runtimeMetadata: PogoRuntimeMetadata?
        get() = ready?.let {
            PogoRuntimeMetadata(
                ready = it,
                lastObservationSeq = lastObservationSeq,
                lastMessageSeq = lastMessageSeq,
                lastObservationEpochMs = lastObservationEpochMs,
                lastObservationElapsedNs = lastObservationElapsedNs,
            )
        }

    override fun connect(): Result<Unit> = runCatching {
        val runtime = bridge.connect().getOrThrow()
        require(runtime.protocolVersion == BridgeProtocol.VERSION) { "bridge protocol mismatch" }
        ready = runtime
        lastMessageSeq = runtime.messageSeq
        lastObservationSeq = 0L
        lastObservationEpochMs = null
        lastObservationElapsedNs = null
        lifecycle = GameLifecycleState.STARTING
        nearby = null
        encounter = null
        forts = null
        inventory = null
        lastError = null
        pendingEvents.clear()
        observationStates.clear()
        selectedObservationState = null
    }

    override fun disconnect() {
        bridge.disconnect()
        ready = null
        lastMessageSeq = 0L
        lastObservationSeq = 0L
        lastObservationEpochMs = null
        lastObservationElapsedNs = null
        lifecycle = GameLifecycleState.DISCONNECTED
        nearby = null
        encounter = null
        forts = null
        inventory = null
        pendingEvents.clear()
        observationStates.clear()
        selectedObservationState = null
    }

    override fun lifecycleState(): GameLifecycleState = selectedObservationState?.lifecycle ?: lifecycle

    override fun readNearby(): Result<RawNearbyObservation> = runCatching {
        val state = selectedObservationState
        state?.nearby ?: if (state == null) {
            nearby ?: error(lastError ?: "no nearby observation received")
        } else {
            error(lastError ?: "no nearby observation in selected observation state")
        }
    }

    override fun readEncounter(): Result<RawEncounterObservation?> = runCatching {
        val state = selectedObservationState
        if (state != null) {
            if (state.lifecycle != GameLifecycleState.ENCOUNTER) null
            else state.encounter ?: error(lastError ?: "no encounter observation in selected observation state")
        } else if (lifecycle != GameLifecycleState.ENCOUNTER) {
            null
        } else {
            encounter ?: error(lastError ?: "no encounter observation received")
        }
    }

    override fun readForts(): Result<RawFortObservation> = runCatching {
        val state = selectedObservationState
        if (state != null) {
            state.forts ?: error(lastError ?: "no forts observation in selected observation state")
        } else {
            forts ?: error(lastError ?: "no forts observation received")
        }
    }

    override fun readInventory(): Result<RawInventoryObservation> = runCatching {
        val state = selectedObservationState
        if (state != null) {
            state.inventory ?: error(lastError ?: "no inventory observation in selected observation state")
        } else {
            inventory ?: error(lastError ?: "no inventory observation received")
        }
    }

    fun drainEvents(): Result<List<BridgeEvent>> = runCatching {
        buildList {
            while (pendingEvents.isNotEmpty()) add(pendingEvents.removeFirst())
        }
    }

    fun selectObservation(observationSeq: Long): Result<Unit> = runCatching {
        selectedObservationState = observationStates[observationSeq]
            ?: error("observation state $observationSeq is no longer cached")
    }

    fun clearObservationSelection() {
        selectedObservationState = null
    }

    fun refresh(): Result<Unit> = runCatching {
        val current = ready ?: error("runtime is not connected")
        bridge.receiveEvents().getOrThrow()
            .sortedBy { it.messageSeq ?: Long.MAX_VALUE }
            .forEach { event ->
                if (event.protocolVersion != BridgeProtocol.VERSION) {
                    error("bridge protocol mismatch")
                }
                val eventSession = event.runtimeSessionId
                if (eventSession != null && eventSession != current.runtimeSessionId) return@forEach
                if (!matchesRuntimeIdentity(event, current)) {
                    lastError = "runtime identity mismatch"
                    return@forEach
                }
                val sequence = event.messageSeq
                if (sequence != null && sequence <= lastMessageSeq) return@forEach
                if (sequence != null) lastMessageSeq = sequence
                consume(event)
                when (event) {
                    is BridgeEvent.ObservationEvent,
                    is BridgeEvent.AutomationCommandResult,
                    is BridgeEvent.BindingLost,
                    is BridgeEvent.RuntimeError,
                    -> pendingEvents.addLast(event)
                    else -> Unit
                }
            }
    }

    private fun consume(event: BridgeEvent) {
        when (event) {
            is BridgeEvent.RuntimeReady -> {
                // The broker first announces a conservative probe-only
                // identity, then may publish a stronger identity/capability
                // update after its post-init managed binding check.
                ready = event
                lifecycle = GameLifecycleState.STARTING
                lastError = null
            }
            is BridgeEvent.ObservationEvent -> consumeObservation(event)
            is BridgeEvent.BindingLost -> {
                lifecycle = GameLifecycleState.DISCONNECTED
                lastError = event.reason
            }
            is BridgeEvent.RuntimeError -> {
                lifecycle = GameLifecycleState.ERROR
                lastError = "${event.code}: ${event.message}"
            }
            is BridgeEvent.RuntimeStatus -> lifecycle = event.lifecycleState
            else -> Unit
        }
    }

    private fun matchesRuntimeIdentity(
        event: BridgeEvent,
        current: BridgeEvent.RuntimeReady,
    ): Boolean = when (event) {
        is BridgeEvent.ObservationEvent -> event.pid == current.pid &&
            event.processName == current.processName &&
            event.packageName == current.packageName &&
            event.buildFingerprint == current.buildFingerprint
        is BridgeEvent.BindingLost -> event.pid == current.pid && event.processName == current.processName
        else -> true
    }

    private fun consumeObservation(event: BridgeEvent.ObservationEvent) {
        lastObservationSeq = event.messageSeq
        lastObservationEpochMs = event.observedAtEpochMs
        lastObservationElapsedNs = event.observedAtElapsedNs
        val structuredEncounter = event.observationType == ObservationType.ENCOUNTER &&
            event.payloadVersion == BridgeProtocol.RUNTIME_ENCOUNTER_PAYLOAD_VERSION
        val structuredNearby = event.observationType == ObservationType.NEARBY &&
            event.payloadVersion == BridgeProtocol.RUNTIME_NEARBY_PAYLOAD_VERSION
        val structuredForts = event.observationType == ObservationType.FORTS &&
            event.payloadVersion == BridgeProtocol.RUNTIME_FORTS_PAYLOAD_VERSION
        val structuredInventory = event.observationType == ObservationType.INVENTORY &&
            event.payloadVersion == BridgeProtocol.RUNTIME_INVENTORY_PAYLOAD_VERSION
        if (event.payloadVersion != BridgeProtocol.OBSERVATION_PAYLOAD_VERSION &&
            !structuredEncounter && !structuredNearby && !structuredForts && !structuredInventory) {
            lastError = "unsupported observation payload version ${event.payloadVersion}"
            return
        }
        event.lifecycleState?.let { state ->
            lifecycle = state
            if (state != GameLifecycleState.ENCOUNTER) encounter = null
        }
        when (event.observationType) {
            ObservationType.LIFECYCLE -> Unit
            ObservationType.NEARBY -> (if (structuredNearby) {
                RuntimeNearbyPayloadCodec.decode(
                    payload = event.payload,
                    observedAtEpochMs = event.observedAtEpochMs,
                )
            } else {
                decoder.decodeMapObjects(
                    payload = event.payload,
                    observedAtEpochMs = event.observedAtEpochMs,
                    playerLatitude = event.playerLatitude,
                    playerLongitude = event.playerLongitude,
                )
            }).onSuccess {
                nearby = it
                if (event.lifecycleState == null && lifecycle != GameLifecycleState.ENCOUNTER) {
                    lifecycle = GameLifecycleState.OVERWORLD
                }
            }.onFailure {
                nearby = null
                lastError = it.message
            }
            ObservationType.ENCOUNTER -> (if (structuredEncounter) {
                RuntimeEncounterPayloadCodec.decode(
                    payload = event.payload,
                    observedAtEpochMs = event.observedAtEpochMs,
                )
            } else {
                decoder.decodeEncounter(
                    payload = event.payload,
                    observedAtEpochMs = event.observedAtEpochMs,
                )
            }).onSuccess {
                encounter = it
                lifecycle = GameLifecycleState.ENCOUNTER
            }.onFailure {
                encounter = null
                lastError = it.message
            }
            ObservationType.FORTS -> if (
                event.payloadVersion == BridgeProtocol.RUNTIME_FORTS_PAYLOAD_VERSION
            ) {
                RuntimeFortsPayloadCodec.decode(
                    payload = event.payload,
                    observedAtEpochMs = event.observedAtEpochMs,
                ).onSuccess {
                    forts = it
                }.onFailure {
                    forts = null
                    lastError = it.message
                }
            }
            ObservationType.INVENTORY -> if (structuredInventory) {
                RuntimeInventoryPayloadCodec.decode(
                    payload = event.payload,
                    observedAtEpochMs = event.observedAtEpochMs,
                ).onSuccess {
                    inventory = it
                }.onFailure {
                    inventory = null
                    lastError = it.message
                }
            }
            ObservationType.POKEMON_STORAGE,
            ObservationType.MAP_TARGET,
            ObservationType.THROW_DIAGNOSTIC,
            -> Unit
        }
        observationStates[event.messageSeq] =
            CachedRuntimeState(lifecycle, nearby, encounter, forts, inventory)
        while (observationStates.size > MAX_OBSERVATION_STATES) {
            observationStates.remove(observationStates.entries.first().key)
        }
    }

    private companion object {
        const val MAX_OBSERVATION_STATES = 64
    }
}

/**
 * Bridge-backed, asynchronous POGO executor. Version-specific binding and
 * client-owned invocation remain on the injected runtime side.
 */
class BridgeBackedPogoActionExecutor(
    private val bridge: RuntimeBridge,
    private val runtimeReady: () -> BridgeEvent.RuntimeReady?,
    allowedBuildFingerprints: Set<String> = emptySet(),
    private val nowEpochMs: () -> Long = System::currentTimeMillis,
    private val nowElapsedNs: () -> Long = System::nanoTime,
    private val timeoutNs: Long = 15_000_000_000L,
) : PogoActionExecutor {
    @Volatile private var currentAllowedBuildFingerprints = allowedBuildFingerprints.toSet()

    fun updateAllowedBuildFingerprints(fingerprints: Set<String>) {
        currentAllowedBuildFingerprints = fingerprints.toSet()
    }

    override val capabilities: Set<GameCapability>
        get() = runtimeReady()?.capabilities.orEmpty().mapNotNull { raw ->
            runCatching { GameCapability.valueOf(raw) }.getOrNull()
        }.toSet()

    override fun execute(action: AutomationAction): Result<AutomationActionResult> {
        val ready = runtimeReady() ?: return Result.failure(IllegalStateException("runtime is not ready"))
        val request = ActionRequest.create(
            runtimeSessionId = ready.runtimeSessionId,
            action = action,
            basedOnObservationSeq = ready.messageSeq,
            expectedLifecycle = expectedLifecycle(action),
            createdAtEpochMs = nowEpochMs(),
            createdAtElapsedNs = nowElapsedNs(),
            timeoutNs = timeoutNs,
            pid = ready.pid,
            processName = ready.processName,
            packageName = ready.packageName,
            buildFingerprint = ready.buildFingerprint,
        )
        return submit(request).map {
            AutomationActionResult(action = action, success = true, message = "accepted")
        }
    }

    override fun submit(request: ActionRequest): Result<Unit> = runCatching {
        val ready = runtimeReady() ?: error("runtime is not ready")
        require(bridge.connected) { "runtime bridge is disconnected" }
        require(request.runtimeSessionId == ready.runtimeSessionId) { "action belongs to an old runtime session" }
        require(request.pid == null || request.pid == ready.pid) { "action PID does not match runtime" }
        require(request.packageName == null || request.packageName == ready.packageName) {
            "action package does not match runtime"
        }
        require(request.buildFingerprint == null || request.buildFingerprint == ready.buildFingerprint) {
            "action build fingerprint does not match runtime"
        }
        require(ready.strongIdentityVerified) {
            "mutation blocked: runtime identity is not strongly verified"
        }
        require(currentAllowedBuildFingerprints.contains(ready.buildFingerprint)) {
            "mutation blocked: build fingerprint is not allowlisted"
        }

        val missingCapabilities = requiredCapabilities(request.action).filterNot { it in capabilities }
        require(missingCapabilities.isEmpty()) {
            "mutation blocked: missing capability ${missingCapabilities.joinToString(",")}"
        }

        bridge.send(
            BridgeEvent.AutomationCommand(
                runtimeSessionId = ready.runtimeSessionId,
                commandId = request.commandId,
                action = request.action,
                basedOnObservationSeq = request.basedOnObservationSeq,
                expectedLifecycle = request.expectedLifecycle,
                expiresAtElapsedNs = request.expiresAtElapsedNs,
                pid = ready.pid,
                processName = ready.processName,
                packageName = ready.packageName,
                buildFingerprint = ready.buildFingerprint,
            ),
        ).getOrThrow()
    }

    private fun requiredCapabilities(action: AutomationAction): Set<GameCapability> = when (action) {
        is AutomationAction.MoveTo -> setOf(GameCapability.MOVE)
        is AutomationAction.OpenEncounter -> setOf(GameCapability.OPEN_ENCOUNTER)
        is AutomationAction.Catch -> buildSet {
            if (action.mode == CatchMode.DIRECT_MAP) {
                add(GameCapability.DIRECT_CATCH)
            } else {
                add(if (action.closePreviewAfterCaught) {
                    GameCapability.CATCH_AND_CLOSE_PREVIEW
                } else {
                    GameCapability.CATCH
                })
                if (!action.throwProfile.isDefault) {
                    add(GameCapability.THROW_CONTROL)
                    if (action.throwProfile.requiresStructuredOutcome) {
                        add(GameCapability.OBSERVE_THROW_OUTCOME)
                    }
                }
                if (action.throwProfile.encounterMode == dev.pogoroot.automation.core.automation.EncounterMode.AR_PLUS) {
                    add(GameCapability.AR_ENCOUNTER)
                }
            }
        }
        is AutomationAction.TakeEncounterSnapshot -> buildSet {
            add(GameCapability.SNAPSHOT_DURING_ENCOUNTER)
            if (action.encounterMode == dev.pogoroot.automation.core.automation.EncounterMode.AR_PLUS) {
                add(GameCapability.AR_ENCOUNTER)
            }
        }
        is AutomationAction.Spin -> setOf(GameCapability.SPIN)
        is AutomationAction.DiscardItem -> setOf(GameCapability.DISCARD_ITEM)
        is AutomationAction.TransferPokemon -> setOf(GameCapability.TRANSFER_POKEMON)
        is AutomationAction.UseBerry -> setOf(GameCapability.USE_BERRY)
        is AutomationAction.Alert -> emptySet()
    }

    private fun expectedLifecycle(action: AutomationAction): GameLifecycleState? = when (action) {
        is AutomationAction.Catch -> if (action.mode == CatchMode.DIRECT_MAP) {
            GameLifecycleState.OVERWORLD
        } else {
            GameLifecycleState.ENCOUNTER
        }
        is AutomationAction.TakeEncounterSnapshot,
        is AutomationAction.UseBerry,
        -> GameLifecycleState.ENCOUNTER
        is AutomationAction.Alert -> null
        else -> GameLifecycleState.OVERWORLD
    }
}
