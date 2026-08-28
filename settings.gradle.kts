pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "pogo-root-automation"

include(":app")
include(":core")
include(":bridge:protocol")
include(":game-adapter:api")
include(":game-adapter:fake")
include(":game-adapter:pogo")
