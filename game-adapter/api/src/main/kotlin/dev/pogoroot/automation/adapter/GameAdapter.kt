package dev.pogoroot.automation.adapter

import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.NearbySnapshot

interface GameAdapter {
    val id: String
    val capabilities: Set<GameCapability>

    fun connect(): Result<Unit>

    fun disconnect()

    fun lifecycleState(): GameLifecycleState

    fun readNearby(): Result<NearbySnapshot>
}
