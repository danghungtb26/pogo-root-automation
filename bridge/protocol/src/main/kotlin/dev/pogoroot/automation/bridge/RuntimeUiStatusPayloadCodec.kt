package dev.pogoroot.automation.bridge

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream

enum class RuntimeNativeLifecycle(val wireValue: Int) {
    UNKNOWN(0),
    ATTACHED_IDLE(1),
    STARTING(2),
    DIAGNOSTIC_PENDING(3),
    BINDING_READY(4),
    APPLYING(5),
    READY(6),
    STOPPING(7),
    ERROR(8),
    ;

    companion object {
        fun fromWireValue(value: Int): RuntimeNativeLifecycle =
            entries.firstOrNull { it.wireValue == value }
                ?: error("invalid native lifecycle wire value: $value")
    }
}

enum class RuntimeModuleState(val wireValue: Int) {
    UNKNOWN(0),
    REGISTERED(1),
    CONFIGURED(2),
    READY(3),
    DISABLED(4),
    ERROR(5),
    ;

    companion object {
        fun fromWireValue(value: Int): RuntimeModuleState =
            entries.firstOrNull { it.wireValue == value }
                ?: error("invalid runtime module state wire value: $value")
    }
}

data class RuntimeUiModuleStatus(
    val moduleWire: Int,
    val moduleName: String,
    val state: RuntimeModuleState,
    val revision: Long? = null,
    val errorCode: String? = null,
) {
    init {
        require(moduleWire > 0) { "runtime module wire must be positive" }
        require(moduleName.isNotBlank()) { "runtime module name is required" }
        require(revision == null || revision > 0L) { "runtime module revision must be positive" }
    }
}

data class RuntimeUiStatus(
    val runtimeSessionId: String,
    val pid: Int,
    val processName: String,
    val packageName: String,
    val buildFingerprint: String,
    val nativeLifecycle: RuntimeNativeLifecycle,
    val strongIdentityVerified: Boolean,
    val capabilities: Set<String>,
    val desiredRevision: Long? = null,
    val appliedRevision: Long? = null,
    val ready: Boolean = false,
    val modules: List<RuntimeUiModuleStatus> = emptyList(),
    val errorCode: String? = null,
    val errorMessage: String? = null,
    val observedAtEpochMs: Long,
    val observedAtElapsedNs: Long,
) {
    init {
        require(runtimeSessionId.isNotBlank()) { "runtime status session is required" }
        require(pid > 0) { "runtime status pid must be positive" }
        require(processName.isNotBlank()) { "runtime status process is required" }
        require(packageName.isNotBlank()) { "runtime status package is required" }
        require(buildFingerprint.isNotBlank()) { "runtime status build fingerprint is required" }
        require(capabilities.size <= MAX_CAPABILITIES) {
            "runtime status contains too many capabilities"
        }
        require(capabilities.all { it.isNotBlank() }) {
            "runtime status capabilities must not be blank"
        }
        require(modules.size <= MAX_MODULES) { "runtime status contains too many modules" }
        require(desiredRevision == null || desiredRevision > 0L) {
            "runtime status desired revision must be positive"
        }
        require(appliedRevision == null || appliedRevision > 0L) {
            "runtime status applied revision must be positive"
        }
        require(errorCode == null || errorCode.isNotBlank()) {
            "runtime status error code must not be blank"
        }
        require(observedAtEpochMs >= 0L) { "runtime status epoch timestamp must be non-negative" }
        require(observedAtElapsedNs >= 0L) {
            "runtime status monotonic timestamp must be non-negative"
        }
    }

    companion object {
        const val SCHEMA_VERSION = 1
        const val MARKER = 0x52555354
        const val MAX_CAPABILITIES = 256
        const val MAX_MODULES = 8
        const val MAX_ERROR_CODE_BYTES = 256
        const val MAX_ERROR_MESSAGE_BYTES = 4 * 1024
        const val MAX_LOGICAL_ID_BYTES = 256
        const val MAX_BUILD_FINGERPRINT_BYTES = 4 * 1024
    }
}

object RuntimeUiStatusPayloadCodec {
    private val codec = BridgePayloadCodecSupport

    fun encode(status: RuntimeUiStatus): Result<ByteArray> = runCatching {
        validateLogicalFields(status)
        ByteArrayOutputStream().use { bytes ->
            DataOutputStream(bytes).use { output ->
                output.writeInt(codec.PAYLOAD_VERSION)
                output.writeInt(RuntimeUiStatus.MARKER)
                output.writeInt(RuntimeUiStatus.SCHEMA_VERSION)
                codec.writeString(output, status.runtimeSessionId)
                output.writeInt(status.pid)
                codec.writeString(output, status.processName)
                codec.writeString(output, status.packageName)
                codec.writeString(output, status.buildFingerprint)
                output.writeInt(status.nativeLifecycle.wireValue)
                output.writeBoolean(status.strongIdentityVerified)
                val capabilities = status.capabilities.sorted()
                output.writeInt(capabilities.size)
                capabilities.forEach { codec.writeString(output, it) }
                writeNullableRevision(output, status.desiredRevision)
                writeNullableRevision(output, status.appliedRevision)
                output.writeBoolean(status.ready)
                output.writeInt(status.modules.size)
                status.modules.forEach { module ->
                    output.writeInt(module.moduleWire)
                    codec.writeString(output, module.moduleName)
                    output.writeInt(module.state.wireValue)
                    writeNullableRevision(output, module.revision)
                    codec.writeNullableString(output, module.errorCode)
                }
                codec.writeNullableString(output, status.errorCode)
                codec.writeNullableString(output, status.errorMessage)
                output.writeLong(status.observedAtEpochMs)
                output.writeLong(status.observedAtElapsedNs)
            }
            bytes.toByteArray().also {
                require(it.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
                    "runtime UI status exceeds hard limit"
                }
            }
        }
    }

