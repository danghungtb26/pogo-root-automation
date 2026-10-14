package dev.pogoroot.automation.root

import android.os.Handler
import android.os.Looper
import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.CommandPhase
import dev.pogoroot.automation.bridge.RuntimeDesiredState
import dev.pogoroot.automation.bridge.RuntimeBootstrapStatus
import dev.pogoroot.automation.bridge.RuntimeUiStatus

data class DesiredStateReceipt(
    val runtimeSessionId: String,
    val requestId: String,
    val phase: CommandPhase,
    val message: String?,
)

/**
 * UI-facing transport facade. It owns no readiness policy and never sends
 * START/DIAGNOSTIC implicitly; native status is the only applied/ready source.
 */
class RuntimeUiClient(
    private val transport: RuntimeBridgeClient = RuntimeBridgeClient(),
    private val onStatus: (RuntimeUiStatus) -> Unit = {},
    private val onEvent: (BridgeEvent) -> Unit = {},
) {
    private val mainHandler = Handler(Looper.getMainLooper())
    @Volatile private var latestDesiredState: RuntimeDesiredState? = null
    @Volatile private var activeSessionId: String? = null
    @Volatile private var latestStatus: RuntimeUiStatus? = null

    init {
        bindTransportListeners()
    }

    val connected: Boolean
        get() = transport.connected

    fun currentStatus(): RuntimeUiStatus? = latestStatus

    fun rememberDesiredState(state: RuntimeDesiredState) {
        latestDesiredState = state
    }

    fun connect(): Result<RuntimeBootstrapStatus> = runCatching {
        bindTransportListeners()
        val wasConnected = transport.connected
        val session = transport.connectSession().getOrThrow()
        val sessionChanged = activeSessionId != session.sessionId
        if (sessionChanged) {
            activeSessionId = null
            latestStatus = null
            latestDesiredState = latestDesiredState?.copy(catchSpinArmed = false)
        }
        activeSessionId = session.sessionId
        if (!wasConnected || sessionChanged) latestDesiredState?.let { desired ->
            transport.submitDesiredState(desired).getOrThrow()
        }
        session
    }.onFailure {
        activeSessionId = null
        latestStatus = null
    }

    fun submitDesiredState(state: RuntimeDesiredState): Result<DesiredStateReceipt> = runCatching {
        val session = transport.connectSession().getOrThrow()
        val safeState = if (activeSessionId == session.sessionId) state else {
            state.copy(catchSpinArmed = false)
        }
        rememberDesiredState(safeState)
        activeSessionId = session.sessionId
        transport.submitDesiredState(safeState).getOrThrow()
    }.onFailure {
        activeSessionId = null
        latestStatus = null
    }

    /** Explicit user/API diagnostic request; it never starts or configures runtime modules. */
    fun requestRuntimeDiagnostic(): Result<Unit> = transport.requestRuntimeDiagnostic()

    fun receiveEvents(): Result<List<BridgeEvent>> = transport.receiveEvents()

    fun disconnect() {
        activeSessionId = null
        latestStatus = null
        transport.disconnect()
    }

    private fun bindTransportListeners() {
        transport.setRuntimeEventListener { event ->
            mainHandler.post {
                if (event is BridgeEvent.RuntimeUiStatus) {
                    latestStatus = event.status
                    onStatus(event.status)
                }
                onEvent(event)
            }
        }
    }
}
