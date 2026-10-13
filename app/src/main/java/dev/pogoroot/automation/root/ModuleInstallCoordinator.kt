package dev.pogoroot.automation.root

import android.os.Handler
import android.os.Looper
import java.util.concurrent.CopyOnWriteArraySet
import java.util.concurrent.Executors
import java.util.concurrent.Future
import java.util.concurrent.atomic.AtomicBoolean

class ModuleInstallCoordinator(
    private val installer: BundledModuleInstaller,
    private val stateRepository: ModuleInstallStateRepository,
) {
    private val executor = Executors.newSingleThreadExecutor()
    private val mainHandler = Handler(Looper.getMainLooper())
    private val observers = CopyOnWriteArraySet<(ModuleInstallSnapshot) -> Unit>()
    private val foreground = AtomicBoolean(false)
    private val automaticAttemptConsumed = AtomicBoolean(false)
    private val operationActive = AtomicBoolean(false)
    @Volatile private var operation: Future<*>? = null

    fun snapshot(): ModuleInstallSnapshot = stateRepository.read()

    fun bundledInfo(): Result<BundledModuleInfo> = installer.bundledInfo()

    fun observe(observer: (ModuleInstallSnapshot) -> Unit) {
        observers.add(observer)
        mainHandler.post { observer(snapshot()) }
    }

    fun removeObserver(observer: (ModuleInstallSnapshot) -> Unit) {
        observers.remove(observer)
    }

    fun onForegroundEntry() {
        foreground.set(true)
        if (snapshot().state in AUTO_PREFLIGHT_STATES &&
            automaticAttemptConsumed.compareAndSet(false, true)
        ) {
            startOperation(allowReplacement = false)
        } else {
            publish(snapshot())
        }
    }

    fun onForegroundExit() {
        foreground.set(false)
        automaticAttemptConsumed.set(false)
    }

    fun retryFromUser() {
        check(foreground.get()) { "Module retry requires a foreground Activity." }
        startOperation(allowReplacement = true)
    }

    fun rebootFromUser(onResult: (RootCommandResult) -> Unit) {
        check(foreground.get()) { "Reboot requires a foreground Activity." }
        if (!operationActive.compareAndSet(false, true)) return
        operation = executor.submit {
            val result = installer.reboot()
            operationActive.set(false)
            mainHandler.post { onResult(result) }
        }
    }

    private fun startOperation(allowReplacement: Boolean) {
        if (!foreground.get() || !operationActive.compareAndSet(false, true)) return
        operation = executor.submit {
            val result = runCatching { installer.ensureInstalled(allowReplacement) }
                .getOrElse { error ->
                    stateRepository.transition(
                        state = ModuleInstallState.FAILED,
                        errorCode = "install_coordinator_failure",
                        message = error.message ?: error::class.java.simpleName,
                    )
                }
            operationActive.set(false)
            publish(result)
        }
    }

    private fun publish(snapshot: ModuleInstallSnapshot) {
        mainHandler.post {
            observers.forEach { observer -> runCatching { observer(snapshot) } }
        }
    }

    private companion object {
        val AUTO_PREFLIGHT_STATES = setOf(
            ModuleInstallState.NOT_CHECKED,
            ModuleInstallState.INSTALLED,
            ModuleInstallState.REBOOT_REQUIRED,
        )
    }
}
