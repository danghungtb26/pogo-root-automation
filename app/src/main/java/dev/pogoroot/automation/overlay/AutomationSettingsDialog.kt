package dev.pogoroot.automation.overlay

import android.app.AlertDialog
import android.content.Context
import android.graphics.Typeface
import android.os.Build
import android.text.InputType
import android.view.ContextThemeWrapper
import android.view.WindowManager
import android.widget.ArrayAdapter
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.Switch
import android.widget.TextView
import dev.pogoroot.automation.headless.AutomationConfigRepository
import dev.pogoroot.automation.headless.BerryMode
import dev.pogoroot.automation.headless.HeadlessAutomationConfig

class AutomationSettingsDialog(
    private val context: Context,
    private val repository: AutomationConfigRepository,
) {
    fun show() {
        val themed = ContextThemeWrapper(context, android.R.style.Theme_Material_Light_Dialog_Alert)
        val density = themed.resources.displayMetrics.density
        val padding = (16 * density).toInt()
        val config = repository.read()

        val autoDiscard = switch(themed, "Auto discard items", config.autoDiscard)
        val ballLimit = numberInput(themed, config.discardLimits[1] ?: 200)
        val greatBallLimit = numberInput(themed, config.discardLimits[2] ?: 150)
        val ultraBallLimit = numberInput(themed, config.discardLimits[3] ?: 100)
        val razzLimit = numberInput(themed, config.discardLimits[701] ?: 50)
        val nanabLimit = numberInput(themed, config.discardLimits[703] ?: 50)
        val pinapLimit = numberInput(themed, config.discardLimits[705] ?: 80)

        val autoTransfer = switch(themed, "Auto transfer Pokémon", config.autoTransfer)
        val keepHundo = switch(themed, "Keep 100 IV", config.transferKeepHundo)
        val keepShiny = switch(themed, "Keep Shiny", config.transferKeepShiny)
        val keepBackground = switch(themed, "Keep Special Background (BG)", config.transferKeepSpecialBackground)
        val keepFavorite = switch(themed, "Keep Favorite", config.transferKeepFavorite)
        val minIv = numberInput(themed, config.transferMinimumIvPercent.toInt())

        val berrySpinner = Spinner(themed).apply {
            adapter = ArrayAdapter(
                themed,
                android.R.layout.simple_spinner_dropdown_item,
                BerryMode.entries.map(::berryLabel),
            )
            setSelection(BerryMode.entries.indexOf(config.berryMode).coerceAtLeast(0))
        }
        val showToasts = switch(themed, "Show catch/spin/action toast", config.showActionToasts)

        val content = LinearLayout(themed).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(padding, padding / 2, padding, padding)

            addSection("Item discard")
            addView(autoDiscard)
            addLimitRow("Poké Ball", ballLimit)
            addLimitRow("Great Ball", greatBallLimit)
            addLimitRow("Ultra Ball", ultraBallLimit)
            addLimitRow("Razz Berry", razzLimit)
            addLimitRow("Nanab Berry", nanabLimit)
            addLimitRow("Pinap Berry", pinapLimit)

            addSection("Pokémon transfer")
            addView(autoTransfer)
            addView(keepHundo)
            addView(keepShiny)
            addView(keepBackground)
            addView(keepFavorite)
            addLimitRow("Minimum IV % to keep", minIv)

            addSection("Encounter berry")
            addView(TextView(themed).apply { text = "Use berry only after encounter screen is confirmed" })
            addView(berrySpinner)

            addSection("Feedback")
            addView(showToasts)
        }

        val scroll = ScrollView(themed).apply { addView(content) }
        val dialog = AlertDialog.Builder(themed)
            .setTitle("Automation Settings")
            .setView(scroll)
            .setPositiveButton("Save", null)
            .setNegativeButton("Cancel", null)
            .create()

        dialog.window?.setType(overlayWindowType())
        dialog.setOnShowListener {
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
                val limits = mapOf(
                    1 to ballLimit.intValue(200),
                    2 to greatBallLimit.intValue(150),
                    3 to ultraBallLimit.intValue(100),
                    701 to razzLimit.intValue(50),
                    703 to nanabLimit.intValue(50),
                    705 to pinapLimit.intValue(80),
                )
                repository.update { current ->
                    current.copy(
                        autoDiscard = autoDiscard.isChecked,
                        discardLimits = limits,
                        autoTransfer = autoTransfer.isChecked,
                        transferKeepHundo = keepHundo.isChecked,
                        transferKeepShiny = keepShiny.isChecked,
                        transferKeepSpecialBackground = keepBackground.isChecked,
                        transferKeepFavorite = keepFavorite.isChecked,
                        transferMinimumIvPercent = minIv.intValue(80).coerceIn(0, 100).toDouble(),
                        berryMode = BerryMode.entries[berrySpinner.selectedItemPosition],
                        showActionToasts = showToasts.isChecked,
                    )
                }
                dialog.dismiss()
            }
        }
        dialog.show()
    }

    private fun LinearLayout.addSection(title: String) {
        addView(TextView(context).apply {
            text = title
            textSize = 18f
            setTypeface(typeface, Typeface.BOLD)
            setPadding(0, 18, 0, 6)
        })
    }

    private fun LinearLayout.addLimitRow(label: String, input: EditText) {
        addView(LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            addView(TextView(context).apply {
                text = label
                gravity = android.view.Gravity.CENTER_VERTICAL
            }, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            addView(input, LinearLayout.LayoutParams((92 * resources.displayMetrics.density).toInt(), LinearLayout.LayoutParams.WRAP_CONTENT))
        })
    }

    private fun switch(context: Context, label: String, checked: Boolean) = Switch(context).apply {
        text = label
        isChecked = checked
    }

    private fun numberInput(context: Context, value: Int) = EditText(context).apply {
        inputType = InputType.TYPE_CLASS_NUMBER
        setSingleLine(true)
        setText(value.toString())
        selectAll()
    }

    private fun EditText.intValue(defaultValue: Int): Int =
        text.toString().toIntOrNull()?.coerceAtLeast(0) ?: defaultValue

    private fun berryLabel(mode: BerryMode): String = when (mode) {
        BerryMode.NONE -> "Off"
        BerryMode.RAZZ -> "Razz Berry"
        BerryMode.NANAB -> "Nanab Berry"
        BerryMode.PINAP -> "Pinap Berry"
        BerryMode.GOLDEN_RAZZ -> "Golden Razz Berry"
        BerryMode.SILVER_PINAP -> "Silver Pinap Berry"
    }

    private fun overlayWindowType(): Int = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
    } else {
        @Suppress("DEPRECATION")
        WindowManager.LayoutParams.TYPE_PHONE
    }
}
