package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.PokemonStorageSnapshot
import dev.pogoroot.automation.core.model.StoredPokemon

class TransferPlanner {
    fun plan(
        storage: PokemonStorageSnapshot,
        policy: TransferPolicy,
    ): List<AutomationAction.TransferPokemon> = storage.pokemon
        .filter { shouldTransfer(it, policy) }
        .map { AutomationAction.TransferPokemon(it.pokemonId) }

    fun shouldTransfer(
        pokemon: StoredPokemon,
        policy: TransferPolicy,
    ): Boolean {
        // Destructive actions fail closed if a game update prevents the runtime
        // from proving all metadata needed by the configured keep rules.
        if (!pokemon.transferMetadataComplete) return false
        if (policy.keepFavorite && pokemon.favorite) return false
        if (policy.keepShiny && pokemon.shiny) return false
        if (policy.keepHundo && pokemon.iv?.isHundo == true) return false
        if (policy.keepSpecialBackground && pokemon.specialBackground) return false
        if (policy.keepLegendary && pokemon.legendary) return false
        if (policy.keepMythical && pokemon.mythical) return false

        val iv = pokemon.iv
        if (iv == null) {
            return !policy.keepUnknownIv
        }

        return iv.percentage < policy.minimumIvPercentToKeep
    }
}
