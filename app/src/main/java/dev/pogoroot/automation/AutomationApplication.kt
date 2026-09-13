package dev.pogoroot.automation

import android.app.Application
import dev.pogoroot.automation.engine.AutomationRunState
import dev.pogoroot.automation.location.RootMockLocationProvider

class AutomationApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        // Clear live execution first. The service restores persisted intent,
        // then waits for a fresh verified game session before enabling modules.
        AutomationRunState.resetForProcessStart()
        // A killed process may not receive Service.onDestroy(), so remove
        // test providers left behind by an earlier joystick session before
        // any new controller/service is started.
        RootMockLocationProvider(this).stop()
    }
}
