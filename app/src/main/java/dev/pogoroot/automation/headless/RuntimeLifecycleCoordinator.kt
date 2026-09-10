package dev.pogoroot.automation.headless

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
 * Zygisk attachment is process-driven. START prepares verified bindings only;
 * module activation is synchronized separately from [HeadlessAutomationConfig].
 */
class RuntimeLifecycleCoordinator(
    private val bridge: RuntimeBridgeClient,
) {
    @Volatile private var state = RuntimeControlState.DETACHED
    @Volatile private var activeRuntimeSessionId: String? = null
    @Volatile private var lastError: String? = null
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
            state = RuntimeControlState.ATTACHED_IDLE
        }

        if (state != RuntimeControlState.RUNNING ||
            activeRuntimeSessionId != ready.runtimeSessionId) {
            state = RuntimeControlState.STARTING
            bridge.startRuntime().getOrThrow()
            activeRuntimeSessionId = ready.runtimeSessionId
            state = RuntimeControlState.RUNNING
            lastError = null
            resetModules()
        }

        syncModules(config)
        bridge.currentRuntimeReady() ?: ready
    }.onFailure(::recordFailure)

    @Synchronized
    fun ensureIdle(): Result<Unit> = runCatching {
        if (!bridge.connected) {
            activeRuntimeSessionId = null
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
        state = RuntimeControlState.ATTACHED_IDLE
        lastError = null
        resetModules()
    }.onFailure(::recordFailure)

    @Synchronized
    fun runDiagnostic(): Result<Unit> = runCatching {
        bridge.connect().getOrThrow()
        bridge.requestRuntimeDiagnostic().getOrThrow()
        if (state != RuntimeControlState.RUNNING && state != RuntimeControlState.ERROR) {
            state = RuntimeControlState.ATTACHED_IDLE
        }
        lastError = null
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
        state = RuntimeControlState.DETACHED
        resetModules()
    }

    private fun syncModules(config: HeadlessAutomationConfig) {
        val desiredModules = config.desiredRuntimeFeatureModules()
        RuntimeFeatureModule.entries.forEach { module ->
            val desired = module in desiredModules
            val current = modules[module]

            if (desired) {
                if (current?.desired == true && current.state in setOf(
                        RuntimeFeatureModuleState.ENABLED,
                        RuntimeFeatureModuleState.UNAVAILABLE,
                    )
                ) {
                    return@forEach
                }
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
            state = RuntimeControlState.ERROR
        } else {
            activeRuntimeSessionId = null
            state = RuntimeControlState.DETACHED
            resetModules()
        }
    }
}
