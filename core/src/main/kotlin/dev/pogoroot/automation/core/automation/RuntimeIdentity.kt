package dev.pogoroot.automation.core.automation

/** Transport-neutral identity announced by a runtime side. */
data class RuntimeIdentity(
    val runtimeSessionId: String,
    val pid: Int,
    val processName: String,
    val packageName: String,
    val buildFingerprint: String,
    val capabilities: Set<String>,
    val mutationsAllowed: Boolean,
) {
    init {
        require(runtimeSessionId.isNotBlank()) { "runtimeSessionId must not be blank" }
        require(pid > 0) { "pid must be positive" }
        require(processName.isNotBlank()) { "processName must not be blank" }
        require(packageName.isNotBlank()) { "packageName must not be blank" }
        require(buildFingerprint.isNotBlank()) { "buildFingerprint must not be blank" }
    }
}
