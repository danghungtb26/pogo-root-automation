package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.NearbySpawn
import dev.pogoroot.automation.core.model.PokemonIv
import dev.pogoroot.automation.core.model.SpawnExpiryConfidence
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class AutomationCoordinatorTest {
    private val coordinator = AutomationCoordinator()

    @Test
    fun `shundo encounter produces alert then catch`() {
        val actions = coordinator.plan(
            snapshot = AutomationSnapshot(
                lifecycleState = GameLifecycleState.ENCOUNTER,
                encounter = EncounterSnapshot(
                    encounterId = "enc-1",
                    speciesId = 25,
                    speciesName = "Pikachu",
                    observedAtEpochMs = 1_000L,
                    iv = PokemonIv(15, 15, 15),
                    shiny = true,
                ),
            ),
            policy = AutomationPolicy(
                autoCatch = true,
                catchPolicy = CatchPolicy(catchAll = false),
            ),
        )

        assertEquals(2, actions.size)
        assertTrue(actions[0] is AutomationAction.Alert)
        assertTrue(actions[1] is AutomationAction.Catch)
        assertEquals(CatchReason.SHUNDO, (actions[1] as AutomationAction.Catch).reason)
    }

    @Test
    fun `auto encounter picks the spawn expiring first`() {
        val now = 10_000L
        val position = GeoPoint(21.0, 105.0)
        val actions = coordinator.plan(
            snapshot = AutomationSnapshot(
                lifecycleState = GameLifecycleState.OVERWORLD,
                nearby = NearbySnapshot(
                    observedAtEpochMs = now,
                    playerPosition = position,
                    spawns = listOf(
                        NearbySpawn(
                            spawnId = "later",
                            speciesId = 1,
                            speciesName = "Bulbasaur",
                            position = position,
                            firstSeenAtEpochMs = now,
                            expiresAtEpochMs = now + 60_000L,
                            expiryConfidence = SpawnExpiryConfidence.EXACT,
                        ),
                        NearbySpawn(
                            spawnId = "first",
                            speciesId = 4,
                            speciesName = "Charmander",
                            position = position,
                            firstSeenAtEpochMs = now,
                            expiresAtEpochMs = now + 20_000L,
                            expiryConfidence = SpawnExpiryConfidence.EXACT,
                        ),
                    ),
                ),
            ),
            policy = AutomationPolicy(autoEncounter = true),
        )

        assertEquals(
            listOf(AutomationAction.OpenEncounter("first")),
            actions,
        )
    }

    @Test
    fun `berry is a separate action before catch`() {
        val actions = coordinator.plan(
            snapshot = AutomationSnapshot(
                lifecycleState = GameLifecycleState.ENCOUNTER,
                encounter = EncounterSnapshot(
                    encounterId = "enc-berry",
                    speciesId = 25,
                    speciesName = "Pikachu",
                    observedAtEpochMs = 1_000L,
                ),
            ),
            policy = AutomationPolicy(
                autoCatch = true,
                berryType = BerryType.RAZZ,
            ),
        )

        assertEquals(
            listOf(
                AutomationAction.UseBerry("enc-berry", BerryType.RAZZ),
                AutomationAction.Catch("enc-berry", CatchReason.CATCH_ALL),
            ),
            actions,
        )
    }

    @Test
    fun `catch preview close intent is opt in`() {
        val snapshot = AutomationSnapshot(
            lifecycleState = GameLifecycleState.ENCOUNTER,
            encounter = EncounterSnapshot(
                encounterId = "enc-close",
                speciesId = 25,
                speciesName = "Pikachu",
                observedAtEpochMs = 1_000L,
            ),
        )

        val enabled = coordinator.plan(
            snapshot = snapshot,
            policy = AutomationPolicy(autoCatch = true, autoCloseCatchPreview = true),
        ).single() as AutomationAction.Catch
        val disabled = coordinator.plan(
            snapshot = snapshot,
            policy = AutomationPolicy(autoCatch = true),
        ).single() as AutomationAction.Catch

        assertTrue(enabled.closePreviewAfterCaught)
        assertTrue(!disabled.closePreviewAfterCaught)
    }
}
