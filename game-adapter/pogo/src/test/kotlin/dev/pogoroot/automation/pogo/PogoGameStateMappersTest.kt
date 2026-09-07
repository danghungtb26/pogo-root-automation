package dev.pogoroot.automation.pogo

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class PogoGameStateMappersTest {
    @Test
    fun `maps fort inventory and storage observations`() {
        val fort = PogoFortMapper().map(
            RawFortObservation(
                observedAtEpochMs = 1_000L,
                forts = listOf(
                    RawFort(
                        fortId = "stop-1",
                        type = RawFortType.POKESTOP,
                        latitude = 21.0,
                        longitude = 105.0,
                        spinAvailable = true,
                    ),
                ),
            ),
        ).getOrThrow()

        val inventory = PogoInventoryMapper().map(
            RawInventoryObservation(
                observedAtEpochMs = 1_000L,
                usedSlots = 100,
                capacity = 350,
                items = listOf(RawItemStack(1, "Poke Ball", 100)),
            ),
        ).getOrThrow()

        val storage = PogoStorageMapper(SpeciesNameResolver { "Pikachu" }).map(
            RawPokemonStorageObservation(
                observedAtEpochMs = 1_000L,
                usedSlots = 1,
                capacity = 300,
                pokemon = listOf(
                    RawStoredPokemon(
                        pokemonId = "mon-1",
                        speciesId = 25,
                        individualAttack = 15,
                        individualDefense = 15,
                        individualStamina = 15,
                        shiny = true,
                    ),
                ),
            ),
        ).getOrThrow()

        assertTrue(fort.forts.single().spinAvailable)
        assertEquals(250, inventory.freeSlots)
        assertTrue(storage.pokemon.single().iv!!.isHundo)
        assertTrue(storage.pokemon.single().shiny)
    }

    @Test
    fun `rejects partial storage iv before transfer rules can run`() {
        val result = PogoStorageMapper().map(
            RawPokemonStorageObservation(
                observedAtEpochMs = 1_000L,
                usedSlots = 1,
                capacity = 300,
                pokemon = listOf(
                    RawStoredPokemon(
                        pokemonId = "mon-1",
                        speciesId = 25,
                        individualAttack = 15,
                        individualDefense = 15,
                    ),
                ),
            ),
        )

        assertTrue(result.isFailure)
    }
}
