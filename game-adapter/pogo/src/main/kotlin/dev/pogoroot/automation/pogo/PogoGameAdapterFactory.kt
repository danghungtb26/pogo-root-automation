package dev.pogoroot.automation.pogo

import dev.pogoroot.automation.adapter.GameAdapter
import dev.pogoroot.automation.adapter.GameAdapterFactory
import dev.pogoroot.automation.adapter.GameBuild

/**
 * Version-scoped factory. An empty allowlist intentionally matches nothing;
 * callers must pin a verified build fingerprint before enabling mutation.
 */
class PogoGameAdapterFactory(
    private val supportedBuildFingerprints: Set<String>,
    private val runtimeSourceFactory: () -> PogoRuntimeSource,
    private val actionExecutorFactory: (PogoRuntimeSource) -> PogoActionExecutor? = { null },
    private val requireStrongBuildIdentity: Boolean = true,
    override val id: String = "pogo-runtime-v1",
) : GameAdapterFactory {
    override fun supports(build: GameBuild): Boolean =
            build.packageName in SUPPORTED_PACKAGES &&
            build.engine == "il2cpp" &&
            build.bindingStrategy != null &&
            build.bindingStrategy != "unavailable" &&
            (!requireStrongBuildIdentity || build.hasStrongIdentity()) &&
            build.fingerprint() in supportedBuildFingerprints

    private fun GameBuild.hasStrongIdentity(): Boolean =
        !il2cppBuildId.isNullOrBlank() || !metadataHash.isNullOrBlank() || !apkDigest.isNullOrBlank()

    override fun create(): GameAdapter {
        val source = runtimeSourceFactory()
        return PogoGameAdapter(
            runtimeSource = source,
            actionExecutor = actionExecutorFactory(source),
        )
    }

    companion object {
        val SUPPORTED_PACKAGES = setOf(
            "com.nianticlabs.pokemongo",
            "com.nianticlabs.pokemongo.ares",
        )
    }
}
