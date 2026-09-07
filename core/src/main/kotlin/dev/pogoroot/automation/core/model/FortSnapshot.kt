package dev.pogoroot.automation.core.model

enum class FortType {
    POKESTOP,
    GYM,
}

data class Fort(
    val fortId: String,
    val type: FortType,
    val position: GeoPoint,
    val spinAvailable: Boolean,
    val cooldownEndsAtEpochMs: Long? = null,
)

data class FortSnapshot(
    val observedAtEpochMs: Long,
    val forts: List<Fort>,
)
