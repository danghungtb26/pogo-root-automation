package dev.pogoroot.automation.engine

data class CatchSpinArmSnapshot(
    val armed: Boolean,
    val runtimeSessionId: String?,
    val generation: Long,
    val reason: String,
)

/** Process-local permission to run catch/spin for one exact runtime session. */
object CatchSpinArmState {
    private var state = CatchSpinArmSnapshot(false, null, 0L, "process_start")

    @Synchronized
    fun arm(sessionId: String): CatchSpinArmSnapshot {
        require(sessionId.isNotBlank()) { "runtime session id is required to arm" }
        state = CatchSpinArmSnapshot(true, sessionId, state.generation + 1L, "explicit_user_action")
        return state
    }

    @Synchronized
    fun disarm(reason: String): Boolean {
        val changed = state.armed || state.runtimeSessionId != null
        state = CatchSpinArmSnapshot(
            false,
            null,
            state.generation + if (changed) 1L else 0L,
            reason.ifBlank { "unspecified" },
        )
        return changed
    }

    @Synchronized
    fun snapshot(sessionId: String?): Boolean = state.armed &&
        !sessionId.isNullOrBlank() && state.runtimeSessionId == sessionId

    @Synchronized
    fun current(): CatchSpinArmSnapshot = state

    @Synchronized
    fun isArmed(): Boolean = state.armed
}
