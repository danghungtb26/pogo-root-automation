package dev.pogoroot.automation.runtime.observation

import android.util.Log
import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.BridgeProtocol
import dev.pogoroot.automation.bridge.MapTargetPayloadCodec
import dev.pogoroot.automation.bridge.ObservationType
import dev.pogoroot.automation.bridge.RuntimeAutomationEventPayloadCodec
import dev.pogoroot.automation.bridge.RuntimeNavigationPayload
import dev.pogoroot.automation.bridge.RuntimeNavigationPayloadCodec
import dev.pogoroot.automation.bridge.RuntimeThrowDiagnosticPayloadCodec
import dev.pogoroot.automation.core.model.MapTargetObservation
import dev.pogoroot.automation.events.AutomationEvent
import dev.pogoroot.automation.events.AutomationEventSink
import dev.pogoroot.automation.events.AutomationEventType
import dev.pogoroot.automation.root.RuntimeUiClient
import dev.pogoroot.automation.runtime.RuntimeUiEvent
import dev.pogoroot.automation.runtime.RuntimeUiState
import dev.pogoroot.automation.runtime.RuntimeUiStateStore

/** Routes native UI/status messages without parsing game-state observations. */
class RuntimeUiEventRouter(
    private val bridge: RuntimeUiClient,
    private val stateStore: RuntimeUiStateStore = RuntimeUiStateStore(),
    private val eventSink: AutomationEventSink = AutomationEventSink { },
    private val onMapTarget: (MapTargetObservation) -> Unit = {},
    private val onNavigation: (RuntimeNavigationPayload, Long) -> Unit = { _, _ -> },
    private val onNavigationReset: () -> Unit = {},
    private val nowNanos: () -> Long = System::nanoTime,
) {
    private var runtimeSessionId: String? = null
    private var processedObservationSeq = 0L
    private var lastStatusElapsedNs = 0L

    fun tick(): Result<RuntimeUiState> = runCatching {
        ensureSession()
        for (event in bridge.receiveEvents().getOrThrow()) consume(event)
        stateStore.snapshot()
    }.onFailure { error ->
        reset("runtime bridge failure: ${error.message ?: error::class.java.simpleName}")
    }

    fun snapshot(): RuntimeUiState = stateStore.snapshot()

    fun reset(reason: String = "runtime disconnected") {
        onNavigationReset()
        runtimeSessionId = null
        processedObservationSeq = 0L
        lastStatusElapsedNs = 0L
        stateStore.reset(reason)
    }

    fun stop() {
        reset()
        bridge.disconnect()
    }

    private fun ensureSession() {
        val ready = bridge.connect().getOrThrow()
        if (runtimeSessionId != ready.runtimeSessionId) {
            onNavigationReset()
            runtimeSessionId = ready.runtimeSessionId
            processedObservationSeq = 0L
            lastStatusElapsedNs = 0L
            stateStore.accept(RuntimeUiEvent.Ready(ready))
        }
    }

    private fun consume(event: BridgeEvent) {
        when (event) {
            is BridgeEvent.RuntimeUiStatus -> consumeStatus(event.status)
            is BridgeEvent.ObservationEvent -> consumeObservation(event)
            is BridgeEvent.BindingLost -> reset(event.reason.ifBlank { "runtime binding lost" })
            is BridgeEvent.RuntimeError -> acceptError("${event.code}: ${event.message}")
            else -> Unit
        }
    }

    private fun consumeStatus(status: dev.pogoroot.automation.bridge.RuntimeUiStatus) {
        if (!identityMatches(status.runtimeSessionId, status.pid, status.processName,
                status.packageName, status.buildFingerprint)) return
        if (status.observedAtElapsedNs <= lastStatusElapsedNs) return
        lastStatusElapsedNs = status.observedAtElapsedNs
        stateStore.accept(RuntimeUiEvent.Status(status))
    }

    private fun consumeObservation(event: BridgeEvent.ObservationEvent) {
        if (!identityMatches(
                event.runtimeSessionId,
                event.pid,
                event.processName,
                event.packageName,
                event.buildFingerprint,
            ) || event.messageSeq <= processedObservationSeq || !isFresh(event.observedAtElapsedNs) ||
            event.payload.size > BridgeProtocol.HARD_MESSAGE_BYTES
        ) return
        processedObservationSeq = event.messageSeq
        when (event.observationType) {
            ObservationType.AUTOMATION_EVENT -> decodeAutomationEvent(event)
            ObservationType.MAP_TARGET -> decodeMapTarget(event)
            ObservationType.NAVIGATION -> decodeNavigation(event)
            ObservationType.THROW_DIAGNOSTIC -> decodeThrowDiagnostic(event)
            else -> Log.i(LOG_TAG, "ignored raw runtime observation type=${event.observationType}")
        }
    }

    private fun decodeAutomationEvent(event: BridgeEvent.ObservationEvent) {
        RuntimeAutomationEventPayloadCodec.decode(event.payload, event.payloadVersion)
            .onSuccess { decoded ->
                decoded.toAutomationEvent()?.let {
                    stateStore.accept(RuntimeUiEvent.Automation(it))
                    eventSink.publish(it)
                }
            }
            .onFailure { acceptError("automation event decode: ${it.message}") }
    }

    private fun decodeMapTarget(event: BridgeEvent.ObservationEvent) {
        if (!capabilities().contains("READ_MAP_TARGET")) return
        MapTargetPayloadCodec.decode(event.payload)
            .onSuccess {
                stateStore.accept(RuntimeUiEvent.MapTarget(it))
                onMapTarget(it)
            }
            .onFailure { acceptError("map target decode: ${it.message}") }
    }

    private fun decodeNavigation(event: BridgeEvent.ObservationEvent) {
        if (event.payloadVersion != RuntimeNavigationPayloadCodec.VERSION) {
            onNavigationReset()
            return
        }
        RuntimeNavigationPayloadCodec.decode(event.payload)
            .onSuccess {
                stateStore.accept(RuntimeUiEvent.Navigation(it, event.observedAtElapsedNs))
                onNavigation(it, event.observedAtElapsedNs)
            }
            .onFailure {
                onNavigationReset()
                acceptError("navigation decode: ${it.message}")
            }
    }

    private fun decodeThrowDiagnostic(event: BridgeEvent.ObservationEvent) {
        RuntimeThrowDiagnosticPayloadCodec.decode(event.payload)
            .onSuccess {
                stateStore.accept(RuntimeUiEvent.ThrowDiagnostic(it))
                eventSink.publish(AutomationEvent(AutomationEventType.INFO, it.message()))
            }
            .onFailure { acceptError("throw diagnostic decode: ${it.message}") }
    }

    private fun identityMatches(
        sessionId: String,
        pid: Int,
        processName: String,
        packageName: String,
        fingerprint: String,
    ): Boolean {
        val ready = stateStore.snapshot().ready ?: return false
        return sessionId == runtimeSessionId && pid == ready.pid &&
            processName == ready.processName && packageName == ready.packageName &&
            fingerprint == ready.buildFingerprint
    }

    private fun capabilities(): Set<String> = stateStore.snapshot().nativeStatus?.capabilities
        ?: stateStore.snapshot().ready?.capabilities.orEmpty()

    private fun isFresh(observedAtElapsedNs: Long): Boolean {
        val age = nowNanos() - observedAtElapsedNs
        return observedAtElapsedNs > 0L && age in -CLOCK_SKEW_NS..OBSERVATION_MAX_AGE_NS
    }

    private fun acceptError(message: String) {
        stateStore.accept(RuntimeUiEvent.Error(message))
        eventSink.publish(AutomationEvent(AutomationEventType.ERROR, message))
    }

    private companion object {
        const val LOG_TAG = "PogoRootAutomation"
        const val CLOCK_SKEW_NS = 1_000_000_000L
        const val OBSERVATION_MAX_AGE_NS = 30_000_000_000L
    }
}
