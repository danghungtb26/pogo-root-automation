package dev.pogoroot.automation.location

import dev.pogoroot.automation.core.location.GeoMath
import dev.pogoroot.automation.core.location.WalkPlanner
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.time.TeleportCooldown
import dev.pogoroot.automation.core.time.TeleportCooldownService
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit
import kotlin.math.max


data class JoystickLocationState(
    val point: GeoPoint? = null,
    val providerReady: Boolean = false,
    val maxSpeedKmh: Double = 9.0,
    val currentSpeedKmh: Double = 0.0,
    val bearingDegrees: Double = 0.0,
    val strengthPercent: Int = 0,
    val teleportCooldown: TeleportCooldown? = null,
    val walkTarget: GeoPoint? = null,
    val walkStatus: WalkStatus = WalkStatus.IDLE,
    val walkToleranceMeters: Double = DEFAULT_WALK_TOLERANCE_METERS,
    val walkDistanceMeters: Double? = null,
    val error: String? = null,
) {
    companion object {
        const val DEFAULT_WALK_TOLERANCE_METERS = 8.0
    }
}

enum class WalkStatus {
    IDLE,
    WALKING,
    ARRIVED,
    STOPPED,
    ERROR,
}

class JoystickLocationController(
    private val sink: MockLocationSink,
    private val onStateChanged: (JoystickLocationState) -> Unit = {},
    private val cooldownService: TeleportCooldownService = TeleportCooldownService(),
    private val nowEpochMs: () -> Long = System::currentTimeMillis,
) {
    private val lock = Any()
    private val executor = Executors.newSingleThreadScheduledExecutor()

    private var tickTask: ScheduledFuture<*>? = null
    private var state = JoystickLocationState()
    private var lastTickNanos = 0L

    fun start(initialPoint: GeoPoint? = null) {
        synchronized(lock) {
            state = state.copy(point = initialPoint)
        }

        executor.execute {
            val result = sink.start()
            synchronized(lock) {
                state = state.copy(
                    providerReady = result.isSuccess,
                    error = result.exceptionOrNull()?.message,
                )
                lastTickNanos = System.nanoTime()
            }
            dispatchState()

            if (result.isSuccess) {
                initialPoint?.let { point ->
                    sink.publish(point, 0f, 0f)
                }
                tickTask = executor.scheduleAtFixedRate(
                    ::tick,
                    50L,
                    50L,
                    TimeUnit.MILLISECONDS,
                )
            }
        }
    }

    fun stop() {
        tickTask?.cancel(true)
        executor.shutdownNow()
        sink.stop()
    }

    fun setJoystick(angleDegrees: Int, strengthPercent: Int) {
        synchronized(lock) {
            val strength = strengthPercent.coerceIn(0, 100)
            state = state.copy(
                bearingDegrees = GeoMath.joystickAngleToBearing(angleDegrees),
                strengthPercent = strength,
                currentSpeedKmh = if (strength == 0) 0.0 else state.currentSpeedKmh,
                walkTarget = null,
                walkStatus = if (state.walkTarget != null) WalkStatus.STOPPED else state.walkStatus,
                walkDistanceMeters = null,
            )
        }
        if (strengthPercent == 0) dispatchState()
    }

    /** Starts target-following on the same single writer used by joystick ticks. */
    fun walkTo(
        target: GeoPoint,
        toleranceMeters: Double = JoystickLocationState.DEFAULT_WALK_TOLERANCE_METERS,
    ) {
        require(target.latitude in -90.0..90.0) { "invalid latitude" }
        require(target.longitude in -180.0..180.0) { "invalid longitude" }
        require(toleranceMeters.isFinite() && toleranceMeters >= 0.0) {
            "toleranceMeters must be finite and non-negative"
        }

        synchronized(lock) {
            val current = state.point
            if (current == null) {
                state = state.copy(
                    walkTarget = null,
                    walkStatus = WalkStatus.ERROR,
                    walkDistanceMeters = null,
                    error = "Cannot walk without a current location",
                )
            } else {
                val distance = GeoMath.distanceMeters(current, target)
                state = if (distance <= toleranceMeters) {
                    state.copy(
                        point = current,
                        walkTarget = null,
                        walkStatus = WalkStatus.ARRIVED,
                        walkToleranceMeters = toleranceMeters,
                        walkDistanceMeters = distance,
                        currentSpeedKmh = 0.0,
                        strengthPercent = 0,
                        error = null,
                    )
                } else {
                    state.copy(
                        walkTarget = target,
                        walkStatus = WalkStatus.WALKING,
                        walkToleranceMeters = toleranceMeters,
                        walkDistanceMeters = distance,
                        currentSpeedKmh = 0.0,
                        strengthPercent = 0,
                        error = null,
                    )
                }
            }
        }
        dispatchState()
    }

    fun stopWalking() {
        synchronized(lock) {
            state = state.copy(
                walkTarget = null,
                walkStatus = if (state.walkTarget != null) WalkStatus.STOPPED else state.walkStatus,
                walkDistanceMeters = null,
                currentSpeedKmh = 0.0,
                strengthPercent = 0,
            )
        }
        dispatchState()
    }

    fun setMaxSpeedKmh(speed: Double) {
        require(speed in 0.5..120.0) { "speed must be between 0.5 and 120 km/h" }
        synchronized(lock) {
            state = state.copy(maxSpeedKmh = speed)
        }
        dispatchState()
    }

    fun teleport(point: GeoPoint) {
        require(point.latitude in -90.0..90.0) { "invalid latitude" }
        require(point.longitude in -180.0..180.0) { "invalid longitude" }

        val ready: Boolean
        val previousPoint: GeoPoint?
        synchronized(lock) {
            previousPoint = state.point
            state = state.copy(
                point = point,
                currentSpeedKmh = 0.0,
                strengthPercent = 0,
                walkTarget = null,
                walkStatus = if (state.walkTarget != null) WalkStatus.STOPPED else state.walkStatus,
                walkDistanceMeters = null,
                error = null,
            )
            ready = state.providerReady
        }
        dispatchState()

        if (ready) {
            executor.execute {
                val result = sink.publish(point, 0f, 0f)
                if (result.isFailure) {
                    synchronized(lock) {
                        state = state.copy(error = result.exceptionOrNull()?.message)
                    }
                    dispatchState()
                } else {
                    synchronized(lock) {
                        state = state.copy(
                            teleportCooldown = cooldownService.forTeleport(
                                previousPoint = previousPoint,
                                destination = point,
                                startedAtEpochMs = nowEpochMs(),
                            ),
                        )
                    }
                    dispatchState()
                }
            }
        }
    }

    fun snapshot(): JoystickLocationState = synchronized(lock) { state }

    private fun tick() {
        val now = System.nanoTime()
        val snapshot = synchronized(lock) {
            val previous = lastTickNanos
            lastTickNanos = now
            state to previous
        }

        val current = snapshot.first
        val previousTick = snapshot.second
        val point = current.point ?: return
        if (!current.providerReady) return

        val elapsedSeconds = if (previousTick == 0L) {
            0.05
        } else {
            max(0.0, (now - previousTick) / 1_000_000_000.0).coerceAtMost(0.25)
        }
        val isWalkingToTarget = current.walkTarget != null
        if (!isWalkingToTarget && current.strengthPercent <= 0) return

        val speedKmh = if (isWalkingToTarget) current.maxSpeedKmh else {
            current.maxSpeedKmh * current.strengthPercent / 100.0
        }
        val speedMetersPerSecond = speedKmh / 3.6
        val distanceMeters = speedMetersPerSecond * elapsedSeconds
        val walkStep = current.walkTarget?.let { target ->
            WalkPlanner.step(
                current = point,
                target = target,
                maxStepMeters = distanceMeters,
                toleranceMeters = current.walkToleranceMeters,
            )
        }
        if (walkStep?.arrived == true) {
            synchronized(lock) {
                state = state.copy(
                    walkTarget = null,
                    walkStatus = WalkStatus.ARRIVED,
                    walkDistanceMeters = walkStep.distanceAfterMeters,
                    currentSpeedKmh = 0.0,
                    strengthPercent = 0,
                    error = null,
                )
            }
            dispatchState()
            return
        }

        val next = walkStep?.nextPoint ?: GeoMath.destination(point, current.bearingDegrees, distanceMeters)
        val bearing = walkStep?.bearingDegrees ?: current.bearingDegrees

        val result = sink.publish(
            point = next,
            speedMetersPerSecond = speedMetersPerSecond.toFloat(),
            bearingDegrees = bearing.toFloat(),
        )

        synchronized(lock) {
            state = if (result.isSuccess) {
                val remaining = current.walkTarget?.let { GeoMath.distanceMeters(next, it) }
                val arrived = current.walkTarget != null && remaining!! <= current.walkToleranceMeters
                state.copy(
                    point = next,
                    bearingDegrees = bearing,
                    currentSpeedKmh = if (arrived) 0.0 else speedKmh,
                    strengthPercent = if (arrived) 0 else state.strengthPercent,
                    walkTarget = if (arrived) null else state.walkTarget,
                    walkStatus = when {
                        arrived -> WalkStatus.ARRIVED
                        state.walkTarget != null -> WalkStatus.WALKING
                        else -> state.walkStatus
                    },
                    walkDistanceMeters = remaining,
                    error = null,
                )
            } else {
                if (state.walkTarget != null) {
                    state.copy(
                        walkTarget = null,
                        walkStatus = WalkStatus.ERROR,
                        walkDistanceMeters = null,
                        currentSpeedKmh = 0.0,
                        strengthPercent = 0,
                        error = result.exceptionOrNull()?.message,
                    )
                } else {
                    state.copy(
                        currentSpeedKmh = 0.0,
                        error = result.exceptionOrNull()?.message,
                    )
                }
            }
        }
        dispatchState()
    }

    private fun dispatchState() {
        onStateChanged(snapshot())
    }
}
