package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.PokemonIv
import dev.pogoroot.automation.core.model.PokemonStorageSnapshot
import dev.pogoroot.automation.core.model.StoredPokemon
import org.junit.Assert.assertEquals
import org.junit.Test

class TransferPlannerTest {
    private val planner = TransferPlanner()

    @Test
    fun `protects shiny hundo background favorite legendary and mythical pokemon`() {
        val storage = PokemonStorageSnapshot(
            observedAtEpochMs = 1_000L,
            usedSlots = 7,
            capacity = 100,
            pokemon = listOf(
                pokemon("normal-low", iv = PokemonIv(1, 1, 1)),
                pokemon("shiny-low", iv = PokemonIv(1, 1, 1), shiny = true),
                pokemon("hundo", iv = PokemonIv(15, 15, 15)),
                pokemon("background-low", iv = PokemonIv(1, 1, 1), specialBackground = true),
                pokemon("favorite-low", iv = PokemonIv(1, 1, 1), favorite = true),
                pokemon("legendary-low", iv = PokemonIv(1, 1, 1), legendary = true),
                pokemon("mythical-low", iv = PokemonIv(1, 1, 1), mythical = true),
            ),
        )

        assertEquals(
            listOf(AutomationAction.TransferPokemon("normal-low")),
            planner.plan(storage, TransferPolicy(minimumIvPercentToKeep = 80.0)),
        )
    }

    private fun pokemon(
        id: String,
        iv: PokemonIv,
        shiny: Boolean = false,
        specialBackground: Boolean = false,
        favorite: Boolean = false,
        legendary: Boolean = false,
        mythical: Boolean = false,
    ) = StoredPokemon(
        pokemonId = id,
        speciesId = 25,
        speciesName = "Pikachu",
        iv = iv,
        shiny = shiny,
        specialBackground = specialBackground,
        favorite = favorite,
        legendary = legendary,
        mythical = mythical,
    )
}
