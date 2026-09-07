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
import dev.pogoroot.automation.core.model.GameLifecycleState

data class PogoRuntimeMetadata(
    val ready: BridgeEvent.RuntimeReady,
    val lastObservationSeq: Long,
    val lastMessageSeq: Long = lastObservationSeq,
    val lastObservationEpochMs: Long? = null,
    val lastObservationElapsedNs: Long? = null,
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
    private var lastError: String? = null
    private val commandResults = ArrayDeque<BridgeEvent.AutomationCommandResult>()

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
        lastError = null
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
        commandResults.clear()
    }

    override fun lifecycleState(): GameLifecycleState = lifecycle

    override fun readNearby(): Result<RawNearbyObservation> = refresh().mapCatching {
        nearby ?: error(lastError ?: "no nearby observation received")
    }

    override fun readEncounter(): Result<RawEncounterObservation?> = refresh().mapCatching {
        if (lifecycle != GameLifecycleState.ENCOUNTER) null
        else encounter ?: error(lastError ?: "no encounter observation received")
    }

    fun drainCommandResults(): Result<List<BridgeEvent.AutomationCommandResult>> = refresh().map {
        buildList {
            while (commandResults.isNotEmpty()) add(commandResults.removeFirst())
        }
    }

    fun refresh(): Result<Unit> = runCatching {
        val current = ready ?: error("runtime is not connected")
        bridge.receiveEvents().getOrThrow().forEach { event ->
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
        }
    }

    private fun consume(event: BridgeEvent) {
        when (event) {
            is BridgeEvent.ObservationEvent -> consumeObservation(event)
            is BridgeEvent.AutomationCommandResult -> commandResults.addLast(event)
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
        if (event.payloadVersion != BridgeProtocol.OBSERVATION_PAYLOAD_VERSION) {
            lastError = "unsupported observation payload version ${event.payloadVersion}"
            return
        }
        event.lifecycleState?.let { state ->
            lifecycle = state
            if (state != GameLifecycleState.ENCOUNTER) encounter = null
        }
        when (event.observationType) {
            ObservationType.LIFECYCLE -> Unit
            ObservationType.NEARBY -> decoder.decodeMapObjects(
                payload = event.payload,
                observedAtEpochMs = event.observedAtEpochMs,
                playerLatitude = event.playerLatitude,
                playerLongitude = event.playerLongitude,
            ).onSuccess {
                nearby = it
                if (event.lifecycleState == null && lifecycle != GameLifecycleState.ENCOUNTER) {
                    lifecycle = GameLifecycleState.OVERWORLD
                }
            }.onFailure {
                nearby = null
                lastError = it.message
            }
            ObservationType.ENCOUNTER -> decoder.decodeEncounter(
                payload = event.payload,
                observedAtEpochMs = event.observedAtEpochMs,
            ).onSuccess {
                encounter = it
                lifecycle = GameLifecycleState.ENCOUNTER
            }.onFailure {
                encounter = null
                lastError = it.message
            }
            ObservationType.FORTS,
            ObservationType.INVENTORY,
            ObservationType.POKEMON_STORAGE,
            -> Unit
        }
    }
}

/**
 * Bridge-backed, asynchronous POGO executor. Version-specific binding and
 * client-owned invocation remain on the injected runtime side.
 */
class BridgeBackedPogoActionExecutor(
    private val bridge: RuntimeBridge,
    private val runtimeReady: () -> BridgeEvent.RuntimeReady?,
    private val allowedBuildFingerprints: Set<String> = emptySet(),
    private val nowEpochMs: () -> Long = System::currentTimeMillis,
    private val nowElapsedNs: () -> Long = System::nanoTime,
    private val timeoutNs: Long = 15_000_000_000L,
) : PogoActionExecutor {
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
        require(allowedBuildFingerprints.contains(ready.buildFingerprint)) {
            "mutation blocked: build fingerprint is not allowlisted"
        }

        val capability = requiredCapability(request.action)
        require(capability == null || capability in capabilities) {
            "mutation blocked: missing capability $capability"
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

    private fun requiredCapability(action: AutomationAction): GameCapability? = when (action) {
        is AutomationAction.MoveTo -> GameCapability.MOVE
        is AutomationAction.OpenEncounter -> GameCapability.OPEN_ENCOUNTER
        is AutomationAction.Catch -> GameCapability.CATCH
        is AutomationAction.Spin -> GameCapability.SPIN
        is AutomationAction.DiscardItem -> GameCapability.DISCARD_ITEM
        is AutomationAction.TransferPokemon -> GameCapability.TRANSFER_POKEMON
        is AutomationAction.UseBerry -> GameCapability.USE_BERRY
        is AutomationAction.Alert -> null
    }

    private fun expectedLifecycle(action: AutomationAction): GameLifecycleState? = when (action) {
        is AutomationAction.Catch,
        is AutomationAction.UseBerry,
        -> GameLifecycleState.ENCOUNTER
        is AutomationAction.Alert -> null
        else -> GameLifecycleState.OVERWORLD
    }
}
