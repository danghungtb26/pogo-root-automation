package dev.pogoroot.automation.root

import android.net.LocalSocket
import android.net.LocalSocketAddress
import android.os.Process
import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.BridgeFrame
import dev.pogoroot.automation.bridge.BridgeFrameCodec
import dev.pogoroot.automation.bridge.BridgeMessageType
import dev.pogoroot.automation.bridge.BridgePayloadCodec
import dev.pogoroot.automation.bridge.BridgeProtocol
import dev.pogoroot.automation.bridge.RuntimeBridge
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
    private val socketPath: String = DEFAULT_SOCKET_PATH,
    private val connectTimeoutMs: Long = 3_000L,
    private val rootShell: RootShell = ProcessRootShell(),
) : RuntimeBridge {
    private val outgoingSeq = AtomicLong(0L)
    private val events = ConcurrentLinkedQueue<BridgeEvent>()
    private val outputLock = Any()
    @Volatile private var socket: LocalSocket? = null
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
                socketPath,
                LocalSocketAddress.Namespace.FILESYSTEM,
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
            if (ready is BridgeEvent.RuntimeReady) return@runCatching ready
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

    override fun disconnect() {
        val oldSocket = socket
        socket = null
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
                val event = BridgePayloadCodec.decode(frame.messageType, frame.payload).getOrThrow()
                events.add(event)
            }
        } catch (error: Throwable) {
            if (socket === current) readerError = error
        }
    }

    private fun registerControllerUid(): Boolean {
        val uid = Process.myUid()
        val command = "mkdir -p $BROKER_DIRECTORY && chmod 0711 $BROKER_DIRECTORY && " +
            "touch $CONTROLLER_UID_FILE && " +
            "(grep -qx '$uid' $CONTROLLER_UID_FILE || echo '$uid' >> $CONTROLLER_UID_FILE) && " +
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
        const val DEFAULT_SOCKET_PATH = "/data/adb/pogo_root_automation/runtime.sock"
    }
}
