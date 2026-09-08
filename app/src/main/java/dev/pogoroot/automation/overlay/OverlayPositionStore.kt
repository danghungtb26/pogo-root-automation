package dev.pogoroot.automation.overlay

import android.content.Context
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.time.TeleportCooldown
import dev.pogoroot.automation.core.time.TeleportCooldownMode

internal data class OverlayPosition(
    val x: Int,
    val y: Int,
)

internal class OverlayPositionStore(context: Context) {
    companion object {
        private const val PREFS = "built_in_joystick"
        private const val PREF_LAT = "latitude"
        private const val PREF_LON = "longitude"
        private const val PREF_COOLDOWN_STARTED_AT = "cooldown_started_at"
        private const val PREF_COOLDOWN_READY_AT = "cooldown_ready_at"
        private const val PREF_COOLDOWN_DISTANCE = "cooldown_distance"
        private const val PREF_COOLDOWN_MODE = "cooldown_mode"
        private const val PREF_OVERLAY_X = "overlay_x"
        private const val PREF_OVERLAY_Y = "overlay_y"
        private const val PREF_COOLDOWN_X = "cooldown_overlay_x"
        private const val PREF_COOLDOWN_Y = "cooldown_overlay_y"
        private const val PREF_HUNDO_X = "hundo_results_overlay_x"
        private const val PREF_HUNDO_Y = "hundo_results_overlay_y"
        private const val PREF_SHINY_X = "shiny_results_overlay_x"
        private const val PREF_SHINY_Y = "shiny_results_overlay_y"
    }

    private val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    fun loadPoint(): GeoPoint? {
        if (!prefs.contains(PREF_LAT) || !prefs.contains(PREF_LON)) return null
        val latitude = Double.fromBits(prefs.getLong(PREF_LAT, 0L))
        val longitude = Double.fromBits(prefs.getLong(PREF_LON, 0L))
        return GeoPoint(latitude, longitude)
    }

    fun persistPoint(point: GeoPoint?) {
        if (point == null) return
        prefs.edit()
            .putLong(PREF_LAT, point.latitude.toBits())
            .putLong(PREF_LON, point.longitude.toBits())
            .apply()
    }

    fun loadCooldown(): TeleportCooldown? {
        if (!prefs.contains(PREF_COOLDOWN_STARTED_AT) ||
            !prefs.contains(PREF_COOLDOWN_READY_AT) ||
            !prefs.contains(PREF_COOLDOWN_DISTANCE)
        ) {
            return null
        }

        val distanceMeters = Double.fromBits(prefs.getLong(PREF_COOLDOWN_DISTANCE, 0L))
        val startedAtEpochMs = prefs.getLong(PREF_COOLDOWN_STARTED_AT, 0L)
        val readyAtEpochMs = prefs.getLong(PREF_COOLDOWN_READY_AT, 0L)
        if (!distanceMeters.isFinite() || distanceMeters < 0.0 || readyAtEpochMs < startedAtEpochMs) {
            return null
        }

        return TeleportCooldown(distanceMeters, startedAtEpochMs, readyAtEpochMs)
    }

    fun persistCooldown(cooldown: TeleportCooldown) {
        prefs.edit()
            .putLong(PREF_COOLDOWN_STARTED_AT, cooldown.startedAtEpochMs)
            .putLong(PREF_COOLDOWN_READY_AT, cooldown.readyAtEpochMs)
            .putLong(PREF_COOLDOWN_DISTANCE, cooldown.distanceMeters.toBits())
            .apply()
    }

    fun loadCooldownMode(): TeleportCooldownMode =
        prefs.getString(PREF_COOLDOWN_MODE, null)
            ?.let { value -> runCatching { TeleportCooldownMode.valueOf(value) }.getOrNull() }
            ?: TeleportCooldownMode.CURRENT_POSITION

    fun persistCooldownMode(mode: TeleportCooldownMode) {
        prefs.edit().putString(PREF_COOLDOWN_MODE, mode.name).apply()
    }

    fun loadMainPosition(default: OverlayPosition): OverlayPosition = OverlayPosition(
        x = prefs.getInt(PREF_OVERLAY_X, default.x),
        y = prefs.getInt(PREF_OVERLAY_Y, default.y),
    )

    fun persistMainPosition(position: OverlayPosition) {
        prefs.edit()
            .putInt(PREF_OVERLAY_X, position.x)
            .putInt(PREF_OVERLAY_Y, position.y)
            .apply()
    }

    fun loadCooldownPosition(default: OverlayPosition): OverlayPosition = OverlayPosition(
        x = prefs.getInt(PREF_COOLDOWN_X, default.x),
        y = prefs.getInt(PREF_COOLDOWN_Y, default.y),
    )

    fun persistCooldownPosition(position: OverlayPosition) {
        prefs.edit()
            .putInt(PREF_COOLDOWN_X, position.x)
            .putInt(PREF_COOLDOWN_Y, position.y)
            .apply()
    }

    fun loadHundoPosition(default: OverlayPosition): OverlayPosition = OverlayPosition(
        x = prefs.getInt(PREF_HUNDO_X, default.x),
        y = prefs.getInt(PREF_HUNDO_Y, default.y),
    )

    fun persistHundoPosition(position: OverlayPosition) {
        prefs.edit()
            .putInt(PREF_HUNDO_X, position.x)
            .putInt(PREF_HUNDO_Y, position.y)
            .apply()
    }

    fun loadShinyPosition(default: OverlayPosition): OverlayPosition = OverlayPosition(
        x = prefs.getInt(PREF_SHINY_X, default.x),
        y = prefs.getInt(PREF_SHINY_Y, default.y),
    )

    fun persistShinyPosition(position: OverlayPosition) {
        prefs.edit()
            .putInt(PREF_SHINY_X, position.x)
            .putInt(PREF_SHINY_Y, position.y)
            .apply()
    }
}
