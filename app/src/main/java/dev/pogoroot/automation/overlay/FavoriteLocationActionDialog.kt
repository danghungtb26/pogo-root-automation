package dev.pogoroot.automation.overlay

import android.app.AlertDialog
import android.content.Context
import android.view.ContextThemeWrapper
import dev.pogoroot.automation.data.FavoriteLocation

internal class FavoriteLocationActionDialog(
    private val context: Context,
    private val favorite: FavoriteLocation,
    private val onTeleport: () -> Unit,
    private val onWalk: () -> Unit,
) {
    fun create(): AlertDialog {
        val themedContext = ContextThemeWrapper(
            context,
            android.R.style.Theme_Material_Light_Dialog_Alert,
        )
        return AlertDialog.Builder(themedContext)
            .setTitle(favorite.name)
            .setItems(arrayOf("Teleport", "Walk")) { _, which ->
                when (which) {
                    0 -> onTeleport()
                    1 -> onWalk()
                }
            }
            .setNegativeButton("Cancel", null)
            .create()
            .also { it.window?.setType(overlayWindowType()) }
    }
}
