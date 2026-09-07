package dev.pogoroot.automation.adapter

data class GameBuild(
    val packageName: String,
    val versionName: String?,
    val versionCode: Long?,
)

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
