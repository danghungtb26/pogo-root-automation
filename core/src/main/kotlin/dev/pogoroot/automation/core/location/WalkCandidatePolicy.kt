package dev.pogoroot.automation.core.location

import dev.pogoroot.automation.core.model.GeoPoint

/** Pure admission policy. Callers supply all clock, session, sequence, and generation inputs. */
object WalkCandidatePolicy {
    const val DEFAULT_MAX_AGE_NANOS = 5_000_000_000L
    const val DEFAULT_FUTURE_SKEW_NANOS = 1_000_000_000L
    const val MAX_CANDIDATE_ID_LENGTH = 128

    fun submit(
        candidate: NativeWalkCandidate,
        activeRoute: ActiveWalkRoute?,
        expectedSessionId: String,
        nowNanos: Long,
        lastProcessedSequence: Long,
        nextGeneration: Long,
        maxAgeNanos: Long = DEFAULT_MAX_AGE_NANOS,
        futureSkewNanos: Long = DEFAULT_FUTURE_SKEW_NANOS,
    ): WalkCandidatePolicyResult {
        val invalidReason = validateCandidate(
            candidate = candidate,
            expectedSessionId = expectedSessionId,
            nowNanos = nowNanos,
            lastProcessedSequence = lastProcessedSequence,
            nextGeneration = nextGeneration,
            maxAgeNanos = maxAgeNanos,
            futureSkewNanos = futureSkewNanos,
        )
        if (invalidReason != null) {
            val stale = invalidReason.startsWith("stale:")
            return result(
                if (stale) WalkCandidateDecision.REJECTED_STALE else WalkCandidateDecision.REJECTED_INVALID,
                activeRoute,
                reason = invalidReason.removePrefix("stale:").trim(),
            )
        }

        val (current, invalidated) = currentNativeRoute(activeRoute, expectedSessionId)

        if (current?.owner == WalkRouteOwner.USER) {
            return result(
                WalkCandidateDecision.REJECTED_OWNER_CONFLICT,
                current,
                invalidatedRoute = invalidated,
                reason = "user-owned location route has priority",
            )
        }

        if (current != null && current.candidateId == candidate.candidateId) {
            if (current.target != candidate.target) {
                return result(
                    WalkCandidateDecision.REJECTED_INVALID,
                    current,
                    reason = "candidate ID reused with a different target",
                )
            }
            return result(WalkCandidateDecision.UNCHANGED, current)
        }

        if (current != null && !candidate.force) {
            return result(WalkCandidateDecision.IGNORED_BUSY, current)
        }
        val previousGeneration = listOfNotNull(current, invalidated)
            .firstOrNull { it.owner == WalkRouteOwner.NATIVE && it.sessionId == candidate.sessionId }
            ?.generation
        if (previousGeneration != null && nextGeneration <= previousGeneration) {
            return result(
                WalkCandidateDecision.REJECTED_INVALID,
                current ?: invalidated,
                reason = "replacement generation must increase",
            )
        }

        val accepted = ActiveWalkRoute(
            owner = WalkRouteOwner.NATIVE,
            target = candidate.target,
            candidateId = candidate.candidateId,
            sessionId = candidate.sessionId,
            generation = nextGeneration,
        )
        val decision = if (current == null) {
            WalkCandidateDecision.ACCEPTED
        } else {
            WalkCandidateDecision.REPLACED
        }
        return result(decision, accepted, invalidatedRoute = current ?: invalidated)
    }

    fun terminate(
        terminal: WalkCandidateTerminal,
        activeRoute: ActiveWalkRoute?,
        expectedSessionId: String,
        lastProcessedSequence: Long,
    ): WalkCandidatePolicyResult {
        if (expectedSessionId.isBlank() || terminal.sessionId != expectedSessionId) {
            return result(
                WalkCandidateDecision.REJECTED_STALE,
                activeRoute,
                reason = "terminal session does not match the active runtime",
            )
        }
        if (terminal.sequence <= lastProcessedSequence || terminal.sequence <= 0L) {
            return result(WalkCandidateDecision.REJECTED_STALE, activeRoute, reason = "terminal sequence is stale")
        }
        if (terminal.candidateId.isBlank() || terminal.generation <= 0L) {
            return result(WalkCandidateDecision.REJECTED_INVALID, activeRoute, reason = "terminal identity is invalid")
        }

        val matches = activeRoute?.owner == WalkRouteOwner.NATIVE &&
            activeRoute.candidateId == terminal.candidateId &&
            activeRoute.sessionId == terminal.sessionId &&
            activeRoute.generation == terminal.generation
        if (!matches) {
            return result(
                WalkCandidateDecision.IGNORED_STALE_TERMINAL,
                activeRoute,
                reason = "terminal does not identify the active route generation",
            )
        }

        val decision = when (terminal.kind) {
            WalkCandidateTerminalKind.ARRIVED -> WalkCandidateDecision.COMPLETED
            WalkCandidateTerminalKind.STOP,
            WalkCandidateTerminalKind.CANCELLED -> WalkCandidateDecision.CANCELLED
        }
        return result(decision, activeRoute = null, invalidatedRoute = activeRoute)
    }

    private fun validateCandidate(
        candidate: NativeWalkCandidate,
        expectedSessionId: String,
        nowNanos: Long,
        lastProcessedSequence: Long,
        nextGeneration: Long,
        maxAgeNanos: Long,
        futureSkewNanos: Long,
    ): String? {
        if (candidate.sessionId != expectedSessionId || expectedSessionId.isBlank()) {
            return "stale: candidate session does not match the active runtime"
        }
        if (candidate.sequence <= lastProcessedSequence || candidate.sequence <= 0L || lastProcessedSequence < 0L) {
            return "stale: candidate sequence is not strictly increasing"
        }
        if (candidate.observedAtNanos <= 0L || nowNanos <= 0L || maxAgeNanos <= 0L || futureSkewNanos < 0L) {
            return "stale: candidate time bounds are invalid"
        }
        if (candidate.observedAtNanos > nowNanos) {
            if (candidate.observedAtNanos - nowNanos > futureSkewNanos) {
                return "stale: candidate timestamp is too far in the future"
            }
        } else if (nowNanos - candidate.observedAtNanos > maxAgeNanos) {
            return "stale: candidate timestamp exceeds maximum age"
        }
        if (candidate.candidateId.isBlank() || candidate.candidateId.length > MAX_CANDIDATE_ID_LENGTH) {
            return "candidate ID is empty or exceeds the domain limit"
        }
        if (!candidate.target.isValidCoordinate()) return "candidate coordinate is invalid"
        if (nextGeneration <= 0L) return "next generation must be positive"
        return null
    }

    private fun currentNativeRoute(
        route: ActiveWalkRoute?,
        expectedSessionId: String,
    ): Pair<ActiveWalkRoute?, ActiveWalkRoute?> {
        if (route?.owner != WalkRouteOwner.NATIVE) return route to null
        if (route.sessionId != expectedSessionId) {
            return null to route
        }
        return route to null
    }

    private fun GeoPoint.isValidCoordinate(): Boolean =
        latitude.isFinite() && longitude.isFinite() &&
            latitude in -90.0..90.0 && longitude in -180.0..180.0

    private fun result(
        decision: WalkCandidateDecision,
        activeRoute: ActiveWalkRoute?,
        invalidatedRoute: ActiveWalkRoute? = null,
        reason: String? = null,
    ) = WalkCandidatePolicyResult(decision, activeRoute, invalidatedRoute, reason)
}
