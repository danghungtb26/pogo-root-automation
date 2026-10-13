package dev.pogoroot.automation.root

import java.io.ByteArrayOutputStream
import java.io.InputStream
import java.util.concurrent.Executors
import java.util.concurrent.Future
import java.util.concurrent.TimeUnit

sealed class RootCommand protected constructor(
    internal val script: String,
    internal val stdin: String = "",
) {
    data class RegisterControllerUid(val uid: Int) : RootCommand(
        script = REGISTER_CONTROLLER_UID_SCRIPT,
        stdin = "$uid\n",
    ) {
        init {
            require(uid > 0) { "controller UID must be positive" }
        }
    }

    data class SetMockLocation(val packageName: String, val allowed: Boolean) : RootCommand(
        script = if (allowed) GRANT_MOCK_LOCATION_SCRIPT else DENY_MOCK_LOCATION_SCRIPT,
        stdin = "$packageName\n",
    ) {
        init {
            require(PACKAGE_NAME.matches(packageName)) { "invalid Android package name" }
        }
    }

    data object ProbeModuleState : RootCommand(PROBE_MODULE_STATE_SCRIPT)

    class InstallModule internal constructor(val zipPath: String) : RootCommand(
        script = INSTALL_MODULE_SCRIPT,
        stdin = "$zipPath\n",
    ) {
        init {
            require(zipPath.startsWith("/data/") && zipPath.endsWith(".zip")) {
                "module ZIP must be an app-private Android data path"
            }
            require(!zipPath.contains('\n') && !zipPath.contains('\r')) {
                "module ZIP path contains a line break"
            }
        }
    }

    data object RebootDevice : RootCommand("sync\nreboot\n")

    private companion object {
        val PACKAGE_NAME = Regex("[A-Za-z0-9_]+(?:\\.[A-Za-z0-9_]+)+")

        fun shell(raw: String): String = raw.replace('@', '$')

        val REGISTER_CONTROLLER_UID_SCRIPT = shell("""
IFS= read -r controller_uid
case "@controller_uid" in ''|*[!0-9]*) exit 64 ;; esac
state_dir=/data/adb/pogo_root_automation
uid_file="@state_dir/controller.uids"
uid_temp="@state_dir/controller.uids.tmp.@@"
umask 077
mkdir -p "@state_dir" || exit 65
chmod 0711 "@state_dir" || exit 66
printf '%s\n' "@controller_uid" > "@uid_temp" || exit 67
chmod 0600 "@uid_temp" || exit 68
mv -f "@uid_temp" "@uid_file" || exit 69
""")

        val GRANT_MOCK_LOCATION_SCRIPT = shell("""
IFS= read -r package_name
case "@package_name" in ''|*[!A-Za-z0-9._]*) exit 64 ;; esac
exec appops set "@package_name" android:mock_location allow
""")

        val DENY_MOCK_LOCATION_SCRIPT = shell("""
IFS= read -r package_name
case "@package_name" in ''|*[!A-Za-z0-9._]*) exit 64 ;; esac
exec appops set "@package_name" android:mock_location deny
""")

        val PROBE_MODULE_STATE_SCRIPT = shell("""
root_uid=@(id -u 2>/dev/null) || exit 10
[ "@root_uid" = 0 ] || exit 11
printf 'ROOT_AVAILABLE=1\n'
if ! command -v magisk >/dev/null 2>&1; then
  printf 'MAGISK_AVAILABLE=0\n'
  exit 0
fi
printf 'MAGISK_AVAILABLE=1\n'
magisk_version=@(magisk -v 2>/dev/null | head -n 1)
printf 'MAGISK_VERSION=%s\n' "@magisk_version"
zygisk_setting=@(magisk --sqlite "SELECT value FROM settings WHERE key='zygisk';" 2>/dev/null | tail -n 1)
case "@zygisk_setting" in 1|*value=1*) printf 'ZYGISK_ENABLED=1\n' ;; *) printf 'ZYGISK_ENABLED=0\n' ;; esac
module_dir=/data/adb/modules/pogo_root_automation
if [ -d "@module_dir" ]; then printf 'MODULE_PRESENT=1\n'; else printf 'MODULE_PRESENT=0\n'; fi
if [ -d "@module_dir" ] && [ ! -e "@module_dir/disable" ] && [ ! -e "@module_dir/remove" ]; then
  printf 'MODULE_ENABLED=1\n'
else
  printf 'MODULE_ENABLED=0\n'
fi
module_version=@(sed -n 's/^version=//p' "@module_dir/module.prop" 2>/dev/null | head -n 1)
printf 'MODULE_VERSION=%s\n' "@module_version"
runtime_file=/data/adb/pogo_root_automation/runtime.status
runtime_active=0
runtime_session=
if [ -f "@runtime_file" ]; then
  runtime_pid=@(sed -n 's/^pid=//p' "@runtime_file" 2>/dev/null | head -n 1)
  runtime_process=@(sed -n 's/^process=//p' "@runtime_file" 2>/dev/null | head -n 1)
  runtime_session=@(sed -n 's/^runtime_session_id=//p' "@runtime_file" 2>/dev/null | head -n 1)
  case "@runtime_pid" in ''|*[!0-9]*) runtime_pid=0 ;; esac
  if [ "@runtime_pid" -gt 0 ] 2>/dev/null && kill -0 "@runtime_pid" 2>/dev/null; then
    current_process=@(tr '\000' '\n' < "/proc/@runtime_pid/cmdline" 2>/dev/null | head -n 1)
    if [ "@current_process" = "@runtime_process" ] && [ "@runtime_process" = com.nianticlabs.pokemongo ]; then
      runtime_active=1
    fi
  fi
fi
printf 'RUNTIME_ACTIVE=%s\n' "@runtime_active"
printf 'RUNTIME_SESSION=%s\n' "@runtime_session"
""")

        val INSTALL_MODULE_SCRIPT = shell("""
IFS= read -r module_zip
case "@module_zip" in /data/*.zip) ;; *) exit 64 ;; esac
[ -f "@module_zip" ] || exit 65
command -v magisk >/dev/null 2>&1 || exit 66
exec magisk --install-module "@module_zip"
""")
    }
}

