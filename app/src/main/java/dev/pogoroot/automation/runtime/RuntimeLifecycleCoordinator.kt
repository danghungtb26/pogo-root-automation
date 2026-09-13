package dev.pogoroot.automation.runtime

import dev.pogoroot.automation.engine.CatchSpinArmState
import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.root.RuntimeBridgeClient
import dev.pogoroot.automation.config.HeadlessAutomationConfig

enum class RuntimeControlState {
    DETACHED,
    ATTACHED_IDLE,
    STARTING,
    RUNNING,
    STOPPING,
    ERROR,
}

data class RuntimeControlSnapshot(
    val state: RuntimeControlState = RuntimeControlState.DETACHED,
    val runtimeSessionId: String? = null,
    val lastError: String? = null,
)

/**
 * Kotlin control plane for the injected runtime host.
 *
 * Owns connect/START/STOP, managed DIAGNOSTIC scheduling, and CONFIG_SET mirrors.
 * Feature-module enable/disable and map-ready gating live entirely in native
 * (`sync_auto_enabled_modules`).
 */
class RuntimeLifecycleCoordinator(
    private val bridge: RuntimeBridgeClient,
) {
    private companion object {
        const val AUTO_DIAGNOSTIC_INITIAL_DELAY_MS = 3_000L
        const val AUTO_DIAGNOSTIC_RETRY_DELAY_MS = 5_000L
    }

    @Volatile private var state = RuntimeControlState.DETACHED
    @Volatile private var activeRuntimeSessionId: String? = null
    @Volatile private var managedReadySessionId: String? = null
    @Volatile private var managedReadinessBlockedBeforeMessageSeq: Long? = null
    @Volatile private var lastError: String? = null
    @Volatile private var managedConfigsApplied = false
    private var nextAutomaticDiagnosticAtNanos = Long.MAX_VALUE
    private var onManagedConfigsAppliedListener: (() -> Unit)? = null
    private val catchSpinConfigDispatcher = CatchSpinConfigDispatcher(bridge)
    private val transferConfigDispatcher = TransferConfigDispatcher(bridge)
    private val discardConfigDispatcher = DiscardConfigDispatcher(bridge)

    val connected: Boolean
        get() = bridge.connected

    val configsAppliedToNative: Boolean
        get() = managedConfigsApplied

    @Synchronized
    fun setOnManagedConfigsAppliedListener(listener: (() -> Unit)?) {
        onManagedConfigsAppliedListener = listener
    }

    @Synchronized
    fun ensureRunning(config: HeadlessAutomationConfig): Result<BridgeEvent.RuntimeReady> = runCatching {
        val ready = bridge.connect().getOrThrow()
        val sessionChanged = activeRuntimeSessionId != null &&
            activeRuntimeSessionId != ready.runtimeSessionId
        if (sessionChanged) {
            resetConfigDispatchers()
            activeRuntimeSessionId = null
            managedReadySessionId = null
            managedReadinessBlockedBeforeMessageSeq = null
            nextAutomaticDiagnosticAtNanos = Long.MAX_VALUE
            state = RuntimeControlState.ATTACHED_IDLE
        }

        if (state != RuntimeControlState.RUNNING ||
            activeRuntimeSessionId != ready.runtimeSessionId) {
            state = RuntimeControlState.STARTING
            managedReadinessBlockedBeforeMessageSeq = bridge.currentRuntimeReady()?.messageSeq
            bridge.startRuntime().getOrThrow()
            activeRuntimeSessionId = ready.runtimeSessionId
            managedReadySessionId = null
            nextAutomaticDiagnosticAtNanos = System.nanoTime() +
                AUTO_DIAGNOSTIC_INITIAL_DELAY_MS * 1_000_000L
            state = RuntimeControlState.RUNNING
            lastError = null
            resetConfigDispatchers()
        }

        var advertisedReady = bridge.currentRuntimeReady() ?: ready
        if (!isManagedRuntimeReadyForSession(advertisedReady)) {
            runAutomaticDiagnosticIfDue()
            advertisedReady = bridge.currentRuntimeReady() ?: advertisedReady
        }
        val effectiveReady = effectiveReady(advertisedReady)
        if (isManagedRuntimeReadyForSession(advertisedReady)) {
            managedReadySessionId = advertisedReady.runtimeSessionId
            // Push CONFIG_SET only after managed DIAGNOSTIC publishes the verified
            // build fingerprint. Sending earlier rejects with build_fingerprint_mismatch
            // and was resetting the diagnostic timer through ERROR/START loops.
            syncRuntimeConfigs(config)
        }
        effectiveReady
    }.onFailure(::recordFailure)

    @Synchronized
    fun ensureIdle(): Result<Unit> = runCatching {
        if (!bridge.connected) {
            activeRuntimeSessionId = null
            managedReadySessionId = null
            managedReadinessBlockedBeforeMessageSeq = null
            nextAutomaticDiagnosticAtNanos = Long.MAX_VALUE
            state = RuntimeControlState.DETACHED
            lastError = null
            resetConfigDispatchers()
            return@runCatching
        }

        if (state != RuntimeControlState.ATTACHED_IDLE) {
            state = RuntimeControlState.STOPPING
            bridge.stopRuntime().getOrThrow()
        }
        activeRuntimeSessionId = null
        managedReadySessionId = null
        managedReadinessBlockedBeforeMessageSeq = null
        nextAutomaticDiagnosticAtNanos = Long.MAX_VALUE
        state = RuntimeControlState.ATTACHED_IDLE
        lastError = null
        resetConfigDispatchers()
    }.onFailure(::recordFailure)

    @Synchronized
    fun runDiagnostic(config: HeadlessAutomationConfig): Result<Unit> = runCatching {
        bridge.connect().getOrThrow()
        bridge.requestRuntimeDiagnostic().getOrThrow()
        nextAutomaticDiagnosticAtNanos = System.nanoTime() +
            AUTO_DIAGNOSTIC_RETRY_DELAY_MS * 1_000_000L
        val advertisedReady = bridge.currentRuntimeReady()
        if (advertisedReady != null && isManagedRuntimeReadyForSession(advertisedReady)) {
            managedReadySessionId = advertisedReady.runtimeSessionId
            syncRuntimeConfigs(config)
        }
        if (state != RuntimeControlState.RUNNING) {
            state = RuntimeControlState.ATTACHED_IDLE
        }
        lastError = null
    }.onFailure(::recordFailure)

    @Synchronized
    fun syncCatchSpinConfig(
        config: HeadlessAutomationConfig,
        armed: Boolean,
    ): Result<Unit> = runCatching {
        catchSpinConfigDispatcher.sync(
            config = config,
            armed = armed,
        ).getOrThrow()
    }.onFailure(::recordFailure)

    @Synchronized
    fun syncTransferConfig(config: HeadlessAutomationConfig): Result<Unit> = runCatching {
        transferConfigDispatcher.sync(config).getOrThrow()
    }.onFailure(::recordFailure)

    @Synchronized
    fun syncDiscardConfig(config: HeadlessAutomationConfig): Result<Unit> = runCatching {
        discardConfigDispatcher.sync(config).getOrThrow()
    }.onFailure(::recordFailure)

    /**
     * Push the current config snapshot to native immediately (UI/API path).
     * No-ops when the runtime is not RUNNING or managed DIAGNOSTIC is pending.
     */
    @Synchronized
    fun pushRuntimeConfigs(config: HeadlessAutomationConfig): Result<Unit> = runCatching {
        if (state != RuntimeControlState.RUNNING) return@runCatching
        val ready = bridge.currentRuntimeReady() ?: return@runCatching
        if (!isManagedRuntimeReadyForSession(ready)) return@runCatching
        syncRuntimeConfigs(config)
    }.onFailure(::recordFailure)

    @Synchronized
    fun snapshot(): RuntimeControlSnapshot = RuntimeControlSnapshot(
        state = if (!bridge.connected && state != RuntimeControlState.ERROR) {
            RuntimeControlState.DETACHED
        } else {
            state
        },
        runtimeSessionId = bridge.currentRuntimeReady()?.runtimeSessionId
            ?: activeRuntimeSessionId,
        lastError = lastError,
    )

    @Synchronized
    fun shutdown() {
        runCatching { ensureIdle().getOrThrow() }
        bridge.disconnect()
        activeRuntimeSessionId = null
        managedReadySessionId = null
        managedReadinessBlockedBeforeMessageSeq = null
        nextAutomaticDiagnosticAtNanos = Long.MAX_VALUE
        state = RuntimeControlState.DETACHED
        resetConfigDispatchers()
    }

    private fun syncRuntimeConfigs(config: HeadlessAutomationConfig) {
        if (state != RuntimeControlState.RUNNING) return
        val applied = syncCatchSpinConfig(config, CatchSpinArmState.isArmed()).isSuccess &&
            syncTransferConfig(config).isSuccess &&
            syncDiscardConfig(config).isSuccess
        if (applied) {
            markManagedConfigsApplied()
        }
    }

    private fun markManagedConfigsApplied() {
        if (managedConfigsApplied) return
        managedConfigsApplied = true
        onManagedConfigsAppliedListener?.invoke()
    }

    private fun effectiveReady(ready: BridgeEvent.RuntimeReady): BridgeEvent.RuntimeReady =
        if (managedReadySessionId == ready.runtimeSessionId && isManagedRuntimeReadyForSession(ready)) {
            ready
        } else {
            ready.copy(
                strongIdentityVerified = false,
                capabilities = emptySet(),
            )
        }

    private fun isManagedRuntimeReady(ready: BridgeEvent.RuntimeReady): Boolean =
        ready.strongIdentityVerified && ready.capabilities.isNotEmpty()

    private fun isManagedRuntimeReadyForSession(ready: BridgeEvent.RuntimeReady): Boolean {
        if (!isManagedRuntimeReady(ready)) return false
        val blockedBefore = managedReadinessBlockedBeforeMessageSeq ?: return true
        return ready.messageSeq > blockedBefore
    }

    private fun runAutomaticDiagnosticIfDue() {
        if (System.nanoTime() < nextAutomaticDiagnosticAtNanos) return
        nextAutomaticDiagnosticAtNanos = System.nanoTime() +
            AUTO_DIAGNOSTIC_RETRY_DELAY_MS * 1_000_000L
        val result = runCatching {
            bridge.requestRuntimeDiagnostic().getOrThrow()
        }
        result.onSuccess {
            lastError = null
        }.onFailure { error ->
            lastError = "runtime diagnostic pending: " +
                (error.message ?: error::class.java.simpleName)
        }
    }

    private fun resetConfigDispatchers() {
        catchSpinConfigDispatcher.reset()
        transferConfigDispatcher.reset()
        discardConfigDispatcher.reset()
        managedConfigsApplied = false
    }

    private fun recordFailure(error: Throwable) {
        lastError = error.message ?: error::class.java.simpleName
        if (bridge.connected) {
            managedReadySessionId = null
            managedReadinessBlockedBeforeMessageSeq = null
            nextAutomaticDiagnosticAtNanos = Long.MAX_VALUE
            state = RuntimeControlState.ERROR
        } else {
            activeRuntimeSessionId = null
            managedReadySessionId = null
            managedReadinessBlockedBeforeMessageSeq = null
            nextAutomaticDiagnosticAtNanos = Long.MAX_VALUE
            state = RuntimeControlState.DETACHED
            resetConfigDispatchers()
        }
    }
}
