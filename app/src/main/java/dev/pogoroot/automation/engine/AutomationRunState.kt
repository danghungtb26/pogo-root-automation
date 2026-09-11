package dev.pogoroot.automation.engine

import java.util.concurrent.atomic.AtomicBoolean

/** Volatile process-local master switch; it is intentionally never persisted. */
object AutomationRunState {
    private val active = AtomicBoolean(false)

    fun isActive(): Boolean = active.get()

    internal fun resetForProcessStart() {
        active.set(false)
    }

    internal fun setActive(value: Boolean) {
        active.set(value)
    }
}