    fun decode(payload: ByteArray): Result<RuntimeUiStatus> = runCatching {
        require(payload.size <= BridgeProtocol.HARD_MESSAGE_BYTES) {
            "runtime UI status exceeds hard limit"
        }
        DataInputStream(ByteArrayInputStream(payload)).use { input ->
            require(input.readInt() == codec.PAYLOAD_VERSION) {
                "unsupported runtime UI status payload version"
            }
            require(input.readInt() == RuntimeUiStatus.MARKER) {
                "invalid runtime UI status marker"
            }
            require(input.readInt() == RuntimeUiStatus.SCHEMA_VERSION) {
                "unsupported runtime UI status schema"
            }
            val status = RuntimeUiStatus(
                runtimeSessionId = codec.readString(input),
                pid = input.readInt(),
                processName = codec.readString(input),
                packageName = codec.readString(input),
                buildFingerprint = codec.readString(input),
                nativeLifecycle = RuntimeNativeLifecycle.fromWireValue(input.readInt()),
                strongIdentityVerified = input.readStrictBoolean(),
                capabilities = readCapabilities(input),
                desiredRevision = readNullableRevision(input),
                appliedRevision = readNullableRevision(input),
                ready = input.readStrictBoolean(),
                modules = readModules(input),
                errorCode = codec.readNullableString(input),
                errorMessage = codec.readNullableString(input),
                observedAtEpochMs = input.readLong(),
                observedAtElapsedNs = input.readLong(),
            )
            require(input.available() == 0) { "trailing bytes in runtime UI status" }
            validateLogicalFields(status)
            status
        }
    }

    private fun readCapabilities(input: DataInputStream): Set<String> {
        val count = input.readInt()
        require(count in 0..RuntimeUiStatus.MAX_CAPABILITIES) {
            "invalid runtime capability count"
        }
        val values = List(count) { codec.readString(input) }
        require(values.all { it.isNotBlank() }) { "runtime capability must not be blank" }
        require(values == values.sorted() && values.toSet().size == values.size) {
            "runtime capabilities must be sorted and unique"
        }
        return values.toSet()
    }

    private fun readModules(input: DataInputStream): List<RuntimeUiModuleStatus> {
        val count = input.readInt()
        require(count in 0..RuntimeUiStatus.MAX_MODULES) {
            "invalid runtime module count"
        }
        return List(count) {
            RuntimeUiModuleStatus(
                moduleWire = input.readInt(),
                moduleName = codec.readString(input),
                state = RuntimeModuleState.fromWireValue(input.readInt()),
                revision = readNullableRevision(input),
                errorCode = codec.readNullableString(input),
            )
        }
    }

    private fun writeNullableRevision(output: DataOutputStream, value: Long?) {
        output.writeBoolean(value != null)
        if (value != null) {
            require(value > 0L) { "runtime status revision must be positive" }
            output.writeLong(value)
        }
    }

    private fun readNullableRevision(input: DataInputStream): Long? =
        if (input.readStrictBoolean()) input.readLong().also {
            require(it > 0L) { "runtime status revision must be positive" }
        } else {
            null
        }

    private fun validateLogicalFields(status: RuntimeUiStatus) {
        requireUtf8Size(status.runtimeSessionId, RuntimeUiStatus.MAX_LOGICAL_ID_BYTES, "session")
        requireUtf8Size(status.processName, RuntimeUiStatus.MAX_LOGICAL_ID_BYTES, "process")
        requireUtf8Size(status.packageName, RuntimeUiStatus.MAX_LOGICAL_ID_BYTES, "package")
        requireUtf8Size(
            status.buildFingerprint,
            RuntimeUiStatus.MAX_BUILD_FINGERPRINT_BYTES,
            "build fingerprint",
        )
        status.capabilities.forEach {
            requireUtf8Size(it, RuntimeUiStatus.MAX_ERROR_CODE_BYTES, "capability")
        }
        status.modules.forEach { module ->
            requireUtf8Size(module.moduleName, RuntimeUiStatus.MAX_LOGICAL_ID_BYTES, "module name")
            require(module.errorCode == null || module.errorCode.isNotBlank()) {
                "runtime module error code must not be blank"
            }
            if (module.errorCode != null) {
                requireUtf8Size(
                    module.errorCode,
                    RuntimeUiStatus.MAX_ERROR_CODE_BYTES,
                    "module error code",
                )
            }
        }
        status.errorCode?.let {
            requireUtf8Size(it, RuntimeUiStatus.MAX_ERROR_CODE_BYTES, "error code")
        }
        status.errorMessage?.let {
            requireUtf8Size(it, RuntimeUiStatus.MAX_ERROR_MESSAGE_BYTES, "error message")
        }
    }

    private fun requireUtf8Size(value: String, maxBytes: Int, label: String) {
        require(value.toByteArray(Charsets.UTF_8).size <= maxBytes) {
            "runtime status $label exceeds logical limit"
        }
    }

    private fun DataInputStream.readStrictBoolean(): Boolean = when (readUnsignedByte()) {
        0 -> false
        1 -> true
        else -> error("invalid boolean wire value")
    }
}
