package dev.pogoroot.automation.bridge

/**
 * A control action owned by a feature [owner] module (not a runtime lifecycle
 * action). Sent as a raw [wireValue] in the control frame; the native dispatcher
 * routes it via the module owner map, exactly like a gameplay action tag.
 *
 * This is the protocol-layer (wire) declaration. The full per-module action
 * ownership + config-desired rules live in the app-layer
 * `RuntimeFeatureModuleCatalog`, which references these values.
 */
enum class ModuleControlAction(val wireValue: Int, val owner: RuntimeFeatureModule) {
    /** catch_spin's per-cycle world-snapshot pull (nearby/forts/inventory/player). */
    SCAN_MAP(5, RuntimeFeatureModule.CATCH_SPIN),
}
