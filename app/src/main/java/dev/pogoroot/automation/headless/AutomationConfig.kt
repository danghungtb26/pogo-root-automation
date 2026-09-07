package dev.pogoroot.automation.headless

import android.content.Context

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
    val autoSpin: Boolean = true,
    val encounterSweep: Boolean = true,
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
    val catchThrowDurationMs: Int = 280,
    val catchResultDelayMs: Long = 3_500L,
    val spinOpenDelayMs: Long = 1_200L,
    val spinSwipeDurationMs: Int = 450,
    val spinResultDelayMs: Long = 1_000L,
    val actionCooldownMs: Long = 1_000L,
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

    fun read(): HeadlessAutomationConfig = HeadlessAutomationConfig(
        enabled = prefs.getBoolean(KEY_ENABLED, false),
        autoCatch = prefs.getBoolean(KEY_AUTO_CATCH, true),
        autoSpin = prefs.getBoolean(KEY_AUTO_SPIN, true),
        encounterSweep = prefs.getBoolean(KEY_ENCOUNTER_SWEEP, true),
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
        catchThrowDurationMs = prefs.getInt(KEY_CATCH_THROW_DURATION, 280).coerceIn(100, 1_200),
        catchResultDelayMs = prefs.getLong(KEY_CATCH_RESULT_DELAY, 3_500L).coerceIn(1_000L, 10_000L),
        spinOpenDelayMs = prefs.getLong(KEY_SPIN_OPEN_DELAY, 1_200L).coerceIn(300L, 5_000L),
        spinSwipeDurationMs = prefs.getInt(KEY_SPIN_SWIPE_DURATION, 450).coerceIn(100, 1_500),
        spinResultDelayMs = prefs.getLong(KEY_SPIN_RESULT_DELAY, 1_000L).coerceIn(300L, 5_000L),
        actionCooldownMs = prefs.getLong(KEY_ACTION_COOLDOWN, 1_000L).coerceIn(250L, 10_000L),
    )

    fun update(transform: (HeadlessAutomationConfig) -> HeadlessAutomationConfig): HeadlessAutomationConfig {
        val next = transform(read())
        prefs.edit()
            .putBoolean(KEY_ENABLED, next.enabled)
            .putBoolean(KEY_AUTO_CATCH, next.autoCatch)
            .putBoolean(KEY_AUTO_SPIN, next.autoSpin)
            .putBoolean(KEY_ENCOUNTER_SWEEP, next.encounterSweep)
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
            .putInt(KEY_CATCH_THROW_DURATION, next.catchThrowDurationMs.coerceIn(100, 1_200))
            .putLong(KEY_CATCH_RESULT_DELAY, next.catchResultDelayMs.coerceIn(1_000L, 10_000L))
            .putLong(KEY_SPIN_OPEN_DELAY, next.spinOpenDelayMs.coerceIn(300L, 5_000L))
            .putInt(KEY_SPIN_SWIPE_DURATION, next.spinSwipeDurationMs.coerceIn(100, 1_500))
            .putLong(KEY_SPIN_RESULT_DELAY, next.spinResultDelayMs.coerceIn(300L, 5_000L))
            .putLong(KEY_ACTION_COOLDOWN, next.actionCooldownMs.coerceIn(250L, 10_000L))
            .apply()
        return read()
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

    companion object {
        private const val PREFS_NAME = "headless_automation"
        private const val KEY_ENABLED = "enabled"
        private const val KEY_AUTO_CATCH = "auto_catch"
        private const val KEY_AUTO_SPIN = "auto_spin"
        private const val KEY_ENCOUNTER_SWEEP = "encounter_sweep"
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
        private const val KEY_CATCH_THROW_DURATION = "catch_throw_duration_ms"
        private const val KEY_CATCH_RESULT_DELAY = "catch_result_delay_ms"
        private const val KEY_SPIN_OPEN_DELAY = "spin_open_delay_ms"
        private const val KEY_SPIN_SWIPE_DURATION = "spin_swipe_duration_ms"
        private const val KEY_SPIN_RESULT_DELAY = "spin_result_delay_ms"
        private const val KEY_ACTION_COOLDOWN = "action_cooldown_ms"
    }
}
