package dev.pogoroot.automation.location

import dev.pogoroot.automation.core.location.GeoMath
import dev.pogoroot.automation.core.model.GeoPoint
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
    val error: String? = null,
)

class JoystickLocationController(
    private val sink: MockLocationSink,
    private val onStateChanged: (JoystickLocationState) -> Unit = {},
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
            )
        }
        if (strengthPercent == 0) dispatchState()
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
        synchronized(lock) {
            state = state.copy(
                point = point,
                currentSpeedKmh = 0.0,
                strengthPercent = 0,
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
        if (!current.providerReady || current.strengthPercent <= 0) return

        val elapsedSeconds = if (previousTick == 0L) {
            0.05
        } else {
            max(0.0, (now - previousTick) / 1_000_000_000.0).coerceAtMost(0.25)
        }
        val speedKmh = current.maxSpeedKmh * current.strengthPercent / 100.0
        val speedMetersPerSecond = speedKmh / 3.6
        val distanceMeters = speedMetersPerSecond * elapsedSeconds
        val next = GeoMath.destination(point, current.bearingDegrees, distanceMeters)

        val result = sink.publish(
            point = next,
            speedMetersPerSecond = speedMetersPerSecond.toFloat(),
            bearingDegrees = current.bearingDegrees.toFloat(),
        )

        synchronized(lock) {
            state = if (result.isSuccess) {
                state.copy(
                    point = next,
                    currentSpeedKmh = speedKmh,
                    error = null,
                )
            } else {
                state.copy(
                    currentSpeedKmh = 0.0,
                    error = result.exceptionOrNull()?.message,
                )
            }
        }
        dispatchState()
    }

    private fun dispatchState() {
        onStateChanged(snapshot())
    }
}
