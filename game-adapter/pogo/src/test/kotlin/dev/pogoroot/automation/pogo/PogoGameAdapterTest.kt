package dev.pogoroot.automation.pogo

import dev.pogoroot.automation.adapter.GameCapability
import dev.pogoroot.automation.core.model.GameLifecycleState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class PogoGameAdapterTest {
    @Test
    fun `maps runtime source through adapter`() {
        val source = object : PogoRuntimeSource {
            override val capabilities = setOf(
                GameCapability.READ_LIFECYCLE,
                GameCapability.READ_NEARBY,
                GameCapability.ENCOUNTER,
            )

            override fun connect(): Result<Unit> = Result.success(Unit)

            override fun disconnect() = Unit

            override fun lifecycleState(): GameLifecycleState = GameLifecycleState.ENCOUNTER

            override fun readNearby(): Result<RawNearbyObservation> = Result.success(
                RawNearbyObservation(
                    observedAtEpochMs = 1_000L,
                    playerLatitude = 21.0,
                    playerLongitude = 105.0,
                    spawns = emptyList(),
                ),
            )

            override fun readEncounter(): Result<RawEncounterObservation?> = Result.success(
                RawEncounterObservation(
                    encounterId = "enc-1",
                    speciesId = 25,
                    observedAtEpochMs = 1_000L,
                    individualAttack = 15,
                    individualDefense = 15,
                    individualStamina = 15,
                    shiny = true,
                ),
            )
        }

        val adapter = PogoGameAdapter(
            runtimeSource = source,
            nearbyMapper = PogoNearbyMapper(SpeciesNameResolver { "Pikachu" }),
            encounterMapper = PogoEncounterMapper(SpeciesNameResolver { "Pikachu" }),
        )

        assertTrue(adapter.connect().isSuccess)
        assertEquals(GameLifecycleState.ENCOUNTER, adapter.lifecycleState())
        assertTrue(adapter.readEncounter().getOrThrow()!!.isShundo)
        assertTrue(adapter.readNearby().getOrThrow().spawns.isEmpty())
    }
}
