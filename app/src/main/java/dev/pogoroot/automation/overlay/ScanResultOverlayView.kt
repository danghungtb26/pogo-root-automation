package dev.pogoroot.automation.overlay

import android.content.Context
import android.view.Gravity
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import dev.pogoroot.automation.core.scan.ScanMatchType
import dev.pogoroot.automation.core.scan.ScanResultSummary

internal class ScanResultOverlayView(
    context: Context,
    private val matchType: ScanMatchType,
    private val onOpenAll: () -> Unit,
) : LinearLayout(context) {
    companion object {
        private const val MAX_VISIBLE_ITEMS = 6
        private const val ICON_DP = 38
        private const val GAP_DP = 2
        private const val HEADER_DP = 18
    }

    init {
        orientation = VERTICAL
        gravity = Gravity.CENTER_HORIZONTAL
        val padding = context.dp(4)
        setPadding(padding, padding, padding, padding)
        background = context.roundedBackground(0xE6202124.toInt(), 10)
        isClickable = true
        isFocusable = true
        setOnClickListener { onOpenAll() }
        contentDescription = if (matchType == ScanMatchType.HUNDO) {
            "100 IV results"
        } else {
            "Shiny results"
        }
        visibility = View.INVISIBLE
    }

    fun render(allResults: List<ScanResultSummary>?) {
        val results = allResults.orEmpty()
        removeAllViews()
        if (results.isEmpty()) {
            visibility = View.INVISIBLE
            return
        }

        val marker = TextView(context).apply {
            gravity = Gravity.CENTER
            text = if (matchType == ScanMatchType.HUNDO) "💯" else "✨"
            textSize = 11f
        }
        addView(marker, LayoutParams(context.dp(ICON_DP), context.dp(HEADER_DP)))

        val visibleCount = minOf(MAX_VISIBLE_ITEMS, results.size)
        for (index in 0 until visibleCount) {
            val icon = PokemonIconView(context, results[index])
            val params = LayoutParams(context.dp(ICON_DP), context.dp(ICON_DP))
            if (index > 0) params.topMargin = context.dp(GAP_DP)
            addView(icon, params)
        }
        visibility = View.VISIBLE
        requestLayout()
    }

    fun desiredHeightPx(): Int {
        val visibleCount = minOf(MAX_VISIBLE_ITEMS, childCount - 1)
        val padding = paddingTop + paddingBottom
        if (visibleCount <= 0) return padding
        return padding + context.dp(HEADER_DP) +
            visibleCount * context.dp(ICON_DP) +
            maxOf(0, visibleCount - 1) * context.dp(GAP_DP)
    }
}
