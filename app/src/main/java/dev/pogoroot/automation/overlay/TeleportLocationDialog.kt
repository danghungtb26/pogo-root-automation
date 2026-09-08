package dev.pogoroot.automation.overlay

import android.app.AlertDialog
import android.content.Context
import android.view.ContextThemeWrapper
import android.widget.EditText
import dev.pogoroot.automation.core.model.GeoPoint
import java.util.Locale

internal class TeleportLocationDialog(
    private val context: Context,
    private val currentPoint: () -> GeoPoint?,
    private val onTeleport: (GeoPoint) -> Unit,
) {
    fun show() {
        val themedContext = ContextThemeWrapper(
            context,
            android.R.style.Theme_Material_Light_Dialog_Alert,
        )
        val input = EditText(themedContext).apply {
            hint = "21.0285, 105.8542"
            setSingleLine(true)
            currentPoint()?.let { point ->
                setText(String.format(Locale.US, "%.6f, %.6f", point.latitude, point.longitude))
                setSelection(text.length)
            }
        }
        val dialog = AlertDialog.Builder(themedContext)
            .setTitle("Change location")
            .setMessage("Enter latitude, longitude")
            .setView(input)
            .setPositiveButton("Teleport", null)
            .setNegativeButton("Cancel", null)
            .create()
        dialog.window?.setType(overlayWindowType())
        dialog.setOnShowListener {
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
                val point = parsePoint(input.text.toString())
                if (point == null) {
                    input.error = "Use: latitude, longitude"
                } else {
                    onTeleport(point)
                    dialog.dismiss()
                }
            }
        }
        dialog.show()
    }

    private fun parsePoint(raw: String): GeoPoint? {
        val parts = raw.trim().split(',', ' ', ';').filter(String::isNotBlank)
        if (parts.size != 2) return null
        val latitude = parts[0].toDoubleOrNull() ?: return null
        val longitude = parts[1].toDoubleOrNull() ?: return null
        if (latitude !in -90.0..90.0 || longitude !in -180.0..180.0) return null
        return GeoPoint(latitude, longitude)
    }
}
