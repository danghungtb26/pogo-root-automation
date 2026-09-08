package dev.pogoroot.automation.overlay

import android.app.Fragment
import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import dev.pogoroot.automation.core.scan.ScanMatchType
import dev.pogoroot.automation.core.scan.ScanResultSummary
import dev.pogoroot.automation.scan.ScanResultRepository
import java.util.Locale

class ScanResultsFragment : Fragment() {
    companion object {
        private const val ARG_TYPE = "match_type"

        fun newInstance(matchType: ScanMatchType): ScanResultsFragment =
            ScanResultsFragment().apply {
                arguments = Bundle().apply { putString(ARG_TYPE, matchType.name) }
            }
    }

    private val handler = Handler(Looper.getMainLooper())
    private lateinit var matchType: ScanMatchType
    private lateinit var repository: ScanResultRepository
    private lateinit var list: LinearLayout

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        matchType = readType(arguments?.getString(ARG_TYPE))
        repository = ScanResultRepository(context())

        val scroll = ScrollView(context())
        list = LinearLayout(context()).apply {
            orientation = LinearLayout.VERTICAL
            val padding = context().dp(12)
            setPadding(0, padding, 0, padding)
        }
        scroll.addView(list, ViewGroup.LayoutParams(-1, -2))
        return scroll
    }

    override fun onResume() {
        super.onResume()
        render()
        handler.removeCallbacks(refresh)
        handler.post(refresh)
    }

    override fun onPause() {
        handler.removeCallbacks(refresh)
        super.onPause()
    }

    private val refresh = object : Runnable {
        override fun run() {
            if (!isAdded) return
            render()
            handler.postDelayed(this, 1_000L)
        }
    }

    private fun render() {
        if (!::list.isInitialized || !::repository.isInitialized) return
        list.removeAllViews()
        val results = repository.read(matchType)
        if (results.isEmpty()) {
            val empty = TextView(context()).apply {
                text = if (matchType == ScanMatchType.HUNDO) {
                    "No confirmed 100 IV results"
                } else {
                    "No confirmed shiny results"
                }
                gravity = Gravity.CENTER
                setPadding(0, context().dp(32), 0, 0)
            }
            list.addView(empty, LinearLayout.LayoutParams(-1, -2))
            return
        }

        results.forEach { result -> list.addView(row(result)) }
    }

    private fun row(result: ScanResultSummary): View {
        val row = LinearLayout(context()).apply {
            gravity = Gravity.CENTER_VERTICAL
            val verticalPadding = context().dp(6)
            setPadding(0, verticalPadding, 0, verticalPadding)
        }

        val icon = PokemonIconView(context(), result)
        val iconSize = context().dp(48)
        row.addView(icon, LinearLayout.LayoutParams(iconSize, iconSize))

        val textContainer = LinearLayout(context()).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(context().dp(12), 0, 0, 0)
        }
        val name = TextView(context()).apply {
            text = result.speciesName
            textSize = 17f
        }
        val detail = TextView(context()).apply {
            val iv = result.ivPercentage?.let {
                String.format(Locale.US, "IV %.1f%%", it)
            } ?: "IV unknown"
            val shiny = if (result.shiny == true) " · shiny" else ""
            text = "$iv$shiny · ${result.encounterId}"
        }
        textContainer.addView(name, LinearLayout.LayoutParams(-1, -2))
        textContainer.addView(detail, LinearLayout.LayoutParams(-1, -2))
        row.addView(textContainer, LinearLayout.LayoutParams(0, -2, 1f))
        return row
    }

    private fun context(): Context =
        activity ?: error("fragment is not attached")

    private fun readType(value: String?): ScanMatchType =
        if (value == ScanMatchType.HUNDO.name) ScanMatchType.HUNDO else ScanMatchType.SHINY
}
