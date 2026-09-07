package dev.pogoroot.automation.root

import java.util.concurrent.Callable
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

interface RootBinaryShell {
    fun executeBytes(command: String, timeoutMillis: Long = 5_000L): RootBinaryCommandResult
}

data class RootBinaryCommandResult(
    val exitCode: Int?,
    val stdout: ByteArray,
    val stderr: String,
    val timedOut: Boolean,
) {
    val isSuccess: Boolean
        get() = !timedOut && exitCode == 0
}

class ProcessRootBinaryShell : RootBinaryShell {
    override fun executeBytes(command: String, timeoutMillis: Long): RootBinaryCommandResult {
        val process = runCatching {
            ProcessBuilder("su", "-c", command).start()
        }.getOrElse { error ->
            return RootBinaryCommandResult(
                exitCode = null,
                stdout = byteArrayOf(),
                stderr = error.message ?: error::class.java.simpleName,
                timedOut = false,
            )
        }

        val executor = Executors.newFixedThreadPool(2)
        return try {
            val stdoutFuture = executor.submit(Callable { process.inputStream.use { it.readBytes() } })
            val stderrFuture = executor.submit(Callable { process.errorStream.bufferedReader().use { it.readText() } })

            val completed = process.waitFor(timeoutMillis, TimeUnit.MILLISECONDS)
            if (!completed) {
                process.destroyForcibly()
                RootBinaryCommandResult(
                    exitCode = null,
                    stdout = byteArrayOf(),
                    stderr = "root binary command timed out",
                    timedOut = true,
                )
            } else {
                RootBinaryCommandResult(
                    exitCode = process.exitValue(),
                    stdout = stdoutFuture.get(1, TimeUnit.SECONDS),
                    stderr = stderrFuture.get(1, TimeUnit.SECONDS),
                    timedOut = false,
                )
            }
        } catch (error: Exception) {
            process.destroyForcibly()
            RootBinaryCommandResult(
                exitCode = null,
                stdout = byteArrayOf(),
                stderr = error.message ?: error::class.java.simpleName,
                timedOut = false,
            )
        } finally {
            executor.shutdownNow()
        }
    }
}
