package dev.pogoroot.automation.core.location

import dev.pogoroot.automation.core.model.GeoPoint
import kotlin.math.asin
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

object GeoMath {
    private const val EARTH_RADIUS_METERS = 6_371_000.0

    fun destination(
        start: GeoPoint,
        bearingDegrees: Double,
        distanceMeters: Double,
    ): GeoPoint {
        if (distanceMeters == 0.0) return start

        val angularDistance = distanceMeters / EARTH_RADIUS_METERS
        val bearing = Math.toRadians(normalizeBearing(bearingDegrees))
        val latitude1 = Math.toRadians(start.latitude)
        val longitude1 = Math.toRadians(start.longitude)

        val latitude2 = asin(
            sin(latitude1) * cos(angularDistance) +
                cos(latitude1) * sin(angularDistance) * cos(bearing),
        )
        val longitude2 = longitude1 + atan2(
            sin(bearing) * sin(angularDistance) * cos(latitude1),
            cos(angularDistance) - sin(latitude1) * sin(latitude2),
        )

        return GeoPoint(
            latitude = Math.toDegrees(latitude2),
            longitude = normalizeLongitude(Math.toDegrees(longitude2)),
        )
    }

    fun distanceMeters(start: GeoPoint, end: GeoPoint): Double {
        val latitude1 = Math.toRadians(start.latitude)
        val latitude2 = Math.toRadians(end.latitude)
        val deltaLatitude = Math.toRadians(end.latitude - start.latitude)
        val deltaLongitude = Math.toRadians(normalizeLongitude(end.longitude - start.longitude))
        val haversine =
            sin(deltaLatitude / 2.0) * sin(deltaLatitude / 2.0) +
                cos(latitude1) * cos(latitude2) *
                sin(deltaLongitude / 2.0) * sin(deltaLongitude / 2.0)

        return 2.0 * EARTH_RADIUS_METERS * atan2(
            sqrt(haversine.coerceIn(0.0, 1.0)),
            sqrt((1.0 - haversine).coerceAtLeast(0.0)),
        )
    }

    fun joystickAngleToBearing(angleDegrees: Int): Double =
        normalizeBearing(450.0 - angleDegrees.toDouble())

    private fun normalizeBearing(value: Double): Double = ((value % 360.0) + 360.0) % 360.0

    private fun normalizeLongitude(value: Double): Double = ((value + 540.0) % 360.0) - 180.0
}
