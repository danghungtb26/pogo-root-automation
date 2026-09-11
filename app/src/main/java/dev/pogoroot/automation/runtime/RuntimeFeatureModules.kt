package dev.pogoroot.automation.runtime

import dev.pogoroot.automation.bridge.ModuleControlAction
import dev.pogoroot.automation.bridge.ObservationType
import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.modules.CatchSpinModuleDescriptor
import dev.pogoroot.automation.modules.DiscardModuleDescriptor
import dev.pogoroot.automation.modules.EncounterModuleDescriptor
import dev.pogoroot.automation.modules.TransferModuleDescriptor
import dev.pogoroot.automation.config.HeadlessAutomationConfig

enum class RuntimeFeatureModuleState {
    DISABLED,
    ENABLING,
    ENABLED,
    DISABLING,
    UNAVAILABLE,
    ERROR,
}

data class RuntimeFeatureModuleSnapshot(
    val module: RuntimeFeatureModule,
    val desired: Boolean,
    val state: RuntimeFeatureModuleState,
    val lastError: String? = null,
)

/**
 * How a module's planning is triggered (see docs/automation-flow.md — Phần 3.2b).
 * Orthogonal to [ExecutionMode].
 */
enum class ModuleTriggerType {
    /** Driven purely by native-pushed lifecycle/observation events; no own cadence. */
    REACTIVE,

    /** Self-emits a request on its own cadence (e.g. catch_spin's SCAN_MAP pull). */
    PERIODIC,
}

/**
 * How a module's action execution interacts with other modules (see
 * docs/automation-flow.md — Phần 3.2c). Orthogonal to [ModuleTriggerType].
 */
enum class ExecutionMode {
    /**
     * Holds an exclusive lock for its action window (dispatch → authoritative
     * result or timeout); blocks every other module while held. Two EXCLUSIVE
     * modules also mutually exclude.
     */
    EXCLUSIVE,

    /**
     * Runs concurrently with other CONCURRENT modules; only blocked while an
     * EXCLUSIVE module is holding the lock.
     */
    CONCURRENT,
}

/**
 * Client-side per-module descriptor: the single source of truth for the concerns
 * the Kotlin controller owns — mirroring a native `module.inc`'s self-contained
 * declaration, for the client's role (configure + send, not route + execute).
 * Each module's descriptor lives in its own file under `modules/`.
 *
 * - [gameplayActionTags] / [controlActions]: which actions this module owns
 *   (mirror of the native per-module `action_ownership()`).
 * - [triggerType] / [executionMode]: the two orthogonal classification axes that
 *   drive how the module is scheduled and executed (see docs/automation-flow.md).
 * - [isDesired]: the config rule that makes this module a desired enable target.
 *
 * The wire codec ([dev.pogoroot.automation.bridge.BridgeActionCodec]) stays shared,
 * mirroring native keeping its wire protocol shared rather than per-module.
 */
data class RuntimeFeatureModuleDescriptor(
    val module: RuntimeFeatureModule,
    val gameplayActionTags: Set<Int>,
    val controlActions: Set<ModuleControlAction>,
    val triggerType: ModuleTriggerType,
    val executionMode: ExecutionMode,
    val isDesired: (HeadlessAutomationConfig) -> Boolean,
)

/** Aggregates the per-module descriptors. Adding a module = one file + one entry. */
object RuntimeFeatureModuleCatalog {
    val descriptors: List<RuntimeFeatureModuleDescriptor> = listOf(
        CatchSpinModuleDescriptor,
        DiscardModuleDescriptor,
        TransferModuleDescriptor,
        EncounterModuleDescriptor,
    )

    fun forModule(module: RuntimeFeatureModule): RuntimeFeatureModuleDescriptor? =
        descriptors.firstOrNull { it.module == module }

    /** The module that owns [gameplayTag], or null when none claims it. */
    fun ownerOfGameplayTag(gameplayTag: Int): RuntimeFeatureModule? =
        descriptors.firstOrNull { gameplayTag in it.gameplayActionTags }?.module

    /**
     * The module that consumes [observationType], or null when the type is
     * kernel-level (lifecycle/diagnostics) and owned by no feature module.
     *
     * NEARBY/FORTS/REQUEST_CATCH_SPIN are catch_spin's world-snapshot inputs;
     * INVENTORY feeds discard and POKEMON_STORAGE feeds transfer (native pushes
     * these independently — see docs/automation-flow.md Phần 2 finding).
     */
    fun ownerOfObservationType(observationType: ObservationType): RuntimeFeatureModule? =
        when (observationType) {
            ObservationType.REQUEST_CATCH_SPIN,
            ObservationType.NEARBY,
            ObservationType.FORTS,
            -> RuntimeFeatureModule.CATCH_SPIN

            ObservationType.ENCOUNTER -> RuntimeFeatureModule.ENCOUNTER
            ObservationType.INVENTORY -> RuntimeFeatureModule.DISCARD
            ObservationType.POKEMON_STORAGE -> RuntimeFeatureModule.TRANSFER

            ObservationType.LIFECYCLE,
            ObservationType.MAP_TARGET,
            ObservationType.THROW_DIAGNOSTIC,
            -> null
        }

    /** Modules whose action execution holds the exclusive world-UI lock. */
    fun modulesWithExecution(mode: ExecutionMode): Set<RuntimeFeatureModule> =
        descriptors.filter { it.executionMode == mode }.mapTo(linkedSetOf()) { it.module }
}

/**
 * Convert user-facing switches into native feature-group intent, driven by the
 * per-module [RuntimeFeatureModuleCatalog.descriptors].
 */
fun HeadlessAutomationConfig.desiredRuntimeFeatureModules(): Set<RuntimeFeatureModule> =
    RuntimeFeatureModuleCatalog.descriptors
        .filter { it.isDesired(this) }
        .mapTo(linkedSetOf()) { it.module }
