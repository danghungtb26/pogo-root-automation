package dev.pogoroot.automation.kernel

import dev.pogoroot.automation.runtime.ExecutionMode

/**
 * Shared/exclusive execution lock (readers-writers style) that arbitrates which
 * modules may run an action right now (see docs/automation-flow.md — Phần 3.2c).
 *
 * - An [ExecutionMode.EXCLUSIVE] holder (catch_spin / encounter) blocks everyone,
 *   including other EXCLUSIVE modules.
 * - [ExecutionMode.CONCURRENT] holders (transfer / discard) run alongside each
 *   other, but never while an EXCLUSIVE lock is held.
 *
 * Acquisition is non-blocking (`tryAcquire`): a module that cannot acquire this
 * tick simply defers and retries on the next event/tick — there is no waiting,
 * which keeps the single kernel thread free (a stuck EXCLUSIVE holder is bounded
 * by the arbiter's per-action timeout that eventually [release]s it).
 *
 * Each acquisition is keyed by the owner module tag so a mismatched release
 * cannot free someone else's hold. Thread-safe for a future move to per-module
 * threads even though the kernel drives it from one thread today.
 */
class ExecutionLock {
    private sealed interface State {
        object Idle : State
        data class Exclusive(val owner: String) : State
        data class Concurrent(val owners: Set<String>) : State
    }

    private var state: State = State.Idle

    /** True when no exclusive hold is active (concurrent work may proceed). */
    @Synchronized
    fun isExclusiveHeld(): Boolean = state is State.Exclusive

    @Synchronized
    fun isIdle(): Boolean = state === State.Idle

    /**
     * Try to acquire for [owner] under [mode]. Returns true on success.
     * - EXCLUSIVE succeeds only from [State.Idle].
     * - CONCURRENT succeeds from [State.Idle] or an existing [State.Concurrent]
     *   (adds the owner), never while EXCLUSIVE is held.
     * Re-acquiring while already holding is idempotent and returns true.
     */
    @Synchronized
    fun tryAcquire(owner: String, mode: ExecutionMode): Boolean = when (mode) {
        ExecutionMode.EXCLUSIVE -> when (val s = state) {
            State.Idle -> { state = State.Exclusive(owner); true }
            is State.Exclusive -> s.owner == owner // idempotent for the same holder
            is State.Concurrent -> false
        }
        ExecutionMode.CONCURRENT -> when (val s = state) {
            State.Idle -> { state = State.Concurrent(setOf(owner)); true }
            is State.Concurrent -> { state = State.Concurrent(s.owners + owner); true }
            is State.Exclusive -> false
        }
    }

    /** Release [owner]'s hold. No-op if [owner] does not currently hold. */
    @Synchronized
    fun release(owner: String) {
        when (val s = state) {
            State.Idle -> Unit
            is State.Exclusive -> if (s.owner == owner) state = State.Idle
            is State.Concurrent -> {
                val remaining = s.owners - owner
                state = if (remaining.isEmpty()) State.Idle else State.Concurrent(remaining)
            }
        }
    }

    /** Force the lock back to idle (used on teardown / session reset). */
    @Synchronized
    fun reset() {
        state = State.Idle
    }
}
