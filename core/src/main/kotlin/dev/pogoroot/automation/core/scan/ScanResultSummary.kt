package dev.pogoroot.automation.core.scan

import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.GeoPoint

data class ScanResultSummary(
    val matchType: ScanMatchType,
    val spawnId: String,
    val encounterId: String,
    val speciesId: Int,
    val speciesName: String,
    val observedAtEpochMs: Long,
    val ivPercentage: Double?,
    val shiny: Boolean?,
    val position: GeoPoint?,
) {
    companion object {
        @JvmStatic
        fun fromEncounter(
            encounter: EncounterSnapshot,
            matchType: ScanMatchType,
            spawnId: String = encounter.encounterId,
        ): ScanResultSummary = ScanResultSummary(
            matchType = matchType,
            spawnId = spawnId,
            encounterId = encounter.encounterId,
            speciesId = encounter.speciesId,
            speciesName = encounter.speciesName,
            observedAtEpochMs = encounter.observedAtEpochMs,
            ivPercentage = encounter.iv?.percentage,
            shiny = encounter.shiny,
            position = encounter.position,
        )
    }
}
