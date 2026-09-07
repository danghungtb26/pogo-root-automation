package dev.pogoroot.automation.overlay

import android.app.AlertDialog
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.view.ContextThemeWrapper
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import dev.pogoroot.automation.MainActivity
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.headless.AutomationConfigRepository
import dev.pogoroot.automation.location.JoystickLocationController
import dev.pogoroot.automation.location.JoystickLocationState
import dev.pogoroot.automation.location.RootMockLocationProvider
import io.github.controlwear.virtual.joystick.android.JoystickView
import java.util.Locale

class JoystickOverlayService : Service() {
    companion object {
        const val ACTION_START = "dev.pogoroot.automation.action.START_JOYSTICK"
        const val ACTION_STOP = "dev.pogoroot.automation.action.STOP_JOYSTICK"

        private const val CHANNEL_ID = "pogo_joystick"
        private const val NOTIFICATION_ID = 4107
        private const val PREFS = "built_in_joystick"
        private const val PREF_LAT = "latitude"
        private const val PREF_LON = "longitude"
    }

    private val mainHandler = Handler(Looper.getMainLooper())
    private val speedPresets = doubleArrayOf(3.0, 6.0, 9.0, 15.0, 30.0)

    private lateinit var windowManager: WindowManager
    private lateinit var controller: JoystickLocationController
    private lateinit var automationConfigRepository: AutomationConfigRepository
    private lateinit var rootView: LinearLayout
    private lateinit var windowParams: WindowManager.LayoutParams
    private lateinit var statusView: TextView
    private lateinit var locationView: TextView
    private lateinit var automationView: TextView
    private lateinit var speedButton: Button

    private var controllerStarted = false
    private var speedPresetIndex = 2
    private var lastPersistAt = 0L

    override fun onCreate() {
        super.onCreate()
        windowManager = getSystemService(WindowManager::class.java)
        automationConfigRepository = AutomationConfigRepository(this)
        createNotificationChannel()
        startForeground(NOTIFICATION_ID, buildNotification())

        controller = JoystickLocationController(
            sink = RootMockLocationProvider(this),
            onStateChanged = ::onLocationStateChanged,
        )
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopSelf()
            return START_NOT_STICKY
        }

