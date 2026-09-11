package dev.pogoroot.automation.headless

import android.content.Context
import android.content.Intent
import android.provider.Settings
import dev.pogoroot.automation.overlay.GameForegroundDetector
import dev.pogoroot.automation.overlay.JoystickOverlayService

/**
 * Owns the foreground-to-joystick lifecycle so the controller app does not
 * need to be opened when Pokémon GO becomes visible.
 */
internal class JoystickAutoStartCoordinator(
    context: Context,
    private val foregroundDetector: GameForegroundDetector = GameForegroundDetector(context),
    private val onGameUnavailable: () -> Unit = {},
    private val hasOverlayPermission: () -> Boolean = {
        Settings.canDrawOverlays(context)
    },
    private val startJoystick: () -> Unit = {
        context.startForegroundService(
            Intent(context, JoystickOverlayService::class.java)
                .setAction(JoystickOverlayService.ACTION_START),
        )
    },
    private val stopJoystick: () -> Unit = {
        context.stopService(Intent(context, JoystickOverlayService::class.java))
    },
) {
    private var lastEligible: Boolean? = null

    fun sync() {
        val eligible = foregroundDetector.read() == GameForegroundDetector.State.FOREGROUND &&
            hasOverlayPermission()
        if (!eligible) {
            // This is intentionally checked on every poll. Automation may be
            // enabled through the local API while PoGo is already in the
            // background, in which case there is no foreground transition to
            // trigger the safety stop.
            runCatching { onGameUnavailable() }
        }
        if (eligible == lastEligible) return

        if (eligible) {
            // Keep the previous state until startForegroundService succeeds so
            // a temporary Android start restriction is retried on the next poll.
            val started = runCatching { startJoystick() }.isSuccess
            if (started) lastEligible = true
        } else {
            // The first sync also cleans up a stale joystick/mock provider left
            // by an earlier process or an implicit service restart.
            runCatching { stopJoystick() }
            lastEligible = false
        }
    }

    fun stop() {
        runCatching { stopJoystick() }
        lastEligible = null
    }
}
