package dev.pogoroot.automation.headless

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import dev.pogoroot.automation.root.ProcessRootBinaryShell
import dev.pogoroot.automation.root.RootBinaryShell
import dev.pogoroot.automation.root.RootShell
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

data class ScreenPoint(
    val x: Int,
    val y: Int,
)

enum class GameScreenState {
    ENCOUNTER,
    POKESTOP_DETAIL,
    OVERWORLD,
    UNKNOWN,
}

data class ScreenAnalysis(
    val state: GameScreenState,
    val pokestopCandidate: ScreenPoint? = null,
    val encounterConfidence: Double = 0.0,
    val pokestopConfidence: Double = 0.0,
)

class RootScreenCapture(
    private val shell: RootBinaryShell = ProcessRootBinaryShell(),
) {
    fun capture(): Result<Bitmap> = runCatching {
        val result = shell.executeBytes("screencap -p", timeoutMillis = 5_000L)
        check(result.isSuccess) {
            "screencap failed: ${result.stderr.ifBlank { "exit=${result.exitCode}" }}"
        }
        check(result.stdout.isNotEmpty()) { "screencap returned no bytes" }
        BitmapFactory.decodeByteArray(result.stdout, 0, result.stdout.size)
            ?: error("cannot decode screencap PNG")
    }
}

class GameScreenAnalyzer {
    fun analyze(bitmap: Bitmap): ScreenAnalysis {
        val encounterConfidence = encounterConfidence(bitmap)
        if (encounterConfidence >= 0.62) {
            return ScreenAnalysis(
                state = GameScreenState.ENCOUNTER,
                encounterConfidence = encounterConfidence,
            )
        }

        val pokestopDetailConfidence = pokestopDetailConfidence(bitmap)
        if (pokestopDetailConfidence >= 0.58) {
            return ScreenAnalysis(
                state = GameScreenState.POKESTOP_DETAIL,
                pokestopConfidence = pokestopDetailConfidence,
            )
        }

        val candidate = findPokestopCandidate(bitmap)
        return ScreenAnalysis(
            state = if (candidate != null) GameScreenState.OVERWORLD else GameScreenState.UNKNOWN,
            pokestopCandidate = candidate,
            encounterConfidence = encounterConfidence,
            pokestopConfidence = pokestopDetailConfidence,
        )
    }

    private fun encounterConfidence(bitmap: Bitmap): Double {
        val left = (bitmap.width * 0.39).toInt()
        val right = (bitmap.width * 0.61).toInt()
        val top = (bitmap.height * 0.72).toInt()
        val bottom = (bitmap.height * 0.96).toInt()
        val step = sampleStep(bitmap)

        var samples = 0
        var brightNeutral = 0
        var ballColor = 0
        for (y in top until bottom step step) {
            for (x in left until right step step) {
                val pixel = bitmap.getPixel(x, y)
                val r = Color.red(pixel)
                val g = Color.green(pixel)
                val b = Color.blue(pixel)
                val hi = max(r, max(g, b))
                val lo = min(r, min(g, b))
                samples++
                if (hi >= 185 && hi - lo <= 55) brightNeutral++
                if (isBallColor(r, g, b)) ballColor++
            }
        }
        if (samples == 0) return 0.0

        val whiteRatio = brightNeutral.toDouble() / samples
        val colorRatio = ballColor.toDouble() / samples
        val whiteScore = (whiteRatio / 0.12).coerceIn(0.0, 1.0)
        val colorScore = (colorRatio / 0.08).coerceIn(0.0, 1.0)
        return whiteScore * 0.58 + colorScore * 0.42
    }

    private fun pokestopDetailConfidence(bitmap: Bitmap): Double {
        val left = (bitmap.width * 0.22).toInt()
        val right = (bitmap.width * 0.78).toInt()
        val top = (bitmap.height * 0.16).toInt()
        val bottom = (bitmap.height * 0.70).toInt()
        val step = sampleStep(bitmap)

        var samples = 0
        var blue = 0
        for (y in top until bottom step step) {
            for (x in left until right step step) {
                val pixel = bitmap.getPixel(x, y)
                if (isPokestopBlue(Color.red(pixel), Color.green(pixel), Color.blue(pixel))) blue++
                samples++
            }
        }
        if (samples == 0) return 0.0
        val ratio = blue.toDouble() / samples
        return (ratio / 0.075).coerceIn(0.0, 1.0)
    }

