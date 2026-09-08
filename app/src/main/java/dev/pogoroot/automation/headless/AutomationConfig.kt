package dev.pogoroot.automation.headless

import android.content.Context
import dev.pogoroot.automation.core.automation.CurvePreference
import dev.pogoroot.automation.core.automation.EncounterMode
import dev.pogoroot.automation.core.automation.ThrowQualityTarget

enum class BerryMode {
    NONE,
    RAZZ,
    NANAB,
    PINAP,
    GOLDEN_RAZZ,
    SILVER_PINAP,
}

data class HeadlessAutomationConfig(
    val enabled: Boolean = false,
    val autoCatch: Boolean = true,
    val catchThrowQuality: ThrowQualityTarget = ThrowQualityTarget.ANY,
    val catchCurvePreference: CurvePreference = CurvePreference.ANY,
    val catchEncounterMode: EncounterMode = EncounterMode.STANDARD,
    val autoSnapshotDuringEncounter: Boolean = false,
    val snapshotEncounterMode: EncounterMode = EncounterMode.STANDARD,
    /** Accept verified MAP_TARGET observations and start app-side walking. */
    val mapTapWalkEnabled: Boolean = false,
    /** Only effective when the verified runtime advertises CATCH_AND_CLOSE_PREVIEW. */
    val autoCloseCatchPreview: Boolean = false,
    val autoSpin: Boolean = true,
    val autoEncounter: Boolean = true,
    val autoDiscard: Boolean = false,
    val discardLimits: Map<Int, Int> = DEFAULT_DISCARD_LIMITS,
    val autoTransfer: Boolean = false,
    val transferKeepHundo: Boolean = true,
    val transferKeepShiny: Boolean = true,
    val transferKeepSpecialBackground: Boolean = true,
    val transferKeepFavorite: Boolean = true,
    val transferMinimumIvPercent: Double = 80.0,
    val berryMode: BerryMode = BerryMode.NONE,
    val showActionToasts: Boolean = true,
    val loopIntervalMs: Long = 900L,
    /** Exact strong fingerprints verified for client-owned mutation. */
    val structuredAllowedBuildFingerprints: Set<String> = emptySet(),
) {
    companion object {
        // Common Poké Ball / berry limits. Users can override these from overlay settings.
        val DEFAULT_DISCARD_LIMITS = mapOf(
            1 to 200,   // Poké Ball
            2 to 150,   // Great Ball
            3 to 100,   // Ultra Ball
            701 to 50,  // Razz Berry
            703 to 50,  // Nanab Berry
            705 to 80,  // Pinap Berry
        )
    }
}

class AutomationConfigRepository(context: Context) {
    private val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    init {
        migrateLegacyPreferences()
    }

    fun read(): HeadlessAutomationConfig = HeadlessAutomationConfig(
        enabled = prefs.getBoolean(KEY_ENABLED, false),
        autoCatch = prefs.getBoolean(KEY_AUTO_CATCH, true),
        catchThrowQuality = enumPreference(KEY_THROW_QUALITY, ThrowQualityTarget.ANY),
        catchCurvePreference = enumPreference(KEY_THROW_CURVE, CurvePreference.ANY),
        catchEncounterMode = enumPreference(KEY_CATCH_ENCOUNTER_MODE, EncounterMode.STANDARD),
        autoSnapshotDuringEncounter = prefs.getBoolean(KEY_AUTO_SNAPSHOT, false),
        snapshotEncounterMode = enumPreference(KEY_SNAPSHOT_ENCOUNTER_MODE, EncounterMode.STANDARD),
        mapTapWalkEnabled = prefs.getBoolean(KEY_MAP_TAP_WALK, false),
        autoCloseCatchPreview = prefs.getBoolean(KEY_AUTO_CLOSE_CATCH_PREVIEW, false),
        autoSpin = prefs.getBoolean(KEY_AUTO_SPIN, true),
        autoEncounter = prefs.getBoolean(KEY_AUTO_ENCOUNTER, true),
        autoDiscard = prefs.getBoolean(KEY_AUTO_DISCARD, false),
        discardLimits = decodeLimits(
            prefs.getString(KEY_DISCARD_LIMITS, null),
        ).ifEmpty { HeadlessAutomationConfig.DEFAULT_DISCARD_LIMITS },
        autoTransfer = prefs.getBoolean(KEY_AUTO_TRANSFER, false),
        transferKeepHundo = prefs.getBoolean(KEY_TRANSFER_KEEP_HUNDO, true),
        transferKeepShiny = prefs.getBoolean(KEY_TRANSFER_KEEP_SHINY, true),
        transferKeepSpecialBackground = prefs.getBoolean(KEY_TRANSFER_KEEP_BG, true),
        transferKeepFavorite = prefs.getBoolean(KEY_TRANSFER_KEEP_FAVORITE, true),
        transferMinimumIvPercent = prefs.getFloat(KEY_TRANSFER_MIN_IV, 80f).toDouble().coerceIn(0.0, 100.0),
        berryMode = runCatching {
            BerryMode.valueOf(prefs.getString(KEY_BERRY_MODE, BerryMode.NONE.name) ?: BerryMode.NONE.name)
        }.getOrDefault(BerryMode.NONE),
        showActionToasts = prefs.getBoolean(KEY_SHOW_ACTION_TOASTS, true),
        loopIntervalMs = prefs.getLong(KEY_LOOP_INTERVAL, 900L).coerceIn(300L, 5_000L),
        structuredAllowedBuildFingerprints = prefs.getString(KEY_STRUCTURED_ALLOWLIST, null)
            .orEmpty()
            .split(',')
            .map(String::trim)
            .filter(String::isNotBlank)
            .toSet(),
    )

