package dev.pogoroot.automation.headless

import dev.pogoroot.automation.bridge.ModuleControlAction
import dev.pogoroot.automation.bridge.RuntimeFeatureModule
import dev.pogoroot.automation.headless.modules.CatchSpinModuleDescriptor
import dev.pogoroot.automation.headless.modules.DiscardModuleDescriptor
import dev.pogoroot.automation.headless.modules.EncounterModuleDescriptor
import dev.pogoroot.automation.headless.modules.TransferModuleDescriptor

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
 * Client-side per-module descriptor: the single source of truth for the concerns
 * the Kotlin controller owns — mirroring a native `module.inc`'s self-contained
 * declaration, for the client's role (configure + send, not route + execute).
 * Each module's descriptor lives in its own file under `headless/modules/`.
 *
 * - [gameplayActionTags] / [controlActions]: which actions this module owns
 *   (mirror of the native per-module `action_ownership()`).
 * - [isDesired]: the config rule that makes this module a desired enable target.
 *
 * The wire codec ([dev.pogoroot.automation.bridge.BridgeActionCodec]) stays shared,
 * mirroring native keeping its wire protocol shared rather than per-module.
 */
data class RuntimeFeatureModuleDescriptor(
    val module: RuntimeFeatureModule,
    val gameplayActionTags: Set<Int>,
    val controlActions: Set<ModuleControlAction>,
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
}

/**
 * Convert user-facing switches into native feature-group intent, driven by the
 * per-module [RuntimeFeatureModuleCatalog.descriptors].
 */
fun HeadlessAutomationConfig.desiredRuntimeFeatureModules(): Set<RuntimeFeatureModule> =
    RuntimeFeatureModuleCatalog.descriptors
        .filter { it.isDesired(this) }
        .mapTo(linkedSetOf()) { it.module }
