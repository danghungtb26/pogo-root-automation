package dev.pogoroot.automation

import android.app.Application
import dev.pogoroot.automation.engine.AutomationRunState
import dev.pogoroot.automation.location.RootMockLocationProvider

class AutomationApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        // A package update or process recreation must never inherit an active
        // automation master switch from the previous process instance.
        AutomationRunState.resetForProcessStart()
        // A killed process may not receive Service.onDestroy(), so remove
        // test providers left behind by an earlier joystick session before
        // any new controller/service is started.
        RootMockLocationProvider(this).stop()
    }
}
