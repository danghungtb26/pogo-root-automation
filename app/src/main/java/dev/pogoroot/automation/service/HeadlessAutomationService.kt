package dev.pogoroot.automation.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.IBinder
import android.util.Log
import dev.pogoroot.automation.MainActivity
import dev.pogoroot.automation.root.RuntimeBridgeClient
import dev.pogoroot.automation.root.RuntimeModuleLoadStatus
import dev.pogoroot.automation.scan.ScanResultRepository
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit
import dev.pogoroot.automation.config.AutomationConfigRepository
import dev.pogoroot.automation.data.LastActiveLocationRepository
import dev.pogoroot.automation.data.MapTargetRepository
import dev.pogoroot.automation.engine.AutomationRunState
import dev.pogoroot.automation.engine.HeadlessAutomationEngine
import dev.pogoroot.automation.events.AutomationEvent
import dev.pogoroot.automation.events.AutomationEventSink
import dev.pogoroot.automation.events.AutomationEventType
import dev.pogoroot.automation.events.ToastAutomationEventSink
import dev.pogoroot.automation.runtime.RuntimeLifecycleCoordinator
import dev.pogoroot.automation.runtime.structured.StructuredAutomationController

class HeadlessAutomationService : Service() {
    private lateinit var configRepository: AutomationConfigRepository
    private lateinit var engine: HeadlessAutomationEngine
    private lateinit var apiServer: AutomationControlServer
    private lateinit var eventSink: AutomationEventSink
    private var runtimeBridge: RuntimeBridgeClient? = null
    private lateinit var runtimeCoordinator: RuntimeLifecycleCoordinator
    private lateinit var structuredController: StructuredAutomationController
    private lateinit var lastActiveLocationRepository: LastActiveLocationRepository
    private lateinit var mapTargetRepository: MapTargetRepository
    private lateinit var scanResultRepository: ScanResultRepository
    private lateinit var joystickAutoStartCoordinator: JoystickAutoStartCoordinator
    private val joystickAutoStartExecutor = Executors.newSingleThreadScheduledExecutor()
    private var joystickAutoStartPoll: ScheduledFuture<*>? = null

