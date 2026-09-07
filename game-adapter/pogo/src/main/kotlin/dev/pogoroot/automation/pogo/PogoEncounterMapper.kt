package dev.pogoroot.automation.pogo

import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.PokemonIv

data class EncounterMappingResult(
    val snapshot: EncounterSnapshot?,
    val issues: List<String>,
)

class PogoEncounterMapper(
    private val speciesNameResolver: SpeciesNameResolver = SpeciesNameResolver { null },
) {
    fun map(observation: RawEncounterObservation): EncounterMappingResult {
        val issues = mutableListOf<String>()

        if (observation.encounterId.isBlank()) {
            issues += "blank encounter id"
        }
        if (observation.speciesId <= 0) {
            issues += "invalid species id"
        }
        if (observation.cp != null && observation.cp < 0) {
            issues += "invalid cp"
        }

        val iv = mapIv(observation, issues)
        val position = mapPosition(observation, issues)

        if (issues.isNotEmpty()) {
            return EncounterMappingResult(
                snapshot = null,
                issues = issues,
            )
        }

        return EncounterMappingResult(
            snapshot = EncounterSnapshot(
                encounterId = observation.encounterId,
                speciesId = observation.speciesId,
                speciesName = speciesNameResolver.resolve(observation.speciesId)
                    ?: "#${observation.speciesId}",
                observedAtEpochMs = observation.observedAtEpochMs,
                iv = iv,
                shiny = observation.shiny,
                cp = observation.cp,
                position = position,
            ),
            issues = emptyList(),
        )
    }

    private fun mapIv(
        observation: RawEncounterObservation,
        issues: MutableList<String>,
    ): PokemonIv? {
        val values = listOf(
            observation.individualAttack,
            observation.individualDefense,
            observation.individualStamina,
        )

        if (values.all { it == null }) {
            return null
        }
        if (values.any { it == null }) {
            issues += "partial IV data"
            return null
        }

        val attack = observation.individualAttack!!
        val defense = observation.individualDefense!!
        val stamina = observation.individualStamina!!
        if (attack !in 0..15 || defense !in 0..15 || stamina !in 0..15) {
            issues += "invalid IV data"
            return null
        }

        return PokemonIv(
            attack = attack,
            defense = defense,
            stamina = stamina,
        )
    }

    private fun mapPosition(
        observation: RawEncounterObservation,
        issues: MutableList<String>,
    ): GeoPoint? {
        val latitude = observation.latitude
        val longitude = observation.longitude

        if (latitude == null && longitude == null) {
            return null
        }
        if (latitude == null || longitude == null) {
            issues += "partial encounter position"
            return null
        }
        if (latitude !in -90.0..90.0 || longitude !in -180.0..180.0) {
            issues += "invalid encounter position"
            return null
        }

        return GeoPoint(latitude, longitude)
    }
}
