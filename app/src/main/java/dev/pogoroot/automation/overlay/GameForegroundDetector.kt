package dev.pogoroot.automation.overlay

import android.app.AppOpsManager
import android.app.usage.UsageEvents
import android.app.usage.UsageStatsManager
import android.content.Context
import android.os.Build
import android.os.Process

/**
 * Tracks which app owns the foreground activity without relying on task-stack
 * APIs that are restricted for third-party applications.
 */
internal class GameForegroundDetector(
    context: Context,
    private val nowMillis: () -> Long = System::currentTimeMillis,
    private val supportedPackages: Set<String> = SUPPORTED_PACKAGES,
) {
    enum class State {
        FOREGROUND,
        BACKGROUND,
        UNKNOWN,
    }

    private val appContext = context.applicationContext
    private val usageStatsManager =
        appContext.getSystemService(UsageStatsManager::class.java)
    private var queryCursorMillis: Long? = null
    private var state = State.UNKNOWN

    fun read(): State {
        if (!hasUsageAccess(appContext)) {
            reset()
            return State.UNKNOWN
        }

        val now = nowMillis()
        val begin = queryCursorMillis?.let { cursor ->
            maxOf(cursor - QUERY_OVERLAP_MILLIS, now - MAX_LOOKBACK_MILLIS)
        } ?: (now - INITIAL_LOOKBACK_MILLIS)

        val events = runCatching {
            usageStatsManager?.queryEvents(begin, now)
        }.getOrElse {
            reset()
            return State.UNKNOWN
        } ?: run {
            reset()
            return State.UNKNOWN
        }

        val event = UsageEvents.Event()
        var latestEvent: ClassifiedEvent? = null
        while (events.hasNextEvent()) {
            events.getNextEvent(event)
            val classified = classify(event) ?: continue
            if (latestEvent == null || event.timeStamp >= latestEvent.timestamp) {
                latestEvent = ClassifiedEvent(event.timeStamp, classified)
            }
        }

        queryCursorMillis = now
        latestEvent?.let { state = it.state }
        return state
    }

    private fun classify(event: UsageEvents.Event): State? {
        if (event.eventType == UsageEvents.Event.SCREEN_NON_INTERACTIVE ||
            event.eventType == UsageEvents.Event.KEYGUARD_SHOWN
        ) {
            return State.BACKGROUND
        }

        if (event.eventType == resumedEventType()) {
            return if (event.packageName in supportedPackages) {
                State.FOREGROUND
            } else {
                State.BACKGROUND
            }
        }

        // A game activity pausing/stopping is enough to hide the overlay. Do
        // not let an unrelated app's pause/stop event override a later resume.
        if (event.packageName in supportedPackages &&
            event.eventType in pausedEventTypes()
        ) {
            return State.BACKGROUND
        }

        return null
    }

    private fun resumedEventType(): Int = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        UsageEvents.Event.ACTIVITY_RESUMED
    } else {
        @Suppress("DEPRECATION")
        UsageEvents.Event.MOVE_TO_FOREGROUND
    }

    private fun pausedEventTypes(): Set<Int> = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        setOf(
            UsageEvents.Event.ACTIVITY_PAUSED,
            UsageEvents.Event.ACTIVITY_STOPPED,
        )
    } else {
        @Suppress("DEPRECATION")
        setOf(UsageEvents.Event.MOVE_TO_BACKGROUND)
    }

    private fun reset() {
        queryCursorMillis = null
        state = State.UNKNOWN
    }

    private data class ClassifiedEvent(
        val timestamp: Long,
        val state: State,
    )

    companion object {
        val SUPPORTED_PACKAGES = setOf(
            "com.nianticlabs.pokemongo",
            "com.nianticlabs.pokemongo.ares",
        )

        private const val QUERY_OVERLAP_MILLIS = 1_000L
        private const val INITIAL_LOOKBACK_MILLIS = 24 * 60 * 60 * 1_000L
        private const val MAX_LOOKBACK_MILLIS = INITIAL_LOOKBACK_MILLIS

        fun hasUsageAccess(context: Context): Boolean {
            val appOps = context.getSystemService(AppOpsManager::class.java) ?: return false
            val mode = appOps.checkOpNoThrow(
                AppOpsManager.OPSTR_GET_USAGE_STATS,
                Process.myUid(),
                context.packageName,
            )
            return mode == AppOpsManager.MODE_ALLOWED
        }
    }
}
