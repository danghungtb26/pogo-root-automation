package dev.pogoroot.automation.overlay

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.GridLayout
import android.widget.LinearLayout
import android.widget.TextView
import dev.pogoroot.automation.headless.HeadlessAutomationConfig

internal class ShortcutMenuView(
    private val context: Context,
    private val speedPresets: DoubleArray,
    private val onToggle: (String) -> Unit,
    private val onJoystick: () -> Unit,
    private val onTeleport: () -> Unit,
    private val onFavorites: () -> Unit,
    private val onSpeed: () -> Unit,
    private val onSettings: () -> Unit,
    private val onClose: () -> Unit,
) {
    private val shortcutContainers = linkedMapOf<String, LinearLayout>()
    private val shortcutLabels = linkedMapOf<String, TextView>()

    val view: LinearLayout = buildView()

    fun render(
        config: HeadlessAutomationConfig,
        speedPresetIndex: Int,
        automationActive: Boolean,
    ) {
        setToggleShortcut("automation", "Automation", automationActive)
        setToggleShortcut("catch", "Catch", config.autoCatch)
        setToggleShortcut("spin", "Spin", config.autoSpin)
        setToggleShortcut("encounter", "Encounter", config.autoEncounter)
        setToggleShortcut("discard", "Discard", config.autoDiscard)
        setToggleShortcut("transfer", "Transfer", config.autoTransfer)
        shortcutLabels["speed"]?.text = "Speed\n${speedPresets[speedPresetIndex].formatSpeed()} km/h"
    }

    private fun buildView(): LinearLayout {
        val grid = GridLayout(context).apply {
            columnCount = 3
            useDefaultMargins = false
        }

        addShortcut(grid, "automation", "🔴", "Automation") { onToggle("automation") }
        addShortcut(grid, "catch", "C", "Catch") { onToggle("catch") }
        addShortcut(grid, "spin", "↻", "Spin") { onToggle("spin") }
        addShortcut(grid, "encounter", "◎", "Encounter") { onToggle("encounter") }
        addShortcut(grid, "discard", "▣", "Discard") { onToggle("discard") }
        addShortcut(grid, "transfer", "⇆", "Transfer") { onToggle("transfer") }
        addShortcut(grid, "joystick", "✣", "Joystick", onJoystick)
        addShortcut(grid, "teleport", "⌖", "Teleport", onTeleport)
        addShortcut(grid, "favorites", "★", "Favorites", onFavorites)
        addShortcut(grid, "speed", "⚡", "Speed", onSpeed)
        addShortcut(grid, "settings", "⚙", "Settings", onSettings)
        addShortcut(grid, "close", "×", "Close", onClose)

        return LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            val padding = context.dp(8)
            setPadding(padding, padding, padding, padding)
            background = context.roundedBackground(0xE6202124.toInt(), 14)
            addView(TextView(context).apply {
                text = "PoGo Tools"
                textSize = 13f
                setTextColor(Color.WHITE)
                typeface = Typeface.DEFAULT_BOLD
                gravity = Gravity.CENTER
                setPadding(0, 0, 0, context.dp(6))
            })
            addView(grid)
        }
    }

    private fun addShortcut(
        grid: GridLayout,
        key: String,
        symbol: String,
        label: String,
        action: () -> Unit,
    ) {
        val icon = TextView(context).apply {
            text = symbol
            textSize = 22f
            setTextColor(Color.WHITE)
            gravity = Gravity.CENTER
        }
        val labelView = TextView(context).apply {
            text = label
            textSize = 10f
            setTextColor(0xFFE8EAED.toInt())
            gravity = Gravity.CENTER
            maxLines = 2
        }
        val item = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            isClickable = true
            isFocusable = true
            contentDescription = label
            setPadding(context.dp(2), context.dp(4), context.dp(2), context.dp(4))
            setOnClickListener { action() }
            addView(icon, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, context.dp(32)))
            addView(labelView, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, context.dp(28)))
        }
        shortcutContainers[key] = item
        shortcutLabels[key] = labelView
        grid.addView(item, GridLayout.LayoutParams().apply {
            width = context.dp(72)
            height = context.dp(66)
            setMargins(context.dp(2), context.dp(2), context.dp(2), context.dp(2))
        })
    }

    private fun setToggleShortcut(key: String, label: String, enabled: Boolean) {
        val container = shortcutContainers[key] ?: return
        val labelView = shortcutLabels[key] ?: return
        labelView.text = "$label\n${if (enabled) "ON" else "OFF"}"
        labelView.setTextColor(if (enabled) 0xFFB9F6CA.toInt() else 0xFFE8EAED.toInt())
        container.background = context.roundedBackground(
            if (enabled) 0xE62E7D32.toInt() else 0xB8323438.toInt(),
            10,
        )
        container.contentDescription = "$label ${if (enabled) "on" else "off"}"
    }

    private fun Double.formatSpeed(): String = if (this % 1.0 == 0.0) {
        toInt().toString()
    } else {
        String.format(java.util.Locale.US, "%.1f", this)
    }
}
