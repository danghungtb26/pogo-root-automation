package dev.pogoroot.automation.bridge

import dev.pogoroot.automation.core.automation.AlertKind
import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.BerryType
import dev.pogoroot.automation.core.automation.CatchReason
import dev.pogoroot.automation.core.automation.CurvePreference
import dev.pogoroot.automation.core.automation.EncounterMode
import dev.pogoroot.automation.core.automation.ThrowProfile
import dev.pogoroot.automation.core.automation.ThrowQualityTarget
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.automation.MovementMode
import java.io.DataInputStream
import java.io.DataOutputStream

internal object BridgeActionCodec {
    private val codec = BridgePayloadCodecSupport

    fun write(output: DataOutputStream, action: AutomationAction) {
        when (action) {
            is AutomationAction.MoveTo -> {
                output.writeInt(1)
                output.writeDouble(action.target.latitude)
                output.writeDouble(action.target.longitude)
                output.writeInt(codec.movementWireValue(action.mode))
            }
            is AutomationAction.OpenEncounter -> {
                output.writeInt(2)
                codec.writeString(output, action.spawnId)
            }
            is AutomationAction.Catch -> {
                if (action.throwProfile.isDefault) {
                    output.writeInt(if (action.closePreviewAfterCaught) 9 else 3)
                    codec.writeString(output, action.encounterId)
                    output.writeInt(action.reason.wireValue())
                } else {
                    output.writeInt(10)
                    codec.writeString(output, action.encounterId)
                    output.writeInt(action.reason.wireValue())
                    output.writeBoolean(action.closePreviewAfterCaught)
                    writeThrowProfile(output, action.throwProfile)
                }
            }
            is AutomationAction.TakeEncounterSnapshot -> {
                output.writeInt(11)
                codec.writeString(output, action.encounterId)
                output.writeInt(codec.encounterModeWireValue(action.encounterMode))
            }
            is AutomationAction.Spin -> {
                output.writeInt(4)
                codec.writeString(output, action.fortId)
            }
            is AutomationAction.DiscardItem -> {
                output.writeInt(5)
                output.writeInt(action.itemId)
                output.writeInt(action.amount)
            }
            is AutomationAction.TransferPokemon -> {
                output.writeInt(6)
                codec.writeString(output, action.pokemonId)
            }
            is AutomationAction.Alert -> {
                output.writeInt(7)
                output.writeInt(codec.alertWireValue(action.kind))
                codec.writeString(output, action.message)
            }
            is AutomationAction.UseBerry -> {
                output.writeInt(8)
                codec.writeString(output, action.encounterId)
                output.writeInt(codec.berryWireValue(action.berryType))
            }
        }
    }

    fun read(input: DataInputStream): AutomationAction = when (input.readInt()) {
        1 -> AutomationAction.MoveTo(
            target = GeoPoint(input.readDouble(), input.readDouble()),
            mode = codec.readEnum(input, MovementMode.entries, codec::movementWireValue),
        )
        2 -> AutomationAction.OpenEncounter(codec.readString(input))
        3 -> readCatch(input, closePreviewAfterCaught = false, hasThrowProfile = false)
        9 -> readCatch(input, closePreviewAfterCaught = true, hasThrowProfile = false)
        10 -> {
            val encounterId = codec.readString(input)
            val reason = codec.readEnum(input, CatchReason.entries) { it.wireValue() }
            AutomationAction.Catch(
                encounterId = encounterId,
                reason = reason,
                closePreviewAfterCaught = input.readBoolean(),
                throwProfile = readThrowProfile(input),
            )
        }
        11 -> AutomationAction.TakeEncounterSnapshot(
            encounterId = codec.readString(input),
            encounterMode = codec.readEnum(
                input,
                EncounterMode.entries,
                codec::encounterModeWireValue,
            ),
        )
        4 -> AutomationAction.Spin(codec.readString(input))
        5 -> AutomationAction.DiscardItem(input.readInt(), input.readInt())
        6 -> AutomationAction.TransferPokemon(codec.readString(input))
        7 -> AutomationAction.Alert(
            kind = codec.readEnum(input, AlertKind.entries, codec::alertWireValue),
            message = codec.readString(input),
        )
        8 -> AutomationAction.UseBerry(
            encounterId = codec.readString(input),
            berryType = codec.readEnum(input, BerryType.entries, codec::berryWireValue),
        )
        else -> error("unknown automation action tag")
    }

    private fun readCatch(
        input: DataInputStream,
        closePreviewAfterCaught: Boolean,
        hasThrowProfile: Boolean,
    ): AutomationAction.Catch = AutomationAction.Catch(
        encounterId = codec.readString(input),
        reason = codec.readEnum(input, CatchReason.entries) { it.wireValue() },
        closePreviewAfterCaught = closePreviewAfterCaught,
        throwProfile = if (hasThrowProfile) readThrowProfile(input) else ThrowProfile(),
    )

    private fun writeThrowProfile(output: DataOutputStream, value: ThrowProfile) {
        output.writeInt(codec.throwQualityTargetWireValue(value.qualityTarget))
        output.writeInt(codec.curvePreferenceWireValue(value.curvePreference))
        output.writeInt(codec.encounterModeWireValue(value.encounterMode))
    }

    private fun readThrowProfile(input: DataInputStream): ThrowProfile = ThrowProfile(
        qualityTarget = codec.readEnum(
            input,
            ThrowQualityTarget.entries,
            codec::throwQualityTargetWireValue,
        ),
        curvePreference = codec.readEnum(
            input,
            CurvePreference.entries,
            codec::curvePreferenceWireValue,
        ),
        encounterMode = codec.readEnum(
            input,
            EncounterMode.entries,
            codec::encounterModeWireValue,
        ),
    )

    private fun CatchReason.wireValue(): Int = when (this) {
        CatchReason.SHUNDO -> 1
        CatchReason.SHINY -> 2
        CatchReason.HUNDO -> 3
        CatchReason.IV_THRESHOLD -> 4
        CatchReason.CATCH_ALL -> 5
    }
}
