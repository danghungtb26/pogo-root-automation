import groovy.json.JsonSlurper
import java.security.MessageDigest

plugins {
    id("com.android.application")
}

val canonicalModuleZip = rootProject.layout.buildDirectory.file(
    "pogo-root-automation-magisk-multiabi.zip",
)
val canonicalModuleManifest = rootProject.layout.buildDirectory.file(
    "pogo-root-automation-magisk-multiabi.manifest.json",
)
val generatedPogoModuleRoot = layout.buildDirectory.dir("generated/pogoModule")
val generatedPogoModuleAssets = generatedPogoModuleRoot.map { it.dir("assets") }
val generatedPogoModuleResources = generatedPogoModuleRoot.map { it.dir("res") }

fun sha256(file: File): String {
    val digest = MessageDigest.getInstance("SHA-256")
    file.inputStream().buffered().use { input ->
        val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
        while (true) {
            val count = input.read(buffer)
            if (count < 0) break
            digest.update(buffer, 0, count)
        }
    }
    return digest.digest().joinToString("") { "%02x".format(it) }
}

val prepareBundledPogoModule = tasks.register("prepareBundledPogoModule") {
    group = "build"
    description = "Validate and bundle the canonical Magisk module into the controller APK."
    inputs.file(canonicalModuleZip)
    inputs.file(canonicalModuleManifest)
    inputs.files(
        rootProject.fileTree("zygisk/jni"),
        rootProject.fileTree("zygisk/module"),
        rootProject.files(
            rootProject.file("scripts/build-magisk.sh"),
            rootProject.file("scripts/package-magisk.sh"),
        ),
    )
    outputs.dir(generatedPogoModuleRoot)

    doLast {
        val zipFile = canonicalModuleZip.get().asFile
        val manifestFile = canonicalModuleManifest.get().asFile
        check(zipFile.isFile && zipFile.length() > 0L) {
            "Missing canonical Magisk ZIP. Run scripts/build-magisk.sh first: $zipFile"
        }
        check(manifestFile.isFile && manifestFile.length() > 0L) {
            "Missing module manifest. Run scripts/build-magisk.sh first: $manifestFile"
        }
        val manifest = JsonSlurper().parse(manifestFile) as Map<*, *>
        check(manifest["schemaVersion"].toString() == "1") { "Unsupported module manifest schema" }
        check(manifest["moduleId"] == "pogo_root_automation") { "Unexpected module id" }
        check(manifest["zipFile"] == zipFile.name) { "Module manifest ZIP name mismatch" }
        check(manifest["zipSha256"] == sha256(zipFile)) { "Module ZIP SHA-256 mismatch" }
        val sourceRevision = providers.exec {
            commandLine("git", "-C", rootProject.projectDir, "rev-parse", "HEAD")
        }.standardOutput.asText.get().trim()
        check(manifest["sourceRevision"] == sourceRevision) {
            "Module artifact is from a different source revision"
        }
        val nativeInputs = inputs.files.files.filter { it.isFile }
        val newestNativeInput = nativeInputs.maxOfOrNull(File::lastModified) ?: 0L
        check(newestNativeInput <= manifestFile.lastModified()) {
            "Module artifact is stale; run scripts/build-magisk.sh again"
        }

        val root = generatedPogoModuleRoot.get().asFile
        project.delete(root)
        val assetDirectory = root.resolve("assets/pogo-module").apply { mkdirs() }
        zipFile.copyTo(assetDirectory.resolve(zipFile.name), overwrite = true)
        manifestFile.copyTo(assetDirectory.resolve(manifestFile.name), overwrite = true)

        val valuesDirectory = root.resolve("res/values").apply { mkdirs() }
        valuesDirectory.resolve("pogo_module.xml").writeText(
            """<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="pogo_module_id">${manifest["moduleId"]}</string>
    <string name="pogo_module_version">${manifest["moduleVersion"]}</string>
    <string name="pogo_module_sha256">${manifest["zipSha256"]}</string>
</resources>
""",
        )
    }
}

android {
    namespace = "dev.pogoroot.automation"
    compileSdk = 36

    defaultConfig {
        applicationId = "dev.pogoroot.automation"
        minSdk = 28
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
    }

    sourceSets.getByName("main") {
        assets.directories.add(generatedPogoModuleAssets.get().asFile.absolutePath)
        res.directories.add(generatedPogoModuleResources.get().asFile.absolutePath)
    }
}

tasks.named("preBuild").configure {
    dependsOn(prepareBundledPogoModule)
}

dependencies {
    testImplementation(kotlin("test-junit"))
    implementation(files("libs/virtualjoystick-1.10.1.aar"))

    implementation(project(":core"))
    implementation(project(":bridge:protocol"))
}
