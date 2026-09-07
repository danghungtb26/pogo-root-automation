package dev.pogoroot.automation.core.model

data class StoredPokemon(
    val pokemonId: String,
    val speciesId: Int,
    val speciesName: String,
    val iv: PokemonIv? = null,
    val shiny: Boolean = false,
    val specialBackground: Boolean = false,
    val favorite: Boolean = false,
    val legendary: Boolean = false,
    val mythical: Boolean = false,
)

data class PokemonStorageSnapshot(
    val observedAtEpochMs: Long,
    val usedSlots: Int,
    val capacity: Int,
    val pokemon: List<StoredPokemon>,
) {
    val freeSlots: Int
        get() = (capacity - usedSlots).coerceAtLeast(0)
}
