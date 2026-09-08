package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.GameLifecycleState
import java.util.UUID

/**
 * Execution metadata deliberately lives outside [AutomationAction].
 * AutomationAction remains a pure domain intent that can be planned and tested
 * without a runtime, a process identity, or a transport.
 */
data class ActionRequest(
    val commandId: String,
    val runtimeSessionId: String,
    val action: AutomationAction,
    val basedOnObservationSeq: Long,
    val expectedLifecycle: GameLifecycleState?,
    val createdAtEpochMs: Long,
    val createdAtElapsedNs: Long,
    val expiresAtElapsedNs: Long,
    /** Controller-side delay before the next mutation may be planned. */
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

enum class ActionExecutionPhase {
    SUBMITTED,
    ACCEPTED,
    STARTED,
    COMPLETED,
    REJECTED,
    FAILED,
    SAFE_TIMEOUT,
    INDETERMINATE,
    ;

    val isTerminal: Boolean
        get() = this in setOf(
            COMPLETED,
            REJECTED,
            FAILED,
            SAFE_TIMEOUT,
            INDETERMINATE,
        )

    val mayHaveRun: Boolean
        get() = this in setOf(
            ACCEPTED,
            STARTED,
            COMPLETED,
            INDETERMINATE,
        )

    val isDefinitive: Boolean
        get() = this in setOf(COMPLETED, REJECTED, FAILED, SAFE_TIMEOUT)
}

data class ActionExecution(
    val request: ActionRequest,
    val phase: ActionExecutionPhase,
    val message: String? = null,
    val errorCode: String? = null,
    val catchOutcome: CatchOutcome? = null,
    val throwOutcome: ThrowOutcome? = null,
    val snapshotResult: EncounterSnapshotResult? = null,
    val runtimeMessageSeq: Long? = null,
    val observedAtEpochMs: Long? = null,
    val observedAtElapsedNs: Long? = null,
)

/** A transport-neutral identity announced by the runtime side. */
data class RuntimeIdentity(
    val runtimeSessionId: String,
    val pid: Int,
    val processName: String,
    val packageName: String,
    val buildFingerprint: String,
    val capabilities: Set<String>,
    val mutationsAllowed: Boolean,
) {
    init {
        require(runtimeSessionId.isNotBlank()) { "runtimeSessionId must not be blank" }
        require(pid > 0) { "pid must be positive" }
        require(processName.isNotBlank()) { "processName must not be blank" }
        require(packageName.isNotBlank()) { "packageName must not be blank" }
        require(buildFingerprint.isNotBlank()) { "buildFingerprint must not be blank" }
    }
}

data class AutomationObservation(
    val identity: RuntimeIdentity,
    val messageSeq: Long,
    val observedAtEpochMs: Long,
    val observedAtElapsedNs: Long,
    val snapshot: AutomationSnapshot,
) {
    init {
        require(messageSeq > 0L) { "messageSeq must be positive" }
    }
}

data class RunnerDispatch(
    val alerts: List<AutomationAction.Alert> = emptyList(),
    val request: ActionRequest? = null,
    val reason: String? = null,
)

data class AutomationRunnerStatus(
    val runtimeSessionId: String? = null,
    val activeExecution: ActionExecution? = null,
    val suspended: Boolean = false,
    val needsResync: Boolean = false,
    val lastObservationSeq: Long? = null,
    val lastError: String? = null,
)

fun interface ActionRequestExecutor {
    /** Submit only. Client outcome arrives later through [AutomationRunner.onResult]. */
    fun submit(request: ActionRequest): Result<Unit>
}

internal val AutomationAction.isMutation: Boolean
    get() = this !is AutomationAction.Alert

internal val AutomationAction.expectedLifecycle: GameLifecycleState?
    get() = when (this) {
        is AutomationAction.Catch,
        is AutomationAction.TakeEncounterSnapshot,
        is AutomationAction.UseBerry,
        -> GameLifecycleState.ENCOUNTER

        is AutomationAction.OpenEncounter,
        is AutomationAction.Spin,
        is AutomationAction.DiscardItem,
        is AutomationAction.TransferPokemon,
        is AutomationAction.MoveTo,
        -> GameLifecycleState.OVERWORLD

        is AutomationAction.Alert -> null
    }

/** Capability names are strings here to keep core independent of adapter APIs. */
internal val AutomationAction.requiredCapabilities: Set<String>
    get() = when (this) {
        is AutomationAction.MoveTo -> setOf("MOVE")
        is AutomationAction.OpenEncounter -> setOf("OPEN_ENCOUNTER")
        is AutomationAction.Catch -> buildSet {
            add(if (closePreviewAfterCaught) "CATCH_AND_CLOSE_PREVIEW" else "CATCH")
            if (!throwProfile.isDefault) {
                add("THROW_CONTROL")
                if (throwProfile.requiresStructuredOutcome) add("OBSERVE_THROW_OUTCOME")
            }
            if (throwProfile.encounterMode == EncounterMode.AR_PLUS) add("AR_ENCOUNTER")
        }
        is AutomationAction.TakeEncounterSnapshot -> buildSet {
            add("SNAPSHOT_DURING_ENCOUNTER")
            if (encounterMode == EncounterMode.AR_PLUS) add("AR_ENCOUNTER")
        }
        is AutomationAction.Spin -> setOf("SPIN")
        is AutomationAction.DiscardItem -> setOf("DISCARD_ITEM")
        is AutomationAction.TransferPokemon -> setOf("TRANSFER_POKEMON")
        is AutomationAction.UseBerry -> setOf("USE_BERRY")
        is AutomationAction.Alert -> emptySet()
    }
