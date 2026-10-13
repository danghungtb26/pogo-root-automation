package dev.pogoroot.automation.root

import android.content.Context

enum class ModuleInstallState {
    NOT_CHECKED,
    ROOT_REQUIRED,
    INSTALLING,
    INSTALLED,
    REBOOT_REQUIRED,
    ACTIVE,
    FAILED,
}

data class ModuleInstallSnapshot(
    val state: ModuleInstallState,
    val moduleVersion: String?,
    val moduleSha256: String?,
    val runtimeSessionId: String?,
    val errorCode: String?,
    val message: String?,
    val updatedAtEpochMs: Long,
) {
    val gameplayAllowed: Boolean
        get() = state == ModuleInstallState.ACTIVE && !runtimeSessionId.isNullOrBlank()
}

class ModuleInstallStateRepository(context: Context) {
    private val prefs = context.applicationContext.getSharedPreferences(
        PREFS_NAME,
        Context.MODE_PRIVATE,
    )

    init {
        val stale = read()
        when (stale.state) {
            ModuleInstallState.INSTALLING -> update(
                stale.copy(
                    state = ModuleInstallState.FAILED,
                    errorCode = "install_interrupted",
                    message = "Module installation was interrupted; retry from the foreground app.",
                    runtimeSessionId = null,
                ),
            )

            ModuleInstallState.ACTIVE -> update(
                stale.copy(
                    state = ModuleInstallState.INSTALLED,
                    runtimeSessionId = null,
                    errorCode = "runtime_recheck_required",
                    message = "Runtime activity must be rechecked for this controller process.",
                ),
            )

            else -> Unit
        }
    }

    @Synchronized
    fun read(): ModuleInstallSnapshot = ModuleInstallSnapshot(
        state = runCatching {
            ModuleInstallState.valueOf(
                prefs.getString(KEY_STATE, ModuleInstallState.NOT_CHECKED.name)
                    ?: ModuleInstallState.NOT_CHECKED.name,
            )
        }.getOrDefault(ModuleInstallState.NOT_CHECKED),
        moduleVersion = prefs.getString(KEY_VERSION, null),
        moduleSha256 = prefs.getString(KEY_SHA256, null),
        runtimeSessionId = prefs.getString(KEY_RUNTIME_SESSION, null),
        errorCode = prefs.getString(KEY_ERROR_CODE, null),
        message = prefs.getString(KEY_MESSAGE, null),
        updatedAtEpochMs = prefs.getLong(KEY_UPDATED_AT, 0L),
    )

    @Synchronized
    fun update(snapshot: ModuleInstallSnapshot): ModuleInstallSnapshot {
        val stored = snapshot.copy(updatedAtEpochMs = System.currentTimeMillis())
        prefs.edit()
            .putString(KEY_STATE, stored.state.name)
            .putString(KEY_VERSION, stored.moduleVersion)
            .putString(KEY_SHA256, stored.moduleSha256)
            .putString(KEY_RUNTIME_SESSION, stored.runtimeSessionId)
            .putString(KEY_ERROR_CODE, stored.errorCode)
            .putString(KEY_MESSAGE, stored.message)
            .putLong(KEY_UPDATED_AT, stored.updatedAtEpochMs)
            .commit()
        return stored
    }

    fun transition(
        state: ModuleInstallState,
        moduleVersion: String? = read().moduleVersion,
        moduleSha256: String? = read().moduleSha256,
        runtimeSessionId: String? = null,
        errorCode: String? = null,
        message: String? = null,
    ): ModuleInstallSnapshot = update(
        ModuleInstallSnapshot(
            state = state,
            moduleVersion = moduleVersion,
            moduleSha256 = moduleSha256,
            runtimeSessionId = runtimeSessionId,
            errorCode = errorCode,
            message = message,
            updatedAtEpochMs = 0L,
        ),
    )

    private companion object {
        const val PREFS_NAME = "module_install_state"
        const val KEY_STATE = "state"
        const val KEY_VERSION = "module_version"
        const val KEY_SHA256 = "module_sha256"
        const val KEY_RUNTIME_SESSION = "runtime_session_id"
        const val KEY_ERROR_CODE = "error_code"
        const val KEY_MESSAGE = "message"
        const val KEY_UPDATED_AT = "updated_at_epoch_ms"
    }
}
