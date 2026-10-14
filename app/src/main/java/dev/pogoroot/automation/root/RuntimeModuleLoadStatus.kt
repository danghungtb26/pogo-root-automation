package dev.pogoroot.automation.root

import dev.pogoroot.automation.bridge.RuntimeFeatureModule

data class RuntimeModuleLoadStatus(
    val runtimeSessionId: String,
    val module: RuntimeFeatureModule,
    val loaded: Boolean,
    val errorCode: String?,
    val message: String?,
)
