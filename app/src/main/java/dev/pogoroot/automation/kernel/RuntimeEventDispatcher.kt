package dev.pogoroot.automation.kernel

import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.ObservationType

/**
 * A consumer of routed runtime events. Each client lane/module declares which
 * [ObservationType]s it wants and which command results it owns, so the
 * [RuntimeEventDispatcher] can fan a single native event stream out to the right
 * place instead of a monolithic `when(type)` (see docs/automation-flow.md —
 * Phần 3.1). Implementations must not block: routing runs on the single kernel
 * thread and a slow handler stalls every other lane.
 */
interface RuntimeEventSubscriber {
    /** Observation types this subscriber consumes. */
    val observationTypes: Set<ObservationType>

    /** True when [commandId] belongs to a mutation this subscriber submitted. */
    fun ownsCommand(commandId: String): Boolean

    /** Handle an owned observation. */
    fun onObservation(event: BridgeEvent.ObservationEvent)

    /** Handle a command result correlated back to this subscriber. */
    fun onResult(event: BridgeEvent.AutomationCommandResult)
}

/**
 * Owns the single native event stream and fans each drained event out to the one
 * subscriber that owns it — observations by [ObservationType], command results by
 * `commandId`. Binding-lost / runtime-error are broadcast to dedicated sinks;
 * anything unrouted goes to [onUnrouted] (kept, not dropped, so nothing is lost
 * silently).
 *
 * Pure routing only: it holds no automation state and never talks to the bridge.
 * The first subscriber whose [RuntimeEventSubscriber.observationTypes] contains
 * the type wins, so registration order defines precedence for overlapping claims.
 */
class RuntimeEventDispatcher(
    private val subscribers: List<RuntimeEventSubscriber>,
    private val onBindingLost: (BridgeEvent.BindingLost) -> Unit = {},
    private val onRuntimeError: (BridgeEvent.RuntimeError) -> Unit = {},
    private val onUnrouted: (BridgeEvent) -> Unit = {},
) {
    fun dispatch(events: List<BridgeEvent>) {
        for (event in events) route(event)
    }

    private fun route(event: BridgeEvent) {
        when (event) {
            is BridgeEvent.ObservationEvent ->
                (subscribers.firstOrNull { event.observationType in it.observationTypes }
                    ?.onObservation(event) ?: onUnrouted(event))

            is BridgeEvent.AutomationCommandResult ->
                (subscribers.firstOrNull { it.ownsCommand(event.commandId) }
                    ?.onResult(event) ?: onUnrouted(event))

            is BridgeEvent.BindingLost -> onBindingLost(event)
            is BridgeEvent.RuntimeError -> onRuntimeError(event)
            else -> onUnrouted(event)
        }
    }
}
