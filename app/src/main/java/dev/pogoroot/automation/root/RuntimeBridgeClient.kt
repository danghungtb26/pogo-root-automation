package dev.pogoroot.automation.root

import android.net.LocalSocket
import android.net.LocalSocketAddress
import android.os.Process
import android.util.Log
import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.BridgeFrame
import dev.pogoroot.automation.bridge.BridgeFrameCodec
import dev.pogoroot.automation.bridge.BridgeMessageType
import dev.pogoroot.automation.bridge.BridgePayloadCodec
import dev.pogoroot.automation.bridge.BridgeProtocol
import dev.pogoroot.automation.bridge.CommandPhase
import dev.pogoroot.automation.bridge.RuntimeControlAction
import dev.pogoroot.automation.bridge.RuntimeControlPayloadCodec
import dev.pogoroot.automation.bridge.RuntimeControlRequest
import dev.pogoroot.automation.bridge.RuntimeCatchSpinConfig
import dev.pogoroot.automation.bridge.RuntimeCatchSpinConfigPayloadCodec
import dev.pogoroot.automation.bridge.RuntimeCatchSpinConfigRequest
import dev.pogoroot.automation.bridge.RuntimeDiscardConfig
import dev.pogoroot.automation.bridge.RuntimeDiscardConfigPayloadCodec
import dev.pogoroot.automation.bridge.RuntimeDiscardConfigRequest
import dev.pogoroot.automation.bridge.RuntimeDesiredState
import dev.pogoroot.automation.bridge.RuntimeDesiredStatePayloadCodec
import dev.pogoroot.automation.bridge.RuntimeDesiredStateRequest
import dev.pogoroot.automation.bridge.RuntimeTransferConfig
import dev.pogoroot.automation.bridge.RuntimeTransferConfigPayloadCodec
import dev.pogoroot.automation.bridge.RuntimeTransferConfigRequest
import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicLong

data class RuntimeModuleLoadStatus(
    val runtimeSessionId: String,
    val module: RuntimeFeatureModule,
    val loaded: Boolean,
    val errorCode: String?,
    val message: String?,
)

