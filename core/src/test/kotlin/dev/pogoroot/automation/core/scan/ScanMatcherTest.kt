package dev.pogoroot.automation.core.scan

import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.PokemonIv
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ScanMatcherTest {
    private val matcher = ScanMatcher()

    @Test
    fun `matches hundo and shiny independently`() {
        val result = matcher.match(
            encounter = encounter(iv = PokemonIv(15, 15, 15), shiny = true),
            mode = ScanMode.BOTH,
        )

        assertEquals(setOf(ScanMatchType.HUNDO, ScanMatchType.SHINY), result.types)
        assertTrue(result.isShundo)
    }

    @Test
    fun `null shiny and partial iv do not create a false match`() {
        val result = matcher.match(
            encounter = encounter(iv = null, shiny = null),
            mode = ScanMode.BOTH,
        )

        assertEquals(emptySet<ScanMatchType>(), result.types)
    }

    private fun encounter(iv: PokemonIv?, shiny: Boolean?) = EncounterSnapshot(
        encounterId = "encounter-1",
        speciesId = 25,
        speciesName = "Pikachu",
        observedAtEpochMs = 1_000L,
        iv = iv,
        shiny = shiny,
    )
}
