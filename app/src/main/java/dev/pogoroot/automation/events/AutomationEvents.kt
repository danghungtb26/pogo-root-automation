package dev.pogoroot.automation.events

import android.content.Context
import android.util.Log
import dev.pogoroot.automation.config.AutomationConfigRepository
import dev.pogoroot.automation.overlay.CustomToast

enum class AutomationEventType {
    CATCH_THROWN,
    CAUGHT,
    RAN_AWAY,
    SPUN,
    BERRY_USED,
    DISCARDED,
    TRANSFERRED,
    MODULE_LOADED,
    MODULE_LOAD_FAILED,
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

    override fun publish(event: AutomationEvent) {
        Log.i(LOG_TAG, "automation event type=${event.type} message=${event.message}")
        val isRuntimeModuleStatus = event.type == AutomationEventType.MODULE_LOADED ||
            event.type == AutomationEventType.MODULE_LOAD_FAILED
        if (!isRuntimeModuleStatus && !configRepository.read().showActionToasts) return
        CustomToast.show(appContext, event.message, accentColor = event.type.accentColor())
    }

    private fun AutomationEventType.accentColor(): Int = when (this) {
        AutomationEventType.ERROR,
        AutomationEventType.MODULE_LOAD_FAILED,
        -> 0xFFF28B82.toInt()
        AutomationEventType.CAUGHT,
        AutomationEventType.TRANSFERRED,
        AutomationEventType.SPUN,
        AutomationEventType.BERRY_USED,
        AutomationEventType.DISCARDED,
        AutomationEventType.MODULE_LOADED,
        -> 0xFFA5D6A7.toInt()
        AutomationEventType.RAN_AWAY -> 0xFFFFD180.toInt()
        else -> 0xFF8AB4F8.toInt()
    }

    private companion object {
        const val LOG_TAG = "PogoRootAutomation"
    }
}