        ensureOverlay()
        renderAutomationSummary()
        if (!controllerStarted) {
            controllerStarted = true
            controller.start(loadSavedPoint())
        }
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        if (controllerStarted) {
            persistPoint(controller.snapshot().point)
            controller.stop()
            controllerStarted = false
        }
        if (::rootView.isInitialized) {
            runCatching { windowManager.removeView(rootView) }
        }
        super.onDestroy()
    }

    private fun ensureOverlay() {
        if (::rootView.isInitialized) return

        val density = resources.displayMetrics.density
        val padding = (10 * density).toInt()
        val panelWidth = (220 * density).toInt()

        statusView = TextView(this).apply {
            setTextColor(Color.WHITE)
            textSize = 12f
            text = "Starting mock location…"
        }
        locationView = TextView(this).apply {
            setTextColor(Color.WHITE)
            textSize = 12f
            typeface = android.graphics.Typeface.MONOSPACE
            text = "Teleport to a location first"
        }
        automationView = TextView(this).apply {
            setTextColor(0xFFB3E5FC.toInt())
            textSize = 11f
            typeface = android.graphics.Typeface.MONOSPACE
            setPadding(0, padding / 3, 0, padding / 3)
        }

        val header = TextView(this).apply {
            setTextColor(Color.WHITE)
            textSize = 15f
            text = "PoGo Joystick  ↕"
            gravity = Gravity.CENTER
            setPadding(0, padding / 2, 0, padding / 2)
        }

        val joystick = JoystickView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                (190 * density).toInt(),
                (190 * density).toInt(),
            ).apply { gravity = Gravity.CENTER_HORIZONTAL }
            setOnMoveListener(object : JoystickView.OnMoveListener {
                override fun onMove(angle: Int, strength: Int) {
                    controller.setJoystick(angle, strength)
                }
            })
        }

        speedButton = Button(this).apply {
            text = "Speed ${speedPresets[speedPresetIndex].formatSpeed()} km/h"
            setOnClickListener {
                speedPresetIndex = (speedPresetIndex + 1) % speedPresets.size
                val speed = speedPresets[speedPresetIndex]
                controller.setMaxSpeedKmh(speed)
                text = "Speed ${speed.formatSpeed()} km/h"
            }
        }

        val settingsButton = Button(this).apply {
            text = "Automation ⚙"
            setOnClickListener {
                AutomationSettingsOverlay(
                    context = this@JoystickOverlayService,
                    repository = automationConfigRepository,
                    onSaved = { renderAutomationSummary() },
                ).show()
            }
        }

        val teleportButton = Button(this).apply {
            text = "Teleport"
            setOnClickListener { showTeleportDialog() }
        }
        val stopButton = Button(this).apply {
            text = "Close"
            setOnClickListener { stopSelf() }
        }

        val actions = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
            addView(teleportButton, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            addView(stopButton, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        }

        rootView = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(padding, padding, padding, padding)
            background = GradientDrawable().apply {
                setColor(0xDD202124.toInt())
                cornerRadius = 14 * density
            }
            addView(header)
            addView(statusView)
            addView(locationView)
            addView(automationView)
            addView(joystick)
            addView(speedButton)
            addView(settingsButton)
            addView(actions)
        }

        windowParams = WindowManager.LayoutParams(
            panelWidth,
            WindowManager.LayoutParams.WRAP_CONTENT,
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
            } else {
                @Suppress("DEPRECATION")
                WindowManager.LayoutParams.TYPE_PHONE
            },
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
            android.graphics.PixelFormat.TRANSLUCENT,
        ).apply {
            gravity = Gravity.START or Gravity.BOTTOM
            x = padding
            y = padding * 3
        }

        makeDraggable(header)
        windowManager.addView(rootView, windowParams)
        renderAutomationSummary()
    }

    private fun renderAutomationSummary() {
        if (!::automationView.isInitialized) return
        val config = automationConfigRepository.read()
        automationView.text = buildString {
            append(if (config.enabled) "AUTO ON" else "AUTO OFF")
            append(" · Catch ${config.autoCatch.onOff()}")
            append(" · Spin ${config.autoSpin.onOff()}")
            append("\nBerry ${config.autoBerry.onOff()}")
            append(" · Discard ${config.autoDiscard.onOff()}")
            append(" · Transfer ${config.autoTransfer.onOff()}")
        }
    }

    private fun makeDraggable(handle: View) {
        var initialX = 0
        var initialY = 0
        var initialTouchX = 0f
        var initialTouchY = 0f

        handle.setOnTouchListener { _, event ->
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    initialX = windowParams.x
                    initialY = windowParams.y
                    initialTouchX = event.rawX
                    initialTouchY = event.rawY
                    true
                }
                MotionEvent.ACTION_MOVE -> {
                    windowParams.x = initialX + (event.rawX - initialTouchX).toInt()
                    windowParams.y = initialY - (event.rawY - initialTouchY).toInt()
                    windowManager.updateViewLayout(rootView, windowParams)
                    true
                }
                else -> false
            }
        }
    }

    private fun showTeleportDialog() {
        val themedContext = ContextThemeWrapper(this, android.R.style.Theme_Material_Light_Dialog_Alert)
        val input = EditText(themedContext).apply {
            hint = "21.0285, 105.8542"
            setSingleLine(true)
            controller.snapshot().point?.let { point ->
                setText(String.format(Locale.US, "%.6f, %.6f", point.latitude, point.longitude))
                setSelection(text.length)
            }
        }

        val dialog = AlertDialog.Builder(themedContext)
            .setTitle("Change location")
            .setMessage("Enter latitude, longitude")
            .setView(input)
            .setPositiveButton("Teleport", null)
            .setNegativeButton("Cancel", null)
            .create()

        dialog.window?.setType(
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
            } else {
                @Suppress("DEPRECATION")
                WindowManager.LayoutParams.TYPE_PHONE
            },
        )
        dialog.setOnShowListener {
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
                val point = parsePoint(input.text.toString())
                if (point == null) {
                    input.error = "Use: latitude, longitude"
                } else {
                    controller.teleport(point)
                    dialog.dismiss()
                }
            }
        }
        dialog.show()
    }

    private fun parsePoint(raw: String): GeoPoint? {
        val parts = raw.trim().split(',', ' ', ';').filter(String::isNotBlank)
        if (parts.size != 2) return null
        val latitude = parts[0].toDoubleOrNull() ?: return null
        val longitude = parts[1].toDoubleOrNull() ?: return null
        if (latitude !in -90.0..90.0 || longitude !in -180.0..180.0) return null
        return GeoPoint(latitude, longitude)
    }

    private fun onLocationStateChanged(state: JoystickLocationState) {
        mainHandler.post {
            if (!::statusView.isInitialized) return@post

            statusView.text = when {
                state.error != null -> "Mock location error: ${state.error}"
                state.providerReady -> "Mock location: ready (root)"
                else -> "Mock location: preparing…"
            }
            locationView.text = state.point?.let { point ->
                String.format(
                    Locale.US,
                    "%.6f, %.6f\n%.1f km/h · %.0f°",
                    point.latitude,
                    point.longitude,
                    state.currentSpeedKmh,
                    state.bearingDegrees,
                )
            } ?: "Teleport to a location first"

            persistPointOccasionally(state.point)
        }
    }

    private fun loadSavedPoint(): GeoPoint? {
        val prefs = getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        if (!prefs.contains(PREF_LAT) || !prefs.contains(PREF_LON)) return null
        val latitude = Double.fromBits(prefs.getLong(PREF_LAT, 0L))
        val longitude = Double.fromBits(prefs.getLong(PREF_LON, 0L))
        return GeoPoint(latitude, longitude)
    }

    private fun persistPointOccasionally(point: GeoPoint?) {
        val now = android.os.SystemClock.elapsedRealtime()
        if (now - lastPersistAt < 1_000L) return
        lastPersistAt = now
        persistPoint(point)
    }

    private fun persistPoint(point: GeoPoint?) {
        if (point == null) return
        getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit()
            .putLong(PREF_LAT, point.latitude.toBits())
            .putLong(PREF_LON, point.longitude.toBits())
            .apply()
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(
                CHANNEL_ID,
                "Built-in joystick",
                NotificationManager.IMPORTANCE_LOW,
            ),
        )
    }

    private fun buildNotification(): Notification {
        val launchIntent = Intent(this, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(
            this,
            0,
            launchIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )

        val builder = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(this, CHANNEL_ID)
        } else {
            @Suppress("DEPRECATION")
            Notification.Builder(this)
        }
        return builder
            .setSmallIcon(android.R.drawable.ic_menu_mylocation)
            .setContentTitle("PoGo built-in joystick")
            .setContentText("Location + automation overlay is active")
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build()
    }

    private fun Boolean.onOff(): String = if (this) "ON" else "OFF"

    private fun Double.formatSpeed(): String = if (this % 1.0 == 0.0) {
        toInt().toString()
    } else {
        String.format(Locale.US, "%.1f", this)
    }
}
