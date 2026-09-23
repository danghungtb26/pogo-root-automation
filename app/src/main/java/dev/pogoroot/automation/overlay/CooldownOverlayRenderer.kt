package dev.pogoroot.automation.overlay

import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.time.TeleportCooldown
import dev.pogoroot.automation.core.time.TeleportCooldownMode
import dev.pogoroot.automation.core.time.TeleportCooldownService
import dev.pogoroot.automation.data.LastActiveGameAction
import dev.pogoroot.automation.data.LastActiveLocationRepository
import dev.pogoroot.automation.location.JoystickLocationController

internal class CooldownOverlayRenderer(
    private val positionStore: OverlayPositionStore,
    private val lastActiveLocationRepository: LastActiveLocationRepository,
    private val controller: JoystickLocationController,
    private val cooldownOverlay: CooldownOverlayView,
    private val latestTeleportCooldown: () -> TeleportCooldown?,
) {
    private val cooldownService = TeleportCooldownService()

    fun render() {
        val mode = positionStore.loadCooldownMode()
        val lastActive = if (mode == TeleportCooldownMode.LAST_ACTIVE) {
            lastActiveLocationRepository.read()
        } else {
            null
        }
        val currentPoint = if (mode == TeleportCooldownMode.LAST_ACTIVE) {
            controller.snapshot().point ?: positionStore.loadPoint()
        } else {
            null
        }
        val cooldown = when (mode) {
            TeleportCooldownMode.CURRENT_POSITION -> latestTeleportCooldown()
            TeleportCooldownMode.LAST_ACTIVE -> lastActiveCooldown(lastActive, currentPoint)
        }
        val remaining = cooldown?.remainingMillis(System.currentTimeMillis()) ?: 0L
        cooldownOverlay.render(remaining)
    }

    private fun lastActiveCooldown(
        activity: LastActiveGameAction?,
        destination: GeoPoint?,
    ): TeleportCooldown? {
        activity ?: return null
        destination ?: return null
        return cooldownService.forLastActive(
            lastActivePoint = activity.point,
            lastActiveAtEpochMs = activity.activeAtEpochMs,
            destination = destination,
        )
    }
}
