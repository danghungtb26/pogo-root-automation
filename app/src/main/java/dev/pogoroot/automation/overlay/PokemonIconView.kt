package dev.pogoroot.automation.overlay

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.view.Gravity
import android.widget.TextView
import dev.pogoroot.automation.core.scan.ScanResultSummary

/** Small icon-only representation used by the floating scan widgets. */
internal class PokemonIconView(
    context: Context,
    result: ScanResultSummary,
) : TextView(context) {
    init {
        gravity = Gravity.CENTER
        textSize = 21f
        setTypeface(Typeface.DEFAULT, Typeface.NORMAL)
        includeFontPadding = false
        text = glyphFor(result.speciesId)
        setTextColor(Color.WHITE)
        background = context.getDrawable(android.R.drawable.btn_default_small)
        contentDescription = result.speciesName
        tag = result.spawnId
    }

    private fun glyphFor(speciesId: Int): String = when (speciesId) {
        1 -> "🌱"
        4 -> "🔥"
        7 -> "💧"
        25 -> "⚡"
        39 -> "🎵"
        52 -> "🐱"
        54 -> "🦆"
        129 -> "🐟"
        131 -> "🐋"
        133 -> "🦊"
        143 -> "🐻"
        149 -> "🐉"
        150 -> "🧬"
        151 -> "✨"
        else -> "◉"
    }
}