    fun update(transform: (HeadlessAutomationConfig) -> HeadlessAutomationConfig): HeadlessAutomationConfig {
        val next = transform(read())
        prefs.edit()
            .putBoolean(KEY_ENABLED, next.enabled)
            .putBoolean(KEY_AUTO_CATCH, next.autoCatch)
            .putString(KEY_THROW_QUALITY, next.catchThrowQuality.name)
            .putString(KEY_THROW_CURVE, next.catchCurvePreference.name)
            .putString(KEY_CATCH_ENCOUNTER_MODE, next.catchEncounterMode.name)
            .putBoolean(KEY_AUTO_SNAPSHOT, next.autoSnapshotDuringEncounter)
            .putString(KEY_SNAPSHOT_ENCOUNTER_MODE, next.snapshotEncounterMode.name)
            .putBoolean(KEY_MAP_TAP_WALK, next.mapTapWalkEnabled)
            .putBoolean(KEY_AUTO_CLOSE_CATCH_PREVIEW, next.autoCloseCatchPreview)
            .putBoolean(KEY_AUTO_SPIN, next.autoSpin)
            .putBoolean(KEY_AUTO_ENCOUNTER, next.autoEncounter)
            .putBoolean(KEY_AUTO_DISCARD, next.autoDiscard)
            .putString(KEY_DISCARD_LIMITS, encodeLimits(next.discardLimits))
            .putBoolean(KEY_AUTO_TRANSFER, next.autoTransfer)
            .putBoolean(KEY_TRANSFER_KEEP_HUNDO, next.transferKeepHundo)
            .putBoolean(KEY_TRANSFER_KEEP_SHINY, next.transferKeepShiny)
            .putBoolean(KEY_TRANSFER_KEEP_BG, next.transferKeepSpecialBackground)
            .putBoolean(KEY_TRANSFER_KEEP_FAVORITE, next.transferKeepFavorite)
            .putFloat(KEY_TRANSFER_MIN_IV, next.transferMinimumIvPercent.coerceIn(0.0, 100.0).toFloat())
            .putString(KEY_BERRY_MODE, next.berryMode.name)
            .putBoolean(KEY_SHOW_ACTION_TOASTS, next.showActionToasts)
            .putLong(KEY_LOOP_INTERVAL, next.loopIntervalMs.coerceIn(300L, 5_000L))
            .putString(KEY_STRUCTURED_ALLOWLIST, next.structuredAllowedBuildFingerprints
                .filter(String::isNotBlank)
                .sorted()
                .joinToString(","))
            .apply()
        return read()
    }

