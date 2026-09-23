package dev.pogoroot.automation.overlay

import android.content.Context
import android.text.InputType
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.Spinner
import android.widget.Switch
import android.widget.TextView
import dev.pogoroot.automation.config.AutomationConfigRepository
import dev.pogoroot.automation.config.BerryMode
import dev.pogoroot.automation.core.automation.MAX_SETTLE_DELAY_MS
import dev.pogoroot.automation.core.automation.ThrowQualityTarget
import dev.pogoroot.automation.core.time.TeleportCooldownMode

internal data class AutomationSettingsEditor(
    val view: View,
    val save: () -> Unit,
)

internal class AutomationSettingsEditorFactory(
    private val context: Context,
    private val repository: AutomationConfigRepository,
    private val currentCooldownMode: () -> TeleportCooldownMode,
    private val setCooldownMode: (TeleportCooldownMode) -> Unit,
) {
    fun build(categoryId: String): AutomationSettingsEditor = when (categoryId) {
        "automation" -> buildAutomationEditor()
        "discard" -> buildDiscardEditor()
        "transfer" -> buildTransferEditor()
        "berry" -> buildBerryEditor()
        "feedback" -> buildFeedbackEditor()
        "cooldown" -> buildCooldownEditor()
        else -> AutomationSettingsEditor(editorContent()) {}
    }

    private fun buildAutomationEditor(): AutomationSettingsEditor {
        val config = repository.read()
        val mapTapWalk = switch("Walk to verified map taps", config.mapTapWalkEnabled)
        val autoWalkToFort = switch(
            "Walk between forts when no Pokémon",
            config.autoWalkToFort,
        )
        val autoCatch = switch("Auto catch", config.autoCatch)
        val catchAll = switch("Catch all nearby Pokémon", config.catchAll)
        val throwQualityGroup = RadioGroup(context).apply {
            orientation = RadioGroup.VERTICAL
        }
        val throwQualityOptions = ThrowQualityTarget.entries.map { target ->
            target to RadioButton(context).apply {
                text = AutomationSettingsCatalog.throwQualityLabel(target)
                id = View.generateViewId()
                isChecked = target == config.catchThrowQuality
            }
        }
        throwQualityOptions.forEach { (_, option) -> throwQualityGroup.addView(option) }
        val autoCloseCatchPreview = switch(
            "Close catch preview after caught",
            config.autoCloseCatchPreview,
        )
        val autoSpin = switch("Auto spin", config.autoSpin)
        val autoEncounter = switch("Auto encounter", config.autoEncounter)
        val spinSettleDelay = numberInput(config.spinSettleDelayMs.toInt())
        val catchSettleDelay = numberInput(config.catchSettleDelayMs.toInt())
        val content = editorContent().apply {
            addView(description("These switches control the structured headless automation service."))
            addView(description("Start or stop the live automation master switch from the floating overlay. It is process-local and is never stored."))
            addView(description("Only verified MAP_TARGET observations from the exact runtime binding can start a walk."))
            addView(mapTapWalk)
            addView(description("When enabled, an empty complete map scan walks to the nearest available PokéStop. A Pokémon found during the walk pauses movement until catch/flee is complete."))
            addView(autoWalkToFort)
            addView(autoCatch)
            addView(catchAll)
            addView(description("Auto throw quality is a client-owned request. Excellent is used only when the exact runtime advertises THROW_CONTROL and OBSERVE_THROW_OUTCOME; it is not a guaranteed catch."))
            addView(throwQualityGroup)
            addView(description("Guaranteed catch from a user's throw is not available in this runtime: a miss remains a MISSED attempt and is never rewritten as CAUGHT."))
            addView(description("Preview close only activates on a verified runtime that advertises the catch/close binding."))
            addView(autoCloseCatchPreview)
            addView(autoSpin)
            addView(autoEncounter)
            addView(description("Wait after a completed action before planning the next mutation. Values are milliseconds; 0 disables the wait."))
            addLimitRow("Spin settle delay (ms)", spinSettleDelay)
            addLimitRow("Catch settle delay (ms)", catchSettleDelay)
        }
        return AutomationSettingsEditor(content) {
            repository.update { current ->
                current.copy(
                    mapTapWalkEnabled = mapTapWalk.isChecked,
                    autoWalkToFort = autoWalkToFort.isChecked,
                    autoCatch = autoCatch.isChecked,
                    catchAll = catchAll.isChecked,
                    catchThrowQuality = throwQualityOptions.first { it.second.isChecked }.first,
                    autoCloseCatchPreview = autoCloseCatchPreview.isChecked,
                    autoSpin = autoSpin.isChecked,
                    autoEncounter = autoEncounter.isChecked,
                    spinSettleDelayMs = spinSettleDelay.intValue(config.spinSettleDelayMs.toInt())
                        .toLong().coerceIn(0L, MAX_SETTLE_DELAY_MS),
                    catchSettleDelayMs = catchSettleDelay.intValue(config.catchSettleDelayMs.toInt())
                        .toLong().coerceIn(0L, MAX_SETTLE_DELAY_MS),
                )
            }
        }
    }

    private fun buildDiscardEditor(): AutomationSettingsEditor {
        val config = repository.read()
        val autoDiscard = switch("Auto discard items", config.autoDiscard)
        val ballLimit = numberInput(config.discardLimits[1] ?: 200)
        val greatBallLimit = numberInput(config.discardLimits[2] ?: 150)
        val ultraBallLimit = numberInput(config.discardLimits[3] ?: 100)
        val potionLimit = numberInput(config.discardLimits[101] ?: 50)
        val superPotionLimit = numberInput(config.discardLimits[102] ?: 50)
        val hyperPotionLimit = numberInput(config.discardLimits[103] ?: 50)
        val maxPotionLimit = numberInput(config.discardLimits[104] ?: 20)
        val reviveLimit = numberInput(config.discardLimits[201] ?: 50)
        val maxReviveLimit = numberInput(config.discardLimits[202] ?: 30)
        val razzLimit = numberInput(config.discardLimits[701] ?: 50)
        val nanabLimit = numberInput(config.discardLimits[703] ?: 50)
        val pinapLimit = numberInput(config.discardLimits[705] ?: 80)
        val content = editorContent().apply {
            addView(autoDiscard)
            addLimitRow("Poké Ball", ballLimit)
            addLimitRow("Great Ball", greatBallLimit)
            addLimitRow("Ultra Ball", ultraBallLimit)
            addLimitRow("Potion", potionLimit)
            addLimitRow("Super Potion", superPotionLimit)
            addLimitRow("Hyper Potion", hyperPotionLimit)
            addLimitRow("Max Potion", maxPotionLimit)
            addLimitRow("Revive", reviveLimit)
            addLimitRow("Max Revive", maxReviveLimit)
            addLimitRow("Razz Berry", razzLimit)
            addLimitRow("Nanab Berry", nanabLimit)
            addLimitRow("Pinap Berry", pinapLimit)
        }
        return AutomationSettingsEditor(content) {
            repository.update { current ->
                current.copy(
                    autoDiscard = autoDiscard.isChecked,
                    discardLimits = mapOf(
                        1 to ballLimit.intValue(200),
                        2 to greatBallLimit.intValue(150),
                        3 to ultraBallLimit.intValue(100),
                        101 to potionLimit.intValue(50),
                        102 to superPotionLimit.intValue(50),
                        103 to hyperPotionLimit.intValue(50),
                        104 to maxPotionLimit.intValue(20),
                        201 to reviveLimit.intValue(50),
                        202 to maxReviveLimit.intValue(30),
                        701 to razzLimit.intValue(50),
                        703 to nanabLimit.intValue(50),
                        705 to pinapLimit.intValue(80),
                    ),
                )
            }
        }
    }

    private fun buildTransferEditor(): AutomationSettingsEditor {
        val config = repository.read()
        val autoTransfer = switch("Auto transfer Pokémon", config.autoTransfer)
        val keepHundo = switch("Keep 100 IV", config.transferKeepHundo)
        val keepShiny = switch("Keep Shiny", config.transferKeepShiny)
        val keepBackground = switch(
            "Keep Special Background (BG)",
            config.transferKeepSpecialBackground,
        )
        val keepFavorite = switch("Keep Favorite", config.transferKeepFavorite)
        val minIv = numberInput(config.transferMinimumIvPercent.toInt())
        val content = editorContent().apply {
            addView(autoTransfer)
            addView(keepHundo)
            addView(keepShiny)
            addView(keepBackground)
            addView(keepFavorite)
            addLimitRow("Minimum IV % to keep", minIv)
        }
        return AutomationSettingsEditor(content) {
            repository.update { current ->
                current.copy(
                    autoTransfer = autoTransfer.isChecked,
                    transferKeepHundo = keepHundo.isChecked,
                    transferKeepShiny = keepShiny.isChecked,
                    transferKeepSpecialBackground = keepBackground.isChecked,
                    transferKeepFavorite = keepFavorite.isChecked,
                    transferMinimumIvPercent = minIv.intValue(80).coerceIn(0, 100).toDouble(),
                )
            }
        }
    }

    private fun buildBerryEditor(): AutomationSettingsEditor {
        val config = repository.read()
        val berrySpinner = Spinner(context).apply {
            adapter = ArrayAdapter(
                context,
                android.R.layout.simple_spinner_dropdown_item,
                BerryMode.entries.map { mode -> AutomationSettingsCatalog.berryLabel(mode) },
            )
            setSelection(BerryMode.entries.indexOf(config.berryMode).coerceAtLeast(0))
        }
        val content = editorContent().apply {
            addView(description("Use berry only after a structured encounter is confirmed"))
            addView(berrySpinner)
        }
        return AutomationSettingsEditor(content) {
            repository.update { current ->
                current.copy(berryMode = BerryMode.entries[berrySpinner.selectedItemPosition])
            }
        }
    }

    private fun buildFeedbackEditor(): AutomationSettingsEditor {
        val showToasts = switch(
            "Show catch/spin/action toast",
            repository.read().showActionToasts,
        )
        val content = editorContent().apply { addView(showToasts) }
        return AutomationSettingsEditor(content) {
            repository.update { current -> current.copy(showActionToasts = showToasts.isChecked) }
        }
    }

    private fun buildCooldownEditor(): AutomationSettingsEditor {
        val group = RadioGroup(context).apply { orientation = RadioGroup.VERTICAL }
        val current = currentCooldownMode()
        val currentPosition = RadioButton(context).apply {
            text = "Current position"
            id = View.generateViewId()
            isChecked = current == TeleportCooldownMode.CURRENT_POSITION
        }
        val lastActive = RadioButton(context).apply {
            text = "Last active"
            id = View.generateViewId()
            isChecked = current == TeleportCooldownMode.LAST_ACTIVE
        }
        group.addView(currentPosition)
        group.addView(lastActive)
        val content = editorContent().apply {
            addView(description("Cooldown is an estimate and is shown as HH:MM on the floating badge."))
            addView(group)
        }
        return AutomationSettingsEditor(content) {
            setCooldownMode(
                if (currentPosition.isChecked) {
                    TeleportCooldownMode.CURRENT_POSITION
                } else {
                    TeleportCooldownMode.LAST_ACTIVE
                },
            )
        }
    }

    private fun editorContent(): LinearLayout = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        val padding = context.dp(16)
        setPadding(padding, context.dp(8), padding, padding)
    }

    private fun description(text: String): TextView = TextView(context).apply {
        this.text = text
        setTextColor(0xFF5F6368.toInt())
        setPadding(0, 0, 0, context.dp(12))
    }

    private fun LinearLayout.addLimitRow(label: String, input: EditText) {
        addView(LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = android.view.Gravity.CENTER_VERTICAL
            addView(TextView(context).apply {
                text = label
                gravity = android.view.Gravity.CENTER_VERTICAL
            }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            addView(input, LinearLayout.LayoutParams(context.dp(92), ViewGroup.LayoutParams.WRAP_CONTENT))
        })
    }

    private fun switch(label: String, checked: Boolean): Switch = Switch(context).apply {
        text = label
        isChecked = checked
        minHeight = context.dp(48)
    }

    private fun numberInput(value: Int): EditText = EditText(context).apply {
        inputType = InputType.TYPE_CLASS_NUMBER
        setSingleLine(true)
        setText(value.toString())
        selectAll()
    }

    private fun EditText.intValue(defaultValue: Int): Int =
        text.toString().toIntOrNull()?.coerceAtLeast(0) ?: defaultValue
}
