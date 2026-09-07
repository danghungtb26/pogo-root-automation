package dev.pogoroot.automation.headless

import android.graphics.Bitmap
import dev.pogoroot.automation.root.ProcessRootShell
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference

data class HeadlessAutomationStatus(
    val running: Boolean = false,
    val enabled: Boolean = false,
    val pokemonGoForeground: Boolean = false,
    val screenState: GameScreenState = GameScreenState.UNKNOWN,
    val lastAction: String? = null,
    val lastError: String? = null,
    val framesAnalyzed: Long = 0,
    val catchAttempts: Long = 0,
    val spinAttempts: Long = 0,
    val encounterSweepTaps: Long = 0,
    val screenWidth: Int? = null,
    val screenHeight: Int? = null,
    val structuredRuntime: Boolean = false,
    val runtimeSessionId: String? = null,
    val runtimeStrongIdentityVerified: Boolean = false,
    val runtimeLifecycle: String? = null,
    val runtimeSuspended: Boolean = false,
    val observationSeq: Long? = null,
    val updatedAtEpochMs: Long = System.currentTimeMillis(),
)

class HeadlessAutomationEngine(
    private val configRepository: AutomationConfigRepository,
    private val screenCapture: RootScreenCapture = RootScreenCapture(),
    private val analyzer: GameScreenAnalyzer = GameScreenAnalyzer(),
    private val uiDriver: RootUiDriver = RootUiDriver(ProcessRootShell()),
    private val eventSink: AutomationEventSink = AutomationEventSink { },
    private val structuredController: StructuredAutomationController? = null,
) {
    private val executor = Executors.newSingleThreadExecutor()
    private val loopActive = AtomicBoolean(false)
    private val status = AtomicReference(HeadlessAutomationStatus())
    private val frames = AtomicLong(0)
    private val catches = AtomicLong(0)
    private val spins = AtomicLong(0)
    private val sweeps = AtomicLong(0)
    private var sweepIndex = 0
    private var lastActionAt = 0L
    private var berryAppliedForCurrentEncounter = false

    fun start() {
        if (!loopActive.compareAndSet(false, true)) return
        status.updateAndGet { it.copy(running = true, updatedAtEpochMs = System.currentTimeMillis()) }
        executor.execute { runLoop() }
    }

    fun stop() {
        loopActive.set(false)
        status.updateAndGet { it.copy(running = false, updatedAtEpochMs = System.currentTimeMillis()) }
    }

    fun shutdown() {
        loopActive.set(false)
        executor.shutdownNow()
    }

    fun snapshot(): HeadlessAutomationStatus = status.get().copy(
        framesAnalyzed = frames.get(),
        catchAttempts = catches.get(),
        spinAttempts = spins.get(),
        encounterSweepTaps = sweeps.get(),
        updatedAtEpochMs = System.currentTimeMillis(),
    )

    fun manualCatch(): Result<Unit> {
        return withCurrentScreen { bitmap, config ->
            if (!performCatch(bitmap, config)) error("root catch swipe failed")
            publish(AutomationEventType.CATCH_THROWN, "Catch throw sent")
        }
    }

    fun manualSpin(): Result<Unit> {
        return withCurrentScreen { bitmap, config ->
            if (!performSpin(bitmap, config)) error("root spin swipe failed")
            publish(AutomationEventType.SPUN, "PokéStop spun")
        }
    }

    private fun runLoop() {
        var activeMode: AutomationRuntimeMode? = null
        while (loopActive.get()) {
            val config = configRepository.read()
            if (activeMode != config.runtimeMode) {
                // Disconnect structured state before switching to screen mode;
                // the next structured tick will establish a fresh session.
                structuredController?.stop()
                if (config.runtimeMode == AutomationRuntimeMode.SCREEN) {
                    clearStructuredStatus()
                } else {
                    clearScreenStatus()
                }
                activeMode = config.runtimeMode
            }

            when (config.runtimeMode) {
                AutomationRuntimeMode.SCREEN -> runScreenIteration(config)
                AutomationRuntimeMode.STRUCTURED -> runStructuredIteration(config)
            }
        }
        structuredController?.stop()
        status.updateAndGet { it.copy(running = false, updatedAtEpochMs = System.currentTimeMillis()) }
    }

    private fun runScreenIteration(config: HeadlessAutomationConfig) {
        if (!config.enabled) {
            status.updateAndGet {
                it.copy(running = true, enabled = false, lastAction = "idle", updatedAtEpochMs = System.currentTimeMillis())
            }
            sleepInterruptibly(700L)
            return
        }

        val foreground = runCatching { uiDriver.isPokemonGoForeground() }.getOrDefault(false)
        if (!foreground) {
            berryAppliedForCurrentEncounter = false
            status.updateAndGet {
                it.copy(
                    running = true,
                    enabled = true,
                    pokemonGoForeground = false,
                    screenState = GameScreenState.UNKNOWN,
                    lastAction = "waiting-for-pokemon-go",
                    updatedAtEpochMs = System.currentTimeMillis(),
                )
            }
            sleepInterruptibly(1_000L)
            return
        }

        val captureResult = screenCapture.capture()
        if (captureResult.isFailure) {
            val error = captureResult.exceptionOrNull()
            recordError("screencap: ${error?.message ?: error?.javaClass?.simpleName ?: "unknown"}")
            sleepInterruptibly(config.loopIntervalMs)
            return
        }
        val bitmap = captureResult.getOrThrow()

        try {
            frames.incrementAndGet()
            val analysis = analyzer.analyze(bitmap)
            if (analysis.state != GameScreenState.ENCOUNTER) {
                berryAppliedForCurrentEncounter = false
            }
            status.updateAndGet {
                it.copy(
                    running = true,
                    enabled = true,
                    pokemonGoForeground = true,
                    screenState = analysis.state,
                    screenWidth = bitmap.width,
                    screenHeight = bitmap.height,
                    lastError = null,
                    updatedAtEpochMs = System.currentTimeMillis(),
                )
            }

            when {
                config.autoCatch && analysis.state == GameScreenState.ENCOUNTER -> {
                    if (config.berryMode != BerryMode.NONE && !berryAppliedForCurrentEncounter) {
                        if (performBerry(bitmap, config.berryMode)) {
                            berryAppliedForCurrentEncounter = true
                            markAction("berry-${config.berryMode.name.lowercase()}")
                            publish(AutomationEventType.BERRY_USED, "${berryLabel(config.berryMode)} used")
                            sleepInterruptibly(550L)
                        }
                    }
                    if (actionReady(config) && performCatch(bitmap, config)) {
                        catches.incrementAndGet()
                        markAction("catch-throw")
                        publish(AutomationEventType.CATCH_THROWN, "Catch throw sent")
                        sleepInterruptibly(config.catchResultDelayMs)
                    }
                }

                config.autoSpin && analysis.state == GameScreenState.POKESTOP_DETAIL -> {
                    if (actionReady(config) && performSpin(bitmap, config)) {
                        spins.incrementAndGet()
                        markAction("spin-pokestop")
                        publish(AutomationEventType.SPUN, "PokéStop spun")
                        sleepInterruptibly(config.spinResultDelayMs)
                    }
                }

                config.autoSpin && analysis.pokestopCandidate != null -> {
                    if (actionReady(config) && uiDriver.tap(analysis.pokestopCandidate)) {
                        markAction("open-pokestop")
                        sleepInterruptibly(config.spinOpenDelayMs)
                        spinIfDetail(config)
                    }
                }

                config.autoCatch && config.encounterSweep -> {
                    if (actionReady(config) && tapEncounterSweep(bitmap)) {
                        sweeps.incrementAndGet()
                        markAction("encounter-sweep")
                        sleepInterruptibly(650L)
                        catchIfEncounter(config)
                    }
                }
            }
        } catch (error: Throwable) {
            recordError(error.message ?: error::class.java.simpleName)
        } finally {
            bitmap.recycle()
        }

        sleepInterruptibly(config.loopIntervalMs)
    }

    private fun runStructuredIteration(config: HeadlessAutomationConfig) {
        status.updateAndGet {
            it.copy(
                structuredRuntime = true,
                enabled = config.enabled,
                updatedAtEpochMs = System.currentTimeMillis(),
            )
        }
        val controller = structuredController
        if (controller == null) {
            recordError("structured runtime controller is unavailable")
            sleepInterruptibly(config.loopIntervalMs)
            return
        }
        if (!config.enabled) {
            controller.stop()
            status.updateAndGet {
                it.copy(
                    running = true,
                    enabled = false,
                    structuredRuntime = true,
                    lastAction = "idle",
                    updatedAtEpochMs = System.currentTimeMillis(),
                )
            }
            sleepInterruptibly(700L)
            return
        }

        controller.tick(config)
            .onSuccess { tick ->
                status.updateAndGet {
                    it.copy(
                        running = true,
                        enabled = true,
                        structuredRuntime = true,
                        pokemonGoForeground = tick.runtimeSessionId != null,
                        screenState = tick.lifecycleState.toScreenState(),
                        runtimeSessionId = tick.runtimeSessionId,
                        runtimeStrongIdentityVerified = tick.strongIdentityVerified,
                        runtimeLifecycle = tick.lifecycleState.name,
                        runtimeSuspended = tick.suspended,
                        observationSeq = tick.observationSeq,
                        lastAction = tick.lastAction,
                        lastError = tick.lastError,
                        updatedAtEpochMs = System.currentTimeMillis(),
                    )
                }
            }
            .onFailure { error ->
                recordError(
                    "runtime bridge: ${error.message ?: error::class.java.simpleName}",
                )
            }
        sleepInterruptibly(config.loopIntervalMs)
    }

    private fun clearStructuredStatus() {
        status.updateAndGet {
            it.copy(
                structuredRuntime = false,
                runtimeSessionId = null,
                runtimeStrongIdentityVerified = false,
                runtimeLifecycle = null,
                runtimeSuspended = false,
                observationSeq = null,
            )
        }
    }

    private fun clearScreenStatus() {
        status.updateAndGet {
            it.copy(
                pokemonGoForeground = false,
                screenState = GameScreenState.UNKNOWN,
                screenWidth = null,
                screenHeight = null,
                runtimeSessionId = null,
                runtimeStrongIdentityVerified = false,
                runtimeLifecycle = null,
                runtimeSuspended = false,
                observationSeq = null,
            )
        }
    }

    private fun catchIfEncounter(config: HeadlessAutomationConfig) {
        val next = screenCapture.capture().getOrNull() ?: return
        try {
            val analysis = analyzer.analyze(next)
            if (analysis.state == GameScreenState.ENCOUNTER) {
                if (config.berryMode != BerryMode.NONE && !berryAppliedForCurrentEncounter && performBerry(next, config.berryMode)) {
                    berryAppliedForCurrentEncounter = true
                    publish(AutomationEventType.BERRY_USED, "${berryLabel(config.berryMode)} used")
                    sleepInterruptibly(550L)
                }
                if (performCatch(next, config)) {
                    catches.incrementAndGet()
                    markAction("catch-throw")
                    publish(AutomationEventType.CATCH_THROWN, "Catch throw sent")
                    sleepInterruptibly(config.catchResultDelayMs)
                }
            }
        } finally {
            next.recycle()
        }
    }

    private fun spinIfDetail(config: HeadlessAutomationConfig) {
        val next = screenCapture.capture().getOrNull() ?: return
        try {
            val analysis = analyzer.analyze(next)
            if (analysis.state == GameScreenState.POKESTOP_DETAIL && performSpin(next, config)) {
                spins.incrementAndGet()
                markAction("spin-pokestop")
                publish(AutomationEventType.SPUN, "PokéStop spun")
                sleepInterruptibly(config.spinResultDelayMs)
            }
        } finally {
            next.recycle()
        }
    }

    private fun performBerry(bitmap: Bitmap, mode: BerryMode): Boolean {
        if (mode == BerryMode.NONE) return true
        if (!uiDriver.tapNormalized(bitmap.width, bitmap.height, 0.17, 0.88)) return false
        sleepInterruptibly(350L)

        val slotX = when (mode) {
            BerryMode.RAZZ -> 0.22
            BerryMode.NANAB -> 0.36
            BerryMode.PINAP -> 0.50
            BerryMode.GOLDEN_RAZZ -> 0.66
            BerryMode.SILVER_PINAP -> 0.80
            BerryMode.NONE -> return true
        }
        if (!uiDriver.tapNormalized(bitmap.width, bitmap.height, slotX, 0.84)) return false
        sleepInterruptibly(250L)
        return uiDriver.tapNormalized(bitmap.width, bitmap.height, 0.50, 0.46)
    }

    private fun performCatch(bitmap: Bitmap, config: HeadlessAutomationConfig): Boolean {
        val offset = when ((catches.get() % 3L).toInt()) {
            0 -> 0.0
            1 -> -0.035
            else -> 0.035
        }
        return uiDriver.swipeNormalized(
            width = bitmap.width,
            height = bitmap.height,
            fromX = 0.50,
            fromY = 0.865,
            toX = 0.50 + offset,
            toY = 0.315,
            durationMs = config.catchThrowDurationMs,
        )
    }

    private fun performSpin(bitmap: Bitmap, config: HeadlessAutomationConfig): Boolean {
        val first = uiDriver.swipeNormalized(
            width = bitmap.width,
            height = bitmap.height,
            fromX = 0.78,
            fromY = 0.50,
            toX = 0.22,
            toY = 0.50,
            durationMs = config.spinSwipeDurationMs,
        )
        if (!first) return false
        sleepInterruptibly(650L)
        uiDriver.tapNormalized(bitmap.width, bitmap.height, 0.50, 0.92)
        return true
    }

    private fun tapEncounterSweep(bitmap: Bitmap): Boolean {
        val point = ENCOUNTER_SWEEP_POINTS[sweepIndex % ENCOUNTER_SWEEP_POINTS.size]
        sweepIndex = (sweepIndex + 1) % ENCOUNTER_SWEEP_POINTS.size
        return uiDriver.tapNormalized(bitmap.width, bitmap.height, point.first, point.second)
    }

    private fun actionReady(config: HeadlessAutomationConfig): Boolean =
        System.currentTimeMillis() - lastActionAt >= config.actionCooldownMs

    private fun markAction(action: String) {
        lastActionAt = System.currentTimeMillis()
        status.updateAndGet {
            it.copy(lastAction = action, lastError = null, updatedAtEpochMs = System.currentTimeMillis())
        }
    }

    private fun recordError(message: String) {
        status.updateAndGet { it.copy(lastError = message, updatedAtEpochMs = System.currentTimeMillis()) }
        publish(AutomationEventType.ERROR, message)
    }

    private fun publish(type: AutomationEventType, message: String) {
        eventSink.publish(AutomationEvent(type, message))
    }

    private fun dev.pogoroot.automation.core.model.GameLifecycleState.toScreenState(): GameScreenState = when (this) {
        dev.pogoroot.automation.core.model.GameLifecycleState.ENCOUNTER -> GameScreenState.ENCOUNTER
        dev.pogoroot.automation.core.model.GameLifecycleState.OVERWORLD -> GameScreenState.OVERWORLD
        else -> GameScreenState.UNKNOWN
    }

    private fun berryLabel(mode: BerryMode): String = when (mode) {
        BerryMode.NONE -> "Berry"
        BerryMode.RAZZ -> "Razz Berry"
        BerryMode.NANAB -> "Nanab Berry"
        BerryMode.PINAP -> "Pinap Berry"
        BerryMode.GOLDEN_RAZZ -> "Golden Razz Berry"
        BerryMode.SILVER_PINAP -> "Silver Pinap Berry"
    }

    private fun withCurrentScreen(block: (Bitmap, HeadlessAutomationConfig) -> Unit): Result<Unit> = runCatching {
        val config = configRepository.read()
        check(config.runtimeMode == AutomationRuntimeMode.SCREEN) {
            "manual screen actions are disabled for structured runtime"
        }
        check(uiDriver.isPokemonGoForeground()) { "Pokemon GO is not foreground" }
        val bitmap = screenCapture.capture().getOrThrow()
        try {
            block(bitmap, config)
        } finally {
            bitmap.recycle()
        }
    }

    private fun sleepInterruptibly(durationMs: Long) {
        if (durationMs <= 0L) return
        try {
            Thread.sleep(durationMs)
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
        }
    }

    companion object {
        private val ENCOUNTER_SWEEP_POINTS = listOf(
            0.50 to 0.43,
            0.42 to 0.47,
            0.58 to 0.47,
            0.36 to 0.42,
            0.64 to 0.42,
            0.45 to 0.36,
            0.55 to 0.36,
        )
    }
}
