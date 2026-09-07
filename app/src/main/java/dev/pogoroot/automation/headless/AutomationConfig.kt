package dev.pogoroot.automation.headless

import android.content.Context

data class HeadlessAutomationConfig(
    val enabled: Boolean = false,
    val autoCatch: Boolean = true,
    val autoSpin: Boolean = true,
    val encounterSweep: Boolean = true,
    val loopIntervalMs: Long = 900L,
    val catchThrowDurationMs: Int = 280,
    val catchResultDelayMs: Long = 3_500L,
    val spinOpenDelayMs: Long = 1_200L,
    val spinSwipeDurationMs: Int = 450,
    val spinResultDelayMs: Long = 1_000L,
    val actionCooldownMs: Long = 1_000L,
)

class AutomationConfigRepository(context: Context) {
    private val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    fun read(): HeadlessAutomationConfig = HeadlessAutomationConfig(
        enabled = prefs.getBoolean(KEY_ENABLED, false),
        autoCatch = prefs.getBoolean(KEY_AUTO_CATCH, true),
        autoSpin = prefs.getBoolean(KEY_AUTO_SPIN, true),
        encounterSweep = prefs.getBoolean(KEY_ENCOUNTER_SWEEP, true),
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

    companion object {
        private const val PREFS_NAME = "headless_automation"
        private const val KEY_ENABLED = "enabled"
        private const val KEY_AUTO_CATCH = "auto_catch"
        private const val KEY_AUTO_SPIN = "auto_spin"
        private const val KEY_ENCOUNTER_SWEEP = "encounter_sweep"
        private const val KEY_LOOP_INTERVAL = "loop_interval_ms"
        private const val KEY_CATCH_THROW_DURATION = "catch_throw_duration_ms"
        private const val KEY_CATCH_RESULT_DELAY = "catch_result_delay_ms"
        private const val KEY_SPIN_OPEN_DELAY = "spin_open_delay_ms"
        private const val KEY_SPIN_SWIPE_DURATION = "spin_swipe_duration_ms"
        private const val KEY_SPIN_RESULT_DELAY = "spin_result_delay_ms"
        private const val KEY_ACTION_COOLDOWN = "action_cooldown_ms"
    }
}
