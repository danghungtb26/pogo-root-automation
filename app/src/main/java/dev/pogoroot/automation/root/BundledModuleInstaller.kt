package dev.pogoroot.automation.root

import android.content.Context
import dev.pogoroot.automation.R
import org.json.JSONObject
import java.io.File
import java.security.MessageDigest
import java.util.zip.ZipFile

data class BundledModuleInfo(
    val moduleId: String,
    val version: String,
    val versionCode: Int,
    val zipSha256: String,
    val sourceRevision: String,
)

private data class PreparedBundledModule(
    val file: File,
    val info: BundledModuleInfo,
)

private data class RootModuleProbe(
    val magiskAvailable: Boolean,
    val zygiskEnabled: Boolean,
    val modulePresent: Boolean,
    val moduleEnabled: Boolean,
    val moduleVersion: String?,
    val runtimeActive: Boolean,
    val runtimeSessionId: String?,
)

class BundledModuleInstaller(
    context: Context,
    private val stateRepository: ModuleInstallStateRepository,
    private val rootShell: RootShell = ProcessRootShell(),
) {
    private val appContext = context.applicationContext

    fun bundledInfo(): Result<BundledModuleInfo> = runCatching { readManifest() }

    fun ensureInstalled(allowReplacement: Boolean): ModuleInstallSnapshot {
        val prepared = runCatching { prepareAndValidate() }.getOrElse { error ->
            return fail("bundled_module_invalid", error.message ?: "Bundled module is invalid.")
        }
        try {
            val probeResult = rootShell.execute(RootCommand.ProbeModuleState, PREFLIGHT_TIMEOUT_MS)
            if (!probeResult.isSuccess) {
                val rootUnavailable = probeResult.failureKind in setOf(
                    RootFailureKind.ROOT_UNAVAILABLE,
                    RootFailureKind.LAUNCH_FAILED,
                ) || probeResult.exitCode == 10 || probeResult.exitCode == 11
                val code = when {
                    rootUnavailable -> "root_denied"
                    probeResult.failureKind == RootFailureKind.TIMEOUT -> "root_probe_timeout"
                    else -> "root_probe_failed"
                }
                return stateRepository.transition(
                    state = if (rootUnavailable) {
                        ModuleInstallState.ROOT_REQUIRED
                    } else {
                        ModuleInstallState.FAILED
                    },
                    moduleVersion = prepared.info.version,
                    moduleSha256 = prepared.info.zipSha256,
                    errorCode = code,
                    message = probeResult.stderr.ifBlank { "Root access is required." },
                )
            }
            val probe = parseProbe(probeResult.stdout)
            if (!probe.magiskAvailable) return fail(
                "magisk_missing",
                "Magisk is not available on this device.",
                prepared.info,
            )
            if (!probe.zygiskEnabled) return fail(
                "zygisk_disabled",
                "Enable Zygisk in Magisk, then reboot before continuing.",
                prepared.info,
            )
            if (probe.modulePresent && !probe.moduleEnabled) return fail(
                "module_disabled",
                "The Magisk module is disabled or pending removal.",
                prepared.info,
            )

            val previous = stateRepository.read()
            val sameHash = previous.moduleSha256 == prepared.info.zipSha256
            if (probe.modulePresent && !sameHash && !allowReplacement) {
                return fail(
                    "module_update_confirmation_required",
                    "A different module build is installed. Tap retry to approve replacement.",
                    prepared.info,
                )
            }
            if (probe.modulePresent && sameHash) {
                return installedState(prepared.info, probe, previous.state)
            }
            return installPrepared(prepared)
        } finally {
            prepared.file.delete()
            prepared.file.parentFile?.delete()
        }
    }

    fun reboot(): RootCommandResult = rootShell.execute(
        RootCommand.RebootDevice,
        REBOOT_TIMEOUT_MS,
    )

    private fun installPrepared(prepared: PreparedBundledModule): ModuleInstallSnapshot {
        stateRepository.transition(
            state = ModuleInstallState.INSTALLING,
            moduleVersion = prepared.info.version,
            moduleSha256 = prepared.info.zipSha256,
            message = "Installing the validated bundled Magisk module.",
        )
        val result = rootShell.execute(
            RootCommand.InstallModule(prepared.file.absolutePath),
            INSTALL_TIMEOUT_MS,
        )
        if (!result.isSuccess) {
            val code = when (result.failureKind) {
                RootFailureKind.ROOT_UNAVAILABLE -> "root_denied"
                RootFailureKind.TIMEOUT -> "install_timeout"
                else -> "magisk_install_failed"
            }
            return fail(
                code,
                result.stderr.ifBlank { result.stdout.ifBlank { "Magisk installation failed." } },
                prepared.info,
            )
        }
        return stateRepository.transition(
            state = ModuleInstallState.REBOOT_REQUIRED,
            moduleVersion = prepared.info.version,
            moduleSha256 = prepared.info.zipSha256,
            message = "Module installed. Reboot manually to let Zygisk load it.",
        )
    }

    private fun installedState(
        info: BundledModuleInfo,
        probe: RootModuleProbe,
        previousState: ModuleInstallState,
    ): ModuleInstallSnapshot {
        if (probe.runtimeActive && !probe.runtimeSessionId.isNullOrBlank()) {
            return stateRepository.transition(
                state = ModuleInstallState.ACTIVE,
                moduleVersion = info.version,
                moduleSha256 = info.zipSha256,
                runtimeSessionId = probe.runtimeSessionId,
                message = "Zygisk runtime is active in the target process.",
            )
        }
        val state = if (previousState == ModuleInstallState.REBOOT_REQUIRED) {
            ModuleInstallState.REBOOT_REQUIRED
        } else {
            ModuleInstallState.INSTALLED
        }
        return stateRepository.transition(
            state = state,
            moduleVersion = info.version,
            moduleSha256 = info.zipSha256,
            errorCode = if (state == ModuleInstallState.INSTALLED) "module_inactive" else null,
            message = if (state == ModuleInstallState.REBOOT_REQUIRED) {
                "Module installation completed; reboot is still required."
            } else {
                "Module is installed, but no live target runtime is proven."
            },
        )
    }

    private fun prepareAndValidate(): PreparedBundledModule {
        val info = readManifest()
        val directory = File(appContext.cacheDir, INSTALL_DIRECTORY).apply {
            deleteRecursively()
            check(mkdirs()) { "Cannot create app-private module install directory." }
        }
        val output = File(directory, MODULE_ZIP_ASSET.substringAfterLast('/'))
        val digest = MessageDigest.getInstance("SHA-256")
        appContext.assets.open(MODULE_ZIP_ASSET).use { input ->
            output.outputStream().buffered().use { sink ->
                val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
                while (true) {
                    val count = input.read(buffer)
                    if (count < 0) break
                    digest.update(buffer, 0, count)
                    sink.write(buffer, 0, count)
                }
            }
        }
        val actualHash = digest.digest().joinToString("") { "%02x".format(it) }
        check(actualHash == info.zipSha256) { "Bundled module SHA-256 does not match manifest." }
        validateZip(output, info.moduleId)
        return PreparedBundledModule(output, info)
    }

    private fun readManifest(): BundledModuleInfo {
        val raw = appContext.assets.open(MODULE_MANIFEST_ASSET).bufferedReader().use { it.readText() }
        val json = JSONObject(raw)
        check(json.getInt("schemaVersion") == 1) { "Unsupported bundled module schema." }
        val info = BundledModuleInfo(
            moduleId = json.getString("moduleId"),
            version = json.getString("moduleVersion"),
            versionCode = json.getInt("moduleVersionCode"),
            zipSha256 = json.getString("zipSha256"),
            sourceRevision = json.getString("sourceRevision"),
        )
        check(info.moduleId == appContext.getString(R.string.pogo_module_id)) {
            "Generated module id does not match bundled manifest."
        }
        check(info.version == appContext.getString(R.string.pogo_module_version)) {
            "Generated module version does not match bundled manifest."
        }
        check(info.zipSha256 == appContext.getString(R.string.pogo_module_sha256)) {
            "Generated module hash does not match bundled manifest."
        }
        return info
    }

    private fun validateZip(file: File, expectedModuleId: String) {
        ZipFile(file).use { zip ->
            check(zip.getEntry("zygisk/arm64-v8a.so")?.size?.let { it > 0L } == true) {
                "Bundled module is missing arm64-v8a runtime."
            }
            check(zip.getEntry("zygisk/x86_64.so")?.size?.let { it > 0L } == true) {
                "Bundled module is missing x86_64 runtime."
            }
            val moduleProperty = zip.getEntry("module.prop")
                ?: error("Bundled module is missing module.prop.")
            val properties = zip.getInputStream(moduleProperty).bufferedReader().useLines { lines ->
                lines.mapNotNull { line ->
                    val key = line.substringBefore('=', "").trim()
                    if (key.isBlank()) null else key to line.substringAfter('=', "").trim()
                }.toMap()
            }
            check(properties["id"] == expectedModuleId) { "Bundled module id mismatch." }
        }
    }

    private fun parseProbe(output: String): RootModuleProbe {
        val values = output.lineSequence().mapNotNull { line ->
            val key = line.substringBefore('=', "")
            if (key.isBlank()) null else key to line.substringAfter('=', "")
        }.toMap()
        check(values["ROOT_AVAILABLE"] == "1") { "Root probe returned an invalid response." }
        return RootModuleProbe(
            magiskAvailable = values["MAGISK_AVAILABLE"] == "1",
            zygiskEnabled = values["ZYGISK_ENABLED"] == "1",
            modulePresent = values["MODULE_PRESENT"] == "1",
            moduleEnabled = values["MODULE_ENABLED"] == "1",
            moduleVersion = values["MODULE_VERSION"]?.takeIf(String::isNotBlank),
            runtimeActive = values["RUNTIME_ACTIVE"] == "1",
            runtimeSessionId = values["RUNTIME_SESSION"]?.takeIf(String::isNotBlank),
        )
    }

    private fun fail(
        code: String,
        message: String,
        info: BundledModuleInfo? = null,
    ): ModuleInstallSnapshot = stateRepository.transition(
        state = ModuleInstallState.FAILED,
        moduleVersion = info?.version,
        moduleSha256 = info?.zipSha256,
        errorCode = code,
        message = message.take(MAX_STATE_MESSAGE_LENGTH),
    )

    private companion object {
        const val MODULE_ZIP_ASSET = "pogo-module/pogo-root-automation-magisk-multiabi.zip"
        const val MODULE_MANIFEST_ASSET =
            "pogo-module/pogo-root-automation-magisk-multiabi.manifest.json"
        const val INSTALL_DIRECTORY = "pogo-module-install"
        const val PREFLIGHT_TIMEOUT_MS = 10_000L
        const val INSTALL_TIMEOUT_MS = 120_000L
        const val REBOOT_TIMEOUT_MS = 5_000L
        const val MAX_STATE_MESSAGE_LENGTH = 512
    }
}
