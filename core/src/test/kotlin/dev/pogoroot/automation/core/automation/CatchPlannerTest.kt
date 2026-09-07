package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.PokemonIv
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class CatchPlannerTest {
    private val planner = CatchPlanner()

    @Test
    fun `shundo has highest catch priority`() {
        val decision = planner.decide(
            encounter = encounter(
                iv = PokemonIv(15, 15, 15),
                shiny = true,
            ),
            policy = CatchPolicy(catchAll = false),
        )

        assertTrue(decision.shouldCatch)
        assertEquals(CatchReason.SHUNDO, decision.reason)
    }

    @Test
    fun `non shiny low iv is skipped when catch all is disabled`() {
        val decision = planner.decide(
            encounter = encounter(
                iv = PokemonIv(3, 4, 5),
                shiny = false,
            ),
            policy = CatchPolicy(
                catchAll = false,
                minimumIvPercent = 90.0,
            ),
        )

        assertFalse(decision.shouldCatch)
        assertEquals(null, decision.reason)
    }

    @Test
    fun `iv threshold catches matching encounter`() {
        val decision = planner.decide(
            encounter = encounter(
                iv = PokemonIv(14, 14, 14),
                shiny = false,
            ),
            policy = CatchPolicy(
                catchAll = false,
                alwaysCatchHundo = false,
                minimumIvPercent = 90.0,
            ),
        )

        assertTrue(decision.shouldCatch)
        assertEquals(CatchReason.IV_THRESHOLD, decision.reason)
    }

    private fun encounter(
        iv: PokemonIv,
        shiny: Boolean,
    ) = EncounterSnapshot(
        encounterId = "encounter-1",
        speciesId = 25,
        speciesName = "Pikachu",
        observedAtEpochMs = 1_000L,
        iv = iv,
        shiny = shiny,
    )
}
