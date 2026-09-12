package dev.pogoroot.automation.config

import dev.pogoroot.automation.core.automation.AutomationPolicy
import dev.pogoroot.automation.core.automation.AutomationTimingPolicy
import dev.pogoroot.automation.core.automation.BerryType
import dev.pogoroot.automation.core.automation.ThrowProfile
import dev.pogoroot.automation.core.automation.TransferPolicy
import dev.pogoroot.automation.bridge.RuntimeCatchSpinConfig
import dev.pogoroot.automation.bridge.RuntimeDiscardConfig
import dev.pogoroot.automation.bridge.RuntimeTransferConfig

fun HeadlessAutomationConfig.toCorePolicy(): AutomationPolicy = AutomationPolicy(
    autoEncounter = autoEncounter,
    autoCatch = autoCatch,
    autoSnapshotDuringEncounter = autoSnapshotDuringEncounter,
    snapshotEncounterMode = snapshotEncounterMode,
    autoCloseCatchPreview = autoCloseCatchPreview,
    timing = AutomationTimingPolicy(
        spinSettleDelayMs = spinSettleDelayMs,
        catchSettleDelayMs = catchSettleDelayMs,
    ),
    catchPolicy = dev.pogoroot.automation.core.automation.CatchPolicy(
        catchAll = catchAll,
        throwProfile = ThrowProfile(
            qualityTarget = catchThrowQuality,
            curvePreference = catchCurvePreference,
            encounterMode = catchEncounterMode,
        ),
    ),
    autoSpin = autoSpin,
    autoTransfer = autoTransfer,
    berryType = berryMode.toCoreBerryType(),
    transferPolicy = TransferPolicy(
        minimumIvPercentToKeep = transferMinimumIvPercent,
        keepUnknownIv = true,
        keepShiny = transferKeepShiny,
        keepHundo = transferKeepHundo,
        keepSpecialBackground = transferKeepSpecialBackground,
        keepFavorite = transferKeepFavorite,
        keepLegendary = true,
        keepMythical = true,
    ),
)

private fun BerryMode.toCoreBerryType(): BerryType? = when (this) {
    BerryMode.NONE -> null
    BerryMode.RAZZ -> BerryType.RAZZ
    BerryMode.NANAB -> BerryType.NANAB
    BerryMode.PINAP -> BerryType.PINAP
    BerryMode.GOLDEN_RAZZ -> BerryType.GOLDEN_RAZZ
    BerryMode.SILVER_PINAP -> BerryType.SILVER_PINAP
}

fun HeadlessAutomationConfig.toRuntimeCatchSpinConfig(armed: Boolean): RuntimeCatchSpinConfig =
    RuntimeCatchSpinConfig(
        configRevision = configRevision,
        armed = armed,
        autoCatch = autoCatch,
        autoSpin = autoSpin,
        autoEncounter = autoEncounter,
        // Direct-map nearby data currently supports catch-all only. Keep this
        // explicit in the wire snapshot so native does not infer a policy.
        catchAll = catchAll,
        spinSettleDelayMs = spinSettleDelayMs,
        catchSettleDelayMs = catchSettleDelayMs,
    )

fun HeadlessAutomationConfig.toRuntimeTransferConfig(): RuntimeTransferConfig =
    RuntimeTransferConfig(
        configRevision = configRevision,
        autoTransfer = autoTransfer,
        keepUnknownIv = true,
        minimumIvPercentToKeep = transferMinimumIvPercent,
        keepShiny = transferKeepShiny,
        keepHundo = transferKeepHundo,
        keepSpecialBackground = transferKeepSpecialBackground,
        keepFavorite = transferKeepFavorite,
        keepLegendary = true,
        keepMythical = true,
    )

fun HeadlessAutomationConfig.toRuntimeDiscardConfig(): RuntimeDiscardConfig =
    RuntimeDiscardConfig(
        configRevision = configRevision,
        autoDiscard = autoDiscard,
        maxCountByItemId = discardLimits,
    )
