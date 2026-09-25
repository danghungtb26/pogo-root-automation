package dev.pogoroot.automation.location

import dev.pogoroot.automation.core.location.ActiveWalkRoute
import dev.pogoroot.automation.core.location.NativeWalkCandidate
import dev.pogoroot.automation.core.location.WalkCandidateDecision
import dev.pogoroot.automation.core.location.WalkCandidatePolicy
import dev.pogoroot.automation.core.location.WalkCandidatePolicyResult
import dev.pogoroot.automation.core.location.WalkCandidateTerminal
import dev.pogoroot.automation.core.location.WalkCandidateTerminalKind
import dev.pogoroot.automation.core.location.WalkRouteOwner
import dev.pogoroot.automation.core.model.GeoPoint
import android.util.Log

interface WalkCandidateLocationExecutor {
    fun isReady(): Boolean
    fun walkTo(target: GeoPoint, generation: Long): Result<Unit>
    fun stopWalking()
    fun teleport(target: GeoPoint)
    fun setJoystick(angleDegrees: Int, strengthPercent: Int, generation: Long?)
}

/** Serializes native admission, user ownership, and terminal effects. */
object WalkCandidateCoordinator {
    private val lock = Any()
    private val retiredCandidateIds = mutableSetOf<String>()
    private var locationExecutor: WalkCandidateLocationExecutor? = null
    private var activeRoute: ActiveWalkRoute? = null
    private var runtimeSessionId: String? = null
    // Keep generations process-monotonic so late location callbacks cannot match a new session's route.
    private var nextGeneration = 1L

    fun attach(executor: WalkCandidateLocationExecutor) {
        synchronized(lock) {
            locationExecutor = executor
        }
    }

    fun detach(executor: WalkCandidateLocationExecutor) {
        synchronized(lock) {
            if (locationExecutor !== executor) return
            invalidateNativeRoute(stopMovement = true)
            activeRoute = null
            locationExecutor = null
        }
    }

    fun submit(
        candidate: NativeWalkCandidate,
        nowNanos: Long,
        lastProcessedSequence: Long,
    ): WalkCandidatePolicyResult = synchronized(lock) {
        if (runtimeSessionId != candidate.sessionId) beginSession(candidate.sessionId)
        val current = activeRoute
        if (candidate.candidateId in retiredCandidateIds && current?.candidateId != candidate.candidateId) {
            return@synchronized rejected(current, "candidate ID was already active in this session")
        }
        val executor = locationExecutor
        if (executor == null || !executor.isReady()) {
            return@synchronized rejected(current, "location provider is not ready")
        }

        val decision = WalkCandidatePolicy.submit(
            candidate = candidate,
            activeRoute = current,
            expectedSessionId = candidate.sessionId,
            nowNanos = nowNanos,
            lastProcessedSequence = lastProcessedSequence,
            nextGeneration = nextGeneration,
        )
        when (decision.decision) {
            WalkCandidateDecision.ACCEPTED,
            WalkCandidateDecision.REPLACED -> admitNewRoute(decision, executor)
            else -> decision
        }
    }

    fun terminate(
        intent: NativeWalkTerminalIntent,
        lastProcessedSequence: Long,
    ): WalkCandidatePolicyResult = synchronized(lock) {
        val current = activeRoute
        val generation = current?.takeIf {
            it.owner == WalkRouteOwner.NATIVE &&
                it.sessionId == intent.sessionId &&
                it.candidateId == intent.candidateId
        }?.generation
        if (generation == null) {
            return@synchronized WalkCandidatePolicyResult(
                WalkCandidateDecision.IGNORED_STALE_TERMINAL,
                current,
                reason = "terminal does not identify the active route",
            )
        }
        val kind = when (intent.kind) {
            dev.pogoroot.automation.bridge.PointWalkCandidateKind.STOP -> WalkCandidateTerminalKind.STOP
            dev.pogoroot.automation.bridge.PointWalkCandidateKind.ARRIVED -> WalkCandidateTerminalKind.ARRIVED
            dev.pogoroot.automation.bridge.PointWalkCandidateKind.WALK ->
                return@synchronized rejected(current, "walk payload cannot be a terminal")
        }
        val decision = WalkCandidatePolicy.terminate(
            terminal = WalkCandidateTerminal(
                kind = kind,
                candidateId = intent.candidateId,
                sessionId = intent.sessionId,
                generation = generation,
                sequence = intent.sequence,
            ),
            activeRoute = current,
            expectedSessionId = runtimeSessionId ?: return@synchronized rejected(
                current,
                "runtime session is not active",
            ),
            lastProcessedSequence = lastProcessedSequence,
        )
        val invalidatedRoute = decision.invalidatedRoute
        if (decision.activeRoute == null && invalidatedRoute != null) {
            retire(invalidatedRoute)
            locationExecutor?.stopWalking()
            activeRoute = null
        }
        decision
    }

