package dev.pogoroot.automation.headless

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.widget.Toast

enum class AutomationEventType {
    CATCH_THROWN,
    CAUGHT,
    RAN_AWAY,
    SPUN,
    BERRY_USED,
    DISCARDED,
    TRANSFERRED,
    INFO,
    ERROR,
}

data class AutomationEvent(
    val type: AutomationEventType,
    val message: String,
    val timestampEpochMs: Long = System.currentTimeMillis(),
)

fun interface AutomationEventSink {
    fun publish(event: AutomationEvent)
}

class ToastAutomationEventSink(
    context: Context,
    private val configRepository: AutomationConfigRepository,
) : AutomationEventSink {
    private val appContext = context.applicationContext
    private val mainHandler = Handler(Looper.getMainLooper())

    override fun publish(event: AutomationEvent) {
        if (!configRepository.read().showActionToasts) return
        mainHandler.post {
            Toast.makeText(appContext, event.message, Toast.LENGTH_SHORT).show()
        }
    }
}
