package dev.pogoroot.automation.pogo

data class RawEncounterObservation(
    val encounterId: String,
    val speciesId: Int,
    val observedAtEpochMs: Long,
    val individualAttack: Int? = null,
    val individualDefense: Int? = null,
    val individualStamina: Int? = null,
    val shiny: Boolean? = null,
    val cp: Int? = null,
    val latitude: Double? = null,
    val longitude: Double? = null,
)
