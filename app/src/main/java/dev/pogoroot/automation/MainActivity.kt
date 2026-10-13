package dev.pogoroot.automation

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.graphics.Typeface
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import dev.pogoroot.automation.config.AutomationConfigRepository
import dev.pogoroot.automation.engine.CatchSpinArmState
import dev.pogoroot.automation.overlay.CustomToast
import dev.pogoroot.automation.overlay.GameForegroundDetector
import dev.pogoroot.automation.overlay.JoystickOverlayService
import dev.pogoroot.automation.root.ModuleInstallSnapshot
import dev.pogoroot.automation.root.ModuleInstallState
import dev.pogoroot.automation.service.HeadlessAutomationService

class MainActivity : Activity() {
    private val handler = Handler(Looper.getMainLooper())
    private lateinit var configRepository: AutomationConfigRepository
    private lateinit var statusView: TextView
    private lateinit var retryModuleButton: Button
    private lateinit var rebootButton: Button
    private lateinit var automationButton: Button
    private var startJoystickAfterOverlayGrant = false
    private var startJoystickAfterUsageGrant = false
    private var foreground = false
    private var installSnapshot: ModuleInstallSnapshot? = null

    private val app: AutomationApplication
        get() = application as AutomationApplication

    private val installObserver: (ModuleInstallSnapshot) -> Unit = { snapshot ->
        installSnapshot = snapshot
        if (foreground && snapshot.gameplayAllowed) HeadlessAutomationService.start(this)
        renderStatus()
    }

    private val statusTick = object : Runnable {
        override fun run() {
            renderStatus()
            handler.postDelayed(this, 1_000L)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        configRepository = AutomationConfigRepository(this)
        setContentView(buildContent())
        app.moduleInstallCoordinator.observe(installObserver)
        handler.post(statusTick)
    }

    override fun onResume() {
        super.onResume()
        foreground = true
        app.moduleInstallCoordinator.onForegroundEntry()
        if (startJoystickAfterOverlayGrant && Settings.canDrawOverlays(this)) {
            startJoystickAfterOverlayGrant = false
            requestOverlayAndStartJoystick()
        } else if (startJoystickAfterUsageGrant && GameForegroundDetector.hasUsageAccess(this)) {
            startJoystickAfterUsageGrant = false
            startBuiltInJoystick()
        }
    }

    override fun onPause() {
        foreground = false
        app.moduleInstallCoordinator.onForegroundExit()
        super.onPause()
    }

    override fun onDestroy() {
        handler.removeCallbacks(statusTick)
        app.moduleInstallCoordinator.removeObserver(installObserver)
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
                text = "The bundled Zygisk module is verified locally. Its install state is shown below; gameplay stays off until the module is active and you arm the current runtime session."
                textSize = 15f
                setPadding(0, padding / 2, 0, padding / 2)
            })

            statusView = TextView(context).apply {
                textSize = 16f
                typeface = Typeface.MONOSPACE
                setPadding(0, 0, 0, padding / 2)
            }
            addView(statusView)

            retryModuleButton = Button(context).apply {
                text = "Retry module check / install"
                setOnClickListener {
                    runCatching { app.moduleInstallCoordinator.retryFromUser() }
                        .onFailure { CustomToast.show(context, it.message ?: "Retry failed") }
                }
            }
            addView(retryModuleButton)

            rebootButton = Button(context).apply {
                text = "Reboot to activate module"
                setOnClickListener { confirmReboot() }
            }
            addView(rebootButton)

            automationButton = Button(context).apply {
                setOnClickListener {
                    if (CatchSpinArmState.isArmed()) {
                        HeadlessAutomationService.disable(context)
                        CustomToast.show(context, "Automation disarmed for this session")
                    } else {
                        HeadlessAutomationService.enable(
                            context,
                            autoCatch = true,
                            autoSpin = true,
                            autoEncounter = false,
                        )
                        CustomToast.show(context, "Arm requested for the current runtime session")
                    }
                    renderStatus()
                }
            }
            addView(automationButton)

            addView(TextView(context).apply {
                text = "Location"
                textSize = 20f
                setTypeface(typeface, Typeface.BOLD)
                setPadding(0, padding / 2, 0, 0)
            })
            addView(Button(context).apply {
                text = "Start built-in joystick"
                setOnClickListener { requestOverlayAndStartJoystick() }
            })
            addView(Button(context).apply {
                text = "Stop joystick"
                setOnClickListener { stopService(Intent(context, JoystickOverlayService::class.java)) }
            })
        }
    }

    private fun renderStatus() {
        if (!::statusView.isInitialized || !::configRepository.isInitialized) return
        val snapshot = installSnapshot ?: app.moduleInstallStateRepository.read()
        val config = configRepository.read()
        val arm = CatchSpinArmState.current()
        val info = app.moduleInstallCoordinator.bundledInfo().getOrNull()
        statusView.text = buildString {
            append("module: ${snapshot.state}")
            append("\nmoduleVersion: ${snapshot.moduleVersion ?: info?.version ?: "unknown"}")
            append("\nmoduleSha256: ${(snapshot.moduleSha256 ?: info?.zipSha256 ?: "unknown").take(16)}…")
            append("\nmoduleStatus: ${snapshot.errorCode ?: snapshot.message ?: "ready for check"}")
            append("\nruntimeSession: ${snapshot.runtimeSessionId ?: "none"}")
            append("\nautomationArmed: ${arm.armed}")
            append("\narmReason: ${arm.reason}")
            append("\nautoCatchPolicy: ${config.autoCatch}")
            append("\nautoSpinPolicy: ${config.autoSpin}")
        }
        retryModuleButton.visibility = if (snapshot.state == ModuleInstallState.ACTIVE) {
            View.GONE
        } else {
            View.VISIBLE
        }
        rebootButton.visibility = if (snapshot.state == ModuleInstallState.REBOOT_REQUIRED) {
            View.VISIBLE
        } else {
            View.GONE
        }
        automationButton.isEnabled = snapshot.gameplayAllowed
        automationButton.text = if (arm.armed) "Disarm automation" else "Arm auto catch + auto spin"
    }

    private fun confirmReboot() {
        AlertDialog.Builder(this)
            .setTitle("Reboot device?")
            .setMessage("Reboot is required for Zygisk to load the installed module. No reboot happens without this confirmation.")
            .setNegativeButton("Cancel", null)
            .setPositiveButton("Reboot") { _, _ ->
                app.moduleInstallCoordinator.rebootFromUser { result ->
                    if (!result.isSuccess) {
                        CustomToast.show(this, result.stderr.ifBlank { "Reboot command failed" })
                    }
                }
            }
            .show()
    }

    private fun requestOverlayAndStartJoystick() {
        if (!Settings.canDrawOverlays(this)) {
            startJoystickAfterOverlayGrant = true
            CustomToast.show(
                this,
                "Allow display over other apps, then return to PoGo Root Automation.",
                CustomToast.LONG_DURATION_MS,
            )
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
            CustomToast.show(
                this,
                "Allow usage access so the overlay only appears over Pokémon GO.",
                CustomToast.LONG_DURATION_MS,
            )
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
        CustomToast.show(this, "Built-in joystick started")
    }
}
