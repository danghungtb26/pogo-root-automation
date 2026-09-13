package dev.pogoroot.automation.runtime.observation

import android.util.Log
import dev.pogoroot.automation.adapter.GameCapability
import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.MapTargetPayloadCodec
import dev.pogoroot.automation.bridge.ObservationType
import dev.pogoroot.automation.bridge.RuntimeAutomationEventPayloadCodec
import dev.pogoroot.automation.bridge.RuntimeBridge
import dev.pogoroot.automation.config.HeadlessAutomationConfig
import dev.pogoroot.automation.core.automation.AutoFortNavigationSignal
import dev.pogoroot.automation.core.automation.AutomationSnapshot
import dev.pogoroot.automation.core.model.MapTargetObservation
import dev.pogoroot.automation.events.AutomationEvent
import dev.pogoroot.automation.events.AutomationEventSink
import dev.pogoroot.automation.events.AutomationEventType
import dev.pogoroot.automation.pogo.BridgePogoRuntimeSource
import dev.pogoroot.automation.pogo.PogoGameAdapter
import dev.pogoroot.automation.pogo.RuntimeThrowDiagnosticPayloadCodec

/**
 * Drains native bridge observations for app-side features that still live in the
 * controller process: toasts, map-tap walk, throw diagnostics, and fort navigation.
 * Catch/spin/discard/transfer mutations are native-owned and are not planned here.
 */
class RuntimeObservationRouter(
    private val bridge: RuntimeBridge,
    private val eventSink: AutomationEventSink = AutomationEventSink { },
    private val onMapTarget: (MapTargetObservation) -> Unit = {},
    private val onNavigationEnabledChanged: (Boolean) -> Unit = {},
    private val onNavigationSnapshot: (AutomationSnapshot) -> Unit = {},
    private val onNavigationSignal: (AutoFortNavigationSignal) -> Unit = {},
    private val onNavigationReset: () -> Unit = {},
) {
    private val source = BridgePogoRuntimeSource(bridge)
    private val adapter = PogoGameAdapter(source, actionExecutor = null)
    private var connected = false
    private var processedObservationSeq = 0L
    private var lastError: String? = null
    private var mapAutomationReady = false

    fun tick(config: HeadlessAutomationConfig): Result<RuntimeObservationTick> = runCatching {
        onNavigationEnabledChanged(config.autoWalkToFort)
        ensureConnected()
        source.refresh().getOrThrow()
        var observationSeq: Long? = null
        mapAutomationReady = GameCapability.READ_NEARBY in adapter.capabilities
        for (event in source.drainEvents().getOrThrow()) {
            when (event) {
                is BridgeEvent.ObservationEvent -> {
                    if (event.messageSeq <= processedObservationSeq) continue
                    if (handleObservation(event, config)) {
                        processedObservationSeq = event.messageSeq
                        observationSeq = event.messageSeq
                    }
                }
                is BridgeEvent.BindingLost -> {
                    val reason = event.reason.ifBlank { "runtime binding lost" }
                    disconnect(reason)
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
        buildTick(observationSeq)
    }.onFailure { error ->
        disconnect("runtime bridge failure: ${error.message ?: error::class.java.simpleName}")
        lastError = error.message
    }

    fun resetForAutomationDisable() {
        onNavigationReset()
        disconnect(clearProcessedSeq = true)
    }

    fun stop() = resetForAutomationDisable()

    private fun ensureConnected() {
        if (connected) return
        source.connect().getOrThrow()
        processedObservationSeq = 0L
        connected = true
        lastError = null
    }

    private fun disconnect(reason: String? = null, clearProcessedSeq: Boolean = false) {
        if (connected) source.disconnect()
        connected = false
        mapAutomationReady = false
        if (clearProcessedSeq) processedObservationSeq = 0L
        if (reason != null) lastError = reason
    }

    private fun handleObservation(
        event: BridgeEvent.ObservationEvent,
        config: HeadlessAutomationConfig,
    ): Boolean = when (event.observationType) {
        ObservationType.AUTOMATION_EVENT -> {
            if (!mapAutomationReady) return true
            RuntimeAutomationEventPayloadCodec.decode(event.payload)
                .onSuccess { decoded ->
                    decoded.toAutoFortNavigationSignal()?.let(onNavigationSignal)
                    decoded.toAutomationEvent()?.let(eventSink::publish)
                        ?: Log.w(
                            LOG_TAG,
                            "ignored unknown runtime automation event " +
                                "wire_type=${decoded.wireType} " +
                                "primary_id=${decoded.primaryId.toULong()}",
                        )
                }
                .onFailure { error ->
                    lastError = "automation event decode: ${error.message}"
                    eventSink.publish(AutomationEvent(AutomationEventType.ERROR, lastError!!))
                }
            true
        }
        ObservationType.MAP_TARGET -> {
            if (GameCapability.READ_MAP_TARGET !in adapter.capabilities) {
                lastError = "map target ignored: runtime did not advertise READ_MAP_TARGET"
            } else {
                MapTargetPayloadCodec.decode(event.payload)
                    .onSuccess(onMapTarget)
                    .onFailure { error -> lastError = "map target decode: ${error.message}" }
            }
            true
        }
        ObservationType.THROW_DIAGNOSTIC -> {
            if (!mapAutomationReady) return true
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
                    eventSink.publish(AutomationEvent(AutomationEventType.ERROR, lastError!!))
                }
            true
        }
        ObservationType.REQUEST_CATCH_SPIN -> {
            if (config.autoWalkToFort && mapAutomationReady) {
                source.selectObservation(event.messageSeq).getOrThrow()
                try {
                    onNavigationSnapshot(adapter.readNavigationSnapshot())
                } finally {
                    source.clearObservationSelection()
                }
            }
            true
        }
        else -> {
            Log.i(
                LOG_TAG,
                "runtime observation consumed type=${event.observationType} seq=${event.messageSeq}",
            )
            true
        }
    }

    private fun buildTick(observationSeq: Long?): RuntimeObservationTick {
        val metadata = source.runtimeMetadata
        return RuntimeObservationTick(
            runtimeSessionId = metadata?.ready?.runtimeSessionId,
            strongIdentityVerified = metadata?.ready?.strongIdentityVerified == true,
            runtimeCapabilities = adapter.capabilities.map(GameCapability::name).toSortedSet(),
            lifecycleState = source.lifecycleState(),
            observationSeq = observationSeq ?: metadata?.lastObservationSeq?.takeIf { it > 0L },
            lastError = lastError,
        )
    }

    private companion object {
        const val LOG_TAG = "PogoRootAutomation"
    }
}
