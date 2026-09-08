package dev.pogoroot.automation.overlay

import android.app.AlertDialog
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.res.Configuration
import android.graphics.Color
import android.graphics.Typeface
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.view.ContextThemeWrapper
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.TextView
import dev.pogoroot.automation.MainActivity
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.scan.ScanMatchType
import dev.pogoroot.automation.core.time.TeleportCooldown
import dev.pogoroot.automation.core.time.TeleportCooldownMode
import dev.pogoroot.automation.core.time.TeleportCooldownService
import dev.pogoroot.automation.headless.AutomationConfigRepository
import dev.pogoroot.automation.headless.LastActiveGameAction
import dev.pogoroot.automation.headless.LastActiveLocationRepository
import dev.pogoroot.automation.headless.MapTargetRepository
import dev.pogoroot.automation.location.JoystickLocationController
import dev.pogoroot.automation.location.JoystickLocationState
import dev.pogoroot.automation.location.RootMockLocationProvider
import dev.pogoroot.automation.scan.ScanResultRepository
import java.util.Locale
import kotlin.math.max

class JoystickOverlayService : Service() {
    companion object {
        const val ACTION_START = "dev.pogoroot.automation.action.START_JOYSTICK"
        const val ACTION_STOP = "dev.pogoroot.automation.action.STOP_JOYSTICK"

        private const val CHANNEL_ID = "pogo_joystick"
        private const val NOTIFICATION_ID = 4107
        private const val COOLDOWN_REFRESH_MS = 1_000L
        private const val DEFAULT_EDGE_MARGIN_DP = 16
        private const val DEFAULT_BOTTOM_MARGIN_DP = 24
        private const val SCAN_WIDGET_WIDTH_DP = 50
        private const val SCAN_WIDGET_INITIAL_HEIGHT_DP = 72
        private const val SCAN_WIDGET_GAP_DP = 8
    }

    private enum class MainOverlayMode {
        COLLAPSED,
        SHORTCUTS,
        JOYSTICK,
    }

    private val mainHandler = Handler(Looper.getMainLooper())
    private val speedPresets = doubleArrayOf(3.0, 6.0, 9.0, 15.0, 30.0)

    private lateinit var windowManager: WindowManager
    private lateinit var controller: JoystickLocationController
    private lateinit var automationConfigRepository: AutomationConfigRepository
    private lateinit var lastActiveLocationRepository: LastActiveLocationRepository
    private lateinit var mapTargetRepository: MapTargetRepository
    private lateinit var scanResultRepository: ScanResultRepository
    private lateinit var positionStore: OverlayPositionStore
    private lateinit var shortcutMenu: ShortcutMenuView
    private lateinit var joystickPad: JoystickPadView
    private lateinit var rootView: FrameLayout
    private lateinit var windowParams: WindowManager.LayoutParams
    private lateinit var floatButton: TextView
    private lateinit var cooldownView: TextView
    private lateinit var cooldownWindowParams: WindowManager.LayoutParams
    private lateinit var hundoResultsView: ScanResultOverlayView
    private lateinit var shinyResultsView: ScanResultOverlayView
    private lateinit var hundoResultsWindowParams: WindowManager.LayoutParams
    private lateinit var shinyResultsWindowParams: WindowManager.LayoutParams

    private val cooldownService = TeleportCooldownService()
    private var controllerStarted = false
    private var speedPresetIndex = 2
    private var lastPersistAt = 0L
    private var latestTeleportCooldown: TeleportCooldown? = null
    private var cooldownMode = TeleportCooldownMode.CURRENT_POSITION
    private var mainMode = MainOverlayMode.COLLAPSED
    private var mainAnchorX = 0
    private var mainAnchorY = 0
    private var iconSizePx = 0
    private var cooldownWidthPx = 0
    private var cooldownHeightPx = 0
    private var scanWidgetWidthPx = 0
    private var edgeMarginPx = 0
    private var bottomMarginPx = 0

    private val cooldownTick = object : Runnable {
        override fun run() {
            applyPendingMapTarget()
            renderShortcutStates()
            renderCooldown()
            renderScanResults()
            mainHandler.postDelayed(this, COOLDOWN_REFRESH_MS)
        }
    }

