package dev.pogoroot.automation.core.automation

data class AutomationPolicy(
    val autoEncounter: Boolean = false,
    val autoCatch: Boolean = false,
    val autoSpin: Boolean = false,
    val autoDiscard: Boolean = false,
    val autoTransfer: Boolean = false,
    val catchPolicy: CatchPolicy = CatchPolicy(),
    val autoSnapshotDuringEncounter: Boolean = false,
    val snapshotEncounterMode: EncounterMode = EncounterMode.STANDARD,
    val inventoryPolicy: InventoryPolicy = InventoryPolicy(),
    val transferPolicy: TransferPolicy = TransferPolicy(),
    val berryType: BerryType? = null,
    /**
     * Enables the version-gated client-owned close-preview behavior for
     * structured catches. It remains off until a verified runtime advertises
     * the combined catch/close capability.
     */
    val autoCloseCatchPreview: Boolean = false,
)

data class CatchPolicy(
    val catchAll: Boolean = true,
    val alwaysCatchShiny: Boolean = true,
    val alwaysCatchHundo: Boolean = true,
    val minimumIvPercent: Double? = null,
    val throwProfile: ThrowProfile = ThrowProfile(),
) {
    init {
        require(minimumIvPercent == null || minimumIvPercent in 0.0..100.0) {
            "minimumIvPercent must be between 0 and 100"
        }
    }
}

data class InventoryPolicy(
    val maxCountByItemId: Map<Int, Int> = emptyMap(),
) {
    init {
        require(maxCountByItemId.values.all { it >= 0 }) {
            "item max counts must be non-negative"
        }
    }
}

data class TransferPolicy(
    val minimumIvPercentToKeep: Double = 80.0,
    val keepUnknownIv: Boolean = true,
    val keepShiny: Boolean = true,
    val keepHundo: Boolean = true,
    val keepSpecialBackground: Boolean = true,
    val keepFavorite: Boolean = true,
    val keepLegendary: Boolean = true,
    val keepMythical: Boolean = true,
) {
    init {
        require(minimumIvPercentToKeep in 0.0..100.0) {
            "minimumIvPercentToKeep must be between 0 and 100"
        }
    }
}
