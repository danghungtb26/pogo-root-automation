package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.GameLifecycleState
import java.util.UUID

/** Transport-neutral action request retained for adapter submission contracts. */
data class ActionRequest(
    val commandId: String,
    val runtimeSessionId: String,
    val action: AutomationAction,
    val basedOnObservationSeq: Long,
    val expectedLifecycle: GameLifecycleState?,
    val createdAtEpochMs: Long,
    val createdAtElapsedNs: Long,
    val expiresAtElapsedNs: Long,
    val settleDelayNs: Long = 0L,
    val pid: Int? = null,
    val processName: String? = null,
    val packageName: String? = null,
    val buildFingerprint: String? = null,
) {
    init {
        require(commandId.isNotBlank()) { "commandId must not be blank" }
        require(runtimeSessionId.isNotBlank()) { "runtimeSessionId must not be blank" }
        require(basedOnObservationSeq > 0L) { "basedOnObservationSeq must be positive" }
        require(expiresAtElapsedNs >= createdAtElapsedNs) {
            "expiresAtElapsedNs must not precede createdAtElapsedNs"
        }
        require(settleDelayNs >= 0L) { "settleDelayNs must not be negative" }
        require(pid == null || pid > 0) { "pid must be positive when present" }
    }

    companion object {
        fun create(
            runtimeSessionId: String,
            action: AutomationAction,
            basedOnObservationSeq: Long,
            expectedLifecycle: GameLifecycleState?,
            createdAtEpochMs: Long,
            createdAtElapsedNs: Long,
            timeoutNs: Long,
            settleDelayNs: Long = 0L,
            pid: Int? = null,
            processName: String? = null,
            packageName: String? = null,
            buildFingerprint: String? = null,
            commandId: String = UUID.randomUUID().toString(),
        ): ActionRequest {
            require(timeoutNs >= 0L) { "timeoutNs must not be negative" }
            return ActionRequest(
                commandId = commandId,
                runtimeSessionId = runtimeSessionId,
                action = action,
                basedOnObservationSeq = basedOnObservationSeq,
                expectedLifecycle = expectedLifecycle,
                createdAtEpochMs = createdAtEpochMs,
                createdAtElapsedNs = createdAtElapsedNs,
                expiresAtElapsedNs = createdAtElapsedNs + timeoutNs,
                settleDelayNs = settleDelayNs,
                pid = pid,
                processName = processName,
                packageName = packageName,
                buildFingerprint = buildFingerprint,
            )
        }
    }
}
