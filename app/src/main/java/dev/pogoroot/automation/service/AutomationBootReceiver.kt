package dev.pogoroot.automation.service

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import dev.pogoroot.automation.AutomationApplication

class AutomationBootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent?) {
        if (intent?.action != Intent.ACTION_BOOT_COMPLETED) return
        val app = context.applicationContext as? AutomationApplication ?: return
        if (!app.moduleInstallStateRepository.read().gameplayAllowed) return
        runCatching { HeadlessAutomationService.start(context) }
    }
}
