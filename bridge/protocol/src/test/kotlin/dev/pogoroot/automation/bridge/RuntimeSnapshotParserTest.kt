package dev.pogoroot.automation.bridge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class RuntimeSnapshotParserTest {
    @Test
    fun `parses connected runtime status`() {
        val snapshot = RuntimeSnapshotParser.parse(
            """
            protocol=1
            runtime_state=connected
            pid=4242
            process=com.nianticlabs.pokemongo
            package=com.nianticlabs.pokemongo
            version_name=0.999.1
            version_code=2026082801
            """.trimIndent(),
        )

        assertEquals(RuntimeConnectionState.CONNECTED, snapshot.state)
        assertEquals(4242, snapshot.pid)
        assertEquals("com.nianticlabs.pokemongo", snapshot.processName)
        assertEquals("0.999.1", snapshot.gameVersionName)
        assertEquals(2026082801L, snapshot.gameVersionCode)
    }

    @Test
    fun `parses not seen without inventing pid`() {
        val snapshot = RuntimeSnapshotParser.parse(
            """
            protocol=1
            runtime_state=not_seen
            pid=0
            process=
            """.trimIndent(),
        )

        assertEquals(RuntimeConnectionState.NOT_SEEN, snapshot.state)
        assertNull(snapshot.pid)
        assertNull(snapshot.processName)
    }

    @Test
    fun `unknown state fails closed as error`() {
        val snapshot = RuntimeSnapshotParser.parse("runtime_state=surprise")

        assertEquals(RuntimeConnectionState.ERROR, snapshot.state)
    }
}
