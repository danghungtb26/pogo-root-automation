package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.Fort
import dev.pogoroot.automation.core.model.FortSnapshot
import dev.pogoroot.automation.core.model.FortType
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.NearbySnapshot
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class AutoFortNavigationTest {
    private val player = GeoPoint(10.0, 106.0)
    private val firstFort = fort("first", 0.001)
    private val secondFort = fort("second", 0.002)

    @Test
    fun emptyScanStartsWalkingToAnAvailablePokestop() {
        val commands = mutableListOf<AutoFortNavigationCommand>()
        val coordinator = AutoFortNavigationCoordinator(commands::add)
        coordinator.setEnabled(true)

        coordinator.onSnapshot(snapshot(forts = listOf(firstFort, secondFort, fort("gym", 0.0001, FortType.GYM))))

        assertEquals(AutoFortNavigationCommand.WalkTo("first", firstFort.position), commands.single())
    }

    @Test
    fun pokemonPausesAndRequiresTerminalResultBeforeResuming() {
        val commands = mutableListOf<AutoFortNavigationCommand>()
        val coordinator = AutoFortNavigationCoordinator(commands::add)
        coordinator.setEnabled(true)
        coordinator.onSnapshot(snapshot(forts = listOf(firstFort)))
        coordinator.onSignal(AutoFortNavigationSignal.POKEMON_FOUND)

        coordinator.onSnapshot(snapshot(forts = listOf(firstFort)))
        assertEquals(2, commands.size)
        assertEquals(AutoFortNavigationCommand.Stop("pokemon found"), commands.last())

        coordinator.onSignal(AutoFortNavigationSignal.POKEMON_CAUGHT)
        coordinator.onSnapshot(snapshot(forts = listOf(firstFort)))

        assertEquals(AutoFortNavigationCommand.WalkTo("first", firstFort.position), commands.last())
    }

    @Test
    fun arrivalMovesToAnotherFortWhenOneIsAvailable() {
        val commands = mutableListOf<AutoFortNavigationCommand>()
        val coordinator = AutoFortNavigationCoordinator(commands::add)
        coordinator.setEnabled(true)
        coordinator.onSnapshot(snapshot(forts = listOf(firstFort, secondFort)))

        coordinator.onSnapshot(
            snapshot(
                playerPosition = firstFort.position,
                forts = listOf(firstFort, secondFort),
            ),
        )

        assertEquals(AutoFortNavigationCommand.WalkTo("second", secondFort.position), commands.last())
    }

    @Test
    fun arrivalStopsWhenThereIsNoOtherAvailableFort() {
        val commands = mutableListOf<AutoFortNavigationCommand>()
        val coordinator = AutoFortNavigationCoordinator(commands::add)
        coordinator.setEnabled(true)
        coordinator.onSnapshot(snapshot(forts = listOf(firstFort)))
        coordinator.onSnapshot(snapshot(playerPosition = firstFort.position, forts = listOf(firstFort)))

        assertEquals(AutoFortNavigationCommand.Stop("no available PokéStop"), commands.last())
    }

    @Test
    fun fledSignalResumesAfterFreshEmptyScan() {
        val commands = mutableListOf<AutoFortNavigationCommand>()
        val coordinator = AutoFortNavigationCoordinator(commands::add)
        coordinator.setEnabled(true)
        coordinator.onSnapshot(snapshot(forts = listOf(firstFort)))
        coordinator.onSignal(AutoFortNavigationSignal.POKEMON_FOUND)
        coordinator.onSignal(AutoFortNavigationSignal.POKEMON_FLED)
        coordinator.onSnapshot(snapshot(forts = listOf(firstFort)))

        assertEquals(AutoFortNavigationCommand.WalkTo("first", firstFort.position), commands.last())
    }

    @Test
    fun disablingFeatureStopsWalkingAndClearsTarget() {
        val commands = mutableListOf<AutoFortNavigationCommand>()
        val coordinator = AutoFortNavigationCoordinator(commands::add)
        coordinator.setEnabled(true)
        coordinator.onSnapshot(snapshot(forts = listOf(firstFort, secondFort)))
        coordinator.setEnabled(false)

        assertEquals(AutoFortNavigationCommand.Stop("feature disabled"), commands.last())
        commands.clear()
        coordinator.onSnapshot(snapshot(forts = listOf(firstFort, secondFort)))
        assertTrue(commands.isEmpty())
    }

    @Test
    fun incompleteOrUnavailableMapFailsClosed() {
        val commands = mutableListOf<AutoFortNavigationCommand>()
        val coordinator = AutoFortNavigationCoordinator(commands::add)
        coordinator.setEnabled(true)

        coordinator.onSnapshot(snapshot(isComplete = false, forts = listOf(firstFort)))
        coordinator.onSnapshot(snapshot(playerPosition = null, forts = listOf(firstFort)))

        assertTrue(commands.all { it is AutoFortNavigationCommand.Stop })
    }

    private fun snapshot(
        playerPosition: GeoPoint? = player,
        isComplete: Boolean = true,
        forts: List<Fort> = emptyList(),
    ): AutomationSnapshot = AutomationSnapshot(
        lifecycleState = GameLifecycleState.OVERWORLD,
        nearby = NearbySnapshot(
            observedAtEpochMs = 1L,
            playerPosition = playerPosition,
            spawns = emptyList(),
            isComplete = isComplete,
        ),
        forts = FortSnapshot(1L, forts),
    )

    private fun fort(
        id: String,
        longitudeOffset: Double,
        type: FortType = FortType.POKESTOP,
        spinAvailable: Boolean = true,
    ): Fort = Fort(
        fortId = id,
        type = type,
        position = GeoPoint(player.latitude, player.longitude + longitudeOffset),
        spinAvailable = spinAvailable,
    )
}
