package dev.pogoroot.automation.overlay

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.os.Build
import android.content.Intent
import dev.pogoroot.automation.MainActivity

internal const val JOYSTICK_CHANNEL_ID = "pogo_joystick"
internal const val JOYSTICK_NOTIFICATION_ID = 4107

internal fun JoystickOverlayService.createNotificationChannel() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
    val manager = getSystemService(NotificationManager::class.java)
    manager.createNotificationChannel(
        NotificationChannel(
            JOYSTICK_CHANNEL_ID,
            "Built-in joystick",
            NotificationManager.IMPORTANCE_LOW,
        ),
    )
}

internal fun JoystickOverlayService.buildNotification(): Notification {
    val launchIntent = Intent(this, MainActivity::class.java)
    val pendingIntent = PendingIntent.getActivity(
        this,
        0,
        launchIntent,
        PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
    )
    val builder = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        Notification.Builder(this, JOYSTICK_CHANNEL_ID)
    } else {
        @Suppress("DEPRECATION")
        Notification.Builder(this)
    }
    return builder
        .setSmallIcon(android.R.drawable.ic_menu_mylocation)
        .setContentTitle("PoGo built-in joystick")
        .setContentText("Overlay appears while Pokémon GO is on screen")
        .setContentIntent(pendingIntent)
        .setOngoing(true)
        .build()
}