    override fun onCreate() {
        super.onCreate()
        windowManager = getSystemService(WindowManager::class.java)
        automationConfigRepository = AutomationConfigRepository(this)
        lastActiveLocationRepository = LastActiveLocationRepository(this)
        mapTargetRepository = MapTargetRepository(this)
        scanResultRepository = ScanResultRepository()
        positionStore = OverlayPositionStore(this)
        latestTeleportCooldown = positionStore.loadCooldown()
        cooldownMode = positionStore.loadCooldownMode()
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
        renderShortcutStates()
        renderCooldown()
        renderScanResults()
        mainHandler.removeCallbacks(cooldownTick)
        mainHandler.post(cooldownTick)
        if (!controllerStarted) {
            controllerStarted = true
            controller.start(loadSavedPoint())
        }
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        mainHandler.post {
            if (::rootView.isInitialized) {
                clampMainAnchor()
                persistMainPosition()
                relayoutMainOverlay()
            }
            if (::cooldownView.isInitialized) {
                clampCooldownPosition()
                persistCooldownPosition()
                runCatching { windowManager.updateViewLayout(cooldownView, cooldownWindowParams) }
            }
            if (::hundoResultsView.isInitialized && ::shinyResultsView.isInitialized) {
                clampScanWidget(hundoResultsWindowParams, hundoResultsView)
                clampScanWidget(shinyResultsWindowParams, shinyResultsView)
                persistHundoResultsPosition()
                persistShinyResultsPosition()
                runCatching {
                    windowManager.updateViewLayout(hundoResultsView, hundoResultsWindowParams)
                    windowManager.updateViewLayout(shinyResultsView, shinyResultsWindowParams)
                }
            }
        }
    }

    override fun onDestroy() {
        mainHandler.removeCallbacks(cooldownTick)
        if (controllerStarted) {
            persistPoint(controller.snapshot().point)
            controller.stop()
            controllerStarted = false
        }
        if (::cooldownView.isInitialized) {
            runCatching { windowManager.removeView(cooldownView) }
        }
        if (::hundoResultsView.isInitialized) {
            runCatching { windowManager.removeView(hundoResultsView) }
        }
        if (::shinyResultsView.isInitialized) {
            runCatching { windowManager.removeView(shinyResultsView) }
        }
        if (::rootView.isInitialized) {
            runCatching { windowManager.removeView(rootView) }
        }
        super.onDestroy()
    }

    private fun ensureOverlay() {
        if (::rootView.isInitialized) return

        iconSizePx = dp(56)
        edgeMarginPx = dp(DEFAULT_EDGE_MARGIN_DP)
        bottomMarginPx = dp(DEFAULT_BOTTOM_MARGIN_DP)
        cooldownWidthPx = dp(82)
        cooldownHeightPx = dp(44)
        scanWidgetWidthPx = dp(SCAN_WIDGET_WIDTH_DP)

        floatButton = TextView(this).apply {
            gravity = Gravity.CENTER
            text = "✣"
            textSize = 24f
            setTextColor(Color.WHITE)
            isClickable = true
            isFocusable = true
            contentDescription = "PoGo Tools menu"
            setOnClickListener {
                setMainMode(
                    when (mainMode) {
                        MainOverlayMode.COLLAPSED -> MainOverlayMode.SHORTCUTS
                        MainOverlayMode.SHORTCUTS -> MainOverlayMode.COLLAPSED
                        MainOverlayMode.JOYSTICK -> MainOverlayMode.SHORTCUTS
                    },
                )
            }
        }
        OverlayDragHandler(
            context = this,
            readPosition = { OverlayPosition(mainAnchorX, mainAnchorY) },
            writePosition = { position ->
                mainAnchorX = position.x
                mainAnchorY = position.y
                clampMainAnchor()
            },
            onMove = ::relayoutMainOverlay,
            onDrop = ::persistMainPosition,
        ).attachTo(floatButton)

        shortcutMenu = ShortcutMenuView(
            context = this,
            speedPresets = speedPresets,
            onToggle = ::toggleAutomation,
            onJoystick = { setMainMode(MainOverlayMode.JOYSTICK) },
            onTeleport = {
                setMainMode(MainOverlayMode.COLLAPSED)
                showTeleportDialog()
            },
            onSpeed = {
                speedPresetIndex = (speedPresetIndex + 1) % speedPresets.size
                controller.setMaxSpeedKmh(speedPresets[speedPresetIndex])
                renderShortcutStates()
            },
            onSettings = {
                setMainMode(MainOverlayMode.COLLAPSED)
                startActivity(
                    Intent(this@JoystickOverlayService, AutomationSettingsActivity::class.java)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
                )
            },
            onClose = { stopSelf() },
        )
        joystickPad = JoystickPadView(
            context = this,
            onMove = controller::setJoystick,
            onClose = { setMainMode(MainOverlayMode.SHORTCUTS) },
        )

        rootView = FrameLayout(this).apply {
            setBackgroundColor(Color.TRANSPARENT)
            setOnTouchListener { _, event ->
                if (event.actionMasked == MotionEvent.ACTION_OUTSIDE &&
                    mainMode != MainOverlayMode.COLLAPSED
                ) {
                    setMainMode(MainOverlayMode.COLLAPSED)
                    true
                } else {
                    false
                }
            }
            addView(shortcutMenu.view, FrameLayout.LayoutParams(dp(244), ViewGroup.LayoutParams.WRAP_CONTENT))
            addView(joystickPad, FrameLayout.LayoutParams(dp(214), ViewGroup.LayoutParams.WRAP_CONTENT))
            addView(floatButton, FrameLayout.LayoutParams(iconSizePx, iconSizePx))
        }

        val defaultX = edgeMarginPx
        val defaultY = (displayHeight() - iconSizePx - bottomMarginPx).coerceAtLeast(0)
        val savedMainPosition = positionStore.loadMainPosition(OverlayPosition(defaultX, defaultY))
        mainAnchorX = savedMainPosition.x
        mainAnchorY = savedMainPosition.y
        clampMainAnchor()

        windowParams = newOverlayParams(iconSizePx, iconSizePx).apply {
            gravity = Gravity.TOP or Gravity.START
            x = mainAnchorX
            y = mainAnchorY
        }
        windowManager.addView(rootView, windowParams)
        setMainMode(MainOverlayMode.COLLAPSED)
        ensureCooldownOverlay()
        ensureScanResultOverlays()
    }

