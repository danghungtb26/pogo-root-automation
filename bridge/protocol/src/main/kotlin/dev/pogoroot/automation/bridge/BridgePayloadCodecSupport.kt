package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.automation.AlertKind
import dev.pogoroot.automation.core.automation.BerryType
import dev.pogoroot.automation.core.automation.CatchOutcome
import dev.pogoroot.automation.core.automation.CurveOutcome
import dev.pogoroot.automation.core.automation.CurvePreference
import dev.pogoroot.automation.core.automation.EncounterMode
import dev.pogoroot.automation.core.automation.MovementMode
import dev.pogoroot.automation.core.automation.ThrowOutcome
import dev.pogoroot.automation.core.automation.ThrowQuality
import dev.pogoroot.automation.core.automation.ThrowQualityTarget
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.model.SpawnExpiryConfidence
import java.io.DataInputStream
import java.io.DataOutputStream
import java.nio.charset.StandardCharsets

internal object BridgePayloadCodecSupport {
    const val PAYLOAD_VERSION = 1
    private const val MAX_STRING_BYTES = 64 * 1024

    fun writeString(output: DataOutputStream, value: String) {
        val bytes = value.toByteArray(StandardCharsets.UTF_8)
        require(bytes.size <= MAX_STRING_BYTES) { "string exceeds bridge limit" }
        output.writeInt(bytes.size)
        output.write(bytes)
    }

    fun readString(input: DataInputStream): String {
        val size = input.readInt()
        require(size in 0..MAX_STRING_BYTES) { "invalid bridge string length" }
        return String(ByteArray(size).also(input::readFully), StandardCharsets.UTF_8)
    }

    fun writeNullableString(output: DataOutputStream, value: String?) {
        output.writeBoolean(value != null)
        if (value != null) writeString(output, value)
    }

    fun readNullableString(input: DataInputStream): String? =
        if (input.readBoolean()) readString(input) else null

    fun writeBytes(output: DataOutputStream, value: ByteArray) {
        require(value.size <= BridgeProtocol.HARD_MESSAGE_BYTES) { "bytes exceed bridge limit" }
        output.writeInt(value.size)
        output.write(value)
    }

    fun readBytes(input: DataInputStream): ByteArray {
        val size = input.readInt()
        require(size in 0..BridgeProtocol.HARD_MESSAGE_BYTES) { "invalid bridge byte length" }
        return ByteArray(size).also(input::readFully)
    }

    fun writeStringSet(output: DataOutputStream, values: Set<String>) {
        output.writeInt(values.size)
        values.sorted().forEach { writeString(output, it) }
    }

    fun readStringSet(input: DataInputStream): Set<String> {
        val count = input.readInt()
        require(count in 0..256) { "invalid bridge string set size" }
        return List(count) { readString(input) }.toSet()
    }

    fun writeNullableLong(output: DataOutputStream, value: Long?) {
        output.writeBoolean(value != null)
        if (value != null) output.writeLong(value)
    }

    fun readNullableLong(input: DataInputStream): Long? =
        if (input.readBoolean()) input.readLong() else null

    fun writeNullableInt(output: DataOutputStream, value: Int?) {
        output.writeBoolean(value != null)
        if (value != null) output.writeInt(value)
    }

    fun readNullableInt(input: DataInputStream): Int? =
        if (input.readBoolean()) input.readInt() else null

    fun writeNullableDouble(output: DataOutputStream, value: Double?) {
        output.writeBoolean(value != null)
        if (value != null) output.writeDouble(value)
    }

    fun readNullableDouble(input: DataInputStream): Double? =
        if (input.readBoolean()) input.readDouble() else null

    fun writeNullablePoint(output: DataOutputStream, value: GeoPoint?) {
        output.writeBoolean(value != null)
        if (value != null) {
            output.writeDouble(value.latitude)
            output.writeDouble(value.longitude)
        }
    }

    fun readNullablePoint(input: DataInputStream): GeoPoint? =
        if (input.readBoolean()) GeoPoint(input.readDouble(), input.readDouble()) else null

    fun writeNullableLifecycle(output: DataOutputStream, value: GameLifecycleState?) {
        output.writeBoolean(value != null)
        if (value != null) output.writeInt(lifecycleWireValue(value))
    }

    fun readNullableLifecycle(input: DataInputStream): GameLifecycleState? =
        if (input.readBoolean()) {
            readEnum(input, GameLifecycleState.entries, ::lifecycleWireValue)
        } else {
            null
        }

    fun writeNullableCatchOutcome(output: DataOutputStream, value: CatchOutcome?) {
        output.writeBoolean(value != null)
        if (value != null) output.writeInt(catchOutcomeWireValue(value))
    }

    fun readNullableCatchOutcome(input: DataInputStream): CatchOutcome? =
        if (input.readBoolean()) {
            readEnum(input, CatchOutcome.entries, ::catchOutcomeWireValue)
        } else {
            null
        }

