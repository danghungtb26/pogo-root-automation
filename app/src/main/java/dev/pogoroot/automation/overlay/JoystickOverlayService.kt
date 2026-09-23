package dev.pogoroot.automation.overlay

import android.app.AlertDialog
import android.app.Service
import android.content.Intent
import android.content.res.Configuration
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.view.ContextThemeWrapper
import android.view.WindowManager
import android.util.Log
import dev.pogoroot.automation.core.automation.AutoFortNavigationCommand
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.core.scan.ScanMatchType
import dev.pogoroot.automation.core.time.TeleportCooldown
import dev.pogoroot.automation.config.AutomationConfigRepository
import dev.pogoroot.automation.engine.CatchSpinArmState
import dev.pogoroot.automation.data.FavoriteLocation
import dev.pogoroot.automation.data.FavoriteLocationRepository
import dev.pogoroot.automation.data.LastActiveLocationRepository
import dev.pogoroot.automation.data.MapTargetRepository
import dev.pogoroot.automation.location.JoystickLocationController
import dev.pogoroot.automation.location.JoystickLocationState
import dev.pogoroot.automation.location.RootMockLocationProvider
import dev.pogoroot.automation.location.AutoFortNavigationBus
import dev.pogoroot.automation.scan.ScanResultRepository
import dev.pogoroot.automation.service.HeadlessAutomationService
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

class JoystickOverlayService : Service() {
    companion object {
        const val ACTION_START = "dev.pogoroot.automation.action.START_JOYSTICK"
        const val ACTION_STOP = "dev.pogoroot.automation.action.STOP_JOYSTICK"

        private const val COOLDOWN_REFRESH_MS = 1_000L
        private const val FOREGROUND_POLL_MS = 750L
        private const val LOG_TAG = "PogoRootAutomation"
    }

    private val mainHandler = Handler(Looper.getMainLooper())
    private val speedPresets = doubleArrayOf(3.0, 6.0, 9.0, 15.0, 30.0)

    private lateinit var windowManager: WindowManager
    private lateinit var controller: JoystickLocationController
    private lateinit var automationConfigRepository: AutomationConfigRepository
    private lateinit var favoriteLocationRepository: FavoriteLocationRepository
    private lateinit var lastActiveLocationRepository: LastActiveLocationRepository
    private lateinit var mapTargetRepository: MapTargetRepository
    private lateinit var scanResultRepository: ScanResultRepository
    private lateinit var positionStore: OverlayPositionStore
    private lateinit var gameForegroundDetector: GameForegroundDetector
    private lateinit var mainOverlay: MainOverlayView
    private lateinit var cooldownOverlay: CooldownOverlayView
    private lateinit var cooldownRenderer: CooldownOverlayRenderer
    private lateinit var scanResultOverlays: ScanResultOverlays
    private lateinit var settingsOverlay: SettingsOverlayController

    private val foregroundExecutor = Executors.newSingleThreadScheduledExecutor()
    private var foregroundPoll: ScheduledFuture<*>? = null
    private val overlayDialogs = mutableSetOf<AlertDialog>()
    private var favoriteLocationsDialog: FavoriteLocationsDialog? = null
    private var teleportDialog: AlertDialog? = null
    private var controllerStarted = false
    private var overlayVisible = false
    private var destroyed = false
    private var speedPresetIndex = 2
    private var lastPersistAt = 0L
    private var latestTeleportCooldown: TeleportCooldown? = null
    private var latestAutoFortCommand: AutoFortNavigationCommand? = null

    private val autoFortNavigationListener: (AutoFortNavigationCommand) -> Unit = { command ->
        mainHandler.post {
            if (destroyed) return@post
            latestAutoFortCommand = command
            applyAutoFortNavigationCommand()
        }
    }

    private val cooldownTick = object : Runnable {
        override fun run() {
            applyPendingMapTarget()
            renderShortcutStates()
            renderCooldown()
            favoriteLocationsDialog?.refresh()
            if (::scanResultOverlays.isInitialized) scanResultOverlays.render()
            mainHandler.postDelayed(this, COOLDOWN_REFRESH_MS)
        }
    }

