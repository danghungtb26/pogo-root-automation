package dev.pogoroot.automation.config

import dev.pogoroot.automation.bridge.RuntimeCatchSpinConfig
import dev.pogoroot.automation.bridge.RuntimeDiscardConfig
import dev.pogoroot.automation.bridge.RuntimeTransferConfig

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
        autoWalkToFort = autoWalkToFort,
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
