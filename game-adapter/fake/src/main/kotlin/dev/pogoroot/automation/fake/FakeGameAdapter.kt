package dev.pogoroot.automation.fake

import dev.pogoroot.automation.adapter.GameAdapter
import dev.pogoroot.automation.adapter.GameCapability
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.NearbySnapshot
import dev.pogoroot.automation.core.model.NearbySpawn
import dev.pogoroot.automation.core.model.SpawnExpiryConfidence

class FakeGameAdapter(
    private val clock: () -> Long = System::currentTimeMillis,
) : GameAdapter {
    override val id: String = "fake-v1"
    override val capabilities: Set<GameCapability> = setOf(
        GameCapability.READ_LIFECYCLE,
        GameCapability.READ_NEARBY,
    )

    private var connected = false
    private val createdAt = clock()

    private val playerPosition = GeoPoint(
        latitude = 35.681236,
        longitude = 139.767125,
    )

    private val spawns = listOf(
        NearbySpawn(
            spawnId = "fake-pikachu",
            speciesId = 25,
            speciesName = "Pikachu",
            position = playerPosition,
            firstSeenAtEpochMs = createdAt,
            expiresAtEpochMs = createdAt + 2 * 60_000L,
            expiryConfidence = SpawnExpiryConfidence.EXACT,
        ),
        NearbySpawn(
            spawnId = "fake-eevee",
            speciesId = 133,
            speciesName = "Eevee",
            position = GeoPoint(35.6817, 139.7668),
            firstSeenAtEpochMs = createdAt,
            expiresAtEpochMs = createdAt + 4 * 60_000L + 30_000L,
            expiryConfidence = SpawnExpiryConfidence.ESTIMATED,
        ),
        NearbySpawn(
            spawnId = "fake-unknown",
            speciesId = 1,
            speciesName = "Bulbasaur",
            position = GeoPoint(35.6809, 139.7680),
            firstSeenAtEpochMs = createdAt,
            expiresAtEpochMs = null,
            expiryConfidence = SpawnExpiryConfidence.UNKNOWN,
        ),
    )

    override fun connect(): Result<Unit> {
        connected = true
        return Result.success(Unit)
    }

    override fun disconnect() {
        connected = false
    }

    override fun lifecycleState(): GameLifecycleState = if (connected) {
        GameLifecycleState.OVERWORLD
    } else {
        GameLifecycleState.DISCONNECTED
    }

    override fun readNearby(): Result<NearbySnapshot> {
        if (!connected) {
            return Result.failure(IllegalStateException("Adapter is not connected"))
        }

        return Result.success(
            NearbySnapshot(
                observedAtEpochMs = clock(),
                playerPosition = playerPosition,
                spawns = spawns,
            ),
        )
    }
}
