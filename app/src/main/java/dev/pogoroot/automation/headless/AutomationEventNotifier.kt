package dev.pogoroot.automation.headless

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.widget.Toast

enum class AutomationEvent {
    BERRY_USED,
    CATCH_THROW,
    BROKE_FREE,
    CAUGHT,
    RUN_AWAY,
    POKESTOP_SPUN,
    ITEM_DISCARDED,
    POKEMON_TRANSFERRED,
}

class AutomationEventNotifier(
    context: Context,
    private val configRepository: AutomationConfigRepository,
) {
    private val appContext = context.applicationContext
    private val mainHandler = Handler(Looper.getMainLooper())

    fun show(event: AutomationEvent, detail: String? = null) {
        if (!configRepository.read().showToasts) return
        val message = when (event) {
            AutomationEvent.BERRY_USED -> "Berry used"
            AutomationEvent.CATCH_THROW -> "Catch attempt"
            AutomationEvent.BROKE_FREE -> "Pokémon broke free · retrying"
            AutomationEvent.CAUGHT -> "Caught${detail.suffix()}"
            AutomationEvent.RUN_AWAY -> "Run away${detail.suffix()}"
            AutomationEvent.POKESTOP_SPUN -> "PokéStop spun"
            AutomationEvent.ITEM_DISCARDED -> "Discarded${detail.suffix()}"
            AutomationEvent.POKEMON_TRANSFERRED -> "Transferred${detail.suffix()}"
        }
        mainHandler.post {
            Toast.makeText(appContext, message, Toast.LENGTH_SHORT).show()
        }
    }

    private fun String?.suffix(): String = if (this.isNullOrBlank()) "" else ": $this"
}
