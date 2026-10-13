package dev.pogoroot.automation

import android.app.Application
import dev.pogoroot.automation.engine.AutomationRunState
import dev.pogoroot.automation.engine.CatchSpinArmState
import dev.pogoroot.automation.root.BundledModuleInstaller
import dev.pogoroot.automation.root.ModuleInstallCoordinator
import dev.pogoroot.automation.root.ModuleInstallStateRepository

class AutomationApplication : Application() {
    lateinit var moduleInstallStateRepository: ModuleInstallStateRepository
        private set
    lateinit var moduleInstallCoordinator: ModuleInstallCoordinator
        private set
    val catchSpinArmState = CatchSpinArmState

    override fun onCreate() {
        super.onCreate()
        // Clear live execution first. The service restores persisted intent,
        // then waits for a fresh verified game session before enabling modules.
        AutomationRunState.resetForProcessStart()
        catchSpinArmState.disarm("controller_process_start")
        moduleInstallStateRepository = ModuleInstallStateRepository(this)
        moduleInstallCoordinator = ModuleInstallCoordinator(
            installer = BundledModuleInstaller(this, moduleInstallStateRepository),
            stateRepository = moduleInstallStateRepository,
        )
    }
}
