package dev.pogoroot.automation.overlay

import android.app.Activity
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import dev.pogoroot.automation.core.scan.ScanMatchType

/** Full-screen host for the two scan result sections opened from the overlay. */
class ScanResultsActivity : Activity() {
    companion object {
        const val EXTRA_SECTION = "scan_result_section"
    }

    private lateinit var content: FrameLayout
    private var selectedType: ScanMatchType = ScanMatchType.SHINY

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        selectedType = readType(intent.getStringExtra(EXTRA_SECTION))
        setContentView(buildContent())
        showResults(selectedType)
    }

    private fun buildContent(): View {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            val padding = dp(16)
            setPadding(padding, padding, padding, 0)
        }

        val toolbar = LinearLayout(this).apply {
            gravity = Gravity.CENTER_VERTICAL
        }
        val title = TextView(this).apply {
            text = "Scan results"
            textSize = 22f
        }
        toolbar.addView(title, LinearLayout.LayoutParams(0, -2, 1f))

        val close = Button(this).apply {
            text = "Close"
            setOnClickListener { finish() }
        }
        toolbar.addView(close, LinearLayout.LayoutParams(-2, -2))
        root.addView(toolbar)

        val tabs = LinearLayout(this)
        val hundo = Button(this).apply {
            text = "100 IV"
            setOnClickListener { showResults(ScanMatchType.HUNDO) }
        }
        val shiny = Button(this).apply {
            text = "Shiny"
            setOnClickListener { showResults(ScanMatchType.SHINY) }
        }
        tabs.addView(hundo, LinearLayout.LayoutParams(0, -2, 1f))
        tabs.addView(shiny, LinearLayout.LayoutParams(0, -2, 1f))
        root.addView(tabs)

        content = FrameLayout(this).apply {
            id = View.generateViewId()
        }
        root.addView(content, LinearLayout.LayoutParams(-1, 0, 1f))
        return root
    }

    private fun showResults(type: ScanMatchType) {
        selectedType = type
        fragmentManager.beginTransaction()
            .replace(content.id, ScanResultsFragment.newInstance(type))
            .commit()
    }

    private fun readType(value: String?): ScanMatchType =
        if (value == ScanMatchType.HUNDO.name) ScanMatchType.HUNDO else ScanMatchType.SHINY
}
