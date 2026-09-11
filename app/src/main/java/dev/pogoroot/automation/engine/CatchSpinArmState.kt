package dev.pogoroot.automation.engine

import java.util.concurrent.atomic.AtomicBoolean

/**
 * Volatile process-local master arm for the catch_spin cluster (the overlay's
 * Automation button). Default off and intentionally never persisted: reopening the
 * app / restarting the process starts disarmed, so catch/spin never resume on their
 * own. While disarmed, the catch_spin planner produces no catch/spin actions even if
 * the engine is running and autoCatch/autoSpin are on.
 */
object CatchSpinArmState {
    private val armed = AtomicBoolean(false)

    fun isArmed(): Boolean = armed.get()

    fun setArmed(value: Boolean) {
        armed.set(value)
    }
}
