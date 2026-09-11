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
import dev.pogoroot.automation.bridge.RuntimeBridge
import dev.pogoroot.automation.bridge.RuntimeControlAction
import dev.pogoroot.automation.bridge.RuntimeControlPayloadCodec
import dev.pogoroot.automation.bridge.RuntimeControlRequest
import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.bridge.RuntimeModuleControlAction
import dev.pogoroot.automation.bridge.RuntimeModuleControlPayloadCodec
import dev.pogoroot.automation.bridge.RuntimeModuleControlRequest
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

/**
 * Persistent controller-side Unix-domain-socket client. The root companion is
 * the broker/peer; the app never attaches to the Pokémon GO process itself.
 *
 * Connecting is deliberately side-effect free. START only prepares the native
 * runtime host; feature groups are activated independently through
 * [setModuleEnabled].
 */
class RuntimeBridgeClient(
    private val socketName: String = DEFAULT_SOCKET_NAME,
    private val connectTimeoutMs: Long = 3_000L,
    private val rootShell: RootShell = ProcessRootShell(),
    private val onModuleLoadStatus: (RuntimeModuleLoadStatus) -> Unit = {},
) : RuntimeBridge {
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

    override val connected: Boolean
        get() = socket?.isConnected == true && readerError == null

    fun currentRuntimeReady(): BridgeEvent.RuntimeReady? =
        runtimeReady?.takeIf { connected }

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

    override fun receiveEvents(): Result<List<BridgeEvent>> = runCatching {
        readerError?.let { throw IllegalStateException("runtime bridge reader failed", it) }
        buildList {
            while (true) add(events.poll() ?: break)
        }
    }

    override fun send(command: BridgeEvent.AutomationCommand): Result<Unit> = runCatching {
        sendPayload(
            messageType = BridgeMessageType.COMMAND,
            payload = BridgePayloadCodec.encode(command).getOrThrow(),
        )
    }

    /** Start the conservative native host. No feature module is enabled here. */
    fun startRuntime(): Result<Unit> {
        invalidateManagedReadiness()
        return requestRuntimeControl(RuntimeControlAction.START).map { Unit }
    }

    /** Disable all feature modules and leave the Zygisk process attachment idle. */
    fun stopRuntime(): Result<Unit> =
        requestRuntimeControl(RuntimeControlAction.STOP).map { Unit }

    /** Read-only runtime readiness/binding check; it does not enable modules. */
    fun requestRuntimeDiagnostic(): Result<Unit> =
        requestRuntimeControl(RuntimeControlAction.DIAGNOSTIC).map { Unit }

    /** Pull one correlated world read after the native module is running. */
    fun requestRuntimeSnapshot(cycleId: Long): Result<Unit> = runCatching {
        require(cycleId > 0L) { "runtime snapshot cycle must be positive" }
        val result = requestRuntimeControl(
            action = RuntimeControlAction.SNAPSHOT,
            requestIdSuffix = "cycle-$cycleId",
        ).getOrThrow()
        Log.i(
            LOG_TAG,
            "runtime snapshot acknowledged cycle=$cycleId message=${result.message}",
        )
    }

    /** Pull one correlated map/inventory read for the catch/spin automation loop. */
    fun requestRuntimeScanMap(cycleId: Long): Result<Unit> = runCatching {
        require(cycleId > 0L) { "scan map cycle must be positive" }
        val result = requestRuntimeControl(
            action = RuntimeControlAction.SCAN_MAP,
            requestIdSuffix = "cycle-$cycleId",
            cycleId = cycleId,
        ).getOrThrow()
        Log.i(
            LOG_TAG,
            "runtime SCAN_MAP acknowledged cycle=$cycleId message=${result.message}",
        )
    }

    /**
     * Drop capability state from a previous START/STOP cycle. The native host
     * must publish a fresh capability update after the next explicit diagnostic.
     */
    fun invalidateManagedReadiness() {
        runtimeReady = runtimeReady?.copy(
            strongIdentityVerified = false,
            capabilities = emptySet(),
        )
        for (event in events) {
            if (event is BridgeEvent.RuntimeReady) events.remove(event)
        }
    }

    fun setModuleEnabled(module: RuntimeFeatureModule, enabled: Boolean): Result<Unit> =
        requestRuntimeModuleControl(
            module = module,
            action = if (enabled) {
                RuntimeModuleControlAction.ENABLE
            } else {
                RuntimeModuleControlAction.DISABLE
            },
        ).map { Unit }

    override fun disconnect() {
        val oldSocket = socket
        socket = null
        runtimeReady = null
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
    ): Result<BridgeEvent.AutomationCommandResult> = runCatching {
        val ready = currentRuntimeReady() ?: connect().getOrThrow()
        val suffix = requestIdSuffix ?: System.nanoTime().toString()
        val requestId = "runtime-${action.name.lowercase()}-$suffix"
        val request = RuntimeControlRequest(
            runtimeSessionId = ready.runtimeSessionId,
            requestId = requestId,
            action = action,
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

    private fun requestRuntimeModuleControl(
        module: RuntimeFeatureModule,
        action: RuntimeModuleControlAction,
    ): Result<BridgeEvent.AutomationCommandResult> = runCatching {
        val ready = currentRuntimeReady() ?: connect().getOrThrow()
        val requestId = "runtime-module-${module.name.lowercase()}-${action.name.lowercase()}-${System.nanoTime()}"
        val request = RuntimeModuleControlRequest(
            runtimeSessionId = ready.runtimeSessionId,
            requestId = requestId,
            module = module,
            action = action,
            expiresAtElapsedNs = System.nanoTime() + CONTROL_TIMEOUT_NS,
            pid = ready.pid,
            processName = ready.processName,
            packageName = ready.packageName,
        )
        awaitControlResult(requestId) {
            sendPayload(
                messageType = BridgeMessageType.COMMAND,
                payload = RuntimeModuleControlPayloadCodec.encode(request).getOrThrow(),
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
