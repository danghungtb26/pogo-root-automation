package dev.pogoroot.automation.scan

import android.content.Context
import android.content.SharedPreferences
import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.scan.ScanMatchType
import dev.pogoroot.automation.core.scan.ScanMatcher
import dev.pogoroot.automation.core.scan.ScanMode
import dev.pogoroot.automation.core.scan.ScanResultSummary
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject

/** Small app-local result store shared by the headless service and overlay. */
class ScanResultRepository(context: Context) {
    companion object {
        private const val PREFS = "scan_results"
        private const val HUNDO_RESULTS = "hundo_results"
        private const val SHINY_RESULTS = "shiny_results"
        private const val MAX_RESULTS = 100
    }

    private val preferences: SharedPreferences = context.applicationContext
        .getSharedPreferences(PREFS, Context.MODE_PRIVATE)
    private val matcher = ScanMatcher()

    @Synchronized
    fun read(matchType: ScanMatchType): List<ScanResultSummary> {
        val array = readArray(keyFor(matchType))
        val results = mutableListOf<ScanResultSummary>()
        for (index in 0 until array.length()) {
            val json = array.optJSONObject(index) ?: continue
            decode(json)?.let(results::add)
        }
        return results.toList()
    }

    @Synchronized
    fun recordEncounter(encounter: EncounterSnapshot?) {
        if (encounter == null) return
        matcher.match(encounter, ScanMode.BOTH).types.forEach { matchType ->
            record(
                ScanResultSummary.fromEncounter(
                    encounter,
                    matchType,
                    encounter.encounterId,
                ),
            )
        }
    }

    @Synchronized
    fun record(summary: ScanResultSummary?) {
        if (summary == null) return
        val key = keyFor(summary.matchType)
        val current = readArray(key)
        val next = JSONArray()
        val identity = identityOf(summary)
        try {
            next.put(encode(summary))
            var index = 0
            while (index < current.length() && next.length() < MAX_RESULTS) {
                val existing = current.optJSONObject(index)
                if (existing != null && identity != identityOf(existing)) {
                    next.put(existing)
                }
                index++
            }
            preferences.edit().putString(key, next.toString()).apply()
        } catch (_: JSONException) {
            // A malformed single result must not break the runtime loop.
        }
    }

    @Synchronized
    fun clear() {
        preferences.edit()
            .remove(HUNDO_RESULTS)
            .remove(SHINY_RESULTS)
            .apply()
    }

    private fun keyFor(matchType: ScanMatchType): String =
        if (matchType == ScanMatchType.HUNDO) HUNDO_RESULTS else SHINY_RESULTS

    private fun readArray(key: String): JSONArray {
        val raw = preferences.getString(key, "[]") ?: "[]"
        return try {
            JSONArray(raw)
        } catch (_: JSONException) {
            JSONArray()
        }
    }

    @Throws(JSONException::class)
    private fun encode(summary: ScanResultSummary): JSONObject = JSONObject().apply {
        put("matchType", summary.matchType.name)
        put("spawnId", summary.spawnId)
        put("encounterId", summary.encounterId)
        put("speciesId", summary.speciesId)
        put("speciesName", summary.speciesName)
        put("observedAtEpochMs", summary.observedAtEpochMs)
        put("ivPercentage", summary.ivPercentage)
        put("shiny", summary.shiny)
        val position = summary.position
        if (position == null) {
            put("position", JSONObject.NULL)
        } else {
            put(
                "position",
                JSONObject().apply {
                    put("latitude", position.latitude)
                    put("longitude", position.longitude)
                },
            )
        }
    }

    private fun decode(json: JSONObject): ScanResultSummary? {
        return try {
            val type = ScanMatchType.valueOf(json.optString("matchType"))
            val point = json.optJSONObject("position")
            val position = point?.let {
                GeoPoint(it.getDouble("latitude"), it.getDouble("longitude"))
            }
            val iv: Double? = if (json.isNull("ivPercentage")) {
                null
            } else {
                json.optDouble("ivPercentage")
            }
            val shiny: Boolean? = if (json.isNull("shiny")) {
                null
            } else {
                json.optBoolean("shiny")
            }
            ScanResultSummary(
                matchType = type,
                spawnId = json.getString("spawnId"),
                encounterId = json.getString("encounterId"),
                speciesId = json.getInt("speciesId"),
                speciesName = json.getString("speciesName"),
                observedAtEpochMs = json.getLong("observedAtEpochMs"),
                ivPercentage = iv,
                shiny = shiny,
                position = position,
            )
        } catch (_: JSONException) {
            null
        } catch (_: IllegalArgumentException) {
            null
        }
    }

    private fun identityOf(summary: ScanResultSummary): String =
        "${summary.spawnId}|${summary.encounterId}"

    private fun identityOf(json: JSONObject): String =
        "${json.optString("spawnId")}|${json.optString("encounterId")}"
}
