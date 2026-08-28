package dev.pogoroot.automation.bridge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class RuntimeSnapshotParserTest {
    @Test
    fun `parses connected runtime status with assembly and class survey`() {
        val snapshot = RuntimeSnapshotParser.parse(
            """
            protocol=4
            runtime_state=connected
            pid=4242
            process=com.nianticlabs.pokemongo
            package=com.nianticlabs.pokemongo
            version_name=0.999.1
            version_code=2026082801
            probe_state=ready
            binding_engine=il2cpp
            binding_strategy=il2cpp_exported_api
            translation_layer=houdini
            native_probe_state=complete
            native_il2cpp_api_available=1
            native_il2cpp_symbol_count=14
            native_il2cpp_required_symbol_count=10
            native_assembly_survey_state=complete
            native_assembly_count=137
            native_assembly_csharp_found=1
            native_assembly_csharp_name=Assembly-CSharp.dll
            native_class_survey_state=complete
            native_class_count=4312
            native_candidate_class_count=3
            native_candidate_classes=Game.MapService;Game.WildPokemon;Game.FortManager
            device_primary_abi=x86_64
            device_supported_abis=x86_64,arm64-v8a
            native_bridge=libhoudini.so
            kernel_machine=x86_64
            """.trimIndent(),
        )

        assertEquals(RuntimeConnectionState.CONNECTED, snapshot.state)
        assertEquals(4, snapshot.protocolVersion)
        assertEquals("il2cpp_exported_api", snapshot.bindingStrategy)
        assertTrue(snapshot.il2cppApiAvailable)
        assertEquals(14, snapshot.il2cppSymbolCount)
        assertEquals(10, snapshot.il2cppRequiredSymbolCount)
        assertEquals("complete", snapshot.assemblySurveyState)
        assertEquals(137, snapshot.assemblyCount)
        assertTrue(snapshot.assemblyCSharpFound)
        assertEquals("Assembly-CSharp.dll", snapshot.assemblyCSharpName)
        assertEquals("complete", snapshot.classSurveyState)
        assertEquals(4312, snapshot.classCount)
        assertEquals(
            listOf("Game.MapService", "Game.WildPokemon", "Game.FortManager"),
            snapshot.candidateClasses,
        )
        assertEquals("houdini", snapshot.translationLayer)
        assertEquals(listOf("x86_64", "arm64-v8a"), snapshot.deviceSupportedAbis)
    }

    @Test
    fun `mapped only strategy has no class survey`() {
        val snapshot = RuntimeSnapshotParser.parse(
            """
            runtime_state=connected
            probe_state=ready
            binding_engine=il2cpp
            binding_strategy=il2cpp_mapped_only
            native_probe_state=complete
            native_il2cpp_api_available=0
            native_il2cpp_symbol_count=6
            native_il2cpp_required_symbol_count=10
            native_assembly_survey_state=unavailable
            native_class_survey_state=unavailable
            native_candidate_classes=
            """.trimIndent(),
        )

        assertFalse(snapshot.il2cppApiAvailable)
        assertEquals(6, snapshot.il2cppSymbolCount)
        assertEquals(10, snapshot.il2cppRequiredSymbolCount)
        assertEquals("unavailable", snapshot.classSurveyState)
        assertTrue(snapshot.candidateClasses.isEmpty())
    }

    @Test
    fun `not seen remains fail closed`() {
        val snapshot = RuntimeSnapshotParser.parse(
            """
            protocol=4
            runtime_state=not_seen
            pid=0
            process=
            probe_state=not_running
            binding_engine=unknown
            binding_strategy=unavailable
            translation_layer=none
            """.trimIndent(),
        )

        assertEquals(RuntimeConnectionState.NOT_SEEN, snapshot.state)
        assertEquals(BindingProbeState.NOT_RUNNING, snapshot.bindingProbeState)
        assertNull(snapshot.pid)
        assertNull(snapshot.bindingEngine)
        assertNull(snapshot.bindingStrategy)
        assertNull(snapshot.translationLayer)
    }
}
