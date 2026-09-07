plugins {
    id("com.android.application")
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
}

dependencies {
    implementation(fileTree(mapOf("dir" to "libs", "include" to listOf("*.jar", "*.aar"))))
    implementation("com.google.protobuf:protobuf-java:3.23.0")

    implementation(project(":core"))
    implementation(project(":bridge:protocol"))
    implementation(project(":game-adapter:api"))
    implementation(project(":game-adapter:fake"))
    implementation(project(":game-adapter:pogo"))
}
