package dev.pogoroot.automation

import android.app.Activity
import android.graphics.Typeface
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.widget.LinearLayout
import android.widget.TextView
import dev.pogoroot.automation.core.time.CountdownService
import dev.pogoroot.automation.fake.FakeGameAdapter

class MainActivity : Activity() {
    private val handler = Handler(Looper.getMainLooper())
    private val adapter = FakeGameAdapter()
    private val countdownService = CountdownService()

    private lateinit var statusView: TextView
    private lateinit var nearbyView: TextView

    private val renderTick = object : Runnable {
        override fun run() {
            renderNearby()
            handler.postDelayed(this, 1_000L)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(buildContent())

        val connected = adapter.connect().isSuccess
        statusView.text = if (connected) {
            "● Adapter connected (fake/read-only)"
        } else {
            "● Adapter connection failed"
        }

        handler.post(renderTick)
    }

    override fun onDestroy() {
        handler.removeCallbacks(renderTick)
        adapter.disconnect()
        super.onDestroy()
    }

    private fun buildContent(): LinearLayout {
        val padding = (24 * resources.displayMetrics.density).toInt()

        return LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.START
            setPadding(padding, padding, padding, padding)

            addView(TextView(context).apply {
                text = "PoGo Root Automation"
                textSize = 24f
                setTypeface(typeface, Typeface.BOLD)
            })

            statusView = TextView(context).apply {
                textSize = 16f
                setPadding(0, padding / 2, 0, padding)
            }
            addView(statusView)

            addView(TextView(context).apply {
                text = "Nearby"
                textSize = 20f
                setTypeface(typeface, Typeface.BOLD)
            })

            nearbyView = TextView(context).apply {
                textSize = 18f
                setPadding(0, padding / 2, 0, 0)
                typeface = Typeface.MONOSPACE
            }
            addView(nearbyView)
        }
    }

    private fun renderNearby() {
        val snapshot = adapter.readNearby().getOrElse {
            nearbyView.text = "Unavailable: ${it.message ?: "unknown error"}"
            return
        }

        nearbyView.text = snapshot.spawns.joinToString(separator = "\n") { spawn ->
            val countdown = countdownService.forSpawn(spawn)
            val remaining = countdown.remainingMillis?.let(::formatDuration) ?: "--:--"
            val marker = when {
                countdown.isExpired -> "expired"
                countdown.isEstimated -> "~"
                else -> " "
            }
            "${spawn.speciesName.padEnd(12)} $marker$remaining"
        }
    }

    private fun formatDuration(remainingMillis: Long): String {
        val totalSeconds = remainingMillis / 1_000L
        val minutes = totalSeconds / 60L
        val seconds = totalSeconds % 60L
        return "%02d:%02d".format(minutes, seconds)
    }
}
