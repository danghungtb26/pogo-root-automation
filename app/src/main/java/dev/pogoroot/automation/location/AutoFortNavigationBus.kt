package dev.pogoroot.automation.location

import dev.pogoroot.automation.core.automation.AutoFortNavigationCommand

/**
 * Process-local hand-off between the headless runtime loop and the mock-location
 * service. The latest command is replayed when the location service starts.
 */
internal object AutoFortNavigationBus {
    private val lock = Any()
    private val handler = android.os.Handler(android.os.Looper.getMainLooper())
    private var generation = 0L
    private var expiresAtNanos = 0L
    private var listener: ((AutoFortNavigationCommand) -> Unit)? = null
    private var latestCommand: AutoFortNavigationCommand =
        AutoFortNavigationCommand.Stop("navigation idle")

    fun publish(command: AutoFortNavigationCommand, expiresAt: Long) {
        val token: Long
        synchronized(lock) {
            token = ++generation
            expiresAtNanos = expiresAt
            val changed = latestCommand != command
            latestCommand = command
            if (changed) listener?.invoke(command)
        }
        if (command is AutoFortNavigationCommand.WalkTo) {
            val delayMs = ((expiresAt - System.nanoTime()) / 1_000_000L).coerceAtLeast(0L)
            handler.postDelayed({ expire(token) }, delayMs + 1L)
        }
    }

    private fun expire(token: Long) {
        synchronized(lock) {
            if (token != generation || System.nanoTime() < expiresAtNanos) return
            val stop = AutoFortNavigationCommand.Stop("native navigation lease expired")
            latestCommand = stop
            listener?.invoke(stop)
        }
    }

    fun register(commandListener: (AutoFortNavigationCommand) -> Unit) {
        synchronized(lock) {
            listener = commandListener
            if (latestCommand is AutoFortNavigationCommand.WalkTo && System.nanoTime() >= expiresAtNanos) {
                latestCommand = AutoFortNavigationCommand.Stop("native navigation lease expired")
            }
            commandListener(latestCommand)
        }
    }

    fun unregister() {
        synchronized(lock) {
            listener = null
        }
    }
}
