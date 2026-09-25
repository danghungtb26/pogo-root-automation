package dev.pogoroot.automation.core.location

import dev.pogoroot.automation.core.model.GeoPoint

/** Candidate emitted by a native producer for Kotlin-owned fake-location execution. */
data class NativeWalkCandidate(
    val candidateId: String,
    val target: GeoPoint,
    val force: Boolean,
    val sessionId: String,
    val sequence: Long,
    val observedAtNanos: Long,
)

enum class WalkRouteOwner {
    NATIVE,
    USER,
}

/** The one route currently authorized to own the location executor. */
data class ActiveWalkRoute(
    val owner: WalkRouteOwner,
    val target: GeoPoint?,
    val generation: Long,
    val candidateId: String? = null,
    val sessionId: String? = null,
)

enum class WalkCandidateDecision {
    ACCEPTED,
    IGNORED_BUSY,
    REPLACED,
    UNCHANGED,
    REJECTED_OWNER_CONFLICT,
    REJECTED_STALE,
    REJECTED_INVALID,
    IGNORED_STALE_TERMINAL,
    COMPLETED,
    CANCELLED,
}

enum class WalkCandidateTerminalKind {
    STOP,
    ARRIVED,
    CANCELLED,
}

/** Terminal events must identify the route generation they intend to end. */
data class WalkCandidateTerminal(
    val kind: WalkCandidateTerminalKind,
    val candidateId: String,
    val sessionId: String,
    val generation: Long,
    val sequence: Long,
)

data class WalkCandidatePolicyResult(
    val decision: WalkCandidateDecision,
    val activeRoute: ActiveWalkRoute?,
    /** Route invalidated by a session mismatch or an accepted replacement/terminal. */
    val invalidatedRoute: ActiveWalkRoute? = null,
    val reason: String? = null,
)
