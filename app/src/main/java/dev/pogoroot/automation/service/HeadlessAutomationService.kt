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
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit
import dev.pogoroot.automation.config.AutomationConfigRepository
import dev.pogoroot.automation.data.MapTargetRepository
import dev.pogoroot.automation.events.AutomationEventSink
import dev.pogoroot.automation.events.ToastAutomationEventSink
import dev.pogoroot.automation.location.NativeWalkCandidateReceiver
import dev.pogoroot.automation.location.WalkCandidateCoordinator

class HeadlessAutomationService : Service() {
    private lateinit var configRepository: AutomationConfigRepository
    private lateinit var apiServer: AutomationControlServer
    private lateinit var eventSink: AutomationEventSink
    private lateinit var runtimeFacade: RuntimeUiAutomationFacade
    private lateinit var walkCandidateReceiver: NativeWalkCandidateReceiver
    private lateinit var mapTargetRepository: MapTargetRepository
    private lateinit var joystickAutoStartCoordinator: JoystickAutoStartCoordinator
    private val joystickAutoStartExecutor = Executors.newSingleThreadScheduledExecutor()
    private var joystickAutoStartPoll: ScheduledFuture<*>? = null

    override fun onCreate() {
        super.onCreate()
        configRepository = AutomationConfigRepository(this)
        eventSink = ToastAutomationEventSink(this, configRepository)
        mapTargetRepository = MapTargetRepository(this)
        walkCandidateReceiver = NativeWalkCandidateReceiver(
            onCandidate = { candidate, nowNanos, previousSequence ->
                WalkCandidateCoordinator.submit(candidate, nowNanos, previousSequence)
            },
            onTerminal = { terminal, previousSequence ->
                WalkCandidateCoordinator.terminate(terminal, previousSequence)
            },
            onReset = WalkCandidateCoordinator::reset,
        )
        runtimeFacade = RuntimeUiAutomationFacade(
            configRepository = configRepository,
            eventSink = eventSink,
            mapTargetRepository = mapTargetRepository,
            walkCandidateReceiver = walkCandidateReceiver,
        )
        apiServer = AutomationControlServer(
            configRepository = configRepository,
            runtimeFacade = runtimeFacade,
        )
        joystickAutoStartCoordinator = JoystickAutoStartCoordinator(
            context = this,
        )

        createNotificationChannel()
        startForeground(NOTIFICATION_ID, buildNotification())
        apiServer.start()
        runtimeFacade.start()
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
                runtimeFacade.enable { current ->
                    current.copy(
                        autoCatch = intent.booleanExtraOrNull(EXTRA_AUTO_CATCH) ?: current.autoCatch,
                        autoSpin = intent.booleanExtraOrNull(EXTRA_AUTO_SPIN) ?: current.autoSpin,
                        autoEncounter = intent.booleanExtraOrNull(EXTRA_AUTO_ENCOUNTER)
                            ?: current.autoEncounter,
                    )
                }
            }

            ACTION_DISABLE -> {
                Log.i(LOG_TAG, "automation master disable requested from overlay")
                // The service remains alive; native reconciles disabled desired state.
                runtimeFacade.disable()
            }

            ACTION_SYNC_RUNTIME_CONFIG -> {
                runtimeFacade.submitCurrentDesiredStateAsync()
            }

            ACTION_STOP_SERVICE -> {
                runtimeFacade.disable()
                stopSelf()
            }
        }
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        joystickAutoStartPoll?.cancel(true)
        joystickAutoStartExecutor.shutdownNow()
        if (::joystickAutoStartCoordinator.isInitialized) {
            joystickAutoStartCoordinator.stop()
        }
        apiServer.stop()
        if (::walkCandidateReceiver.isInitialized) {
            walkCandidateReceiver.reset("service stopped")
        }
        runtimeFacade.shutdown()
        super.onDestroy()
    }

    private fun syncJoystickAutoStart() {
        runCatching { joystickAutoStartCoordinator.sync() }
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
        const val ACTION_SYNC_RUNTIME_CONFIG =
            "dev.pogoroot.automation.action.SYNC_RUNTIME_CONFIG"
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
            context.startForegroundService(
                Intent(context, HeadlessAutomationService::class.java)
                    .setAction(ACTION_ENABLE)
                    .putExtra(EXTRA_AUTO_CATCH, autoCatch)
                    .putExtra(EXTRA_AUTO_SPIN, autoSpin)
                    .putExtra(EXTRA_AUTO_ENCOUNTER, autoEncounter),
            )
        }

        fun disable(context: Context) {
            context.startService(
                Intent(context, HeadlessAutomationService::class.java)
                    .setAction(ACTION_DISABLE),
            )
        }

        fun requestRuntimeConfigSync(context: Context) {
            context.startService(
                Intent(context, HeadlessAutomationService::class.java)
                    .setAction(ACTION_SYNC_RUNTIME_CONFIG),
            )
        }
    }
}
