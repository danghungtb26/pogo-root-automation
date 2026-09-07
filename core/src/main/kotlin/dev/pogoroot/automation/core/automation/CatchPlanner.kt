package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.EncounterSnapshot

data class CatchDecision(
    val shouldCatch: Boolean,
    val reason: CatchReason? = null,
)

class CatchPlanner {
    fun decide(
        encounter: EncounterSnapshot,
        policy: CatchPolicy,
    ): CatchDecision {
        if (encounter.isShundo) {
            return CatchDecision(true, CatchReason.SHUNDO)
        }
        if (policy.alwaysCatchShiny && encounter.shiny == true) {
            return CatchDecision(true, CatchReason.SHINY)
        }
        if (policy.alwaysCatchHundo && encounter.isHundo) {
            return CatchDecision(true, CatchReason.HUNDO)
        }

        val threshold = policy.minimumIvPercent
        val ivPercentage = encounter.iv?.percentage
        if (threshold != null && ivPercentage != null && ivPercentage >= threshold) {
            return CatchDecision(true, CatchReason.IV_THRESHOLD)
        }

        return if (policy.catchAll) {
            CatchDecision(true, CatchReason.CATCH_ALL)
        } else {
            CatchDecision(false)
        }
    }
}
