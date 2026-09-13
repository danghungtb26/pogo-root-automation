package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.GeoPoint

/** Location-service commands. Target selection and automation policy are native-owned. */
sealed interface AutoFortNavigationCommand {
    data class WalkTo(val fortId: String, val target: GeoPoint) : AutoFortNavigationCommand
    data class Stop(val reason: String) : AutoFortNavigationCommand
}
