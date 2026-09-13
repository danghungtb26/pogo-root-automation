package dev.pogoroot.automation.engine

import java.util.concurrent.atomic.AtomicBoolean

/**
 * UI mirror of the persisted catch-spin arm. The service restores it before
 * connecting; config is the authority used when synchronizing native modules.
 */
object CatchSpinArmState {
    private val armed = AtomicBoolean(false)

    fun isArmed(): Boolean = armed.get()

    fun setArmed(value: Boolean) {
        armed.set(value)
    }
}