    private fun setMainMode(mode: MainOverlayMode) {
        mainMode = mode
        if (!::rootView.isInitialized) return

        shortcutMenu.view.visibility = if (mode == MainOverlayMode.SHORTCUTS) View.VISIBLE else View.GONE
        joystickPad.visibility = if (mode == MainOverlayMode.JOYSTICK) View.VISIBLE else View.GONE
        floatButton.visibility = View.VISIBLE
        renderShortcutStates()
        rootView.post(::relayoutMainOverlay)
    }

    private fun relayoutMainOverlay() {
        if (!::rootView.isInitialized || !::windowParams.isInitialized) return

        if (mainMode == MainOverlayMode.COLLAPSED) {
            floatButton.layoutParams = FrameLayout.LayoutParams(iconSizePx, iconSizePx)
            windowParams.width = iconSizePx
            windowParams.height = iconSizePx
            windowParams.x = clamp(mainAnchorX, displayWidth() - iconSizePx)
            windowParams.y = clamp(mainAnchorY, displayHeight() - iconSizePx)
            runCatching { windowManager.updateViewLayout(rootView, windowParams) }
            return
        }

        val panel = if (mainMode == MainOverlayMode.SHORTCUTS) shortcutMenu.view else joystickPad
        val panelWidth = panel.layoutParams.width.takeIf { it > 0 } ?: dp(214)
        val panelMeasureSpec = View.MeasureSpec.makeMeasureSpec(panelWidth, View.MeasureSpec.EXACTLY)
        panel.measure(panelMeasureSpec, View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
        val panelHeight = panel.measuredHeight
        val rootWidth = panelWidth + dp(8) + iconSizePx
        val rootHeight = max(panelHeight, iconSizePx)
        val opensLeft = mainAnchorX > displayWidth() - iconSizePx - dp(8) - panelWidth
        val panelLeft = if (opensLeft) 0 else iconSizePx + dp(8)
        val floatLeft = if (opensLeft) panelWidth + dp(8) else 0
        val rootLeft = if (opensLeft) mainAnchorX - panelWidth - dp(8) else mainAnchorX
        val rootTop = mainAnchorY - (rootHeight - iconSizePx) / 2

        panel.layoutParams = FrameLayout.LayoutParams(panelWidth, panelHeight).apply {
            leftMargin = panelLeft
            topMargin = 0
        }
        floatButton.layoutParams = FrameLayout.LayoutParams(iconSizePx, iconSizePx).apply {
            leftMargin = floatLeft
            topMargin = (rootHeight - iconSizePx) / 2
        }
        windowParams.width = rootWidth
        windowParams.height = rootHeight
        windowParams.x = clamp(rootLeft, displayWidth() - rootWidth)
        windowParams.y = clamp(rootTop, displayHeight() - rootHeight)
        runCatching { windowManager.updateViewLayout(rootView, windowParams) }
    }

    private fun ensureCooldownOverlay() {
        if (::cooldownView.isInitialized) return

        cooldownView = TextView(this).apply {
            gravity = Gravity.CENTER
            textSize = 16f
            typeface = Typeface.MONOSPACE
            setTextColor(0xFFFFE082.toInt())
            background = roundedBackground(0xE6202124.toInt(), 12)
            setPadding(dp(6), 0, dp(6), 0)
            contentDescription = "Cooldown"
            visibility = View.INVISIBLE
        }
        val defaultX = (displayWidth() - cooldownWidthPx - edgeMarginPx).coerceAtLeast(0)
        val defaultY = edgeMarginPx
        val savedPosition = positionStore.loadCooldownPosition(
            OverlayPosition(
                x = defaultX,
                y = defaultY,
            ),
        )
        cooldownWindowParams = newOverlayParams(cooldownWidthPx, cooldownHeightPx).apply {
            gravity = Gravity.TOP or Gravity.START
            x = savedPosition.x
            y = savedPosition.y
        }
        clampCooldownPosition()
        windowManager.addView(cooldownView, cooldownWindowParams)
        OverlayDragHandler(
            context = this,
            readPosition = { OverlayPosition(cooldownWindowParams.x, cooldownWindowParams.y) },
            writePosition = { position ->
                cooldownWindowParams.x = position.x
                cooldownWindowParams.y = position.y
                clampCooldownPosition()
            },
            onMove = {
                runCatching { windowManager.updateViewLayout(cooldownView, cooldownWindowParams) }
            },
            onDrop = ::persistCooldownPosition,
        ).attachTo(cooldownView)
    }

    private fun ensureScanResultOverlays() {
        if (::hundoResultsView.isInitialized || ::shinyResultsView.isInitialized) return

        hundoResultsView = ScanResultOverlayView(this, ScanMatchType.HUNDO) {
            openScanResults(ScanMatchType.HUNDO)
        }
        shinyResultsView = ScanResultOverlayView(this, ScanMatchType.SHINY) {
            openScanResults(ScanMatchType.SHINY)
        }

        val defaultY = dp(120)
        val hundoDefault = positionStore.loadHundoPosition(
            OverlayPosition(edgeMarginPx, defaultY),
        )
        val shinyDefault = positionStore.loadShinyPosition(
            OverlayPosition(edgeMarginPx + scanWidgetWidthPx + dp(SCAN_WIDGET_GAP_DP), defaultY),
        )
        hundoResultsWindowParams = newOverlayParams(
            scanWidgetWidthPx,
            dp(SCAN_WIDGET_INITIAL_HEIGHT_DP),
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            x = hundoDefault.x
            y = hundoDefault.y
        }
        shinyResultsWindowParams = newOverlayParams(
            scanWidgetWidthPx,
            dp(SCAN_WIDGET_INITIAL_HEIGHT_DP),
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            x = shinyDefault.x
            y = shinyDefault.y
        }
        clampScanWidget(hundoResultsWindowParams, hundoResultsView)
        clampScanWidget(shinyResultsWindowParams, shinyResultsView)
        windowManager.addView(hundoResultsView, hundoResultsWindowParams)
        windowManager.addView(shinyResultsView, shinyResultsWindowParams)

        val dragHandler = ScanWidgetDragHandler(this)
        dragHandler.attachTo(hundoResultsView, object : ScanWidgetDragHandler.Callbacks {
            override fun readPosition(): OverlayPosition = OverlayPosition(
                hundoResultsWindowParams.x,
                hundoResultsWindowParams.y,
            )

            override fun writePosition(position: OverlayPosition) {
                hundoResultsWindowParams.x = position.x
                hundoResultsWindowParams.y = position.y
                clampScanWidget(hundoResultsWindowParams, hundoResultsView)
            }

            override fun onMove() {
                runCatching { windowManager.updateViewLayout(hundoResultsView, hundoResultsWindowParams) }
            }

            override fun onDrop() = persistHundoResultsPosition()
        })
        dragHandler.attachTo(shinyResultsView, object : ScanWidgetDragHandler.Callbacks {
            override fun readPosition(): OverlayPosition = OverlayPosition(
                shinyResultsWindowParams.x,
                shinyResultsWindowParams.y,
            )

            override fun writePosition(position: OverlayPosition) {
                shinyResultsWindowParams.x = position.x
                shinyResultsWindowParams.y = position.y
                clampScanWidget(shinyResultsWindowParams, shinyResultsView)
            }

            override fun onMove() {
                runCatching { windowManager.updateViewLayout(shinyResultsView, shinyResultsWindowParams) }
            }

            override fun onDrop() = persistShinyResultsPosition()
        })
    }

    private fun renderScanResults() {
        if (!::hundoResultsView.isInitialized || !::shinyResultsView.isInitialized) return
        hundoResultsView.render(scanResultRepository.read(ScanMatchType.HUNDO))
        shinyResultsView.render(scanResultRepository.read(ScanMatchType.SHINY))
        resizeScanWidget(hundoResultsView, hundoResultsWindowParams)
        resizeScanWidget(shinyResultsView, shinyResultsWindowParams)
    }

    private fun resizeScanWidget(
        view: ScanResultOverlayView,
        params: WindowManager.LayoutParams,
    ) {
        params.width = scanWidgetWidthPx
        params.height = view.desiredHeightPx().coerceAtLeast(dp(SCAN_WIDGET_INITIAL_HEIGHT_DP))
        clampScanWidget(params, view)
        runCatching { windowManager.updateViewLayout(view, params) }
    }

    private fun clampScanWidget(
        params: WindowManager.LayoutParams,
        view: ScanResultOverlayView,
    ) {
        params.width = scanWidgetWidthPx
        params.height = view.desiredHeightPx().coerceAtLeast(dp(SCAN_WIDGET_INITIAL_HEIGHT_DP))
        params.x = clamp(params.x, displayWidth() - params.width)
        params.y = clamp(params.y, displayHeight() - params.height)
    }

    private fun openScanResults(matchType: ScanMatchType) {
        startActivity(
            Intent(this, ScanResultsActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .putExtra(ScanResultsActivity.EXTRA_SECTION, matchType.name),
        )
    }

    private fun renderShortcutStates() {
        if (!::floatButton.isInitialized) return
        val config = automationConfigRepository.read()
        shortcutMenu.render(config, speedPresetIndex)
        floatButton.background = roundedBackground(
            if (config.enabled) 0xE62E7D32.toInt() else 0xE6202124.toInt(),
            28,
        )
        floatButton.contentDescription = if (config.enabled) {
            "PoGo Tools menu, automation on"
        } else {
            "PoGo Tools menu, automation off"
        }
    }

    private fun toggleAutomation(key: String) {
        val current = automationConfigRepository.read()
        val currentValue = when (key) {
            "automation" -> current.enabled
            "catch" -> current.autoCatch
            "spin" -> current.autoSpin
            "encounter" -> current.autoEncounter
            "discard" -> current.autoDiscard
            "transfer" -> current.autoTransfer
            else -> return
        }
        val nextValue = !currentValue
        if (nextValue && (key == "discard" || key == "transfer")) {
            val label = if (key == "discard") "Auto discard" else "Auto transfer"
            val themed = ContextThemeWrapper(this, android.R.style.Theme_Material_Light_Dialog_Alert)
            val dialog = AlertDialog.Builder(themed)
                .setTitle("Enable $label?")
                .setMessage("This automation can change game inventory or Pokémon. Continue?")
                .setNegativeButton("Cancel", null)
                .setPositiveButton("Enable") { _, _ -> applyAutomationToggle(key, true) }
                .create()
            dialog.window?.setType(overlayWindowType())
            dialog.show()
        } else {
            applyAutomationToggle(key, nextValue)
        }
    }

    private fun applyAutomationToggle(key: String, enabled: Boolean) {
        automationConfigRepository.update { current ->
            when (key) {
                "automation" -> current.copy(enabled = enabled)
                "catch" -> current.copy(autoCatch = enabled)
                "spin" -> current.copy(autoSpin = enabled)
                "encounter" -> current.copy(autoEncounter = enabled)
                "discard" -> current.copy(autoDiscard = enabled)
                "transfer" -> current.copy(autoTransfer = enabled)
                else -> current
            }
        }
        renderShortcutStates()
    }

    private fun showTeleportDialog() {
        val themedContext = ContextThemeWrapper(this, android.R.style.Theme_Material_Light_Dialog_Alert)
        val input = android.widget.EditText(themedContext).apply {
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

        dialog.window?.setType(overlayWindowType())
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
            state.teleportCooldown?.let { cooldown ->
                latestTeleportCooldown = cooldown
                persistCooldown(cooldown)
            }
            renderCooldown()
            persistPointOccasionally(state.point)
        }
    }

    private fun applyPendingMapTarget() {
        if (!::mapTargetRepository.isInitialized || !controllerStarted) return
        if (!automationConfigRepository.read().mapTapWalkEnabled) {
            mapTargetRepository.clear()
            return
        }

        val location = controller.snapshot()
        if (!location.providerReady || location.point == null) return

        val target = mapTargetRepository.read() ?: return
        mapTargetRepository.clear()
        controller.walkTo(target.target)
    }

    private fun renderCooldown() {
        if (!::cooldownView.isInitialized) return
        cooldownMode = positionStore.loadCooldownMode()

        val lastActive = if (cooldownMode == TeleportCooldownMode.LAST_ACTIVE) {
            lastActiveLocationRepository.read()
        } else {
            null
        }
        val currentPoint = if (cooldownMode == TeleportCooldownMode.LAST_ACTIVE) {
            controller.snapshot().point ?: loadSavedPoint()
        } else {
            null
        }
        val cooldown = when (cooldownMode) {
            TeleportCooldownMode.CURRENT_POSITION -> latestTeleportCooldown
            TeleportCooldownMode.LAST_ACTIVE -> lastActiveCooldown(lastActive, currentPoint)
        }
        val remaining = cooldown?.remainingMillis(System.currentTimeMillis()) ?: 0L
        if (remaining <= 0L) {
            cooldownView.visibility = View.INVISIBLE
            return
        }

        val text = formatCooldown(remaining)
        cooldownView.text = text
        cooldownView.contentDescription = "Cooldown $text"
        cooldownView.visibility = View.VISIBLE
    }

    private fun formatCooldown(remainingMillis: Long): String {
        val totalMinutes = (remainingMillis + 59_999L) / 60_000L
        val hours = totalMinutes / 60L
        val minutes = totalMinutes % 60L
        return String.format(Locale.US, "%02d:%02d", hours, minutes)
    }

    private fun lastActiveCooldown(
        activity: LastActiveGameAction?,
        destination: GeoPoint?,
    ): TeleportCooldown? {
        activity ?: return null
        destination ?: return null
        return cooldownService.forLastActive(
            lastActivePoint = activity.point,
            lastActiveAtEpochMs = activity.activeAtEpochMs,
            destination = destination,
        )
    }

    private fun loadSavedPoint(): GeoPoint? = positionStore.loadPoint()

    private fun persistPointOccasionally(point: GeoPoint?) {
        val now = android.os.SystemClock.elapsedRealtime()
        if (now - lastPersistAt < 1_000L) return
        lastPersistAt = now
        persistPoint(point)
    }

    private fun persistPoint(point: GeoPoint?) {
        positionStore.persistPoint(point)
    }

    private fun persistCooldown(cooldown: TeleportCooldown) {
        positionStore.persistCooldown(cooldown)
    }

    private fun persistMainPosition() {
        positionStore.persistMainPosition(OverlayPosition(mainAnchorX, mainAnchorY))
    }

    private fun persistCooldownPosition() {
        positionStore.persistCooldownPosition(
            OverlayPosition(cooldownWindowParams.x, cooldownWindowParams.y),
        )
    }

    private fun persistHundoResultsPosition() {
        positionStore.persistHundoPosition(
            OverlayPosition(hundoResultsWindowParams.x, hundoResultsWindowParams.y),
        )
    }

    private fun persistShinyResultsPosition() {
        positionStore.persistShinyPosition(
            OverlayPosition(shinyResultsWindowParams.x, shinyResultsWindowParams.y),
        )
    }

    private fun clampMainAnchor() {
        mainAnchorX = clamp(mainAnchorX, displayWidth() - iconSizePx)
        mainAnchorY = clamp(mainAnchorY, displayHeight() - iconSizePx)
    }

    private fun clampCooldownPosition() {
        cooldownWindowParams.x = clamp(cooldownWindowParams.x, displayWidth() - cooldownWidthPx)
        cooldownWindowParams.y = clamp(cooldownWindowParams.y, displayHeight() - cooldownHeightPx)
    }

    private fun displayWidth(): Int = resources.displayMetrics.widthPixels

    private fun displayHeight(): Int = resources.displayMetrics.heightPixels

    private fun clamp(value: Int, maxValue: Int): Int = value.coerceIn(0, max(0, maxValue))

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
            .setContentText("Joystick + shortcut overlay is active")
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build()
    }

}
