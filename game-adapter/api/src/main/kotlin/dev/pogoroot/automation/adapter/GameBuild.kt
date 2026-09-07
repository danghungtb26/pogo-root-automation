package dev.pogoroot.automation.adapter

data class GameBuild(
    val packageName: String,
    val versionName: String?,
    val versionCode: Long?,
    val engine: String? = null,
    val bindingStrategy: String? = null,
    val devicePrimaryAbi: String? = null,
    val kernelMachine: String? = null,
    val translationLayer: String? = null,
    val il2cppBuildId: String? = null,
    val metadataHash: String? = null,
    val apkDigest: String? = null,
) {
    fun fingerprint(): String {
        val base = listOf(
        packageName,
        versionCode?.toString() ?: versionName.orEmpty().ifBlank { "unknown-version" },
        engine.orEmpty().ifBlank { "unknown-engine" },
        bindingStrategy.orEmpty().ifBlank { "unknown-strategy" },
        devicePrimaryAbi.orEmpty().ifBlank { "unknown-abi" },
        kernelMachine.orEmpty().ifBlank { "unknown-kernel" },
        translationLayer.orEmpty().ifBlank { "native" },
        ).joinToString("|")

        val strongIdentity = listOfNotNull(
            il2cppBuildId?.takeIf(String::isNotBlank)?.let { "il2cpp=$it" },
            metadataHash?.takeIf(String::isNotBlank)?.let { "metadata=$it" },
            apkDigest?.takeIf(String::isNotBlank)?.let { "apk=$it" },
        )
        return if (strongIdentity.isEmpty()) base else "$base|${strongIdentity.joinToString("|")}"
    }
}

interface GameAdapterFactory {
    val id: String

    fun supports(build: GameBuild): Boolean

    fun create(): GameAdapter
}

sealed interface AdapterResolution {
    data class Resolved(
        val factory: GameAdapterFactory,
    ) : AdapterResolution

    data object Unsupported : AdapterResolution

    data class Ambiguous(
        val factoryIds: List<String>,
    ) : AdapterResolution
}

class GameAdapterRegistry(
    factories: List<GameAdapterFactory>,
) {
    private val factories = factories.toList()

    fun resolve(build: GameBuild): AdapterResolution {
        val matches = factories.filter { it.supports(build) }

        return when (matches.size) {
            0 -> AdapterResolution.Unsupported
            1 -> AdapterResolution.Resolved(matches.single())
            else -> AdapterResolution.Ambiguous(matches.map(GameAdapterFactory::id).sorted())
        }
    }
}
