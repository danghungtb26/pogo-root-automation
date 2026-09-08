package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.MapTargetObservation
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class MapTargetPayloadCodecTest {
    @Test
    fun `round trips versioned map target payload`() {
        val expected = MapTargetObservation(
            tapId = "tap-17",
            target = GeoPoint(21.0285, 105.8542),
            screenX = 431.5f,
            screenY = 812.25f,
            viewportWidth = 1080,
            viewportHeight = 1920,
            cameraSnapshotId = "camera-42",
        )

        val actual = MapTargetPayloadCodec.decode(
            MapTargetPayloadCodec.encode(expected).getOrThrow(),
        ).getOrThrow()

        assertEquals(expected, actual)
    }

    @Test
    fun `rejects unsupported payload version`() {
        val payload = MapTargetPayloadCodec.encode(
            MapTargetObservation(
                tapId = "tap-1",
                target = GeoPoint(0.0, 0.0),
                screenX = 10f,
                screenY = 10f,
                viewportWidth = 100,
                viewportHeight = 100,
            ),
        ).getOrThrow()
        payload[3] = 2

        assertTrue(MapTargetPayloadCodec.decode(payload).isFailure)
    }
}
