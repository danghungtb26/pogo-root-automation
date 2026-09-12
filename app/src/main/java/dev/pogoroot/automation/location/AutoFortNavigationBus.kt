package dev.pogoroot.automation.location

import dev.pogoroot.automation.core.automation.AutoFortNavigationCommand

/**
 * Process-local hand-off between the headless runtime loop and the mock-location
 * service. The latest command is replayed when the location service starts.
 */
internal object AutoFortNavigationBus {
    private val lock = Any()
    private var listener: ((AutoFortNavigationCommand) -> Unit)? = null
    private var latestCommand: AutoFortNavigationCommand =
        AutoFortNavigationCommand.Stop("navigation idle")

    fun publish(command: AutoFortNavigationCommand) {
        val currentListener = synchronized(lock) {
            latestCommand = command
            listener
        }
        currentListener?.invoke(command)
    }

    fun register(commandListener: (AutoFortNavigationCommand) -> Unit) {
        val currentCommand = synchronized(lock) {
            listener = commandListener
            latestCommand
        }
        commandListener(currentCommand)
    }

    fun unregister() {
        synchronized(lock) {
            listener = null
        }
    }
}
