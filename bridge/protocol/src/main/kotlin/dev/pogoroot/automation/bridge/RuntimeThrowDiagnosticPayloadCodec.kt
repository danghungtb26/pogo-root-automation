package dev.pogoroot.automation.bridge

import java.io.ByteArrayInputStream
import java.io.DataInputStream

enum class RuntimeThrowDiagnosticStage(val wireValue: Int) {
    STATE_CHANGED(1),
    CAPTURE_PROMISE_CREATED(2),
    BALL_STOPPED(3),
    USER_LAUNCH(4),
    CAPTURE_ATTEMPT(5),
    MISSED(6),
    THROW_DATA(7),
    ;

    companion object {
        fun fromWireValue(value: Int): RuntimeThrowDiagnosticStage = entries.firstOrNull {
            it.wireValue == value
        } ?: error("invalid runtime throw diagnostic stage: $value")
    }
}

data class RuntimeThrowDiagnostic(
    val encounterId: String?,
    val stage: RuntimeThrowDiagnosticStage,
    val ballType: Int,
    val flags: Int,
) {
    val interactionInPlay: Boolean get() = flags and FLAG_INTERACTION_IN_PLAY != 0
    val ballInPlay: Boolean get() = flags and FLAG_BALL_IN_PLAY != 0
    val hitCollision: Boolean get() = flags and FLAG_HIT_COLLISION != 0
    val capturePromise: Boolean get() = flags and FLAG_CAPTURE_PROMISE != 0

    fun message(): String = "throw ${stage.name.lowercase()} enc=${encounterId ?: "?"} " +
        "ball=$ballType inPlay=${if (interactionInPlay) 1 else 0} " +
        "ballInPlay=${if (ballInPlay) 1 else 0} hit=${if (hitCollision) 1 else 0} " +
        "capturePromise=${if (capturePromise) 1 else 0}"

    private companion object {
        const val FLAG_INTERACTION_IN_PLAY = 1
        const val FLAG_BALL_IN_PLAY = 1 shl 1
        const val FLAG_HIT_COLLISION = 1 shl 7
        const val FLAG_CAPTURE_PROMISE = 1 shl 8
    }
}

object RuntimeThrowDiagnosticPayloadCodec {
    private const val MAGIC = 0x504F4754

    fun decode(payload: ByteArray): Result<RuntimeThrowDiagnostic> = runCatching {
        require(payload.size <= BridgeProtocol.NORMAL_MESSAGE_BYTES) {
            "runtime throw diagnostic exceeds normal limit"
        }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == MAGIC) { "invalid runtime throw diagnostic payload" }
            val stage = RuntimeThrowDiagnosticStage.fromWireValue(input.readInt())
            val encounterId = input.readLong().toULong().toString().takeUnless { it == "0" }
            val ballType = input.readInt()
            require(ballType >= 0) { "invalid runtime throw ball type" }
            val flags = input.readInt()
            require(input.available() == 0) { "trailing runtime throw diagnostic payload" }
            RuntimeThrowDiagnostic(encounterId, stage, ballType, flags)
        }
    }
}
