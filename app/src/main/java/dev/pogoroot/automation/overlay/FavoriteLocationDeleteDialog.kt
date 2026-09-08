package dev.pogoroot.automation.overlay

import android.app.AlertDialog
import android.content.Context
import android.view.ContextThemeWrapper
import dev.pogoroot.automation.headless.FavoriteLocation

internal class FavoriteLocationDeleteDialog(
    private val context: Context,
    private val favorite: FavoriteLocation,
    private val onDelete: () -> Boolean,
    private val onDeleted: () -> Unit,
) {
    fun create(): AlertDialog {
        val themedContext = ContextThemeWrapper(
            context,
            android.R.style.Theme_Material_Light_Dialog_Alert,
        )
        return AlertDialog.Builder(themedContext)
            .setTitle("Delete favorite?")
            .setMessage(favorite.name)
            .setNegativeButton("Cancel", null)
            .setPositiveButton("Delete") { _, _ ->
                if (onDelete()) onDeleted()
            }
            .create()
            .also { it.window?.setType(overlayWindowType()) }
    }
}
