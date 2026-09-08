package dev.pogoroot.automation

import android.app.Application
import dev.pogoroot.automation.location.RootMockLocationProvider

class AutomationApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        // A killed process may not receive Service.onDestroy(), so remove
        // test providers left behind by an earlier joystick session before
        // any new controller/service is started.
        RootMockLocationProvider(this).stop()
    }
}
