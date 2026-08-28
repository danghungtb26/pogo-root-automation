package dev.pogoroot.automation.adapter

import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.NearbySnapshot
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class GameAdapterRegistryTest {
    private val build = GameBuild(
        packageName = "com.nianticlabs.pokemongo",
        versionName = "1.2.3",
        versionCode = 123L,
    )

    @Test
    fun `returns unsupported when no adapter matches`() {
        val resolution = GameAdapterRegistry(emptyList()).resolve(build)

        assertEquals(AdapterResolution.Unsupported, resolution)
    }

    @Test
    fun `resolves exactly one matching adapter`() {
        val expected = factory("v123") { it.versionCode == 123L }
        val resolution = GameAdapterRegistry(listOf(expected)).resolve(build)

        assertTrue(resolution is AdapterResolution.Resolved)
        assertEquals(expected, (resolution as AdapterResolution.Resolved).factory)
    }

    @Test
    fun `fails closed when more than one adapter matches`() {
        val resolution = GameAdapterRegistry(
            listOf(
                factory("b") { true },
                factory("a") { true },
            ),
        ).resolve(build)

        assertEquals(
            AdapterResolution.Ambiguous(listOf("a", "b")),
            resolution,
        )
    }

    @Test
    fun `fingerprint differentiates translated BlueStacks from native arm`() {
        val nativeArm = GameBuild(
            packageName = "com.nianticlabs.pokemongo",
            versionName = "1.2.3",
            versionCode = 123L,
            engine = "il2cpp",
            bindingStrategy = "il2cpp_exported_api",
            devicePrimaryAbi = "arm64-v8a",
            kernelMachine = "aarch64",
        )
        val translatedBlueStacks = nativeArm.copy(
            devicePrimaryAbi = "x86_64",
            kernelMachine = "x86_64",
            translationLayer = "houdini",
        )

        assertEquals(
            "com.nianticlabs.pokemongo|123|il2cpp|il2cpp_exported_api|arm64-v8a|aarch64|native",
            nativeArm.fingerprint(),
        )
        assertEquals(
            "com.nianticlabs.pokemongo|123|il2cpp|il2cpp_exported_api|x86_64|x86_64|houdini",
            translatedBlueStacks.fingerprint(),
        )
    }

    private fun factory(
        factoryId: String,
        predicate: (GameBuild) -> Boolean,
    ) = object : GameAdapterFactory {
        override val id: String = factoryId

        override fun supports(build: GameBuild): Boolean = predicate(build)

        override fun create(): GameAdapter = object : GameAdapter {
            override val id: String = factoryId
            override val capabilities: Set<GameCapability> = emptySet()
            override fun connect(): Result<Unit> = Result.success(Unit)
            override fun disconnect() = Unit
            override fun lifecycleState(): GameLifecycleState = GameLifecycleState.DISCONNECTED
            override fun readNearby(): Result<NearbySnapshot> = Result.failure(UnsupportedOperationException())
        }
    }
}
