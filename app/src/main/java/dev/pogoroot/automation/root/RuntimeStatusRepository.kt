package dev.pogoroot.automation.root

import dev.pogoroot.automation.bridge.RuntimeConnectionState
import dev.pogoroot.automation.bridge.RuntimeSnapshot
import dev.pogoroot.automation.bridge.RuntimeSnapshotParser
import dev.pogoroot.automation.adapter.GameBuild

class RuntimeStatusRepository(
    private val rootShell: RootShell = ProcessRootShell(),
) {
    fun read(): RuntimeSnapshot {
        val result = rootShell.execute(
            "sh /data/adb/modules/pogo_root_automation/bin/runtime-status.sh",
        )

        if (!result.isSuccess) {
            val detail = result.stderr.trim().ifEmpty { "root runtime command failed" }
            return RuntimeSnapshot(
                protocolVersion = null,
                state = RuntimeConnectionState.ERROR,
                pid = null,
                processName = null,
                packageName = null,
                gameVersionName = null,
                gameVersionCode = null,
                error = detail,
            )
        }

        return RuntimeSnapshotParser.parse(result.stdout)
    }
}

fun RuntimeSnapshot.toGameBuild(): GameBuild? = packageName?.let { packageName ->
    GameBuild(
        packageName = packageName,
        versionName = gameVersionName,
        versionCode = gameVersionCode,
        engine = bindingEngine,
        bindingStrategy = bindingStrategy,
        devicePrimaryAbi = devicePrimaryAbi,
        kernelMachine = kernelMachine,
        translationLayer = translationLayer,
        il2cppBuildId = nativeIl2cppBuildId,
        metadataHash = nativeMetadataHash,
        apkDigest = apkDigest,
    )
}
