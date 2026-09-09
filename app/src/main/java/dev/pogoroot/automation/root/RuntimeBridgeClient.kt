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
import dev.pogoroot.automation.bridge.RuntimeBridge
import dev.pogoroot.automation.core.automation.AlertKind
import dev.pogoroot.automation.core.automation.AutomationAction
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicLong

/**
 * Persistent controller-side Unix-domain-socket client. The root companion is
 * the broker/peer; the app never attaches to the Pokémon GO process itself.
 */
class RuntimeBridgeClient(
    private val socketName: String = DEFAULT_SOCKET_NAME,
    private val connectTimeoutMs: Long = 3_000L,
    private val rootShell: RootShell = ProcessRootShell(),
) : RuntimeBridge {
    private val outgoingSeq = AtomicLong(0L)
    private val events = ConcurrentLinkedQueue<BridgeEvent>()
    private val outputLock = Any()
    @Volatile private var socket: LocalSocket? = null
    @Volatile private var runtimeReady: BridgeEvent.RuntimeReady? = null
    @Volatile private var readerError: Throwable? = null
    @Volatile private var readerExecutor: ExecutorService? = null

    override val connected: Boolean
        get() = socket?.isConnected == true && readerError == null

    override fun connect(): Result<BridgeEvent.RuntimeReady> = runCatching {
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
                return@runCatching ready
            }
            readerError?.let { throw IllegalStateException("runtime bridge reader failed", it) }
            Thread.sleep(10L)
        }
        error("runtime ready timeout")
    }.onFailure {
        disconnect()
    }

    override fun receiveEvents(): Result<List<BridgeEvent>> = runCatching {
        readerError?.let { throw IllegalStateException("runtime bridge reader failed", it) }
        buildList {
            while (true) add(events.poll() ?: break)
        }
    }

    override fun send(command: BridgeEvent.AutomationCommand): Result<Unit> = runCatching {
        val current = socket ?: error("runtime bridge is disconnected")
        require(current.isConnected) { "runtime bridge is disconnected" }
        val payload = BridgePayloadCodec.encode(command).getOrThrow()
        val frame = BridgeFrame(
            protocolVersion = BridgeProtocol.VERSION,
            messageType = BridgeMessageType.COMMAND,
            messageSeq = outgoingSeq.incrementAndGet(),
            payload = payload,
        )
        synchronized(outputLock) {
            BridgeFrameCodec.write(frame, current.outputStream).getOrThrow()
        }
    }

    /**
     * Requests the explicit post-init read-only inspector in the injected
     * process. This bypasses gameplay capability gates by design: the payload
     * is a reserved Alert marker and the native side never publishes a
     * mutation capability for it.
     */
    fun requestRuntimeDiagnostic(): Result<Unit> = runCatching {
        val ready = runtimeReady ?: error("runtime bridge is not connected")
        send(
            BridgeEvent.AutomationCommand(
                runtimeSessionId = ready.runtimeSessionId,
                commandId = "runtime-diagnostic-${System.nanoTime()}",
                action = AutomationAction.Alert(AlertKind.SHUNDO, RUNTIME_DIAGNOSTIC_MESSAGE),
                basedOnObservationSeq = ready.messageSeq,
                expectedLifecycle = null,
                expiresAtElapsedNs = System.nanoTime() + DIAGNOSTIC_TIMEOUT_NS,
                pid = ready.pid,
                processName = ready.processName,
                packageName = ready.packageName,
                buildFingerprint = ready.buildFingerprint,
            ),
        ).getOrThrow()
    }

    override fun disconnect() {
        val oldSocket = socket
        socket = null
        runtimeReady = null
        readerError = null
        runCatching { oldSocket?.close() }
        readerExecutor?.shutdownNow()
        readerExecutor = null
        events.clear()
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
                if (event is BridgeEvent.RuntimeReady) {
                    Log.i(
                        LOG_TAG,
                        "runtime ready seq=${event.messageSeq} strong=${event.strongIdentityVerified} " +
                            "capabilities=${event.capabilities.sorted()}",
                    )
                } else if (event is BridgeEvent.ObservationEvent) {
                    Log.i(
                        LOG_TAG,
                        "runtime observation received seq=${event.messageSeq} " +
                            "type=${event.observationType} lifecycle=${event.lifecycleState} " +
                            "payload=${event.payload.size}",
                    )
                } else if (event is BridgeEvent.AutomationCommandResult) {
                    Log.i(
                        LOG_TAG,
                        "runtime command result received seq=${event.messageSeq} " +
                            "command=${event.commandId} phase=${event.phase} " +
                            "error=${event.errorCode}",
                    )
                }
                events.add(event)
            }
        } catch (error: Throwable) {
            if (socket === current) {
                readerError = error
                Log.w(LOG_TAG, "runtime bridge reader failed", error)
            }
        }
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
        const val DEFAULT_SOCKET_NAME = "pogo_root_automation_runtime"
        private const val RUNTIME_DIAGNOSTIC_MESSAGE = "__runtime_diagnostic_v1__"
        private const val DIAGNOSTIC_TIMEOUT_NS = 30_000_000_000L
        private const val LOG_TAG = "PogoRootAutomation"
    }
}
