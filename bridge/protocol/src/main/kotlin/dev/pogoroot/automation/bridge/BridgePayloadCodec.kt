package dev.pogoroot.automation.bridge

/**
 * Versioned payload codec used inside [BridgeFrameCodec]. The event-level
 * encoder/decoder and action codec are split so each source file stays small.
 */
object BridgePayloadCodec {
    fun encode(event: BridgeEvent): Result<ByteArray> =
        BridgePayloadEncoder.encode(event)

    fun decode(type: BridgeMessageType, payload: ByteArray): Result<BridgeEvent> =
        BridgePayloadDecoder.decode(type, payload)
}
