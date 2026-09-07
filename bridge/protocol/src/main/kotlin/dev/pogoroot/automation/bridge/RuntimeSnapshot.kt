package dev.pogoroot.automation.bridge

enum class RuntimeConnectionState {
    CONNECTED,
    DISCONNECTED,
    NOT_SEEN,
    ERROR,
}

data class RuntimeSnapshot(
    val protocolVersion: Int?,
    val state: RuntimeConnectionState,
    val pid: Int?,
    val processName: String?,
    val packageName: String?,
    val gameVersionName: String?,
    val gameVersionCode: Long?,
    val error: String? = null,
)

object RuntimeSnapshotParser {
    fun parse(output: String): RuntimeSnapshot {
        val values = output
            .lineSequence()
            .map(String::trim)
            .filter { it.isNotEmpty() && '=' in it }
            .map { line ->
                val separator = line.indexOf('=')
                line.substring(0, separator) to line.substring(separator + 1)
            }
            .toMap()

        val state = when (values["runtime_state"]) {
            "connected" -> RuntimeConnectionState.CONNECTED
            "disconnected" -> RuntimeConnectionState.DISCONNECTED
            "not_seen" -> RuntimeConnectionState.NOT_SEEN
            else -> RuntimeConnectionState.ERROR
        }

        return RuntimeSnapshot(
            protocolVersion = values["protocol"]?.toIntOrNull(),
            state = state,
            pid = values["pid"]?.toIntOrNull()?.takeIf { it > 0 },
            processName = values["process"].nullIfBlank(),
            packageName = values["package"].nullIfBlank(),
            gameVersionName = values["version_name"].nullIfBlank(),
            gameVersionCode = values["version_code"]?.toLongOrNull(),
            error = values["error"].nullIfBlank(),
        )
    }

    private fun String?.nullIfBlank(): String? = this?.takeIf(String::isNotBlank)
}
