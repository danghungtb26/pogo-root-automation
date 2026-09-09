package dev.pogoroot.automation.core.automation

/**
 * A runtime must provide an authoritative catch result before the runner can
 * release a catch mutation. In particular, a transport acknowledgement or a
 * client-side throw invocation is not proof that the server caught the
 * Pokémon.
 */
internal fun ActionExecution.validateCatchCompletion(): ActionExecution {
    if (phase != ActionExecutionPhase.COMPLETED) return this
    val catchAction = request.action as? AutomationAction.Catch ?: return this
    val outcome = catchOutcome
    if (outcome == null || outcome == CatchOutcome.INDETERMINATE) {
        return copy(
            phase = ActionExecutionPhase.INDETERMINATE,
            errorCode = "catch_outcome_unavailable",
            message = "catch result was not authoritatively observed",
        )
    }
    if (catchAction.throwProfile.requiresStructuredOutcome &&
        (throwOutcome == null || !throwOutcome.isAttempt)
    ) {
        return copy(
            phase = ActionExecutionPhase.INDETERMINATE,
            errorCode = "throw_outcome_unavailable",
            message = "requested throw quality was not authoritatively observed",
        )
    }
    return this
}
