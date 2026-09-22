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
    testImplementation(kotlin("test-junit"))
    implementation(files("libs/virtualjoystick-1.10.1.aar"))

    implementation(project(":core"))
    implementation(project(":bridge:protocol"))
}