    override fun onCreate() {
        super.onCreate()
        windowManager = getSystemService(WindowManager::class.java)
        automationConfigRepository = AutomationConfigRepository(this)
        favoriteLocationRepository = FavoriteLocationRepository(this)
        lastActiveLocationRepository = LastActiveLocationRepository(this)
        mapTargetRepository = MapTargetRepository(this)
        scanResultRepository = ScanResultRepository()
        positionStore = OverlayPositionStore(this)
        gameForegroundDetector = GameForegroundDetector(this)
        latestTeleportCooldown = positionStore.loadCooldown()
        createNotificationChannel()
        startForeground(JOYSTICK_NOTIFICATION_ID, buildNotification())

        controller = JoystickLocationController(
            sink = RootMockLocationProvider(this),
            onStateChanged = ::onLocationStateChanged,
        )
        AutoFortNavigationBus.register(autoFortNavigationListener)
        foregroundPoll = foregroundExecutor.scheduleWithFixedDelay(
            ::pollGameForeground,
            0L,
            FOREGROUND_POLL_MS,
            TimeUnit.MILLISECONDS,
        )
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                stopSelf()
                return START_NOT_STICKY
            }
            ACTION_START -> Unit
            else -> {
                // Never resurrect a mock-location provider after a sticky or
                // implicit service restart without an explicit user action.
                stopSelf()
                return START_NOT_STICKY
            }
        }

        mainHandler.removeCallbacks(cooldownTick)
        mainHandler.post(cooldownTick)
        if (!controllerStarted) {
            controllerStarted = true
            controller.start(positionStore.loadPointOrDefault())
            applyAutoFortNavigationCommand()
        }
        return START_NOT_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        mainHandler.post {
            if (::settingsOverlay.isInitialized) settingsOverlay.onConfigurationChanged()
            if (::mainOverlay.isInitialized) mainOverlay.onConfigurationChanged()
            if (::cooldownOverlay.isInitialized) cooldownOverlay.onConfigurationChanged()
            if (::scanResultOverlays.isInitialized) scanResultOverlays.onConfigurationChanged()
        }
    }

    override fun onDestroy() {
        destroyed = true
        AutoFortNavigationBus.unregister()
        foregroundPoll?.cancel(true)
        foregroundExecutor.shutdownNow()
        mainHandler.removeCallbacks(cooldownTick)
        dismissOverlayDialogs()
        runCatching { favoriteLocationsDialog?.dismiss() }
        favoriteLocationsDialog = null
        if (controllerStarted) {
            persistPoint(controller.snapshot().point)
            controller.stop()
            controllerStarted = false
        }
        if (::scanResultOverlays.isInitialized) scanResultOverlays.dispose()
        if (::settingsOverlay.isInitialized) settingsOverlay.dispose()
        if (::cooldownOverlay.isInitialized) cooldownOverlay.dispose()
        if (::mainOverlay.isInitialized) mainOverlay.dispose()
        super.onDestroy()
    }

    private fun pollGameForeground() {
        val state = gameForegroundDetector.read()
        mainHandler.post {
            if (!destroyed) applyGameForeground(state)
        }
    }

    private fun applyGameForeground(state: GameForegroundDetector.State) {
        val shouldShow = state == GameForegroundDetector.State.FOREGROUND
        if (shouldShow == overlayVisible) return

        if (shouldShow) {
            ensureOverlay()
            overlayVisible = true
            mainOverlay.setVisible(true)
            cooldownOverlay.setVisible(true)
            scanResultOverlays.setVisible(true)
            renderShortcutStates()
            renderCooldown()
            scanResultOverlays.render()
        } else {
            overlayVisible = false
            if (::settingsOverlay.isInitialized) settingsOverlay.dismiss()
            collapseMainOverlay()
            if (::mainOverlay.isInitialized) mainOverlay.setVisible(false)
            if (::cooldownOverlay.isInitialized) cooldownOverlay.setVisible(false)
            if (::scanResultOverlays.isInitialized) scanResultOverlays.setVisible(false)
            dismissOverlayDialogs()
            runCatching { favoriteLocationsDialog?.dismiss() }
        }
    }

    private fun ensureOverlay() {
        if (::mainOverlay.isInitialized) return

        mainOverlay = MainOverlayView(
            context = this,
            windowManager = windowManager,
            positionStore = positionStore,
            controller = controller,
            speedPresets = speedPresets,
            onToggle = ::toggleAutomation,
            onTeleport = {
                collapseMainOverlay()
                showTeleportDialog()
            },
            onFavorites = {
                collapseMainOverlay()
                showFavoriteLocationsDialog()
            },
            onSpeed = {
                speedPresetIndex = (speedPresetIndex + 1) % speedPresets.size
                controller.setMaxSpeedKmh(speedPresets[speedPresetIndex])
                renderShortcutStates()
            },
            onSettings = {
                settingsOverlay.open()
            },
            onClose = { stopSelf() },
        )
        mainOverlay.ensure()

        cooldownOverlay = CooldownOverlayView(this, windowManager, positionStore)
        cooldownOverlay.ensure()
        cooldownRenderer = CooldownOverlayRenderer(
            positionStore = positionStore,
            lastActiveLocationRepository = lastActiveLocationRepository,
            controller = controller,
            cooldownOverlay = cooldownOverlay,
            latestTeleportCooldown = { latestTeleportCooldown },
        )

        scanResultOverlays = ScanResultOverlays(
            context = this,
            windowManager = windowManager,
            positionStore = positionStore,
            scanResultRepository = scanResultRepository,
            onOpenResults = ::openScanResults,
        )
        scanResultOverlays.ensure()

        settingsOverlay = SettingsOverlayController(
            context = this,
            windowManager = windowManager,
            repository = automationConfigRepository,
            positionStore = positionStore,
            isForeground = { overlayVisible && !destroyed },
            onHideMain = {
                collapseMainOverlay()
                mainOverlay.setVisible(false)
            },
            onShowMain = {
                mainOverlay.setVisible(true)
                renderShortcutStates()
                renderCooldown()
            },
            onSaved = {
                requestRuntimeConfigSync()
                renderShortcutStates()
                renderCooldown()
            },
        )
        settingsOverlay.ensure()
    }

    private fun collapseMainOverlay() {
        if (::mainOverlay.isInitialized) mainOverlay.collapse()
    }

    private fun openScanResults(matchType: ScanMatchType) {
        startActivity(
            Intent(this, ScanResultsActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .putExtra(ScanResultsActivity.EXTRA_SECTION, matchType.name),
        )
    }
    private fun renderShortcutStates() {
        if (!::mainOverlay.isInitialized) return
        val config = automationConfigRepository.read()
        mainOverlay.render(config, speedPresetIndex, CatchSpinArmState.isArmed())
    }

    private fun toggleAutomation(key: String) {
        val current = automationConfigRepository.read()
        val currentValue = when (key) {
            "automation" -> CatchSpinArmState.isArmed()
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
            showOverlayDialog(dialog)
        } else {
            applyAutomationToggle(key, nextValue)
        }
    }

    private fun applyAutomationToggle(key: String, enabled: Boolean) {
        if (key == "automation") {
            automationConfigRepository.update { it.copy(catchSpinArmed = enabled) }
            CatchSpinArmState.setArmed(enabled)
            renderShortcutStates()
            requestRuntimeConfigSync()
            return
        }
        automationConfigRepository.update { current ->
            when (key) {
                "catch" -> current.copy(autoCatch = enabled)
                "spin" -> current.copy(autoSpin = enabled)
                "encounter" -> current.copy(autoEncounter = enabled)
                "discard" -> current.copy(autoDiscard = enabled)
                "transfer" -> current.copy(autoTransfer = enabled)
                else -> current
            }
        }
        renderShortcutStates()
        requestRuntimeConfigSync()
    }

    private fun requestRuntimeConfigSync() {
        runCatching { HeadlessAutomationService.requestRuntimeConfigSync(this) }
            .onFailure { error ->
                Log.w(LOG_TAG, "runtime config sync request failed: ${error.message}")
            }
    }

    private fun showTeleportDialog() {
        val dialog = TeleportLocationDialog(
            context = this,
            currentPoint = { controller.snapshot().point },
            onTeleport = controller::teleport,
        ).create()
        teleportDialog = dialog
        showOverlayDialog(dialog) {
            if (teleportDialog === dialog) teleportDialog = null
        }
    }

    private fun showFavoriteLocationsDialog() {
        if (favoriteLocationsDialog == null) {
            favoriteLocationsDialog = FavoriteLocationsDialog(
                context = this,
                repository = favoriteLocationRepository,
                currentPoint = { controller.snapshot().point },
                onTeleport = { favorite -> executeFavoriteAction(favorite, walk = false) },
                onWalk = { favorite -> executeFavoriteAction(favorite, walk = true) },
                onMessage = ::showLocationToast,
                onDismissed = { favoriteLocationsDialog = null },
            )
        }
        favoriteLocationsDialog?.show()
    }

    private fun executeFavoriteAction(favorite: FavoriteLocation, walk: Boolean) {
        val state = controller.snapshot()
        if (!controllerStarted || !state.providerReady) {
            showLocationToast("Location provider is not ready")
            return
        }
        if (walk && state.point == null) {
            showLocationToast("Cannot walk without a current location")
            return
        }

        runCatching {
            if (walk) {
                controller.walkTo(favorite.point)
            } else {
                controller.teleport(favorite.point)
            }
        }.onSuccess {
            favoriteLocationsDialog?.dismiss()
            showLocationToast(if (walk) "Walk started" else "Teleport requested")
        }.onFailure { error ->
            showLocationToast(error.message ?: "Location action failed")
        }
    }

    private fun showLocationToast(message: String) {
        CustomToast.show(this, message)
    }

    private fun showOverlayDialog(dialog: AlertDialog, onDismissed: () -> Unit = {}) {
        overlayDialogs += dialog
        dialog.setOnDismissListener {
            overlayDialogs.remove(dialog)
            onDismissed()
        }
        dialog.show()
    }

    private fun dismissOverlayDialogs() {
        overlayDialogs.toList().forEach { dialog ->
            runCatching { dialog.dismiss() }
        }
        overlayDialogs.clear()
        teleportDialog = null
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

    private fun applyAutoFortNavigationCommand() {
        if (!controllerStarted) return
        when (val command = latestAutoFortCommand) {
            is AutoFortNavigationCommand.WalkTo -> {
                if (controller.snapshot().point == null) return
                Log.i(
                    LOG_TAG,
                    "auto fort navigation walk fort=${command.fortId} " +
                        "target=${command.target.latitude},${command.target.longitude}",
                )
                controller.walkTo(command.target)
            }
            is AutoFortNavigationCommand.Stop -> {
                Log.i(LOG_TAG, "auto fort navigation stop reason=${command.reason}")
                controller.stopWalking()
            }
            null -> Unit
        }
    }

    private fun renderCooldown() {
        if (!::cooldownRenderer.isInitialized) return
        cooldownRenderer.render()
    }

    private fun persistPointOccasionally(point: GeoPoint?) {
        val now = android.os.SystemClock.elapsedRealtime()
        if (now - lastPersistAt < 1_000L) return
        lastPersistAt = now
        persistPoint(point)
    }
    private fun persistPoint(point: GeoPoint?) = positionStore.persistPoint(point)
    private fun persistCooldown(cooldown: TeleportCooldown) = positionStore.persistCooldown(cooldown)
}