    /**
     * The old persisted mode and pixel-driver settings are intentionally not
     * read. Preserve the old encounter preference under its structured name,
     * then remove legacy keys so a stale install cannot re-enable deleted code.
     */
    private fun migrateLegacyPreferences() {
        val editor = prefs.edit()
        if (!prefs.contains(KEY_AUTO_ENCOUNTER) && prefs.contains(KEY_LEGACY_ENCOUNTER_SWEEP)) {
            editor.putBoolean(
                KEY_AUTO_ENCOUNTER,
                prefs.getBoolean(KEY_LEGACY_ENCOUNTER_SWEEP, true),
            )
        }
        editor.remove(KEY_LEGACY_ENCOUNTER_SWEEP)
            .remove(KEY_LEGACY_RUNTIME_MODE)
            .remove(KEY_LEGACY_CATCH_THROW_DURATION)
            .remove(KEY_LEGACY_CATCH_RESULT_DELAY)
            .remove(KEY_LEGACY_SPIN_OPEN_DELAY)
            .remove(KEY_LEGACY_SPIN_SWIPE_DURATION)
            .remove(KEY_LEGACY_SPIN_RESULT_DELAY)
            .remove(KEY_LEGACY_ACTION_COOLDOWN)
            .apply()
    }

    private fun encodeLimits(limits: Map<Int, Int>): String = limits.entries
        .filter { it.key > 0 && it.value >= 0 }
        .sortedBy { it.key }
        .joinToString(",") { "${it.key}:${it.value}" }

    private fun decodeLimits(raw: String?): Map<Int, Int> {
        if (raw.isNullOrBlank()) return emptyMap()
        return raw.split(',').mapNotNull { pair ->
            val itemId = pair.substringBefore(':').toIntOrNull() ?: return@mapNotNull null
            val maxCount = pair.substringAfter(':', "").toIntOrNull() ?: return@mapNotNull null
            if (itemId <= 0 || maxCount < 0) null else itemId to maxCount
        }.toMap()
    }

    private inline fun <reified T : Enum<T>> enumPreference(key: String, default: T): T = runCatching {
        enumValueOf<T>(prefs.getString(key, default.name) ?: default.name)
    }.getOrDefault(default)

    companion object {
        private const val PREFS_NAME = "headless_automation"
        private const val KEY_ENABLED = "enabled"
        private const val KEY_AUTO_CATCH = "auto_catch"
        private const val KEY_THROW_QUALITY = "catch_throw_quality"
        private const val KEY_THROW_CURVE = "catch_throw_curve"
        private const val KEY_CATCH_ENCOUNTER_MODE = "catch_encounter_mode"
        private const val KEY_AUTO_SNAPSHOT = "auto_snapshot_during_encounter"
        private const val KEY_SNAPSHOT_ENCOUNTER_MODE = "snapshot_encounter_mode"
        private const val KEY_MAP_TAP_WALK = "map_tap_walk"
        private const val KEY_AUTO_CLOSE_CATCH_PREVIEW = "auto_close_catch_preview"
        private const val KEY_AUTO_SPIN = "auto_spin"
        private const val KEY_AUTO_ENCOUNTER = "auto_encounter"
        private const val KEY_AUTO_DISCARD = "auto_discard"
        private const val KEY_DISCARD_LIMITS = "discard_limits"
        private const val KEY_AUTO_TRANSFER = "auto_transfer"
        private const val KEY_TRANSFER_KEEP_HUNDO = "transfer_keep_hundo"
        private const val KEY_TRANSFER_KEEP_SHINY = "transfer_keep_shiny"
        private const val KEY_TRANSFER_KEEP_BG = "transfer_keep_special_background"
        private const val KEY_TRANSFER_KEEP_FAVORITE = "transfer_keep_favorite"
        private const val KEY_TRANSFER_MIN_IV = "transfer_min_iv"
        private const val KEY_BERRY_MODE = "berry_mode"
        private const val KEY_SHOW_ACTION_TOASTS = "show_action_toasts"
        private const val KEY_LOOP_INTERVAL = "loop_interval_ms"
        private const val KEY_STRUCTURED_ALLOWLIST = "structured_allowed_build_fingerprints"
        private const val KEY_LEGACY_ENCOUNTER_SWEEP = "encounter_sweep"
        private const val KEY_LEGACY_RUNTIME_MODE = "runtime_mode"
        private const val KEY_LEGACY_CATCH_THROW_DURATION = "catch_throw_duration_ms"
        private const val KEY_LEGACY_CATCH_RESULT_DELAY = "catch_result_delay_ms"
        private const val KEY_LEGACY_SPIN_OPEN_DELAY = "spin_open_delay_ms"
        private const val KEY_LEGACY_SPIN_SWIPE_DURATION = "spin_swipe_duration_ms"
        private const val KEY_LEGACY_SPIN_RESULT_DELAY = "spin_result_delay_ms"
        private const val KEY_LEGACY_ACTION_COOLDOWN = "action_cooldown_ms"
    }
}
