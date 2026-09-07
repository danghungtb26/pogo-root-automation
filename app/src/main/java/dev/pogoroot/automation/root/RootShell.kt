package dev.pogoroot.automation.root

import java.io.BufferedReader
import java.io.InputStreamReader
import java.util.concurrent.TimeUnit

interface RootShell {
    fun execute(command: String, timeoutMillis: Long = 3_000L): RootCommandResult
}

data class RootCommandResult(
    val exitCode: Int?,
    val stdout: String,
    val stderr: String,
    val timedOut: Boolean,
) {
    val isSuccess: Boolean
        get() = !timedOut && exitCode == 0
}

class ProcessRootShell : RootShell {
    override fun execute(command: String, timeoutMillis: Long): RootCommandResult {
        val process = runCatching {
            ProcessBuilder("su", "-c", command).start()
        }.getOrElse { error ->
            return RootCommandResult(
                exitCode = null,
                stdout = "",
                stderr = error.message ?: error::class.java.simpleName,
                timedOut = false,
            )
        }

        val completed = process.waitFor(timeoutMillis, TimeUnit.MILLISECONDS)
        if (!completed) {
            process.destroyForcibly()
            return RootCommandResult(
                exitCode = null,
                stdout = "",
                stderr = "root command timed out",
                timedOut = true,
            )
        }

        return RootCommandResult(
            exitCode = process.exitValue(),
            stdout = process.inputStream.readTextSafely(),
            stderr = process.errorStream.readTextSafely(),
            timedOut = false,
        )
    }

    private fun java.io.InputStream.readTextSafely(): String =
        BufferedReader(InputStreamReader(this)).use { reader -> reader.readText() }
}
