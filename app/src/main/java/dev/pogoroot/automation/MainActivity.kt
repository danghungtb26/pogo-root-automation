package dev.pogoroot.automation

import android.app.Activity
import android.content.Intent
import android.graphics.Typeface
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import dev.pogoroot.automation.headless.AutomationConfigRepository
import dev.pogoroot.automation.headless.AutomationControlServer
import dev.pogoroot.automation.headless.HeadlessAutomationService
import dev.pogoroot.automation.overlay.GameForegroundDetector
import dev.pogoroot.automation.overlay.JoystickOverlayService

class MainActivity : Activity() {
    private val handler = Handler(Looper.getMainLooper())
    private lateinit var configRepository: AutomationConfigRepository
    private lateinit var statusView: TextView
    private var startJoystickAfterOverlayGrant = false
    private var startJoystickAfterUsageGrant = false

    private val statusTick = object : Runnable {
        override fun run() {
            renderStatus()
            handler.postDelayed(this, 1_000L)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        configRepository = AutomationConfigRepository(this)
        HeadlessAutomationService.start(this)
        setContentView(buildContent())
        handler.post(statusTick)
    }

    override fun onResume() {
        super.onResume()
        if (startJoystickAfterOverlayGrant && Settings.canDrawOverlays(this)) {
            startJoystickAfterOverlayGrant = false
            requestOverlayAndStartJoystick()
        } else if (startJoystickAfterUsageGrant &&
            GameForegroundDetector.hasUsageAccess(this)
        ) {
            startJoystickAfterUsageGrant = false
            startBuiltInJoystick()
        }
    }

    override fun onDestroy() {
        handler.removeCallbacks(statusTick)
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

            addView(TextView(context).apply {
                text = "Headless mode uses the structured runtime bridge. Until observation and action capabilities are verified, it stays read-only and fail-closed."
                textSize = 15f
                setPadding(0, padding / 2, 0, padding / 2)
            })

            statusView = TextView(context).apply {
                textSize = 16f
                typeface = Typeface.MONOSPACE
                setPadding(0, 0, 0, padding / 2)
            }
            addView(statusView)

            addView(Button(context).apply {
                text = "Enable auto catch + auto spin"
                setOnClickListener {
                    HeadlessAutomationService.enable(
                        context,
                        autoCatch = true,
                        autoSpin = true,
                        autoEncounter = false,
                    )
                    Toast.makeText(context, "Headless automation enabled", Toast.LENGTH_SHORT).show()
                    renderStatus()
                }
            })

            addView(Button(context).apply {
                text = "Disable automation"
                setOnClickListener {
                    HeadlessAutomationService.disable(context)
                    Toast.makeText(context, "Automation disabled; API service stays available", Toast.LENGTH_SHORT).show()
                    renderStatus()
                }
            })

            addView(TextView(context).apply {
                text = "Local control API"
                textSize = 20f
                setTypeface(typeface, Typeface.BOLD)
                setPadding(0, padding / 2, 0, padding / 4)
            })

            addView(TextView(context).apply {
                text = "127.0.0.1:${AutomationControlServer.DEFAULT_PORT}\nGET /v1/status\nPOST /v1/start?autoEncounter=false&catch=true&spin=true\nPOST /v1/config?mapTapWalk=true\nPOST /v1/stop\nPOST /v1/config?..."
                textSize = 14f
                typeface = Typeface.MONOSPACE
                setPadding(0, 0, 0, padding / 2)
            })

            addView(TextView(context).apply {
                text = "Location"
                textSize = 20f
                setTypeface(typeface, Typeface.BOLD)
            })

            addView(Button(context).apply {
                text = "Start built-in joystick"
                setOnClickListener { requestOverlayAndStartJoystick() }
            })

            addView(Button(context).apply {
                text = "Stop joystick"
                setOnClickListener {
                    stopService(Intent(context, JoystickOverlayService::class.java))
                }
            })
        }
    }

    private fun renderStatus() {
        if (!::statusView.isInitialized || !::configRepository.isInitialized) return
        val config = configRepository.read()
        statusView.text = buildString {
            append("automation: ${if (config.enabled) "ON" else "OFF"}")
            append("\nautoCatch: ${config.autoCatch}")
            append("\nautoCloseCatchPreview: ${config.autoCloseCatchPreview}")
            append("\nautoSpin: ${config.autoSpin}")
            append("\nspinSettleDelayMs: ${config.spinSettleDelayMs}")
            append("\ncatchSettleDelayMs: ${config.catchSettleDelayMs}")
            append("\nautoEncounter: ${config.autoEncounter}")
            append("\nAPI: localhost:${AutomationControlServer.DEFAULT_PORT}")
        }
    }

    private fun requestOverlayAndStartJoystick() {
        if (!Settings.canDrawOverlays(this)) {
            startJoystickAfterOverlayGrant = true
            Toast.makeText(
                this,
                "Allow display over other apps, then return to PoGo Root Automation.",
                Toast.LENGTH_LONG,
            ).show()
            startActivity(
                Intent(
                    Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:$packageName"),
                ),
            )
            return
        }

        if (!GameForegroundDetector.hasUsageAccess(this)) {
            startJoystickAfterUsageGrant = true
            Toast.makeText(
                this,
                "Allow usage access so the overlay only appears over Pokémon GO.",
                Toast.LENGTH_LONG,
            ).show()
            startActivity(Intent(Settings.ACTION_USAGE_ACCESS_SETTINGS))
            return
        }

        startBuiltInJoystick()
    }

    private fun startBuiltInJoystick() {
        startForegroundService(
            Intent(this, JoystickOverlayService::class.java)
                .setAction(JoystickOverlayService.ACTION_START),
        )
        Toast.makeText(this, "Built-in joystick started", Toast.LENGTH_SHORT).show()
    }
}