interface RootShell {
    fun execute(command: RootCommand, timeoutMillis: Long = 3_000L): RootCommandResult
}

enum class RootFailureKind {
    NONE,
    ROOT_UNAVAILABLE,
    TIMEOUT,
    NON_ZERO,
    LAUNCH_FAILED,
}

data class RootCommandResult(
    val exitCode: Int?,
    val stdout: String,
    val stderr: String,
    val timedOut: Boolean,
    val failureKind: RootFailureKind,
) {
    val isSuccess: Boolean
        get() = failureKind == RootFailureKind.NONE && exitCode == 0
}

class ProcessRootShell : RootShell {
    override fun execute(command: RootCommand, timeoutMillis: Long): RootCommandResult {
        require(timeoutMillis in 1L..MAX_TIMEOUT_MS) { "root timeout is out of bounds" }
        val process = runCatching {
            ProcessBuilder("su", "-c", command.script).start()
        }.getOrElse { error ->
            return RootCommandResult(
                exitCode = null,
                stdout = "",
                stderr = error.message ?: error::class.java.simpleName,
                timedOut = false,
                failureKind = RootFailureKind.LAUNCH_FAILED,
            )
        }

        val readers = Executors.newFixedThreadPool(2)
        val stdoutFuture = readers.submit<String> { process.inputStream.readBounded() }
        val stderrFuture = readers.submit<String> { process.errorStream.readBounded() }
        runCatching {
            process.outputStream.bufferedWriter().use { writer ->
                writer.write(command.stdin)
                writer.flush()
            }
        }
        val completed = process.waitFor(timeoutMillis, TimeUnit.MILLISECONDS)
        if (!completed) {
            process.destroyForcibly()
            process.waitFor(STREAM_CLOSE_TIMEOUT_MS, TimeUnit.MILLISECONDS)
        }
        val stdout = stdoutFuture.awaitReader()
        val stderr = stderrFuture.awaitReader()
        readers.shutdownNow()
        if (!completed) {
            return RootCommandResult(
                exitCode = null,
                stdout = stdout,
                stderr = stderr.ifBlank { "root command timed out" },
                timedOut = true,
                failureKind = RootFailureKind.TIMEOUT,
            )
        }

        val exitCode = process.exitValue()
        val failure = when {
            exitCode == 0 -> RootFailureKind.NONE
            rootWasDenied(stdout, stderr) -> RootFailureKind.ROOT_UNAVAILABLE
            else -> RootFailureKind.NON_ZERO
        }
        return RootCommandResult(exitCode, stdout, stderr, false, failure)
    }

    private fun InputStream.readBounded(): String {
        val retained = ByteArrayOutputStream(MAX_OUTPUT_BYTES)
        val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
        var truncated = false
        while (true) {
            val count = read(buffer)
            if (count < 0) break
            val remaining = MAX_OUTPUT_BYTES - retained.size()
            if (remaining > 0) retained.write(buffer, 0, minOf(count, remaining))
            if (count > remaining) truncated = true
        }
        return retained.toString(Charsets.UTF_8.name()) + if (truncated) "\n[output truncated]" else ""
    }

    private fun Future<String>.awaitReader(): String = runCatching {
        get(STREAM_CLOSE_TIMEOUT_MS, TimeUnit.MILLISECONDS)
    }.getOrElse {
        cancel(true)
        "[output unavailable]"
    }

    private fun rootWasDenied(stdout: String, stderr: String): Boolean {
        val combined = "$stdout\n$stderr".lowercase()
        return "permission denied" in combined || "not allowed" in combined ||
            "root access" in combined && "denied" in combined
    }

    private companion object {
        const val MAX_OUTPUT_BYTES = 64 * 1024
        const val MAX_TIMEOUT_MS = 120_000L
        const val STREAM_CLOSE_TIMEOUT_MS = 1_000L
    }
}