    fun writeNullableThrowOutcome(output: DataOutputStream, value: ThrowOutcome?) {
        output.writeBoolean(value != null)
        if (value != null) {
            val hit = value.hit
            output.writeBoolean(hit != null)
            if (hit != null) output.writeBoolean(hit)
            output.writeInt(throwQualityWireValue(value.quality))
            output.writeInt(curveOutcomeWireValue(value.curve))
        }
    }

    fun readNullableThrowOutcome(input: DataInputStream): ThrowOutcome? {
        if (!input.readBoolean()) return null
        val hit = if (input.readBoolean()) input.readBoolean() else null
        return ThrowOutcome(
            hit = hit,
            quality = readEnum(input, ThrowQuality.entries, ::throwQualityWireValue),
            curve = readEnum(input, CurveOutcome.entries, ::curveOutcomeWireValue),
        )
    }

    fun writeNullableSnapshotResult(
        output: DataOutputStream,
        value: dev.pogoroot.automation.core.automation.EncounterSnapshotResult?,
    ) {
        output.writeBoolean(value != null)
        if (value != null) {
            writeString(output, value.encounterId)
            writeNullableString(output, value.mediaReference)
        }
    }

    fun readNullableSnapshotResult(
        input: DataInputStream,
    ): dev.pogoroot.automation.core.automation.EncounterSnapshotResult? {
        if (!input.readBoolean()) return null
        return dev.pogoroot.automation.core.automation.EncounterSnapshotResult(
            encounterId = readString(input),
            mediaReference = readNullableString(input),
        )
    }

    fun <T : Enum<T>> readEnum(
        input: DataInputStream,
        values: List<T>,
        wireValue: (T) -> Int,
    ): T {
        val value = input.readInt()
        return values.firstOrNull { wireValue(it) == value }
            ?: error("invalid wire enum value: $value")
    }

    fun lifecycleWireValue(value: GameLifecycleState): Int = when (value) {
        GameLifecycleState.DISCONNECTED -> 1
        GameLifecycleState.STARTING -> 2
        GameLifecycleState.LOADING -> 3
        GameLifecycleState.OVERWORLD -> 4
        GameLifecycleState.ENCOUNTER -> 5
        GameLifecycleState.ERROR -> 6
    }

    fun movementWireValue(value: MovementMode): Int = when (value) {
        MovementMode.WALK -> 1
        MovementMode.TELEPORT -> 2
    }

    fun encounterModeWireValue(value: EncounterMode): Int = when (value) {
        EncounterMode.STANDARD -> 1
        EncounterMode.AR_PLUS -> 2
    }

    fun berryWireValue(value: BerryType): Int = when (value) {
        BerryType.RAZZ -> 1
        BerryType.NANAB -> 2
        BerryType.PINAP -> 3
        BerryType.GOLDEN_RAZZ -> 4
        BerryType.SILVER_PINAP -> 5
    }

    fun alertWireValue(value: AlertKind): Int = when (value) {
        AlertKind.SHUNDO -> 1
    }

    fun spawnExpiryWireValue(value: SpawnExpiryConfidence): Int = when (value) {
        SpawnExpiryConfidence.EXACT -> 1
        SpawnExpiryConfidence.ESTIMATED -> 2
        SpawnExpiryConfidence.UNKNOWN -> 3
    }

    fun catchOutcomeWireValue(value: CatchOutcome): Int = when (value) {
        CatchOutcome.CAUGHT -> 1
        CatchOutcome.MISSED -> 2
        CatchOutcome.BREAKOUT -> 3
        CatchOutcome.FLED -> 4
        CatchOutcome.NO_BALL -> 5
        CatchOutcome.INDETERMINATE -> 6
    }

    fun throwQualityWireValue(value: ThrowQuality): Int = when (value) {
        ThrowQuality.NONE -> 1
        ThrowQuality.NICE -> 2
        ThrowQuality.GREAT -> 3
        ThrowQuality.EXCELLENT -> 4
        ThrowQuality.UNKNOWN -> 5
    }

    fun curveOutcomeWireValue(value: CurveOutcome): Int = when (value) {
        CurveOutcome.UNKNOWN -> 1
        CurveOutcome.STRAIGHT -> 2
        CurveOutcome.CURVE -> 3
    }

    fun throwQualityTargetWireValue(value: ThrowQualityTarget): Int = when (value) {
        ThrowQualityTarget.ANY -> 1
        ThrowQualityTarget.NICE -> 2
        ThrowQualityTarget.GREAT -> 3
        ThrowQualityTarget.EXCELLENT -> 4
    }

    fun curvePreferenceWireValue(value: CurvePreference): Int = when (value) {
        CurvePreference.ANY -> 1
        CurvePreference.STRAIGHT -> 2
        CurvePreference.CURVE -> 3
    }
}
