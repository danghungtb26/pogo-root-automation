package dev.pogoroot.automation.core.model

data class PokemonIv(
    val attack: Int,
    val defense: Int,
    val stamina: Int,
) {
    init {
        require(attack in 0..15) { "attack must be between 0 and 15" }
        require(defense in 0..15) { "defense must be between 0 and 15" }
        require(stamina in 0..15) { "stamina must be between 0 and 15" }
    }

    val total: Int
        get() = attack + defense + stamina

    val percentage: Double
        get() = total * 100.0 / 45.0

    val isHundo: Boolean
        get() = attack == 15 && defense == 15 && stamina == 15
}

data class EncounterSnapshot(
    val encounterId: String,
    val speciesId: Int,
    val speciesName: String,
    val observedAtEpochMs: Long,
    val iv: PokemonIv? = null,
    val shiny: Boolean? = null,
    val cp: Int? = null,
    val position: GeoPoint? = null,
) {
    val isHundo: Boolean
        get() = iv?.isHundo == true

    val isShundo: Boolean
        get() = isHundo && shiny == true
}
