package dev.pogoroot.automation.pogo

import org.junit.Assert.assertTrue
import org.junit.Test

class PogoProtoDecoderTest {
    private val decoder = PogoProtoDecoder()

    @Test
    fun `rejects malformed encounter protobuf`() {
        assertTrue(
            decoder.decodeEncounter(
                payload = byteArrayOf(0x7f, 0x7f, 0x7f),
                observedAtEpochMs = 1_000L,
            ).isFailure,
        )
    }

    @Test
    fun `rejects malformed map objects protobuf`() {
        assertTrue(
            decoder.decodeMapObjects(
                payload = byteArrayOf(0x7f, 0x7f, 0x7f),
                observedAtEpochMs = 1_000L,
            ).isFailure,
        )
    }
}
