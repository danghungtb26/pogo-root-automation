package dev.pogoroot.automation.headless

import android.util.Log
import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.root.RuntimeBridgeClient

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
    val modules: Map<RuntimeFeatureModule, RuntimeFeatureModuleSnapshot> = emptyMap(),
    val lastError: String? = null,
) {
    val enabledModules: Set<RuntimeFeatureModule>
        get() = modules.values
            .filter { it.state == RuntimeFeatureModuleState.ENABLED }
            .mapTo(linkedSetOf(), RuntimeFeatureModuleSnapshot::module)
}

/**
 * Service-owned control plane for the injected runtime host and its independent
 * native feature modules.
 *
 * Zygisk attachment is process-driven. START starts a conservative native host;
 * managed binding verification is scheduled after startup and module activation
 * is synchronized separately from [HeadlessAutomationConfig].
 */
class RuntimeLifecycleCoordinator(
    private val bridge: RuntimeBridgeClient,
) {
    private companion object {
        const val AUTO_DIAGNOSTIC_INITIAL_DELAY_MS = 3_000L
        const val AUTO_DIAGNOSTIC_RETRY_DELAY_MS = 5_000L
        const val LOG_TAG = "PogoRootAutomation"
    }

    @Volatile private var state = RuntimeControlState.DETACHED
    @Volatile private var activeRuntimeSessionId: String? = null
    @Volatile private var managedReadySessionId: String? = null
    @Volatile private var managedReadinessBlockedBeforeMessageSeq: Long? = null
    @Volatile private var lastError: String? = null
    private var nextAutomaticDiagnosticAtNanos = Long.MAX_VALUE
    private val modules = linkedMapOf<RuntimeFeatureModule, RuntimeFeatureModuleSnapshot>()

    val connected: Boolean
        get() = bridge.connected

    @Synchronized
    fun ensureRunning(config: HeadlessAutomationConfig): Result<BridgeEvent.RuntimeReady> = runCatching {
        val ready = bridge.connect().getOrThrow()
        val sessionChanged = activeRuntimeSessionId != null &&
            activeRuntimeSessionId != ready.runtimeSessionId
        if (sessionChanged) {
            resetModules()
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
            resetModules()
        }

        var advertisedReady = bridge.currentRuntimeReady() ?: ready
        if (!isManagedRuntimeReadyForSession(advertisedReady)) {
            runAutomaticDiagnosticIfDue()
            advertisedReady = bridge.currentRuntimeReady() ?: advertisedReady
        }
        val wasManagedReady = managedReadySessionId == advertisedReady.runtimeSessionId
        val managedReady = isManagedRuntimeReadyForSession(advertisedReady)
        if (managedReady) {
            // Capability updates arrive asynchronously after DIAGNOSTIC. Once
            // observed for this process session, they are the only readiness
            // signal that permits module activation and structured automation.
            managedReadySessionId = advertisedReady.runtimeSessionId
        }
        val effectiveReady = effectiveReady(advertisedReady)
        syncModules(
            config = config,
            ready = effectiveReady,
            forceUnavailableRetry = managedReady && !wasManagedReady,
        )
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
            resetModules()
            return@runCatching
        }

        // STOP is idempotent and disables every native feature module in one
        // operation, leaving only the lightweight process attachment alive.
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
        resetModules()
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
            syncModules(
                config = config,
                ready = advertisedReady,
                forceUnavailableRetry = true,
            )
        }
        if (state != RuntimeControlState.RUNNING) {
            state = RuntimeControlState.ATTACHED_IDLE
        }
        lastError = null
    }.onFailure(::recordFailure)

    @Synchronized
    fun requestCatchSpinScan(cycleId: Long): Result<Unit> = runCatching {
        Log.i(LOG_TAG, "automation SCAN_MAP request cycle=$cycleId")
        bridge.requestRuntimeScanMap(cycleId).getOrThrow()
    }

    /** Compatibility entry point for callers that still use the old name. */
    @Synchronized
    fun requestWorldSnapshot(cycleId: Long): Result<Unit> = requestCatchSpinScan(cycleId)

    @Synchronized
    fun snapshot(): RuntimeControlSnapshot = RuntimeControlSnapshot(
        state = if (!bridge.connected && state != RuntimeControlState.ERROR) {
            RuntimeControlState.DETACHED
        } else {
            state
        },
        runtimeSessionId = bridge.currentRuntimeReady()?.runtimeSessionId
            ?: activeRuntimeSessionId,
        modules = RuntimeFeatureModule.entries.associateWith { module ->
            modules[module] ?: RuntimeFeatureModuleSnapshot(
                module = module,
                desired = false,
                state = RuntimeFeatureModuleState.DISABLED,
            )
        },
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
        resetModules()
    }

    private fun syncModules(
        config: HeadlessAutomationConfig,
        ready: BridgeEvent.RuntimeReady,
        forceUnavailableRetry: Boolean = false,
    ) {
        val desiredModules = config.desiredRuntimeFeatureModules()
        RuntimeFeatureModule.entries.forEach { module ->
            val desired = module in desiredModules
            val current = modules[module]

            if (desired) {
                if (!isManagedRuntimeReadyForSession(ready)) {
                    modules[module] = RuntimeFeatureModuleSnapshot(
                        module = module,
                        desired = true,
                        state = RuntimeFeatureModuleState.UNAVAILABLE,
                        lastError = "runtime managed diagnostic pending",
                    )
                    return@forEach
                }
                if (current?.desired == true &&
                    (current.state == RuntimeFeatureModuleState.ENABLED ||
                        (current.state == RuntimeFeatureModuleState.UNAVAILABLE &&
                            !forceUnavailableRetry))
                ) return@forEach
                modules[module] = RuntimeFeatureModuleSnapshot(
                    module = module,
                    desired = true,
                    state = RuntimeFeatureModuleState.ENABLING,
                )
                bridge.setModuleEnabled(module, true)
                    .onSuccess {
                        modules[module] = RuntimeFeatureModuleSnapshot(
                            module = module,
                            desired = true,
                            state = RuntimeFeatureModuleState.ENABLED,
                        )
                    }
                    .onFailure { error ->
                        val message = error.message ?: error::class.java.simpleName
                        modules[module] = RuntimeFeatureModuleSnapshot(
                            module = module,
                            desired = true,
                            state = if (message.contains("runtime_module_unavailable")) {
                                RuntimeFeatureModuleState.UNAVAILABLE
                            } else {
                                RuntimeFeatureModuleState.ERROR
                            },
                            lastError = message,
                        )
                    }
                return@forEach
            }

            if (current == null ||
                current.state == RuntimeFeatureModuleState.DISABLED ||
                current.state == RuntimeFeatureModuleState.UNAVAILABLE
            ) {
                modules[module] = RuntimeFeatureModuleSnapshot(
                    module = module,
                    desired = false,
                    state = RuntimeFeatureModuleState.DISABLED,
                )
                return@forEach
            }

            modules[module] = RuntimeFeatureModuleSnapshot(
                module = module,
                desired = false,
                state = RuntimeFeatureModuleState.DISABLING,
            )
            bridge.setModuleEnabled(module, false)
                .onSuccess {
                    modules[module] = RuntimeFeatureModuleSnapshot(
                        module = module,
                        desired = false,
                        state = RuntimeFeatureModuleState.DISABLED,
                    )
                }
                .onFailure { error ->
                    modules[module] = RuntimeFeatureModuleSnapshot(
                        module = module,
                        desired = false,
                        state = RuntimeFeatureModuleState.ERROR,
                        lastError = error.message ?: error::class.java.simpleName,
                    )
            }
        }
    }

    private fun effectiveReady(ready: BridgeEvent.RuntimeReady): BridgeEvent.RuntimeReady =
        if (managedReadySessionId == ready.runtimeSessionId && isManagedRuntimeReadyForSession(ready)) {
            ready
        } else {
            // Preserve identity/session metadata for diagnostics, but do not
            // expose probe-only capabilities to the automation controller.
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

    private fun resetModules() {
        modules.clear()
        RuntimeFeatureModule.entries.forEach { module ->
            modules[module] = RuntimeFeatureModuleSnapshot(
                module = module,
                desired = false,
                state = RuntimeFeatureModuleState.DISABLED,
            )
        }
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
            resetModules()
        }
    }

}
