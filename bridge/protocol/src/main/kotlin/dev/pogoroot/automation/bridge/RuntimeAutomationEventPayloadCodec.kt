package dev.pogoroot.automation.bridge

import java.io.ByteArrayInputStream
import java.io.DataInputStream

enum class RuntimeAutomationEventType(val wireValue: Int) {
    POKEMON_FOUND(1),
    POKEMON_CAUGHT(2),
    POKEMON_FLED(3),
    POKEMON_TRANSFERRED(4),
    POKEMON_TRANSFER_TRIGGERED(5),
    POKEMON_TRANSFER_FAILED(6),
}

data class RuntimeAutomationEventPayload(
    val type: RuntimeAutomationEventType,
    val primaryId: Long,
    val secondaryId: Long,
)

/** Decodes native automation telemetry used by the app-owned custom toast. */
object RuntimeAutomationEventPayloadCodec {
    private const val MAGIC = 0x504F4745

    fun decode(payload: ByteArray): Result<RuntimeAutomationEventPayload> = runCatching {
        require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
            "runtime automation event exceeds hard limit"
        }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == MAGIC) { "invalid runtime automation event marker" }
            val eventTypeWire = input.readInt()
            val type = RuntimeAutomationEventType.entries.firstOrNull {
                it.wireValue == eventTypeWire
            } ?: error("unknown runtime automation event type")
            val primaryId = input.readLong()
            val secondaryId = input.readLong()
            // Native IDs are uint64_t. Long preserves the raw 64-bit pattern,
            // so a valid ID may appear negative when its high bit is set.
            require(primaryId != 0L) { "runtime automation event primary id is invalid" }
            require(input.available() == 0) { "trailing runtime automation event bytes" }
            RuntimeAutomationEventPayload(type, primaryId, secondaryId)
        }
    }
}
