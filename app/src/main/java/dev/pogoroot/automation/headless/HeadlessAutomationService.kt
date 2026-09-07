package dev.pogoroot.automation.headless

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.IBinder
import dev.pogoroot.automation.MainActivity
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

class HeadlessAutomationService : Service() {
    private lateinit var configRepository: AutomationConfigRepository
    private lateinit var engine: HeadlessAutomationEngine
    private lateinit var apiServer: AutomationControlServer
    private lateinit var maintenanceRunner: MaintenanceAutomationRunner
    private val maintenanceExecutor = Executors.newSingleThreadScheduledExecutor()

    override fun onCreate() {
        super.onCreate()
        configRepository = AutomationConfigRepository(this)
        val eventSink = ToastAutomationEventSink(this, configRepository)
        engine = HeadlessAutomationEngine(
            configRepository = configRepository,
            eventSink = eventSink,
        )
        maintenanceRunner = MaintenanceAutomationRunner(eventSink = eventSink)
        apiServer = AutomationControlServer(configRepository, engine)

        createNotificationChannel()
        startForeground(NOTIFICATION_ID, buildNotification())
        apiServer.start()
        engine.start()
        maintenanceExecutor.scheduleWithFixedDelay(
            ::runMaintenancePass,
            2L,
            2L,
            TimeUnit.SECONDS,
        )
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_ENABLE -> {
                configRepository.update { current ->
                    current.copy(
                        enabled = true,
                        autoCatch = intent.booleanExtraOrNull(EXTRA_AUTO_CATCH) ?: current.autoCatch,
                        autoSpin = intent.booleanExtraOrNull(EXTRA_AUTO_SPIN) ?: current.autoSpin,
                    )
                }
                engine.start()
            }

            ACTION_DISABLE -> {
                configRepository.update { it.copy(enabled = false) }
            }

            ACTION_STOP_SERVICE -> {
                configRepository.update { it.copy(enabled = false) }
                stopSelf()
            }
        }
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        maintenanceExecutor.shutdownNow()
        apiServer.stop()
        engine.shutdown()
        super.onDestroy()
    }

    private fun runMaintenancePass() {
        val config = configRepository.read()
        if (!config.enabled || (!config.autoDiscard && !config.autoTransfer)) return

        // Missing/stale bindings are a normal state while the game is starting or after a game update.
        // Do not spam a toast; the runtime/API status surface reports readiness separately.
        maintenanceRunner.runOnce(config)
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

        private const val CHANNEL_ID = "pogo_headless_automation"
        private const val NOTIFICATION_ID = 2102

        fun start(context: Context) {
            context.startForegroundService(
                Intent(context, HeadlessAutomationService::class.java),
            )
        }

        fun enable(context: Context, autoCatch: Boolean = true, autoSpin: Boolean = true) {
            context.startForegroundService(
                Intent(context, HeadlessAutomationService::class.java)
                    .setAction(ACTION_ENABLE)
                    .putExtra(EXTRA_AUTO_CATCH, autoCatch)
                    .putExtra(EXTRA_AUTO_SPIN, autoSpin),
            )
        }

        fun disable(context: Context) {
            context.startService(
                Intent(context, HeadlessAutomationService::class.java)
                    .setAction(ACTION_DISABLE),
            )
        }
    }
}
