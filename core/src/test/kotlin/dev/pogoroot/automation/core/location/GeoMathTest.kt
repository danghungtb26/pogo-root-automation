package dev.pogoroot.automation.core.location

import dev.pogoroot.automation.core.model.GeoPoint
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class GeoMathTest {
    @Test
    fun `joystick directions map to compass bearings`() {
        assertEquals(90.0, GeoMath.joystickAngleToBearing(0), 0.001)
        assertEquals(0.0, GeoMath.joystickAngleToBearing(90), 0.001)
        assertEquals(270.0, GeoMath.joystickAngleToBearing(180), 0.001)
        assertEquals(180.0, GeoMath.joystickAngleToBearing(270), 0.001)
    }

    @Test
    fun `destination moves north without changing longitude materially`() {
        val moved = GeoMath.destination(
            start = GeoPoint(0.0, 0.0),
            bearingDegrees = 0.0,
            distanceMeters = 111.2,
        )

        assertTrue(moved.latitude > 0.0009)
        assertTrue(moved.latitude < 0.0011)
        assertEquals(0.0, moved.longitude, 0.00001)
    }
}
