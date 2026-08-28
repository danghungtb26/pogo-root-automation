package dev.pogoroot.automation.fake

import dev.pogoroot.automation.adapter.GameCapability
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class FakeGameAdapterTest {
    @Test
    fun `exposes read only nearby capability`() {
        val adapter = FakeGameAdapter { 1_000L }

        assertTrue(adapter.capabilities.contains(GameCapability.READ_NEARBY))
        assertTrue(adapter.connect().isSuccess)
        assertEquals(3, adapter.readNearby().getOrThrow().spawns.size)
    }
}
