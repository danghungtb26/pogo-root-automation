package dev.pogoroot.automation.bridge

/**
 * Kotlin-side per-module action registry, mirroring the native module owner map
 * (`action_ownership()` in each `module.inc`). It keeps module-specific action
 * identities out of the shared lifecycle/command enums so each feature module owns
 * its actions independently on both sides of the bridge.
 *
 * - Gameplay action tags are carried in the AutomationAction command frame and
 *   routed natively by the owner map; this registry is the authoritative Kotlin
 *   declaration of which module owns which tag.
 * - Control actions (below) that belong to a module — as opposed to runtime
 *   lifecycle START/STOP/DIAGNOSTIC — are declared as [ModuleControlAction].
 */

/**
 * A control action owned by a feature [owner] module (not a runtime lifecycle
 * action). Sent as a raw [wireValue] in the control frame; the native dispatcher
 * routes it via the module owner map.
 */
enum class ModuleControlAction(val wireValue: Int, val owner: RuntimeFeatureModule) {
    /** catch_spin's per-cycle world-snapshot pull (nearby/forts/inventory/player). */
    SCAN_MAP(5, RuntimeFeatureModule.CATCH_SPIN),
}

object RuntimeModuleActionRegistry {
    /**
     * Gameplay action tags owned by each module. Mirrors the `kXxxActionTags`
     * arrays declared in the native module.inc files; keep the two in sync.
     */
    val gameplayActionTags: Map<RuntimeFeatureModule, Set<Int>> = mapOf(
        RuntimeFeatureModule.CATCH_SPIN to setOf(2, 3, 4, 9, 10, 11, 12),
        RuntimeFeatureModule.DISCARD to setOf(5),
        RuntimeFeatureModule.TRANSFER to setOf(6),
        RuntimeFeatureModule.ENCOUNTER to setOf(8),
    )

    /** Control actions owned by [module] (the module-owned subset of control actions). */
    fun controlActions(module: RuntimeFeatureModule): Set<ModuleControlAction> =
        ModuleControlAction.entries.filterTo(mutableSetOf()) { it.owner == module }

    /** The module that owns [gameplayTag], or null when no module claims it. */
    fun ownerOfGameplayTag(gameplayTag: Int): RuntimeFeatureModule? =
        gameplayActionTags.entries.firstOrNull { gameplayTag in it.value }?.key
}
