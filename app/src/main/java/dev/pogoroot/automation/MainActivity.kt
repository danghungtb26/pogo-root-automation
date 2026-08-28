package dev.pogoroot.automation

import android.app.Activity
import android.graphics.Typeface
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.widget.LinearLayout
import android.widget.TextView
import dev.pogoroot.automation.bridge.RuntimeConnectionState
import dev.pogoroot.automation.bridge.RuntimeSnapshot
import dev.pogoroot.automation.core.time.CountdownService
import dev.pogoroot.automation.fake.FakeGameAdapter
import dev.pogoroot.automation.root.RuntimeStatusRepository
import java.util.concurrent.Executors

class MainActivity : Activity() {
    private val handler = Handler(Looper.getMainLooper())
    private val runtimeExecutor = Executors.newSingleThreadExecutor()
    private val adapter = FakeGameAdapter()
    private val countdownService = CountdownService()
    private val runtimeStatusRepository = RuntimeStatusRepository()

    private lateinit var runtimeView: TextView
    private lateinit var adapterView: TextView
    private lateinit var nearbyView: TextView

    private val renderTick = object : Runnable {
        override fun run() {
            renderNearby()
            handler.postDelayed(this, 1_000L)
        }
    }

    private val runtimeTick = object : Runnable {
        override fun run() {
            runtimeExecutor.execute {
                val snapshot = runtimeStatusRepository.read()
                handler.post {
                    if (!isFinishing && !isDestroyed) {
                        renderRuntime(snapshot)
                    }
                }
            }
            handler.postDelayed(this, 2_000L)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(buildContent())

        val connected = adapter.connect().isSuccess
        adapterView.text = if (connected) {
            "● GameAdapter: fake/read-only"
        } else {
            "● GameAdapter connection failed"
        }

        handler.post(renderTick)
        handler.post(runtimeTick)
    }

    override fun onDestroy() {
        handler.removeCallbacks(renderTick)
        handler.removeCallbacks(runtimeTick)
        runtimeExecutor.shutdownNow()
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

            runtimeView = TextView(context).apply {
                text = "● Root runtime: checking…"
                textSize = 16f
                setPadding(0, padding / 2, 0, padding / 4)
            }
            addView(runtimeView)

            adapterView = TextView(context).apply {
                textSize = 16f
                setPadding(0, 0, 0, padding)
            }
            addView(adapterView)

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

    private fun renderRuntime(snapshot: RuntimeSnapshot) {
        runtimeView.text = when (snapshot.state) {
            RuntimeConnectionState.CONNECTED -> buildString {
                append("● Root runtime: connected")
                snapshot.processName?.let { append("\n  process: $it") }
                snapshot.pid?.let { append(" ($it)") }
                snapshot.gameVersionName?.let { version ->
                    append("\n  game: $version")
                    snapshot.gameVersionCode?.let { append(" ($it)") }
                }
            }
            RuntimeConnectionState.DISCONNECTED -> "○ Root runtime: game process stopped"
            RuntimeConnectionState.NOT_SEEN -> "○ Root runtime: waiting for Pokémon GO"
            RuntimeConnectionState.ERROR -> "! Root runtime: ${snapshot.error ?: "unavailable"}"
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
                countdown.isExpired -> "expired "
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
