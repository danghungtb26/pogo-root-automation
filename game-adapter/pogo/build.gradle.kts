plugins {
    id("org.jetbrains.kotlin.jvm")
}

kotlin {
    jvmToolchain(17)
}

dependencies {
    val pogoProtos = files(rootProject.file("app/libs/POGOProtos-2.60.8.jar"))

    implementation(project(":core"))
    implementation(project(":game-adapter:api"))
    implementation("com.google.protobuf:protobuf-java:3.23.0")

    compileOnly(pogoProtos)
    testImplementation(pogoProtos)
    testImplementation("junit:junit:4.13.2")
}
