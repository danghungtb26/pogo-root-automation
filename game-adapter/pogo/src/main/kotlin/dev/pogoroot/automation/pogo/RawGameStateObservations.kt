package dev.pogoroot.automation.pogo

enum class RawFortType {
    POKESTOP,
    GYM,
}

data class RawFort(
    val fortId: String,
    val type: RawFortType,
    val latitude: Double,
    val longitude: Double,
    val spinAvailable: Boolean,
    val cooldownEndsAtEpochMs: Long? = null,
)

data class RawFortObservation(
    val observedAtEpochMs: Long,
    val forts: List<RawFort>,
)

data class RawItemStack(
    val itemId: Int,
    val itemName: String,
    val count: Int,
)

data class RawInventoryObservation(
    val observedAtEpochMs: Long,
    val usedSlots: Int,
    val capacity: Int,
    val items: List<RawItemStack>,
)

data class RawStoredPokemon(
    val pokemonId: String,
    val speciesId: Int,
    val individualAttack: Int? = null,
    val individualDefense: Int? = null,
    val individualStamina: Int? = null,
    val shiny: Boolean = false,
    val specialBackground: Boolean = false,
    val favorite: Boolean = false,
    val legendary: Boolean = false,
    val mythical: Boolean = false,
)

data class RawPokemonStorageObservation(
    val observedAtEpochMs: Long,
    val usedSlots: Int,
    val capacity: Int,
    val pokemon: List<RawStoredPokemon>,
)
