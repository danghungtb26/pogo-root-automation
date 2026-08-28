package dev.pogoroot.automation.bridge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class RuntimeSnapshotParserTest {
    @Test
    fun `parses connected runtime status and binding probe`() {
        val snapshot = RuntimeSnapshotParser.parse(
            """
            protocol=1
            runtime_state=connected
            pid=4242
            process=com.nianticlabs.pokemongo
            package=com.nianticlabs.pokemongo
            version_name=0.999.1
            version_code=2026082801
            probe_state=ready
            binding_engine=il2cpp
            process_exe=/system/bin/app_process64
            libil2cpp_path=/data/app/example/lib/x86_64/libil2cpp.so
            libunity_path=/data/app/example/lib/x86_64/libunity.so
            libmain_path=/data/app/example/lib/x86_64/libmain.so
            translation_layer=houdini
            device_primary_abi=x86_64
            device_supported_abis=x86_64,arm64-v8a
            native_bridge=libhoudini.so
            zygote=zygote64_32
            kernel_machine=x86_64
            """.trimIndent(),
        )

        assertEquals(RuntimeConnectionState.CONNECTED, snapshot.state)
        assertEquals(4242, snapshot.pid)
        assertEquals("com.nianticlabs.pokemongo", snapshot.processName)
        assertEquals("0.999.1", snapshot.gameVersionName)
        assertEquals(2026082801L, snapshot.gameVersionCode)
        assertEquals(BindingProbeState.READY, snapshot.bindingProbeState)
        assertEquals("il2cpp", snapshot.bindingEngine)
        assertEquals("/data/app/example/lib/x86_64/libil2cpp.so", snapshot.il2cppPath)
        assertEquals("houdini", snapshot.translationLayer)
        assertEquals("x86_64", snapshot.devicePrimaryAbi)
        assertEquals(listOf("x86_64", "arm64-v8a"), snapshot.deviceSupportedAbis)
        assertEquals("libhoudini.so", snapshot.nativeBridge)
        assertEquals("x86_64", snapshot.kernelMachine)
    }

    @Test
    fun `parses not seen without inventing pid or engine`() {
        val snapshot = RuntimeSnapshotParser.parse(
            """
            protocol=1
            runtime_state=not_seen
            pid=0
            process=
            probe_state=not_running
            binding_engine=unknown
            translation_layer=none
            """.trimIndent(),
        )

        assertEquals(RuntimeConnectionState.NOT_SEEN, snapshot.state)
        assertEquals(BindingProbeState.NOT_RUNNING, snapshot.bindingProbeState)
        assertNull(snapshot.pid)
        assertNull(snapshot.processName)
        assertNull(snapshot.bindingEngine)
        assertNull(snapshot.translationLayer)
    }

    @Test
    fun `unity without il2cpp remains a non-ready state`() {
        val snapshot = RuntimeSnapshotParser.parse(
            """
            runtime_state=connected
            probe_state=unity_loaded
            binding_engine=unity_unknown_backend
            libunity_path=/data/app/libunity.so
            """.trimIndent(),
        )

        assertEquals(BindingProbeState.UNITY_LOADED, snapshot.bindingProbeState)
        assertEquals("unity_unknown_backend", snapshot.bindingEngine)
        assertEquals("/data/app/libunity.so", snapshot.unityPath)
        assertNull(snapshot.il2cppPath)
    }

    @Test
    fun `unknown state fails closed as error`() {
        val snapshot = RuntimeSnapshotParser.parse("runtime_state=surprise")

        assertEquals(RuntimeConnectionState.ERROR, snapshot.state)
        assertEquals(BindingProbeState.UNKNOWN, snapshot.bindingProbeState)
    }
}
