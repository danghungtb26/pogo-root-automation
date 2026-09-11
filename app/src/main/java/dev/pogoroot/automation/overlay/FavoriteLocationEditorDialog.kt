package dev.pogoroot.automation.overlay

import android.app.AlertDialog
import android.content.Context
import android.text.InputType
import android.view.ContextThemeWrapper
import android.view.ViewGroup
import android.widget.EditText
import android.widget.LinearLayout
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.data.FavoriteLocation
import java.util.Locale

internal class FavoriteLocationEditorDialog(
    private val context: Context,
    private val currentPoint: () -> GeoPoint?,
    private val onSave: (String, GeoPoint) -> Result<FavoriteLocation>,
    private val onSaved: () -> Unit,
) {
    fun create(): AlertDialog {
        val themedContext = ContextThemeWrapper(
            context,
            android.R.style.Theme_Material_Light_Dialog_Alert,
        )
        val nameInput = EditText(themedContext).apply {
            hint = "Name"
            setSingleLine(true)
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
        }
        val coordinateInput = EditText(themedContext).apply {
            hint = "latitude, longitude"
            setSingleLine(true)
            inputType = InputType.TYPE_CLASS_TEXT
            currentPoint()?.let { point ->
                setText(String.format(Locale.US, "%.6f, %.6f", point.latitude, point.longitude))
                setSelection(text.length)
            }
        }
        val form = LinearLayout(themedContext).apply {
            orientation = LinearLayout.VERTICAL
            addView(nameInput, LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
            ))
            addView(coordinateInput, LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
            ).apply {
                topMargin = themedContext.dp(8)
            })
        }

        val dialog = AlertDialog.Builder(themedContext)
            .setTitle("Add favorite")
            .setMessage("Save a location for quick access")
            .setView(form)
            .setPositiveButton("Save", null)
            .setNegativeButton("Cancel", null)
            .create()
        dialog.window?.setType(overlayWindowType())
        dialog.setOnShowListener {
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
                val name = nameInput.text.toString().trim()
                val point = parsePoint(coordinateInput.text.toString())
                if (name.isBlank()) {
                    nameInput.error = "Enter a name"
                    return@setOnClickListener
                }
                if (point == null) {
                    coordinateInput.error = "Use: latitude, longitude"
                    return@setOnClickListener
                }

                onSave(name, point)
                    .onSuccess {
                        onSaved()
                        dialog.dismiss()
                    }
                    .onFailure { error ->
                        nameInput.error = error.message ?: "Could not save favorite"
                    }
            }
        }
        return dialog
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