    private fun findPokestopCandidate(bitmap: Bitmap): ScreenPoint? {
        val cellSize = max(24, min(bitmap.width, bitmap.height) / 18)
        val cols = (bitmap.width + cellSize - 1) / cellSize
        val rows = (bitmap.height + cellSize - 1) / cellSize
        val counts = IntArray(cols * rows)
        val sumsX = LongArray(cols * rows)
        val sumsY = LongArray(cols * rows)
        val step = sampleStep(bitmap)

        val minY = (bitmap.height * 0.18).toInt()
        val maxY = (bitmap.height * 0.76).toInt()
        val minX = (bitmap.width * 0.06).toInt()
        val maxX = (bitmap.width * 0.94).toInt()

        for (y in minY until maxY step step) {
            for (x in minX until maxX step step) {
                val pixel = bitmap.getPixel(x, y)
                if (!isPokestopBlue(Color.red(pixel), Color.green(pixel), Color.blue(pixel))) continue
                val col = x / cellSize
                val row = y / cellSize
                val index = row * cols + col
                counts[index]++
                sumsX[index] += x.toLong()
                sumsY[index] += y.toLong()
            }
        }

        val centerX = bitmap.width / 2.0
        val centerY = bitmap.height * 0.52
        var bestIndex = -1
        var bestScore = Double.NEGATIVE_INFINITY
        for (index in counts.indices) {
            val count = counts[index]
            if (count < 7) continue
            val x = sumsX[index].toDouble() / count
            val y = sumsY[index].toDouble() / count
            val dx = abs(x - centerX) / bitmap.width
            val dy = abs(y - centerY) / bitmap.height
            val distancePenalty = dx * 1.8 + dy * 1.2
            val score = count.toDouble() - distancePenalty * 18.0
            if (score > bestScore) {
                bestScore = score
                bestIndex = index
            }
        }

        if (bestIndex < 0) return null
        val count = counts[bestIndex]
        return ScreenPoint(
            x = (sumsX[bestIndex] / count).toInt(),
            y = (sumsY[bestIndex] / count).toInt(),
        )
    }

    private fun sampleStep(bitmap: Bitmap): Int = max(3, min(bitmap.width, bitmap.height) / 240)

    private fun isBallColor(r: Int, g: Int, b: Int): Boolean {
        val red = r >= 150 && r >= g * 1.25 && r >= b * 1.20
        val blue = b >= 135 && b >= r * 1.18 && b >= g * 1.05
        val yellow = r >= 155 && g >= 130 && b <= 120
        return red || blue || yellow
    }

    private fun isPokestopBlue(r: Int, g: Int, b: Int): Boolean {
        val blue = b >= 135 && b >= r * 1.20 && b >= g * 1.04 && g >= 65
        val cyan = b >= 120 && g >= 120 && r <= 145 && b + g >= r * 2 + 80
        return blue || cyan
    }
}

class RootUiDriver(
    private val shell: RootShell,
) {
    fun isPokemonGoForeground(): Boolean {
        val result = shell.execute(
            "dumpsys activity activities | grep -m 1 -E 'mResumedActivity|topResumedActivity'",
            timeoutMillis = 2_000L,
        )
        if (!result.isSuccess) return false
        return result.stdout.contains("com.nianticlabs.pokemongo") ||
            result.stdout.contains("com.nianticlabs.pokemongo.ares")
    }

    fun tap(point: ScreenPoint): Boolean = shell.execute(
        "input tap ${point.x} ${point.y}",
        timeoutMillis = 2_000L,
    ).isSuccess

    fun tapNormalized(width: Int, height: Int, x: Double, y: Double): Boolean = tap(
        ScreenPoint(
            x = (width * x.coerceIn(0.0, 1.0)).toInt(),
            y = (height * y.coerceIn(0.0, 1.0)).toInt(),
        ),
    )

    fun swipeNormalized(
        width: Int,
        height: Int,
        fromX: Double,
        fromY: Double,
        toX: Double,
        toY: Double,
        durationMs: Int,
    ): Boolean {
        val x1 = (width * fromX.coerceIn(0.0, 1.0)).toInt()
        val y1 = (height * fromY.coerceIn(0.0, 1.0)).toInt()
        val x2 = (width * toX.coerceIn(0.0, 1.0)).toInt()
        val y2 = (height * toY.coerceIn(0.0, 1.0)).toInt()
        return shell.execute(
            "input swipe $x1 $y1 $x2 $y2 ${durationMs.coerceAtLeast(1)}",
            timeoutMillis = 3_000L,
        ).isSuccess
    }
}