    override fun onCreate() {
        super.onCreate()
        configRepository = AutomationConfigRepository(this)
        AutomationRunState.setActive(false)
        eventSink = ToastAutomationEventSink(this, configRepository)
        lastActiveLocationRepository = LastActiveLocationRepository(this)
        mapTargetRepository = MapTargetRepository(this)
        scanResultRepository = ScanResultRepository()
        scanResultRepository.clear()
        runtimeBridge = RuntimeBridgeClient(
            onModuleLoadStatus = ::publishRuntimeModuleLoadStatus,
        )
        runtimeCoordinator = RuntimeLifecycleCoordinator(runtimeBridge!!)
        structuredController = StructuredAutomationController(
            bridge = runtimeBridge!!,
            eventSink = eventSink,
            // A verified fingerprint must be explicitly provisioned per device/build.
            // Empty means structured observation is available but mutations stay disabled.
            allowedBuildFingerprintsProvider = {
                configRepository.read().structuredAllowedBuildFingerprints
            },
            onGameAction = lastActiveLocationRepository::record,
            onEncounterSnapshot = scanResultRepository::recordEncounter,
            onMapTarget = { target ->
                if (configRepository.read().mapTapWalkEnabled) {
                    mapTargetRepository.publish(target)
                }
            },
        )
        engine = HeadlessAutomationEngine(
            configRepository = configRepository,
            runtimeCoordinator = runtimeCoordinator,
            structuredController = structuredController,
            eventSink = eventSink,
        )
        apiServer = AutomationControlServer(
            configRepository = configRepository,
            engine = engine,
            runtimeDiagnostic = { runtimeCoordinator.runDiagnostic(configRepository.read()) },
        )
        joystickAutoStartCoordinator = JoystickAutoStartCoordinator(
            context = this,
            onGameAvailable = ::enableAutomationForGameForeground,
            onGameUnavailable = ::disableAutomationForGameExit,
        )

        createNotificationChannel()
        startForeground(NOTIFICATION_ID, buildNotification())
        apiServer.start()
        engine.start()
        joystickAutoStartPoll = joystickAutoStartExecutor.scheduleWithFixedDelay(
            ::syncJoystickAutoStart,
            0L,
            JOYSTICK_AUTO_START_POLL_MS,
            TimeUnit.MILLISECONDS,
        )
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_ENABLE -> {
                Log.i(LOG_TAG, "automation master enable requested from overlay")
                configRepository.update { current ->
                    current.copy(
                        autoCatch = intent.booleanExtraOrNull(EXTRA_AUTO_CATCH) ?: current.autoCatch,
                        autoSpin = intent.booleanExtraOrNull(EXTRA_AUTO_SPIN) ?: current.autoSpin,
                        autoEncounter = intent.booleanExtraOrNull(EXTRA_AUTO_ENCOUNTER)
                            ?: current.autoEncounter,
                    )
                }
                engine.activate()
            }

            ACTION_DISABLE -> {
                Log.i(LOG_TAG, "automation master disable requested from overlay")
                // The worker remains alive. Its next loop sends STOP_RUNTIME and
                // leaves the injected process in ATTACHED_IDLE for fast restart.
                engine.deactivate()
            }

            ACTION_STOP_SERVICE -> {
                engine.deactivate()
                stopSelf()
            }
        }
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        AutomationRunState.setActive(false)
        joystickAutoStartPoll?.cancel(true)
        joystickAutoStartExecutor.shutdownNow()
        if (::joystickAutoStartCoordinator.isInitialized) {
            joystickAutoStartCoordinator.stop()
        }
        apiServer.stop()
        engine.shutdown()
        super.onDestroy()
    }

    private fun publishRuntimeModuleLoadStatus(status: RuntimeModuleLoadStatus) {
        val moduleName = status.module.name
        if (status.loaded) {
            eventSink.publish(
                AutomationEvent(
                    type = AutomationEventType.MODULE_LOADED,
                    message = "$moduleName module loaded",
                ),
            )
            return
        }

        val detail = status.errorCode
            ?: status.message
            ?: "unknown error"
        eventSink.publish(
            AutomationEvent(
                type = AutomationEventType.MODULE_LOAD_FAILED,
                message = "$moduleName module load failed: $detail",
            ),
        )
    }

    private fun syncJoystickAutoStart() {
        runCatching { joystickAutoStartCoordinator.sync() }
    }

    private fun enableAutomationForGameForeground() {
        if (AutomationRunState.isActive()) return
        Log.i(LOG_TAG, "automation auto-enabled: Pokémon GO is foreground")
        eventSink.publish(
            AutomationEvent(
                type = AutomationEventType.INFO,
                message = "Automation running while Pokémon GO is open",
            ),
        )
        engine.activate()
    }

    private fun disableAutomationForGameExit() {
        if (!AutomationRunState.isActive()) return
        Log.i(LOG_TAG, "automation auto-disabled: Pokémon GO is no longer foreground")
        eventSink.publish(
            AutomationEvent(
                type = AutomationEventType.INFO,
                message = "Automation disabled because Pokémon GO was closed",
            ),
        )
        engine.deactivate()
    }

    private fun createNotificationChannel() {
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(
                CHANNEL_ID,
                "Headless automation",
                NotificationManager.IMPORTANCE_LOW,
            ).apply {
                description = "Keeps the local control API and automation worker running"
                setShowBadge(false)
            },
        )
    }

    private fun buildNotification(): Notification {
        val openIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        val disableIntent = PendingIntent.getService(
            this,
            1,
            Intent(this, HeadlessAutomationService::class.java).setAction(ACTION_DISABLE),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )

        return Notification.Builder(this, CHANNEL_ID)
            .setContentTitle("PoGo automation service")
            .setContentText("Headless API on 127.0.0.1:${AutomationControlServer.DEFAULT_PORT}")
            .setSmallIcon(android.R.drawable.stat_notify_sync_noanim)
            .setContentIntent(openIntent)
            .addAction(
                Notification.Action.Builder(
                    android.R.drawable.ic_media_pause,
                    "Disable automation",
                    disableIntent,
                ).build(),
            )
            .setOngoing(true)
            .build()
    }

    private fun Intent.booleanExtraOrNull(name: String): Boolean? =
        if (hasExtra(name)) getBooleanExtra(name, false) else null

    companion object {
        const val ACTION_ENABLE = "dev.pogoroot.automation.action.ENABLE_HEADLESS"
        const val ACTION_DISABLE = "dev.pogoroot.automation.action.DISABLE_HEADLESS"
        const val ACTION_STOP_SERVICE = "dev.pogoroot.automation.action.STOP_HEADLESS_SERVICE"
        const val EXTRA_AUTO_CATCH = "autoCatch"
        const val EXTRA_AUTO_SPIN = "autoSpin"
        const val EXTRA_AUTO_ENCOUNTER = "autoEncounter"

        private const val JOYSTICK_AUTO_START_POLL_MS = 750L
        private const val CHANNEL_ID = "pogo_headless_automation"
        private const val NOTIFICATION_ID = 2102
        private const val LOG_TAG = "PogoRootAutomation"

        fun start(context: Context) {
            context.startForegroundService(
                Intent(context, HeadlessAutomationService::class.java),
            )
        }

        fun enable(
            context: Context,
            autoCatch: Boolean = true,
            autoSpin: Boolean = true,
            autoEncounter: Boolean = false,
        ) {
            AutomationRunState.setActive(true)
            context.startForegroundService(
                Intent(context, HeadlessAutomationService::class.java)
                    .setAction(ACTION_ENABLE)
                    .putExtra(EXTRA_AUTO_CATCH, autoCatch)
                    .putExtra(EXTRA_AUTO_SPIN, autoSpin)
                    .putExtra(EXTRA_AUTO_ENCOUNTER, autoEncounter),
            )
        }

        fun disable(context: Context) {
            AutomationRunState.setActive(false)
            context.startService(
                Intent(context, HeadlessAutomationService::class.java)
                    .setAction(ACTION_DISABLE),
            )
        }
    }
}
