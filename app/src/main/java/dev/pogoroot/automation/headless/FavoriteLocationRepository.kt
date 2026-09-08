package dev.pogoroot.automation.headless

import android.content.Context
import dev.pogoroot.automation.core.model.GeoPoint
import org.json.JSONArray
import java.util.UUID

data class FavoriteLocation(
    val id: String,
    val name: String,
    val point: GeoPoint,
    val createdAtEpochMs: Long,
) {
    init {
        require(id.isNotBlank()) { "id must not be blank" }
        require(name.isNotBlank() && name.length <= MAX_NAME_LENGTH) {
            "name must contain 1-$MAX_NAME_LENGTH characters"
        }
        require(point.latitude.isFinite() && point.latitude in -90.0..90.0) {
            "invalid latitude"
        }
        require(point.longitude.isFinite() && point.longitude in -180.0..180.0) {
            "invalid longitude"
        }
        require(createdAtEpochMs > 0L) { "createdAtEpochMs must be positive" }
    }

    private companion object {
        const val MAX_NAME_LENGTH = 80
    }
}

/** App-local persistence for the small, user-managed favorite location list. */
class FavoriteLocationRepository(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences(
        PREFS,
        Context.MODE_PRIVATE,
    )

    fun read(): List<FavoriteLocation> {
        val raw = preferences.getString(KEY_ITEMS, null) ?: return emptyList()
        val array = runCatching { JSONArray(raw) }.getOrNull() ?: return emptyList()
        return (0 until array.length()).mapNotNull { index ->
            runCatching {
                val item = array.getJSONObject(index)
                FavoriteLocation(
                    id = item.optString(KEY_ID).trim(),
                    name = item.optString(KEY_NAME).trim(),
                    point = GeoPoint(
                        latitude = item.optDouble(KEY_LATITUDE, Double.NaN),
                        longitude = item.optDouble(KEY_LONGITUDE, Double.NaN),
                    ),
                    createdAtEpochMs = item.optLong(KEY_CREATED_AT, 0L),
                )
            }.getOrNull()
        }
    }

    fun add(
        name: String,
        point: GeoPoint,
        createdAtEpochMs: Long = System.currentTimeMillis(),
    ): Result<FavoriteLocation> = runCatching {
        val favorite = FavoriteLocation(
            id = UUID.randomUUID().toString(),
            name = name.trim(),
            point = point,
            createdAtEpochMs = createdAtEpochMs,
        )
        val current = read()
        require(current.size < MAX_FAVORITES) {
            "Maximum of $MAX_FAVORITES favorite locations reached"
        }
        write(current + favorite)
        favorite
    }

    fun delete(id: String): Boolean {
        if (id.isBlank()) return false
        val current = read()
        val remaining = current.filterNot { it.id == id }
        if (remaining.size == current.size) return false
        write(remaining)
        return true
    }

    private fun write(items: List<FavoriteLocation>) {
        val array = JSONArray()
        items.forEach { favorite ->
            array.put(
                org.json.JSONObject()
                    .put(KEY_ID, favorite.id)
                    .put(KEY_NAME, favorite.name)
                    .put(KEY_LATITUDE, favorite.point.latitude)
                    .put(KEY_LONGITUDE, favorite.point.longitude)
                    .put(KEY_CREATED_AT, favorite.createdAtEpochMs),
            )
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    private companion object {
        const val PREFS = "favorite_locations"
        const val KEY_ITEMS = "items"
        const val KEY_ID = "id"
        const val KEY_NAME = "name"
        const val KEY_LATITUDE = "latitude"
        const val KEY_LONGITUDE = "longitude"
        const val KEY_CREATED_AT = "created_at_epoch_ms"
        const val MAX_FAVORITES = 200
    }
}
