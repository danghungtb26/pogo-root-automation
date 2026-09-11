package dev.pogoroot.automation.data

import android.content.Context
import dev.pogoroot.automation.core.model.GeoPoint

data class LastActiveGameAction(
    val point: GeoPoint,
    val action: String,
    val activeAtEpochMs: Long,
)

/** Small app-local bridge between headless action execution and the overlay. */
class LastActiveLocationRepository(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences(
        PREFS,
        Context.MODE_PRIVATE,
    )

    fun record(activity: LastActiveGameAction) {
        if (!activity.point.latitude.isFinite() || !activity.point.longitude.isFinite()) return
        preferences.edit()
            .putLong(KEY_LATITUDE, activity.point.latitude.toBits())
            .putLong(KEY_LONGITUDE, activity.point.longitude.toBits())
            .putString(KEY_ACTION, activity.action)
            .putLong(KEY_ACTIVE_AT, activity.activeAtEpochMs)
            .apply()
    }

    fun read(): LastActiveGameAction? {
        if (!preferences.contains(KEY_LATITUDE) ||
            !preferences.contains(KEY_LONGITUDE) ||
            !preferences.contains(KEY_ACTIVE_AT)
        ) {
            return null
        }

        val latitude = Double.fromBits(preferences.getLong(KEY_LATITUDE, 0L))
        val longitude = Double.fromBits(preferences.getLong(KEY_LONGITUDE, 0L))
        val action = preferences.getString(KEY_ACTION, null).orEmpty()
        val activeAtEpochMs = preferences.getLong(KEY_ACTIVE_AT, 0L)
        if (!latitude.isFinite() || latitude !in -90.0..90.0 ||
            !longitude.isFinite() || longitude !in -180.0..180.0 ||
            action.isBlank() || activeAtEpochMs <= 0L
        ) {
            return null
        }

        return LastActiveGameAction(
            point = GeoPoint(latitude, longitude),
            action = action,
            activeAtEpochMs = activeAtEpochMs,
        )
    }

    private companion object {
        const val PREFS = "last_active_location"
        const val KEY_LATITUDE = "latitude"
        const val KEY_LONGITUDE = "longitude"
        const val KEY_ACTION = "action"
        const val KEY_ACTIVE_AT = "active_at_epoch_ms"
    }
}
