package dev.pogoroot.automation.overlay

import android.app.AlertDialog
import android.content.Context
import android.os.Build
import android.text.InputType
import android.view.ContextThemeWrapper
import android.view.Gravity
import android.view.WindowManager
import android.widget.CheckBox
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import dev.pogoroot.automation.headless.AutomationConfigRepository
import dev.pogoroot.automation.headless.HeadlessAutomationConfig
import dev.pogoroot.automation.headless.HeadlessAutomationService

class AutomationSettingsOverlay(
    private val context: Context,
    private val repository: AutomationConfigRepository,
    private val onSaved: (HeadlessAutomationConfig) -> Unit = {},
) {
    fun show() {
        val config = repository.read()
        val themedContext = ContextThemeWrapper(
            context,
            android.R.style.Theme_Material_Light_Dialog_Alert,
        )
        val density = context.resources.displayMetrics.density
        val padding = (16 * density).toInt()

        val content = LinearLayout(themedContext).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(padding, padding / 2, padding, padding)
        }

        val enabledSwitch = switch(themedContext, "Automation master", config.enabled)
        val catchSwitch = switch(themedContext, "Auto catch", config.autoCatch)
        val spinSwitch = switch(themedContext, "Auto spin PokéStop", config.autoSpin)
        val berrySwitch = switch(themedContext, "Auto berry on encounter", config.autoBerry)
        val discardSwitch = switch(themedContext, "Auto discard items", config.autoDiscard)
        val transferSwitch = switch(themedContext, "Auto transfer Pokémon", config.autoTransfer)
        val toastSwitch = switch(themedContext, "Show action/result toasts", config.showToasts)

        content.addView(sectionTitle(themedContext, "General"))
        content.addView(enabledSwitch)
        content.addView(catchSwitch)
        content.addView(spinSwitch)
        content.addView(berrySwitch)

        content.addView(sectionTitle(themedContext, "Auto discard"))
        content.addView(discardSwitch)
        content.addView(TextView(themedContext).apply {
            text = "Keep at most this many. Leave blank to ignore an item."
            textSize = 12f
        })

        val limitInputs = COMMON_ITEMS.associateWith { item ->
            itemLimitRow(
                context = themedContext,
                item = item,
                current = config.discardLimits[item.id],
            ).also { row -> content.addView(row.container) }.input
        }

        content.addView(sectionTitle(themedContext, "Auto transfer"))
        content.addView(transferSwitch)

        val transferThresholdInput = EditText(themedContext).apply {
            hint = "Transfer below IV %"
            inputType = InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL
            setText(config.transferBelowIvPercent.formatPercent())
        }
        content.addView(labeledInput(themedContext, "Transfer below IV %", transferThresholdInput))

        val keepHundo = checkBox(themedContext, "Exclude 100 IV", config.keepHundo)
        val keepShiny = checkBox(themedContext, "Exclude Shiny", config.keepShiny)
        val keepBackground = checkBox(themedContext, "Exclude special background (BG)", config.keepSpecialBackground)
        val keepFavorite = checkBox(themedContext, "Exclude Favorite", config.keepFavorite)
        content.addView(keepHundo)
        content.addView(keepShiny)
        content.addView(keepBackground)
        content.addView(keepFavorite)

        content.addView(sectionTitle(themedContext, "Notifications"))
        content.addView(toastSwitch)

        val scroll = ScrollView(themedContext).apply {
            addView(content)
        }

        val dialog = AlertDialog.Builder(themedContext)
            .setTitle("PoGo automation settings")
            .setView(scroll)
            .setPositiveButton("Save", null)
            .setNegativeButton("Cancel", null)
            .create()

        dialog.window?.setType(overlayWindowType())
        dialog.setOnShowListener {
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
                val threshold = transferThresholdInput.text.toString().trim().toDoubleOrNull()
                if (threshold == null || threshold !in 0.0..100.0) {
                    transferThresholdInput.error = "Use a value from 0 to 100"
                    return@setOnClickListener
                }

                val limits = mutableMapOf<Int, Int>()
                for ((item, input) in limitInputs) {
                    val raw = input.text.toString().trim()
                    if (raw.isBlank()) continue
                    val value = raw.toIntOrNull()
                    if (value == null || value < 0) {
                        input.error = "Use 0 or greater"
                        return@setOnClickListener
                    }
                    limits[item.id] = value
                }

                val saved = repository.update { current ->
                    current.copy(
                        enabled = enabledSwitch.isChecked,
                        autoCatch = catchSwitch.isChecked,
                        autoSpin = spinSwitch.isChecked,
                        autoBerry = berrySwitch.isChecked,
                        autoDiscard = discardSwitch.isChecked,
                        discardLimits = limits,
                        autoTransfer = transferSwitch.isChecked,
                        transferBelowIvPercent = threshold,
                        keepHundo = keepHundo.isChecked,
                        keepShiny = keepShiny.isChecked,
                        keepSpecialBackground = keepBackground.isChecked,
                        keepFavorite = keepFavorite.isChecked,
                        showToasts = toastSwitch.isChecked,
                    )
                }

                HeadlessAutomationService.start(context)
                onSaved(saved)
                Toast.makeText(context, "Automation settings saved", Toast.LENGTH_SHORT).show()
                dialog.dismiss()
            }
        }
        dialog.show()
    }

    private fun switch(context: Context, label: String, checked: Boolean): Switch =
        @Suppress("DEPRECATION")
        Switch(context).apply {
            text = label
            isChecked = checked
        }

    private fun checkBox(context: Context, label: String, checked: Boolean): CheckBox =
        CheckBox(context).apply {
            text = label
            isChecked = checked
        }

    private fun sectionTitle(context: Context, title: String): TextView = TextView(context).apply {
        text = title
        textSize = 17f
        setTypeface(typeface, android.graphics.Typeface.BOLD)
        val vertical = (10 * resources.displayMetrics.density).toInt()
        setPadding(0, vertical, 0, vertical / 3)
    }

    private fun itemLimitRow(
        context: Context,
        item: ManagedItem,
        current: Int?,
    ): ItemLimitRow {
        val input = EditText(context).apply {
            inputType = InputType.TYPE_CLASS_NUMBER
            gravity = Gravity.END
            hint = "ignore"
            current?.let { setText(it.toString()) }
        }
        val label = TextView(context).apply {
            text = item.name
            textSize = 14f
            gravity = Gravity.CENTER_VERTICAL
        }
        val container = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            addView(label, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            addView(input, LinearLayout.LayoutParams((100 * resources.displayMetrics.density).toInt(), LinearLayout.LayoutParams.WRAP_CONTENT))
        }
        return ItemLimitRow(container, input)
    }

    private fun labeledInput(context: Context, label: String, input: EditText): LinearLayout =
        LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            addView(TextView(context).apply {
                text = label
                textSize = 14f
            }, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            addView(input, LinearLayout.LayoutParams((100 * resources.displayMetrics.density).toInt(), LinearLayout.LayoutParams.WRAP_CONTENT))
        }

    private fun overlayWindowType(): Int = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
    } else {
        @Suppress("DEPRECATION")
        WindowManager.LayoutParams.TYPE_PHONE
    }

    private fun Double.formatPercent(): String = if (this % 1.0 == 0.0) {
        toInt().toString()
    } else {
        String.format(java.util.Locale.US, "%.1f", this)
    }

    private data class ItemLimitRow(
        val container: LinearLayout,
        val input: EditText,
    )

    data class ManagedItem(
        val id: Int,
        val name: String,
    )

    companion object {
        val COMMON_ITEMS = listOf(
            ManagedItem(1, "Poké Ball"),
            ManagedItem(2, "Great Ball"),
            ManagedItem(3, "Ultra Ball"),
            ManagedItem(101, "Potion"),
            ManagedItem(102, "Super Potion"),
            ManagedItem(103, "Hyper Potion"),
            ManagedItem(104, "Max Potion"),
            ManagedItem(201, "Revive"),
            ManagedItem(202, "Max Revive"),
            ManagedItem(701, "Razz Berry"),
            ManagedItem(703, "Nanab Berry"),
            ManagedItem(705, "Pinap Berry"),
        )
    }
}
