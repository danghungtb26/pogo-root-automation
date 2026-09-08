package dev.pogoroot.automation.core.location

import dev.pogoroot.automation.core.model.GeoPoint
import kotlin.math.min

data class WalkStep(
    val nextPoint: GeoPoint,
    val bearingDegrees: Double,
    val distanceBeforeMeters: Double,
    val distanceAfterMeters: Double,
    val arrived: Boolean,
)

/** Pure target-following math used by the app-side mock-location writer. */
object WalkPlanner {
    fun step(
        current: GeoPoint,
        target: GeoPoint,
        maxStepMeters: Double,
        toleranceMeters: Double,
    ): WalkStep {
        require(maxStepMeters.isFinite() && maxStepMeters >= 0.0) {
            "maxStepMeters must be finite and non-negative"
        }
        require(toleranceMeters.isFinite() && toleranceMeters >= 0.0) {
            "toleranceMeters must be finite and non-negative"
        }

        val distanceBefore = GeoMath.distanceMeters(current, target)
        if (distanceBefore <= toleranceMeters || maxStepMeters == 0.0) {
            return WalkStep(
                nextPoint = current,
                bearingDegrees = GeoMath.initialBearingDegrees(current, target),
                distanceBeforeMeters = distanceBefore,
                distanceAfterMeters = distanceBefore,
                arrived = distanceBefore <= toleranceMeters,
            )
        }

        val bearing = GeoMath.initialBearingDegrees(current, target)
        val distance = min(maxStepMeters, distanceBefore)
        val next = GeoMath.destination(current, bearing, distance)
        val distanceAfter = GeoMath.distanceMeters(next, target)
        return WalkStep(
            nextPoint = next,
            bearingDegrees = bearing,
            distanceBeforeMeters = distanceBefore,
            distanceAfterMeters = distanceAfter,
            arrived = distanceAfter <= toleranceMeters,
        )
    }
}
