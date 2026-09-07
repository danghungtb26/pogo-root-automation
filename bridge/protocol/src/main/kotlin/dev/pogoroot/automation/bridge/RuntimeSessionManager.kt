package dev.pogoroot.automation.bridge

/**
 * Tracks the runtime-created session on the controller side. This class never
 * creates a session id; the injected runtime is the sole session authority.
 */
class RuntimeSessionManager(
    private val expectedPackageName: String? = null,
    private val expectedPackageNames: Set<String> = emptySet(),
    private val allowedBuildFingerprints: Set<String> = emptySet(),
    private val requiredCapabilities: Set<String> = emptySet(),
) {
    private var active: BridgeEvent.RuntimeReady? = null
    private var lastMessageSeq = 0L
    private var invalidReason: String? = null

    val current: BridgeEvent.RuntimeReady?
        get() = active

    val mutationsAllowed: Boolean
        get() = active != null && active!!.buildFingerprint in allowedBuildFingerprints

    val lastError: String?
        get() = invalidReason

    fun accept(ready: BridgeEvent.RuntimeReady): Result<Unit> = runCatching {
        require(ready.protocolVersion == BridgeProtocol.VERSION) { "bridge protocol mismatch" }
        val packages = expectedPackageNames.ifEmpty {
            expectedPackageName?.let(::setOf).orEmpty()
        }
        require(ready.packageName in packages) {
            "unexpected runtime package: ${ready.packageName}"
        }
        require(requiredCapabilities.all { it in ready.capabilities }) {
            "runtime is missing required capabilities"
        }

        active = ready
        lastMessageSeq = ready.messageSeq
        invalidReason = null
    }.onFailure { invalidReason = it.message }

    fun accepts(event: BridgeEvent): Boolean {
        val current = active ?: return false
        val session = event.runtimeSessionId ?: return false
        if (session != current.runtimeSessionId) return false
        if (!matchesIdentity(event, current)) return false
        val seq = event.messageSeq ?: return true
        if (seq <= lastMessageSeq) return false
        lastMessageSeq = seq
        return true
    }

    private fun matchesIdentity(
        event: BridgeEvent,
        current: BridgeEvent.RuntimeReady,
    ): Boolean = when (event) {
        is BridgeEvent.ObservationEvent -> event.pid == current.pid &&
            event.processName == current.processName &&
            event.packageName == current.packageName &&
            event.buildFingerprint == current.buildFingerprint
        is BridgeEvent.BindingLost -> event.pid == current.pid && event.processName == current.processName
        else -> true
    }

    fun invalidate(reason: String) {
        invalidReason = reason
        active = null
        lastMessageSeq = 0L
    }
}