    fun userWalkTo(target: GeoPoint): Result<Unit> = synchronized(lock) {
        val executor = locationExecutor
            ?: return@synchronized Result.failure(IllegalStateException("location provider is not ready"))
        if (!executor.isReady()) {
            return@synchronized Result.failure(IllegalStateException("location provider is not ready"))
        }
        invalidateNativeRoute(stopMovement = false)
        val route = ActiveWalkRoute(
            owner = WalkRouteOwner.USER,
            target = target,
            generation = allocateGeneration(),
        )
        activeRoute = route
        executor.walkTo(target, route.generation).onFailure {
            if (activeRoute == route) {
                activeRoute = null
                executor.stopWalking()
            }
        }
    }

    fun userTeleport(target: GeoPoint) = synchronized(lock) {
        invalidateNativeRoute(stopMovement = false)
        activeRoute = null
        runCatching { locationExecutor?.teleport(target) }
            .onFailure { locationExecutor?.stopWalking() }
            .getOrThrow()
    }

    fun userJoystick(angleDegrees: Int, strengthPercent: Int) = synchronized(lock) {
        invalidateNativeRoute(stopMovement = false)
        val route = if (strengthPercent > 0) {
            ActiveWalkRoute(
                owner = WalkRouteOwner.USER,
                target = null,
                generation = allocateGeneration(),
            ).also { activeRoute = it }
        } else if (activeRoute?.owner == WalkRouteOwner.USER) {
            activeRoute = null
            null
        } else {
            null
        }
        locationExecutor?.setJoystick(angleDegrees, strengthPercent, route?.generation)
    }

    fun onLocationState(state: JoystickLocationState) = synchronized(lock) {
        val route = activeRoute ?: return@synchronized
        if (state.walkGeneration != route.generation) return@synchronized
        if (!state.providerReady || state.walkStatus == WalkStatus.ERROR) {
            if (route.owner == WalkRouteOwner.NATIVE) retire(route)
            activeRoute = null
            locationExecutor?.stopWalking()
            return@synchronized
        }
        val target = route.target ?: return@synchronized
        if (state.walkStatus != WalkStatus.ARRIVED || state.point == null) return@synchronized
        val distance = dev.pogoroot.automation.core.location.GeoMath.distanceMeters(state.point, target)
        if (distance <= JoystickLocationState.DEFAULT_WALK_TOLERANCE_METERS) {
            if (route.owner == WalkRouteOwner.NATIVE) retire(route)
            activeRoute = null
        }
    }

    /** Session reset invalidates native routes; a user-owned walk stays user-owned. */
    fun reset(reason: String = "runtime disconnected") = synchronized(lock) {
        invalidateNativeRoute(stopMovement = true)
        runtimeSessionId = null
        retiredCandidateIds.clear()
        Log.i(LOG_TAG, "reset native walk candidate coordinator: $reason")
    }

    fun activeRouteSnapshot(): ActiveWalkRoute? = synchronized(lock) { activeRoute }

    private fun admitNewRoute(
        decision: WalkCandidatePolicyResult,
        executor: WalkCandidateLocationExecutor,
    ): WalkCandidatePolicyResult {
        val accepted = decision.activeRoute ?: return rejected(activeRoute, "policy returned no active route")
        decision.invalidatedRoute?.let(::retire)
        activeRoute = accepted
        nextGeneration = maxOf(nextGeneration, accepted.generation + 1L)
        val dispatch = executor.walkTo(
            accepted.target ?: return failClosed(accepted, executor, "accepted route has no target"),
            accepted.generation,
        )
        if (dispatch.isFailure) return failClosed(
            accepted,
            executor,
            dispatch.exceptionOrNull()?.message ?: "location executor rejected route",
        )
        return decision
    }

    private fun failClosed(
        route: ActiveWalkRoute,
        executor: WalkCandidateLocationExecutor,
        reason: String,
    ): WalkCandidatePolicyResult {
        retire(route)
        if (activeRoute == route) activeRoute = null
        executor.stopWalking()
        return WalkCandidatePolicyResult(
            WalkCandidateDecision.REJECTED_INVALID,
            activeRoute,
            invalidatedRoute = route,
            reason = reason,
        )
    }

    private fun beginSession(sessionId: String) {
        invalidateNativeRoute(stopMovement = true)
        runtimeSessionId = sessionId
        retiredCandidateIds.clear()
    }

    private fun invalidateNativeRoute(stopMovement: Boolean) {
        val route = activeRoute?.takeIf { it.owner == WalkRouteOwner.NATIVE } ?: return
        retire(route)
        activeRoute = null
        if (stopMovement) locationExecutor?.stopWalking()
    }

    private fun retire(route: ActiveWalkRoute) {
        if (route.owner == WalkRouteOwner.NATIVE) route.candidateId?.let(retiredCandidateIds::add)
    }

    private fun allocateGeneration(): Long {
        val value = nextGeneration
        nextGeneration = if (nextGeneration < Long.MAX_VALUE) nextGeneration + 1L else 1L
        return value
    }

    private fun rejected(route: ActiveWalkRoute?, reason: String) = WalkCandidatePolicyResult(
        WalkCandidateDecision.REJECTED_INVALID,
        route,
        reason = reason,
    )

    private const val LOG_TAG = "WalkCandidateCoordinator"
}
