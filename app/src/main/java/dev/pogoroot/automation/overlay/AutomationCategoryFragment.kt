package dev.pogoroot.automation.overlay

import android.app.Fragment
import android.os.Bundle
import android.text.InputType
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.Switch
import android.widget.TextView
import dev.pogoroot.automation.core.time.TeleportCooldownMode
import dev.pogoroot.automation.headless.BerryMode

class AutomationCategoryFragment : Fragment() {
    companion object {
        private const val ARG_CATEGORY_ID = "category_id"

        fun newInstance(categoryId: String): AutomationCategoryFragment =
            AutomationCategoryFragment().apply {
                arguments = Bundle().apply { putString(ARG_CATEGORY_ID, categoryId) }
            }
    }

    val categoryId: String
        get() = arguments?.getString(ARG_CATEGORY_ID).orEmpty()

    private var saveAction: (() -> Unit)? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        val host = activity as AutomationSettingsActivity
        val editor = when (categoryId) {
            "automation" -> buildAutomationEditor(host)
            "discard" -> buildDiscardEditor(host)
            "transfer" -> buildTransferEditor(host)
            "berry" -> buildBerryEditor(host)
            "feedback" -> buildFeedbackEditor(host)
            "cooldown" -> buildCooldownEditor(host)
            else -> editorContent(host) to {}
        }
        saveAction = editor.second
        return ScrollView(host).apply {
            isFillViewport = true
            addView(editor.first)
        }
    }

    fun save() {
        saveAction?.invoke()
    }

    private fun buildAutomationEditor(host: AutomationSettingsActivity): Pair<LinearLayout, () -> Unit> {
        val config = host.repository.read()
        val enabled = switch(host, "Enable automation", config.enabled)
        val autoCatch = switch(host, "Auto catch", config.autoCatch)
        val autoCloseCatchPreview = switch(host, "Close catch preview after caught", config.autoCloseCatchPreview)
        val autoSpin = switch(host, "Auto spin", config.autoSpin)
        val autoEncounter = switch(host, "Auto encounter", config.autoEncounter)
        val content = editorContent(host).apply {
            addView(description(host, "These switches control the structured headless automation service."))
            addView(enabled)
            addView(autoCatch)
            addView(description(host, "Preview close only activates on a verified runtime that advertises the catch/close binding."))
            addView(autoCloseCatchPreview)
            addView(autoSpin)
            addView(autoEncounter)
        }
        return content to {
            host.repository.update { current ->
                current.copy(
                    enabled = enabled.isChecked,
                    autoCatch = autoCatch.isChecked,
                    autoCloseCatchPreview = autoCloseCatchPreview.isChecked,
                    autoSpin = autoSpin.isChecked,
                    autoEncounter = autoEncounter.isChecked,
                )
            }
        }
    }

    private fun buildDiscardEditor(host: AutomationSettingsActivity): Pair<LinearLayout, () -> Unit> {
        val config = host.repository.read()
        val autoDiscard = switch(host, "Auto discard items", config.autoDiscard)
        val ballLimit = numberInput(host, config.discardLimits[1] ?: 200)
        val greatBallLimit = numberInput(host, config.discardLimits[2] ?: 150)
        val ultraBallLimit = numberInput(host, config.discardLimits[3] ?: 100)
        val razzLimit = numberInput(host, config.discardLimits[701] ?: 50)
        val nanabLimit = numberInput(host, config.discardLimits[703] ?: 50)
        val pinapLimit = numberInput(host, config.discardLimits[705] ?: 80)
        val content = editorContent(host).apply {
            addView(autoDiscard)
            addLimitRow("Poké Ball", ballLimit)
            addLimitRow("Great Ball", greatBallLimit)
            addLimitRow("Ultra Ball", ultraBallLimit)
            addLimitRow("Razz Berry", razzLimit)
            addLimitRow("Nanab Berry", nanabLimit)
            addLimitRow("Pinap Berry", pinapLimit)
        }
        return content to {
            val limits = mapOf(
                1 to ballLimit.intValue(200),
                2 to greatBallLimit.intValue(150),
                3 to ultraBallLimit.intValue(100),
                701 to razzLimit.intValue(50),
                703 to nanabLimit.intValue(50),
                705 to pinapLimit.intValue(80),
            )
            host.repository.update { current ->
                current.copy(
                    autoDiscard = autoDiscard.isChecked,
                    discardLimits = limits,
                )
            }
        }
    }

    private fun buildTransferEditor(host: AutomationSettingsActivity): Pair<LinearLayout, () -> Unit> {
        val config = host.repository.read()
        val autoTransfer = switch(host, "Auto transfer Pokémon", config.autoTransfer)
        val keepHundo = switch(host, "Keep 100 IV", config.transferKeepHundo)
        val keepShiny = switch(host, "Keep Shiny", config.transferKeepShiny)
        val keepBackground = switch(host, "Keep Special Background (BG)", config.transferKeepSpecialBackground)
        val keepFavorite = switch(host, "Keep Favorite", config.transferKeepFavorite)
        val minIv = numberInput(host, config.transferMinimumIvPercent.toInt())
        val content = editorContent(host).apply {
            addView(autoTransfer)
            addView(keepHundo)
            addView(keepShiny)
            addView(keepBackground)
            addView(keepFavorite)
            addLimitRow("Minimum IV % to keep", minIv)
        }
        return content to {
            host.repository.update { current ->
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

    private fun buildBerryEditor(host: AutomationSettingsActivity): Pair<LinearLayout, () -> Unit> {
        val config = host.repository.read()
        val berrySpinner = Spinner(host).apply {
            adapter = android.widget.ArrayAdapter(
                host,
                android.R.layout.simple_spinner_dropdown_item,
                BerryMode.entries.map { mode -> AutomationSettingsCatalog.berryLabel(mode) },
            )
            setSelection(BerryMode.entries.indexOf(config.berryMode).coerceAtLeast(0))
        }
        val content = editorContent(host).apply {
            addView(description(host, "Use berry only after a structured encounter is confirmed"))
            addView(berrySpinner)
        }
        return content to {
            host.repository.update { current ->
                current.copy(berryMode = BerryMode.entries[berrySpinner.selectedItemPosition])
            }
        }
    }

    private fun buildFeedbackEditor(host: AutomationSettingsActivity): Pair<LinearLayout, () -> Unit> {
        val showToasts = switch(host, "Show catch/spin/action toast", host.repository.read().showActionToasts)
        val content = editorContent(host).apply { addView(showToasts) }
        return content to {
            host.repository.update { current -> current.copy(showActionToasts = showToasts.isChecked) }
        }
    }

    private fun buildCooldownEditor(host: AutomationSettingsActivity): Pair<LinearLayout, () -> Unit> {
        val group = RadioGroup(host).apply { orientation = RadioGroup.VERTICAL }
        val current = host.currentCooldownMode()
        val currentPosition = RadioButton(host).apply {
            text = "Current position"
            id = View.generateViewId()
            isChecked = current == TeleportCooldownMode.CURRENT_POSITION
        }
        val lastActive = RadioButton(host).apply {
            text = "Last active"
            id = View.generateViewId()
            isChecked = current == TeleportCooldownMode.LAST_ACTIVE
        }
        group.addView(currentPosition)
        group.addView(lastActive)
        val content = editorContent(host).apply {
            addView(description(host, "Cooldown is an estimate and is shown as HH:MM on the floating badge."))
            addView(group)
        }
        return content to {
            host.setCooldownMode(
                if (currentPosition.isChecked) {
                    TeleportCooldownMode.CURRENT_POSITION
                } else {
                    TeleportCooldownMode.LAST_ACTIVE
                },
            )
        }
    }

    private fun editorContent(context: AutomationSettingsActivity): LinearLayout =
        LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            val padding = context.dp(16)
            setPadding(padding, context.dp(8), padding, padding)
        }

    private fun description(context: AutomationSettingsActivity, text: String): TextView =
        TextView(context).apply {
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

    private fun switch(
        context: AutomationSettingsActivity,
        label: String,
        checked: Boolean,
    ): Switch = Switch(context).apply {
        text = label
        isChecked = checked
        minHeight = context.dp(48)
    }

    private fun numberInput(context: AutomationSettingsActivity, value: Int): EditText =
        EditText(context).apply {
            inputType = InputType.TYPE_CLASS_NUMBER
            setSingleLine(true)
            setText(value.toString())
            selectAll()
        }

    private fun EditText.intValue(defaultValue: Int): Int =
        text.toString().toIntOrNull()?.coerceAtLeast(0) ?: defaultValue
}
