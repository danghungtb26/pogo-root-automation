package dev.pogoroot.automation.core.location

import dev.pogoroot.automation.core.model.GeoPoint
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class WalkPlannerTest {
    @Test
    fun `walk step never overshoots target`() {
        val start = GeoPoint(0.0, 0.0)
        val target = GeoPoint(0.001, 0.0)

        val step = WalkPlanner.step(
            current = start,
            target = target,
            maxStepMeters = 20.0,
            toleranceMeters = 1.0,
        )

        assertFalse(step.arrived)
        assertTrue(step.nextPoint.latitude > start.latitude)
        assertTrue(step.nextPoint.latitude < target.latitude)
        assertTrue(step.distanceAfterMeters < step.distanceBeforeMeters)
    }

    @Test
    fun `walk step arrives inside tolerance without publishing another jump`() {
        val point = GeoPoint(21.0, 105.0)

        val step = WalkPlanner.step(
            current = point,
            target = point,
            maxStepMeters = 20.0,
            toleranceMeters = 5.0,
        )

        assertTrue(step.arrived)
        assertEquals(point, step.nextPoint)
        assertEquals(0.0, step.distanceAfterMeters, 0.001)
    }
}
