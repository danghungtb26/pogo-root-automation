package dev.pogoroot.automation.headless

import dev.pogoroot.automation.core.automation.AutomationPolicy
import dev.pogoroot.automation.core.automation.BerryType
import dev.pogoroot.automation.core.automation.InventoryPolicy
import dev.pogoroot.automation.core.automation.TransferPolicy

fun HeadlessAutomationConfig.toCorePolicy(): AutomationPolicy = AutomationPolicy(
    autoEncounter = autoEncounter,
    autoCatch = autoCatch,
    autoCloseCatchPreview = autoCloseCatchPreview,
    autoSpin = autoSpin,
    autoDiscard = autoDiscard,
    autoTransfer = autoTransfer,
    berryType = berryMode.toCoreBerryType(),
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

private fun BerryMode.toCoreBerryType(): BerryType? = when (this) {
    BerryMode.NONE -> null
    BerryMode.RAZZ -> BerryType.RAZZ
    BerryMode.NANAB -> BerryType.NANAB
    BerryMode.PINAP -> BerryType.PINAP
    BerryMode.GOLDEN_RAZZ -> BerryType.GOLDEN_RAZZ
    BerryMode.SILVER_PINAP -> BerryType.SILVER_PINAP
}
