package dev.pogoroot.automation.headless

import android.content.Context
import dev.pogoroot.automation.core.automation.AutomationPolicy
import dev.pogoroot.automation.core.automation.InventoryPolicy
import dev.pogoroot.automation.core.automation.TransferPolicy

data class HeadlessAutomationConfig(
    val enabled: Boolean = false,
    val autoCatch: Boolean = true,
    val autoSpin: Boolean = true,
    val encounterSweep: Boolean = true,
    val autoBerry: Boolean = false,
    val autoDiscard: Boolean = false,
    val discardLimits: Map<Int, Int> = emptyMap(),
    val autoTransfer: Boolean = false,
    val transferBelowIvPercent: Double = 100.0,
    val keepHundo: Boolean = true,
    val keepShiny: Boolean = true,
    val keepSpecialBackground: Boolean = true,
    val keepFavorite: Boolean = true,
    val showToasts: Boolean = true,
    val loopIntervalMs: Long = 900L,
    val catchThrowDurationMs: Int = 280,
    val catchResultDelayMs: Long = 3_500L,
    val spinOpenDelayMs: Long = 1_200L,
    val spinSwipeDurationMs: Int = 450,
    val spinResultDelayMs: Long = 1_000L,
    val actionCooldownMs: Long = 1_000L,
) {
    fun toAutomationPolicy(): AutomationPolicy = AutomationPolicy(
        autoCatch = autoCatch,
        autoSpin = autoSpin,
        autoDiscard = autoDiscard,
        autoTransfer = autoTransfer,
        inventoryPolicy = InventoryPolicy(discardLimits),
        transferPolicy = TransferPolicy(
            minimumIvPercentToKeep = transferBelowIvPercent.coerceIn(0.0, 100.0),
            keepUnknownIv = true,
            keepShiny = keepShiny,
            keepHundo = keepHundo,
            keepSpecialBackground = keepSpecialBackground,
            keepFavorite = keepFavorite,
            keepLegendary = true,
            keepMythical = true,
        ),
    )
}

class AutomationConfigRepository(context: Context) {
    private val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    fun read(): HeadlessAutomationConfig = HeadlessAutomationConfig(
        enabled = prefs.getBoolean(KEY_ENABLED, false),
        autoCatch = prefs.getBoolean(KEY_AUTO_CATCH, true),
        autoSpin = prefs.getBoolean(KEY_AUTO_SPIN, true),
        encounterSweep = prefs.getBoolean(KEY_ENCOUNTER_SWEEP, true),
        autoBerry = prefs.getBoolean(KEY_AUTO_BERRY, false),
        autoDiscard = prefs.getBoolean(KEY_AUTO_DISCARD, false),
        discardLimits = decodeLimits(prefs.getString(KEY_DISCARD_LIMITS, null)),
        autoTransfer = prefs.getBoolean(KEY_AUTO_TRANSFER, false),
        transferBelowIvPercent = Double.fromBits(
            prefs.getLong(KEY_TRANSFER_BELOW_IV, 100.0.toBits()),
        ).coerceIn(0.0, 100.0),
        keepHundo = prefs.getBoolean(KEY_KEEP_HUNDO, true),
        keepShiny = prefs.getBoolean(KEY_KEEP_SHINY, true),
        keepSpecialBackground = prefs.getBoolean(KEY_KEEP_SPECIAL_BACKGROUND, true),
        keepFavorite = prefs.getBoolean(KEY_KEEP_FAVORITE, true),
        showToasts = prefs.getBoolean(KEY_SHOW_TOASTS, true),
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
            .putBoolean(KEY_AUTO_BERRY, next.autoBerry)
            .putBoolean(KEY_AUTO_DISCARD, next.autoDiscard)
            .putString(KEY_DISCARD_LIMITS, encodeLimits(next.discardLimits))
            .putBoolean(KEY_AUTO_TRANSFER, next.autoTransfer)
            .putLong(KEY_TRANSFER_BELOW_IV, next.transferBelowIvPercent.coerceIn(0.0, 100.0).toBits())
            .putBoolean(KEY_KEEP_HUNDO, next.keepHundo)
            .putBoolean(KEY_KEEP_SHINY, next.keepShiny)
            .putBoolean(KEY_KEEP_SPECIAL_BACKGROUND, next.keepSpecialBackground)
            .putBoolean(KEY_KEEP_FAVORITE, next.keepFavorite)
            .putBoolean(KEY_SHOW_TOASTS, next.showToasts)
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

    private fun encodeLimits(limits: Map<Int, Int>): String = limits
        .filter { (itemId, maxCount) -> itemId > 0 && maxCount >= 0 }
        .toSortedMap()
        .entries
        .joinToString(",") { (itemId, maxCount) -> "$itemId:$maxCount" }

    private fun decodeLimits(raw: String?): Map<Int, Int> {
        if (raw.isNullOrBlank()) return emptyMap()
        return raw.split(',').mapNotNull { token ->
            val itemId = token.substringBefore(':').toIntOrNull() ?: return@mapNotNull null
            val maxCount = token.substringAfter(':', "").toIntOrNull() ?: return@mapNotNull null
            if (itemId <= 0 || maxCount < 0) return@mapNotNull null
            itemId to maxCount
        }.toMap()
    }

    companion object {
        private const val PREFS_NAME = "headless_automation"
        private const val KEY_ENABLED = "enabled"
        private const val KEY_AUTO_CATCH = "auto_catch"
        private const val KEY_AUTO_SPIN = "auto_spin"
        private const val KEY_ENCOUNTER_SWEEP = "encounter_sweep"
        private const val KEY_AUTO_BERRY = "auto_berry"
        private const val KEY_AUTO_DISCARD = "auto_discard"
        private const val KEY_DISCARD_LIMITS = "discard_limits"
        private const val KEY_AUTO_TRANSFER = "auto_transfer"
        private const val KEY_TRANSFER_BELOW_IV = "transfer_below_iv"
        private const val KEY_KEEP_HUNDO = "keep_hundo"
        private const val KEY_KEEP_SHINY = "keep_shiny"
        private const val KEY_KEEP_SPECIAL_BACKGROUND = "keep_special_background"
        private const val KEY_KEEP_FAVORITE = "keep_favorite"
        private const val KEY_SHOW_TOASTS = "show_toasts"
        private const val KEY_LOOP_INTERVAL = "loop_interval_ms"
        private const val KEY_CATCH_THROW_DURATION = "catch_throw_duration_ms"
        private const val KEY_CATCH_RESULT_DELAY = "catch_result_delay_ms"
        private const val KEY_SPIN_OPEN_DELAY = "spin_open_delay_ms"
        private const val KEY_SPIN_SWIPE_DURATION = "spin_swipe_duration_ms"
        private const val KEY_SPIN_RESULT_DELAY = "spin_result_delay_ms"
        private const val KEY_ACTION_COOLDOWN = "action_cooldown_ms"
    }
}
