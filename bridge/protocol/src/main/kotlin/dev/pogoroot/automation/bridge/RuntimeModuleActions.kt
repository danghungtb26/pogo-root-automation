package dev.pogoroot.automation.bridge

/**
 * A control action owned by a feature [owner] module (not a runtime lifecycle
 * action). Sent as a raw [wireValue] in the control frame; the native dispatcher
 * routes it via the module owner map, exactly like a gameplay action tag.
 *
 * This is the wire declaration. Action ownership and config activation rules
 * live in the native module registry; Kotlin only serializes configuration.
 */
enum class ModuleControlAction(val wireValue: Int, val owner: RuntimeFeatureModule) {
    /** Replace the validated catch_spin runtime configuration snapshot. */
    CATCH_SPIN_CONFIG_SET(6, RuntimeFeatureModule.CATCH_SPIN),

    /** Replace the validated transfer keep-policy snapshot. */
    TRANSFER_CONFIG_SET(7, RuntimeFeatureModule.TRANSFER),

    /** Replace the validated native auto-discard limit snapshot. */
    DISCARD_CONFIG_SET(8, RuntimeFeatureModule.DISCARD),
}
