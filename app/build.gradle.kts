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
    implementation(project(":core"))
    implementation(project(":bridge:protocol"))
    implementation(project(":game-adapter:api"))
    implementation(project(":game-adapter:fake"))
}