class RuntimeBridgeClient(
    private val socketName: String = DEFAULT_SOCKET_NAME,
    private val connectTimeoutMs: Long = 3_000L,
    private val rootShell: RootShell = ProcessRootShell(),
    private val onModuleLoadStatus: (RuntimeModuleLoadStatus) -> Unit = {},
) : RuntimeControlBridge {
    private val outgoingSeq = AtomicLong(0L)
    private val events = ConcurrentLinkedQueue<BridgeEvent>()
    private val controlResults = ConcurrentLinkedQueue<BridgeEvent.AutomationCommandResult>()
    private val pendingControlIds = ConcurrentHashMap.newKeySet<String>()
    private val publishedModuleLoadStatuses = ConcurrentHashMap.newKeySet<String>()
    private val outputLock = Any()
    @Volatile private var socket: LocalSocket? = null
    @Volatile private var runtimeReady: BridgeEvent.RuntimeReady? = null
    @Volatile private var readerError: Throwable? = null
    @Volatile private var readerExecutor: ExecutorService? = null
    @Volatile private var runtimeReadyListener: ((BridgeEvent.RuntimeReady) -> Unit)? = null
    @Volatile private var runtimeEventListener: ((BridgeEvent) -> Unit)? = null

    override val connected: Boolean
        get() = socket?.isConnected == true && readerError == null

    override fun currentRuntimeReady(): BridgeEvent.RuntimeReady? =
        runtimeReady?.takeIf { connected }

    fun setRuntimeReadyListener(listener: ((BridgeEvent.RuntimeReady) -> Unit)?) {
        runtimeReadyListener = listener
    }

    fun setRuntimeEventListener(listener: ((BridgeEvent) -> Unit)?) {
        runtimeEventListener = listener
    }

    override fun connect(): Result<BridgeEvent.RuntimeReady> {
        currentRuntimeReady()?.let { return Result.success(it) }
        return runCatching {
            disconnect()
            check(registerControllerUid()) { "cannot register controller UID with runtime broker" }
            val next = LocalSocket()
            next.connect(
                LocalSocketAddress(
                    socketName,
                    LocalSocketAddress.Namespace.ABSTRACT,
                ),
            )
            socket = next
            readerError = null
            val executor = Executors.newSingleThreadExecutor()
            readerExecutor = executor
            executor.execute { readLoop(next) }

            val deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(connectTimeoutMs)
            while (System.nanoTime() < deadline) {
                val ready = events.findAndRemove { it is BridgeEvent.RuntimeReady }
                if (ready is BridgeEvent.RuntimeReady) {
                    runtimeReady = ready
                    Log.i(
                        LOG_TAG,
                        "runtime bridge connected session=${ready.runtimeSessionId} " +
                            "pid=${ready.pid} process=${ready.processName}",
                    )
                    return@runCatching ready
                }
                readerError?.let { throw IllegalStateException("runtime bridge reader failed", it) }
                Thread.sleep(10L)
            }
            error("runtime ready timeout")
        }.onFailure {
            disconnect()
        }
    }

    fun receiveEvents(): Result<List<BridgeEvent>> = runCatching {
        readerError?.let { throw IllegalStateException("runtime bridge reader failed", it) }
        buildList {
            while (true) add(events.poll() ?: break)
        }
    }

    override fun startRuntime(): Result<Unit> {
        invalidateManagedReadiness()
        return requestRuntimeControl(RuntimeControlAction.START).map { Unit }
    }

    override fun stopRuntime(): Result<Unit> =
        requestRuntimeControl(RuntimeControlAction.STOP).map { Unit }

    override fun requestRuntimeDiagnostic(): Result<Unit> =
        requestRuntimeControl(RuntimeControlAction.DIAGNOSTIC).map { Unit }

    fun submitDesiredState(state: RuntimeDesiredState): Result<DesiredStateReceipt> = runCatching {
        val ready = currentRuntimeReady() ?: connect().getOrThrow()
        val requestId = "runtime-desired-state-${state.configRevision}-${System.nanoTime()}"
        val request = RuntimeDesiredStateRequest(
            runtimeSessionId = ready.runtimeSessionId,
            requestId = requestId,
            state = state,
            expiresAtElapsedNs = System.nanoTime() + CONTROL_TIMEOUT_NS,
            pid = ready.pid,
            processName = ready.processName,
            packageName = ready.packageName,
            buildFingerprint = ready.buildFingerprint,
        )
        val result = awaitAcceptedResult(requestId) {
            sendPayload(
                messageType = BridgeMessageType.COMMAND,
                payload = RuntimeDesiredStatePayloadCodec.encode(request).getOrThrow(),
            )
        }.getOrThrow()
        DesiredStateReceipt(
            runtimeSessionId = ready.runtimeSessionId,
            requestId = requestId,
            phase = result.phase,
            message = result.message,
        )
    }

    override fun setCatchSpinConfig(config: RuntimeCatchSpinConfig): Result<Unit> = runCatching {
        val ready = currentRuntimeReady() ?: connect().getOrThrow()
        val requestId = "runtime-catch-spin-config-${config.configRevision}-${System.nanoTime()}"
        val request = RuntimeCatchSpinConfigRequest(
            runtimeSessionId = ready.runtimeSessionId,
            requestId = requestId,
            config = config,
            expiresAtElapsedNs = System.nanoTime() + CONTROL_TIMEOUT_NS,
            pid = ready.pid,
            processName = ready.processName,
            packageName = ready.packageName,
            buildFingerprint = ready.buildFingerprint,
        )
        val result = awaitControlResult(requestId) {
            sendPayload(
                messageType = BridgeMessageType.COMMAND,
                payload = RuntimeCatchSpinConfigPayloadCodec.encode(request).getOrThrow(),
            )
        }.getOrThrow()
        Log.i(
            LOG_TAG,
            "runtime catch_spin config acknowledged revision=${config.configRevision} " +
                "phase=${result.phase} message=${result.message}",
        )
    }

    override fun setTransferConfig(config: RuntimeTransferConfig): Result<Unit> = runCatching {
        val ready = currentRuntimeReady() ?: connect().getOrThrow()
        val requestId = "runtime-transfer-config-${config.configRevision}-${System.nanoTime()}"
        val request = RuntimeTransferConfigRequest(
            runtimeSessionId = ready.runtimeSessionId,
            requestId = requestId,
            config = config,
            expiresAtElapsedNs = System.nanoTime() + CONTROL_TIMEOUT_NS,
            pid = ready.pid,
            processName = ready.processName,
            packageName = ready.packageName,
            buildFingerprint = ready.buildFingerprint,
        )
        val result = awaitControlResult(requestId) {
            sendPayload(
                messageType = BridgeMessageType.COMMAND,
                payload = RuntimeTransferConfigPayloadCodec.encode(request).getOrThrow(),
            )
        }.getOrThrow()
        Log.i(
            LOG_TAG,
            "runtime transfer config acknowledged revision=${config.configRevision} " +
                "phase=${result.phase} message=${result.message}",
        )
    }

    override fun setDiscardConfig(config: RuntimeDiscardConfig): Result<Unit> = runCatching {
        val ready = currentRuntimeReady() ?: connect().getOrThrow()
        val requestId = "runtime-discard-config-${config.configRevision}-${System.nanoTime()}"
        val request = RuntimeDiscardConfigRequest(
            runtimeSessionId = ready.runtimeSessionId,
            requestId = requestId,
            config = config,
            expiresAtElapsedNs = System.nanoTime() + CONTROL_TIMEOUT_NS,
            pid = ready.pid,
            processName = ready.processName,
            packageName = ready.packageName,
            buildFingerprint = ready.buildFingerprint,
        )
        val result = awaitControlResult(requestId) {
            sendPayload(
                messageType = BridgeMessageType.COMMAND,
                payload = RuntimeDiscardConfigPayloadCodec.encode(request).getOrThrow(),
            )
        }.getOrThrow()
        Log.i(
            LOG_TAG,
            "runtime discard config acknowledged revision=${config.configRevision} " +
                "auto=${config.autoDiscard} limits=${config.maxCountByItemId.size} " +
                "phase=${result.phase} message=${result.message}",
        )
    }

    fun invalidateManagedReadiness() {
        runtimeReady = runtimeReady?.copy(
            strongIdentityVerified = false,
            capabilities = emptySet(),
        )
        for (event in events) {
            if (event is BridgeEvent.RuntimeReady) events.remove(event)
        }
    }

    override fun disconnect() {
        val oldSocket = socket
        socket = null
        runtimeReady = null
        runtimeReadyListener = null
        readerError = null
        runCatching { oldSocket?.close() }
        readerExecutor?.shutdownNow()
        readerExecutor = null
        events.clear()
        controlResults.clear()
        pendingControlIds.clear()
    }

    private fun requestRuntimeControl(
        action: RuntimeControlAction,
        requestIdSuffix: String? = null,
        cycleId: Long? = null,
    ): Result<BridgeEvent.AutomationCommandResult> =
        dispatchControlFrame(action.wireValue, action.name, requestIdSuffix, cycleId)

    private fun dispatchControlFrame(
        actionWire: Int,
        actionLabel: String,
        requestIdSuffix: String?,
        cycleId: Long?,
    ): Result<BridgeEvent.AutomationCommandResult> = runCatching {
        val ready = currentRuntimeReady() ?: connect().getOrThrow()
        val suffix = requestIdSuffix ?: System.nanoTime().toString()
        val requestId = "runtime-${actionLabel.lowercase()}-$suffix"
        val request = RuntimeControlRequest(
            runtimeSessionId = ready.runtimeSessionId,
            requestId = requestId,
            actionWire = actionWire,
            expiresAtElapsedNs = System.nanoTime() + CONTROL_TIMEOUT_NS,
            pid = ready.pid,
            processName = ready.processName,
            packageName = ready.packageName,
            cycleId = cycleId,
        )
        awaitControlResult(requestId) {
            sendPayload(
                messageType = BridgeMessageType.COMMAND,
                payload = RuntimeControlPayloadCodec.encode(request).getOrThrow(),
            )
        }.getOrThrow()
    }

    private fun awaitControlResult(
        requestId: String,
        sendRequest: () -> Unit,
    ): Result<BridgeEvent.AutomationCommandResult> = runCatching {
        pendingControlIds.add(requestId)
        try {
            sendRequest()
            val deadline = System.nanoTime() + CONTROL_TIMEOUT_NS
            while (System.nanoTime() < deadline) {
                val result = controlResults.findAndRemove { it.commandId == requestId }
                if (result != null) {
                    check(result.phase == CommandPhase.COMPLETED) {
                        result.errorCode ?: result.message ?: result.phase.name
                    }
                    return@runCatching result
                }
                readerError?.let { throw IllegalStateException("runtime bridge reader failed", it) }
                Thread.sleep(10L)
            }
            error("runtime control timeout: $requestId")
        } finally {
            pendingControlIds.remove(requestId)
        }
    }

    private fun awaitAcceptedResult(
        requestId: String,
        sendRequest: () -> Unit,
    ): Result<BridgeEvent.AutomationCommandResult> = runCatching {
        pendingControlIds.add(requestId)
        try {
            sendRequest()
            val deadline = System.nanoTime() + CONTROL_TIMEOUT_NS
            while (System.nanoTime() < deadline) {
                val result = controlResults.findAndRemove { it.commandId == requestId }
                if (result != null) {
                    check(result.phase == CommandPhase.ACCEPTED) {
                        result.errorCode ?: result.message ?: result.phase.name
                    }
                    return@runCatching result
                }
                readerError?.let { throw IllegalStateException("runtime bridge reader failed", it) }
                Thread.sleep(10L)
            }
            error("runtime desired-state receipt timeout: $requestId")
        } finally {
            pendingControlIds.remove(requestId)
        }
    }

    private fun sendPayload(messageType: BridgeMessageType, payload: ByteArray) {
        val current = socket ?: error("runtime bridge is disconnected")
        require(current.isConnected) { "runtime bridge is disconnected" }
        val frame = BridgeFrame(
            protocolVersion = BridgeProtocol.VERSION,
            messageType = messageType,
            messageSeq = outgoingSeq.incrementAndGet(),
            payload = payload,
        )
        synchronized(outputLock) {
            Log.i(
                LOG_TAG,
                "runtime bridge send type=${frame.messageType} seq=${frame.messageSeq} " +
                    "bytes=${frame.payload.size}",
            )
            BridgeFrameCodec.write(frame, current.outputStream).getOrThrow()
        }
    }

    private fun readLoop(current: LocalSocket) {
        try {
            while (socket === current && current.isConnected) {
                val frame = BridgeFrameCodec.read(current.inputStream).getOrThrow()
                val event = BridgePayloadCodec.decode(frame.messageType, frame.payload).getOrElse { error ->
                    Log.w(
                        LOG_TAG,
                        "runtime bridge payload decode failed type=${frame.messageType} " +
                            "seq=${frame.messageSeq} bytes=${frame.payload.size}",
                        error,
                    )
                    throw error
                }
                when (event) {
                    is BridgeEvent.RuntimeReady -> {
                        runtimeReady = event
                        Log.i(
                            LOG_TAG,
                            "runtime ready seq=${event.messageSeq} strong=${event.strongIdentityVerified} " +
                                "capabilities=${event.capabilities.sorted()}",
                        )
                        events.add(event)
                        runtimeReadyListener?.invoke(event)
                    }

                    is BridgeEvent.ObservationEvent -> {
                        Log.i(
                            LOG_TAG,
                            "runtime observation received seq=${event.messageSeq} " +
                                "type=${event.observationType} lifecycle=${event.lifecycleState} " +
                                "payload=${event.payload.size}",
                        )
                        events.add(event)
                    }

                    is BridgeEvent.AutomationCommandResult -> {
                        Log.i(
                            LOG_TAG,
                            "runtime command result received seq=${event.messageSeq} " +
                                "command=${event.commandId} phase=${event.phase} " +
                                "error=${event.errorCode}",
                        )
                        val moduleLoadStatus = decodeModuleLoadStatus(event)
                        if (moduleLoadStatus != null) {
                            publishModuleLoadStatus(moduleLoadStatus)
                        } else if (pendingControlIds.contains(event.commandId)) {
                            controlResults.add(event)
                        } else {
                            events.add(event)
                        }
                    }

                    else -> events.add(event)
                }
                runCatching { runtimeEventListener?.invoke(event) }
                    .onFailure { Log.w(LOG_TAG, "runtime event listener failed", it) }
            }
        } catch (error: Throwable) {
            if (socket === current) {
                readerError = error
                Log.w(LOG_TAG, "runtime bridge reader failed", error)
            }
        }
    }

    private fun decodeModuleLoadStatus(
        event: BridgeEvent.AutomationCommandResult,
    ): RuntimeModuleLoadStatus? {
        if (!event.commandId.startsWith(MODULE_LOAD_COMMAND_PREFIX)) return null
        val moduleWire = event.commandId.removePrefix(MODULE_LOAD_COMMAND_PREFIX).toIntOrNull()
            ?: return null
        val module = RuntimeFeatureModule.entries.firstOrNull { it.wireValue == moduleWire }
            ?: return null
        return RuntimeModuleLoadStatus(
            runtimeSessionId = event.runtimeSessionId,
            module = module,
            loaded = event.phase == CommandPhase.COMPLETED,
            errorCode = event.errorCode?.takeIf { it.isNotBlank() },
            message = event.message?.takeIf { it.isNotBlank() },
        )
    }

    private fun publishModuleLoadStatus(status: RuntimeModuleLoadStatus) {
        val key = "${status.runtimeSessionId}:${status.module.wireValue}"
        if (!publishedModuleLoadStatuses.add(key)) return
        Log.i(
            LOG_TAG,
            "runtime module load session=${status.runtimeSessionId} module=${status.module} " +
                "loaded=${status.loaded} error=${status.errorCode}",
        )
        runCatching { onModuleLoadStatus(status) }
            .onFailure { Log.w(LOG_TAG, "runtime module load callback failed", it) }
    }

    private fun registerControllerUid(): Boolean {
        val uid = Process.myUid()
        val command = "mkdir -p $BROKER_DIRECTORY && chmod 0711 $BROKER_DIRECTORY && " +
            "printf '%s\\n' '$uid' > $CONTROLLER_UID_FILE && " +
            "chmod 0600 $CONTROLLER_UID_FILE"
        return rootShell.execute(command, timeoutMillis = 3_000L).isSuccess
    }

    private fun <T> ConcurrentLinkedQueue<T>.findAndRemove(predicate: (T) -> Boolean): T? {
        for (value in this) {
            if (predicate(value) && remove(value)) return value
        }
        return null
    }

    companion object {
        private const val BROKER_DIRECTORY = "/data/adb/pogo_root_automation"
        private const val CONTROLLER_UID_FILE = "$BROKER_DIRECTORY/controller.uids"
        private const val MODULE_LOAD_COMMAND_PREFIX = "runtime-module-load:"
        const val DEFAULT_SOCKET_NAME = "pogo_root_automation_runtime"
        private const val CONTROL_TIMEOUT_NS = 30_000_000_000L
        private const val LOG_TAG = "PogoRootAutomation"
    }
}
