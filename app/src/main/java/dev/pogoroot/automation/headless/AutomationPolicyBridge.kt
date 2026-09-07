package dev.pogoroot.automation.headless

import dev.pogoroot.automation.core.automation.AutomationPolicy
import dev.pogoroot.automation.core.automation.InventoryPolicy
import dev.pogoroot.automation.core.automation.TransferPolicy

fun HeadlessAutomationConfig.toCorePolicy(): AutomationPolicy = AutomationPolicy(
    autoCatch = autoCatch,
    autoSpin = autoSpin,
    autoDiscard = autoDiscard,
    autoTransfer = autoTransfer,
    inventoryPolicy = InventoryPolicy(
        maxCountByItemId = discardLimits,
    ),
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
