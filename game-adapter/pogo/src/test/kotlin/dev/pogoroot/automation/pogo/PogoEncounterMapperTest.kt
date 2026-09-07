package dev.pogoroot.automation.pogo

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class PogoEncounterMapperTest {
    private val mapper = PogoEncounterMapper(
        speciesNameResolver = SpeciesNameResolver { speciesId ->
            if (speciesId == 25) "Pikachu" else null
        },
    )

    @Test
    fun `maps runtime iv and shiny data into shundo`() {
        val result = mapper.map(
            RawEncounterObservation(
                encounterId = "enc-25",
                speciesId = 25,
                observedAtEpochMs = 1_000L,
                individualAttack = 15,
                individualDefense = 15,
                individualStamina = 15,
                shiny = true,
                cp = 777,
                latitude = 21.0285,
                longitude = 105.8542,
            ),
        )

        assertTrue(result.issues.isEmpty())
        val snapshot = assertNotNull(result.snapshot).let { result.snapshot!! }
        assertEquals("Pikachu", snapshot.speciesName)
        assertTrue(snapshot.isHundo)
        assertTrue(snapshot.isShundo)
    }

    @Test
    fun `rejects partial iv data`() {
        val result = mapper.map(
            RawEncounterObservation(
                encounterId = "enc-25",
                speciesId = 25,
                observedAtEpochMs = 1_000L,
                individualAttack = 15,
                individualDefense = 15,
                shiny = false,
            ),
        )

        assertEquals(null, result.snapshot)
        assertTrue(result.issues.contains("partial IV data"))
    }
}
