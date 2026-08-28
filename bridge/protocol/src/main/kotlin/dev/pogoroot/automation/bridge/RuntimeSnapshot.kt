package dev.pogoroot.automation.bridge

enum class RuntimeConnectionState {
    CONNECTED,
    DISCONNECTED,
    NOT_SEEN,
    ERROR,
}

enum class BindingProbeState {
    READY,
    UNITY_LOADED,
    WAITING,
    NOT_RUNNING,
    UNKNOWN,
}

data class RuntimeSnapshot(
    val protocolVersion: Int?,
    val state: RuntimeConnectionState,
    val pid: Int?,
    val processName: String?,
    val packageName: String?,
    val gameVersionName: String?,
    val gameVersionCode: Long?,
    val bindingProbeState: BindingProbeState = BindingProbeState.UNKNOWN,
    val bindingEngine: String? = null,
    val bindingStrategy: String? = null,
    val processExecutable: String? = null,
    val il2cppPath: String? = null,
    val unityPath: String? = null,
    val libmainPath: String? = null,
    val translationLayer: String? = null,
    val nativeProbeState: String? = null,
    val il2cppApiAvailable: Boolean = false,
    val il2cppSymbolCount: Int = 0,
    val nativeIl2cppPath: String? = null,
    val nativeUnityPath: String? = null,
    val devicePrimaryAbi: String? = null,
    val deviceSupportedAbis: List<String> = emptyList(),
    val nativeBridge: String? = null,
    val zygote: String? = null,
    val kernelMachine: String? = null,
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

        val probeState = when (values["probe_state"]) {
            "ready" -> BindingProbeState.READY
            "unity_loaded" -> BindingProbeState.UNITY_LOADED
            "waiting" -> BindingProbeState.WAITING
            "not_running" -> BindingProbeState.NOT_RUNNING
            else -> BindingProbeState.UNKNOWN
        }

        return RuntimeSnapshot(
            protocolVersion = values["protocol"]?.toIntOrNull(),
            state = state,
            pid = values["pid"]?.toIntOrNull()?.takeIf { it > 0 },
            processName = values["process"].nullIfBlank(),
            packageName = values["package"].nullIfBlank(),
            gameVersionName = values["version_name"].nullIfBlank(),
            gameVersionCode = values["version_code"]?.toLongOrNull(),
            bindingProbeState = probeState,
            bindingEngine = values["binding_engine"].nullIfBlankOrUnknown(),
            bindingStrategy = values["binding_strategy"].nullIfBlankOrUnavailable(),
            processExecutable = values["process_exe"].nullIfBlank(),
            il2cppPath = values["libil2cpp_path"].nullIfBlank(),
            unityPath = values["libunity_path"].nullIfBlank(),
            libmainPath = values["libmain_path"].nullIfBlank(),
            translationLayer = values["translation_layer"].nullIfBlankOrNone(),
            nativeProbeState = values["native_probe_state"].nullIfBlank(),
            il2cppApiAvailable = values["native_il2cpp_api_available"] == "1",
            il2cppSymbolCount = values["native_il2cpp_symbol_count"]?.toIntOrNull() ?: 0,
            nativeIl2cppPath = values["native_libil2cpp_path"].nullIfBlank(),
            nativeUnityPath = values["native_libunity_path"].nullIfBlank(),
            devicePrimaryAbi = values["device_primary_abi"].nullIfBlank(),
            deviceSupportedAbis = values["device_supported_abis"]
                .orEmpty()
                .split(',')
                .map(String::trim)
                .filter(String::isNotEmpty),
            nativeBridge = values["native_bridge"].nullIfBlankOrNone(),
            zygote = values["zygote"].nullIfBlank(),
            kernelMachine = values["kernel_machine"].nullIfBlank(),
            error = values["error"].nullIfBlank(),
        )
    }

    private fun String?.nullIfBlank(): String? = this?.takeIf(String::isNotBlank)

    private fun String?.nullIfBlankOrUnknown(): String? =
        this?.takeIf { it.isNotBlank() && it != "unknown" }

    private fun String?.nullIfBlankOrNone(): String? =
        this?.takeIf { it.isNotBlank() && it != "none" }

    private fun String?.nullIfBlankOrUnavailable(): String? =
        this?.takeIf { it.isNotBlank() && it != "unavailable" }
}
