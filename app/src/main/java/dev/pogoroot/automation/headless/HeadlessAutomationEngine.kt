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
    val berryAttempts: Long = 0,
    val encounterSweepTaps: Long = 0,
    val screenWidth: Int? = null,
    val screenHeight: Int? = null,
    val updatedAtEpochMs: Long = System.currentTimeMillis(),
)

class HeadlessAutomationEngine(
    private val configRepository: AutomationConfigRepository,
    private val screenCapture: RootScreenCapture = RootScreenCapture(),
    private val analyzer: GameScreenAnalyzer = GameScreenAnalyzer(),
    private val uiDriver: RootUiDriver = RootUiDriver(ProcessRootShell()),
    private val notifier: AutomationEventNotifier? = null,
) {
    private val executor = Executors.newSingleThreadExecutor()
    private val loopActive = AtomicBoolean(false)
    private val status = AtomicReference(HeadlessAutomationStatus())
    private val frames = AtomicLong(0)
    private val catches = AtomicLong(0)
    private val spins = AtomicLong(0)
    private val berries = AtomicLong(0)
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
        berryAttempts = berries.get(),
        encounterSweepTaps = sweeps.get(),
        updatedAtEpochMs = System.currentTimeMillis(),
    )

    fun manualCatch(): Result<Unit> = withCurrentScreen { bitmap, config ->
        if (!performCatch(bitmap, config)) error("root catch swipe failed")
        notifier?.show(AutomationEvent.CATCH_THROW)
    }

    fun manualSpin(): Result<Unit> = withCurrentScreen { bitmap, config ->
        if (!performSpin(bitmap, config)) error("root spin swipe failed")
        notifier?.show(AutomationEvent.POKESTOP_SPUN)
    }

    fun manualBerry(): Result<Unit> = withCurrentScreen { bitmap, _ ->
        val analysis = analyzer.analyze(bitmap)
        check(analysis.state == GameScreenState.ENCOUNTER) { "Berry can only be used during an encounter" }
        if (!performBerry(bitmap)) error("root berry input failed")
        berries.incrementAndGet()
        notifier?.show(AutomationEvent.BERRY_USED)
    }

    private fun runLoop() {
        while (loopActive.get()) {
            val config = configRepository.read()
            if (!config.enabled) {
                berryAppliedForCurrentEncounter = false
                status.updateAndGet {
                    it.copy(
                        running = true,
                        enabled = false,
                        lastAction = "idle",
                        updatedAtEpochMs = System.currentTimeMillis(),
                    )
                }
                sleepInterruptibly(700L)
                continue
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
                continue
            }

            val captureResult = screenCapture.capture()
            if (captureResult.isFailure) {
                val error = captureResult.exceptionOrNull()
                recordError("screencap: ${error?.message ?: error?.javaClass?.simpleName ?: "unknown"}")
                sleepInterruptibly(config.loopIntervalMs)
                continue
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
                    analysis.state == GameScreenState.ENCOUNTER && config.autoBerry && !berryAppliedForCurrentEncounter -> {
                        if (actionReady(config) && performBerry(bitmap)) {
                            berryAppliedForCurrentEncounter = true
                            berries.incrementAndGet()
                            markAction("berry")
                            notifier?.show(AutomationEvent.BERRY_USED)
                            sleepInterruptibly(650L)
                        }
                    }

                    config.autoCatch && analysis.state == GameScreenState.ENCOUNTER -> {
                        if (actionReady(config) && performCatch(bitmap, config)) {
                            catches.incrementAndGet()
                            markAction("catch-throw")
                            notifier?.show(AutomationEvent.CATCH_THROW)
                            sleepInterruptibly(config.catchResultDelayMs)
                            inspectCatchResult()
                        }
                    }

                    config.autoSpin && analysis.state == GameScreenState.POKESTOP_DETAIL -> {
                        if (actionReady(config) && performSpin(bitmap, config)) {
                            spins.incrementAndGet()
                            markAction("spin-pokestop")
                            notifier?.show(AutomationEvent.POKESTOP_SPUN)
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
        status.updateAndGet { it.copy(running = false, updatedAtEpochMs = System.currentTimeMillis()) }
    }

    private fun catchIfEncounter(config: HeadlessAutomationConfig) {
        val next = screenCapture.capture().getOrNull() ?: return
        try {
            val analysis = analyzer.analyze(next)
            if (analysis.state != GameScreenState.ENCOUNTER) return

            if (config.autoBerry && !berryAppliedForCurrentEncounter && performBerry(next)) {
                berryAppliedForCurrentEncounter = true
                berries.incrementAndGet()
                notifier?.show(AutomationEvent.BERRY_USED)
                sleepInterruptibly(650L)
            }

            if (performCatch(next, config)) {
                catches.incrementAndGet()
                markAction("catch-throw")
                notifier?.show(AutomationEvent.CATCH_THROW)
                sleepInterruptibly(config.catchResultDelayMs)
                inspectCatchResult()
            }
        } finally {
            next.recycle()
        }
    }

    private fun inspectCatchResult() {
        val next = screenCapture.capture().getOrNull() ?: return
        try {
            when (analyzer.analyze(next).state) {
                GameScreenState.ENCOUNTER -> notifier?.show(AutomationEvent.BROKE_FREE)
                GameScreenState.OVERWORLD -> {
                    berryAppliedForCurrentEncounter = false
                    notifier?.show(AutomationEvent.RUN_AWAY)
                }
                GameScreenState.UNKNOWN -> {
                    berryAppliedForCurrentEncounter = false
                    notifier?.show(AutomationEvent.CAUGHT)
                }
                GameScreenState.POKESTOP_DETAIL -> berryAppliedForCurrentEncounter = false
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
                notifier?.show(AutomationEvent.POKESTOP_SPUN)
                sleepInterruptibly(config.spinResultDelayMs)
            }
        } finally {
            next.recycle()
        }
    }

    private fun performBerry(bitmap: Bitmap): Boolean {
        val opened = uiDriver.tapNormalized(bitmap.width, bitmap.height, 0.17, 0.86)
        if (!opened) return false
        sleepInterruptibly(350L)

        // The current screen executor uses the first visible berry slot. Runtime mode can
        // replace this with an exact berry item id without changing the settings contract.
        val selected = uiDriver.tapNormalized(bitmap.width, bitmap.height, 0.22, 0.79)
        if (!selected) return false
        sleepInterruptibly(250L)

        return uiDriver.tapNormalized(bitmap.width, bitmap.height, 0.50, 0.44)
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
            it.copy(
                lastAction = action,
                lastError = null,
                updatedAtEpochMs = System.currentTimeMillis(),
            )
        }
    }

    private fun recordError(message: String) {
        status.updateAndGet {
            it.copy(
                lastError = message,
                updatedAtEpochMs = System.currentTimeMillis(),
            )
        }
    }

    private fun withCurrentScreen(block: (Bitmap, HeadlessAutomationConfig) -> Unit): Result<Unit> = runCatching {
        check(uiDriver.isPokemonGoForeground()) { "Pokemon GO is not foreground" }
        val bitmap = screenCapture.capture().getOrThrow()
        try {
            block(bitmap, configRepository.read())
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
