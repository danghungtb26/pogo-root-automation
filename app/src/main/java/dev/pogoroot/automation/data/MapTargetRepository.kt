package dev.pogoroot.automation.data

import android.content.Context
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.MapTargetObservation

/**
 * Small app-local handoff from the structured runtime service to the location
 * overlay. A target is intentionally short-lived so a stale tap cannot start a
 * walk after a service restart.
 */
class MapTargetRepository(
    context: Context,
    private val nowEpochMs: () -> Long = System::currentTimeMillis,
) {
    private val preferences = context.applicationContext.getSharedPreferences(
        PREFS,
        Context.MODE_PRIVATE,
    )

    fun publish(observation: MapTargetObservation, observedAtEpochMs: Long = nowEpochMs()) {
        preferences.edit()
            .putString(KEY_TAP_ID, observation.tapId)
            .putLong(KEY_LATITUDE, observation.target.latitude.toBits())
            .putLong(KEY_LONGITUDE, observation.target.longitude.toBits())
            .putFloat(KEY_SCREEN_X, observation.screenX)
            .putFloat(KEY_SCREEN_Y, observation.screenY)
            .putInt(KEY_VIEWPORT_WIDTH, observation.viewportWidth)
            .putInt(KEY_VIEWPORT_HEIGHT, observation.viewportHeight)
            .putString(KEY_CAMERA_ID, observation.cameraSnapshotId)
            .putLong(KEY_OBSERVED_AT, observedAtEpochMs)
            .apply()
    }

    fun read(maxAgeMs: Long = DEFAULT_MAX_AGE_MS): MapTargetObservation? {
        val tapId = preferences.getString(KEY_TAP_ID, null) ?: return null
        val observedAt = preferences.getLong(KEY_OBSERVED_AT, 0L)
        if (observedAt <= 0L || nowEpochMs() - observedAt !in 0L..maxAgeMs) {
            clear()
            return null
        }

        val target = GeoPoint(
            latitude = Double.fromBits(preferences.getLong(KEY_LATITUDE, 0L)),
            longitude = Double.fromBits(preferences.getLong(KEY_LONGITUDE, 0L)),
        )
        val cameraId = preferences.getString(KEY_CAMERA_ID, null)
        return runCatching {
            MapTargetObservation(
                tapId = tapId,
                target = target,
                screenX = preferences.getFloat(KEY_SCREEN_X, Float.NaN),
                screenY = preferences.getFloat(KEY_SCREEN_Y, Float.NaN),
                viewportWidth = preferences.getInt(KEY_VIEWPORT_WIDTH, 0),
                viewportHeight = preferences.getInt(KEY_VIEWPORT_HEIGHT, 0),
                cameraSnapshotId = cameraId,
            )
        }.getOrNull() ?: run {
            clear()
            null
        }
    }

    fun clear() {
        preferences.edit().clear().apply()
    }

    private companion object {
        const val PREFS = "map_target"
        const val KEY_TAP_ID = "tap_id"
        const val KEY_LATITUDE = "latitude"
        const val KEY_LONGITUDE = "longitude"
        const val KEY_SCREEN_X = "screen_x"
        const val KEY_SCREEN_Y = "screen_y"
        const val KEY_VIEWPORT_WIDTH = "viewport_width"
        const val KEY_VIEWPORT_HEIGHT = "viewport_height"
        const val KEY_CAMERA_ID = "camera_id"
        const val KEY_OBSERVED_AT = "observed_at_epoch_ms"
        const val DEFAULT_MAX_AGE_MS = 30_000L
    }
}
