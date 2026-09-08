package dev.pogoroot.automation.core.model

/**
 * A target resolved by a version-scoped Pokémon GO map binding.
 *
 * The screen metadata is retained for calibration/debugging. The controller
 * must never infer a target from screen coordinates without a runtime-provided
 * conversion and matching camera snapshot.
 */
data class MapTargetObservation(
    val tapId: String,
    val target: GeoPoint,
    val screenX: Float,
    val screenY: Float,
    val viewportWidth: Int,
    val viewportHeight: Int,
    val cameraSnapshotId: String? = null,
) {
    init {
        require(tapId.isNotBlank()) { "tapId must not be blank" }
        require(target.latitude.isFinite() && target.latitude in -90.0..90.0) {
            "target latitude is invalid"
        }
        require(target.longitude.isFinite() && target.longitude in -180.0..180.0) {
            "target longitude is invalid"
        }
        require(screenX.isFinite() && screenY.isFinite()) {
            "screen coordinates must be finite"
        }
        require(viewportWidth > 0 && viewportHeight > 0) {
            "viewport dimensions must be positive"
        }
        require(screenX in 0f..viewportWidth.toFloat()) {
            "screenX must be inside viewport"
        }
        require(screenY in 0f..viewportHeight.toFloat()) {
            "screenY must be inside viewport"
        }
        require(cameraSnapshotId == null || cameraSnapshotId.isNotBlank()) {
            "cameraSnapshotId must not be blank when present"
        }
    }
}
