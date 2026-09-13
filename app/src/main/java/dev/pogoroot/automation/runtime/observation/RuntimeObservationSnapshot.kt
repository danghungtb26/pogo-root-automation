package dev.pogoroot.automation.runtime.observation

import android.util.Log
import dev.pogoroot.automation.adapter.GameCapability
import dev.pogoroot.automation.core.automation.AutomationSnapshot
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.pogo.PogoGameAdapter

/** Builds the fort-walk navigation view from a native catch-spin scan snapshot. */
internal fun PogoGameAdapter.readNavigationSnapshot(): AutomationSnapshot {
    val lifecycle = lifecycleState()
    val nearby = readNearby().getOrElse { error ->
        Log.w(LOG_TAG, "navigation nearby unavailable: ${error.message}")
        null
    }
    val forts = if (GameCapability.READ_FORTS in capabilities) {
        readForts().getOrElse { error ->
            Log.w(LOG_TAG, "navigation forts unavailable: ${error.message}")
            null
        }
    } else {
        null
    }
    return AutomationSnapshot(
        lifecycleState = lifecycle,
        nearby = nearby,
        encounter = if (lifecycle == GameLifecycleState.ENCOUNTER) {
            readEncounter().getOrNull()
        } else {
            null
        },
        forts = forts,
        inventory = null,
        storage = null,
        outOfBalls = false,
    )
}

private const val LOG_TAG = "PogoRootAutomation"
