package dev.pogoroot.automation.location

import dev.pogoroot.automation.core.model.GeoPoint

/** Overlay adapter: user actions update ownership before reaching the location writer. */
class WalkCandidateLocationActions(
    private val controller: JoystickLocationController,
) : WalkCandidateLocationExecutor {
    override fun isReady(): Boolean {
        val state = controller.snapshot()
        return state.providerReady && state.point != null
    }

    override fun walkTo(target: GeoPoint, generation: Long): Result<Unit> = runCatching {
        check(isReady()) { "location provider is not ready" }
        controller.walkTo(target, walkGeneration = generation)
        check(controller.snapshot().walkStatus != WalkStatus.ERROR) {
            controller.snapshot().error ?: "location executor rejected walk"
        }
    }

    override fun stopWalking() = controller.stopWalking()

    override fun teleport(target: GeoPoint) = controller.teleport(target)

    override fun setJoystick(angleDegrees: Int, strengthPercent: Int, generation: Long?) =
        controller.setJoystick(angleDegrees, strengthPercent, walkGeneration = generation)

    fun userWalkTo(target: GeoPoint): Result<Unit> = WalkCandidateCoordinator.userWalkTo(target)

    fun userTeleport(target: GeoPoint) = WalkCandidateCoordinator.userTeleport(target)

    fun userJoystick(angleDegrees: Int, strengthPercent: Int) =
        WalkCandidateCoordinator.userJoystick(angleDegrees, strengthPercent)
}
