package dev.pogoroot.automation.location

import dev.pogoroot.automation.bridge.RuntimeNavigationKind
import dev.pogoroot.automation.bridge.RuntimeNavigationPayload
import dev.pogoroot.automation.core.automation.AutoFortNavigationCommand
import dev.pogoroot.automation.events.AutomationEvent
import dev.pogoroot.automation.events.AutomationEventSink
import dev.pogoroot.automation.events.AutomationEventType

/** UI/service hand-off only. Native owns target selection, pauses and arrival. */
class NativeNavigationReceiver(
    private val eventSink: AutomationEventSink,
    private val commandSink: (AutoFortNavigationCommand, Long) -> Unit,
    private val nowNanos: () -> Long = System::nanoTime,
) {
    private var lastCommand: AutoFortNavigationCommand? = null

    fun receive(payload: RuntimeNavigationPayload, observedAtNanos: Long) {
        val now = nowNanos()
        if (observedAtNanos <= 0 || observedAtNanos > now || now - observedAtNanos > LEASE_NS) {
            reset("native navigation expired")
            return
        }
        if (payload.kind == RuntimeNavigationKind.ARRIVED) {
            eventSink.publish(AutomationEvent(AutomationEventType.INFO, "Arrived at PokéStop ${payload.fortId}"))
            return
        }
        val command = if (payload.kind == RuntimeNavigationKind.WALK) {
            AutoFortNavigationCommand.WalkTo(payload.fortId, payload.target)
        } else {
            AutoFortNavigationCommand.Stop(payload.reason)
        }
        commandSink(command, observedAtNanos + LEASE_NS)
        if (command != lastCommand) {
            val message = when (command) {
                is AutoFortNavigationCommand.WalkTo -> "Walking to PokéStop ${command.fortId}"
                is AutoFortNavigationCommand.Stop -> "Auto-walk stopped: ${command.reason}"
            }
            eventSink.publish(AutomationEvent(AutomationEventType.INFO, message))
        }
        lastCommand = command
    }

    fun reset(reason: String = "runtime disconnected") {
        val stopped = AutoFortNavigationCommand.Stop(reason)
        commandSink(stopped, 0L)
        lastCommand = stopped
    }

    companion object {
        const val LEASE_NS = 5_000_000_000L
    }
}
