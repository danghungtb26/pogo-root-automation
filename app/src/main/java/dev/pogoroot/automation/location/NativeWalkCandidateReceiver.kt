package dev.pogoroot.automation.location

import dev.pogoroot.automation.bridge.PointWalkCandidateKind
import dev.pogoroot.automation.bridge.PointWalkCandidatePayload
import dev.pogoroot.automation.core.location.NativeWalkCandidate

data class NativeWalkCandidateObservation(
    val payload: PointWalkCandidatePayload,
    val sessionId: String,
    val sequence: Long,
    val observedAtNanos: Long,
)

data class NativeWalkTerminalIntent(
    val kind: PointWalkCandidateKind,
    val candidateId: String,
    val sessionId: String,
    val sequence: Long,
    val observedAtNanos: Long,
)

/** Session/freshness guard between the UI event router and route admission. */
class NativeWalkCandidateReceiver(
    private val onCandidate: (NativeWalkCandidate, Long, Long) -> Unit = { _, _, _ -> },
    private val onTerminal: (NativeWalkTerminalIntent, Long) -> Unit = { _, _ -> },
    private val onReset: (String) -> Unit = {},
    private val nowNanos: () -> Long = System::nanoTime,
) {
    private var activeSessionId: String? = null
    private var lastCandidateSequence = 0L

    fun receive(observation: NativeWalkCandidateObservation, previousObservationSequence: Long) {
        if (observation.sessionId.isBlank() || observation.sequence <= previousObservationSequence ||
            previousObservationSequence < 0L || observation.sequence <= 0L) return

        if (activeSessionId != observation.sessionId) {
            if (activeSessionId != null) onReset("runtime session changed")
            activeSessionId = observation.sessionId
            lastCandidateSequence = 0L
        }
        if (observation.sequence <= lastCandidateSequence) return
        // Consume an ingress-valid candidate sequence even when stale or ignored by admission.
        lastCandidateSequence = observation.sequence

        val now = nowNanos()
        if (!isFresh(observation.observedAtNanos, now)) return
        when (observation.payload.kind) {
            PointWalkCandidateKind.WALK -> {
                val target = observation.payload.target ?: return
                onCandidate(
                    NativeWalkCandidate(
                        candidateId = observation.payload.candidateId,
                        target = target,
                        force = observation.payload.force,
                        sessionId = observation.sessionId,
                        sequence = observation.sequence,
                        observedAtNanos = observation.observedAtNanos,
                    ),
                    now,
                    previousObservationSequence,
                )
            }
            PointWalkCandidateKind.STOP,
            PointWalkCandidateKind.ARRIVED -> onTerminal(
                NativeWalkTerminalIntent(
                    kind = observation.payload.kind,
                    candidateId = observation.payload.candidateId,
                    sessionId = observation.sessionId,
                    sequence = observation.sequence,
                    observedAtNanos = observation.observedAtNanos,
                ),
                previousObservationSequence,
            )
        }
    }

    fun reset(reason: String = "runtime disconnected") {
        activeSessionId = null
        lastCandidateSequence = 0L
        onReset(reason)
    }

    private fun isFresh(observedAtNanos: Long, now: Long): Boolean {
        if (observedAtNanos <= 0L || now <= 0L) return false
        return if (observedAtNanos > now) {
            observedAtNanos - now <= CLOCK_SKEW_NS
        } else {
            now - observedAtNanos <= MAX_AGE_NS
        }
    }

    private companion object {
        const val CLOCK_SKEW_NS = 1_000_000_000L
        const val MAX_AGE_NS = 30_000_000_000L
    }
}
