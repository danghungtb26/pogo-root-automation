package dev.pogoroot.automation.overlay

import android.app.AlertDialog
import android.content.Context
import android.graphics.Typeface
import android.view.Gravity
import android.view.ContextThemeWrapper
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.ListView
import android.widget.LinearLayout
import android.widget.TextView
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.headless.FavoriteLocation
import dev.pogoroot.automation.headless.FavoriteLocationRepository

internal class FavoriteLocationsDialog(
    private val context: Context,
    private val repository: FavoriteLocationRepository,
    private val currentPoint: () -> GeoPoint?,
    private val onTeleport: (FavoriteLocation) -> Unit,
    private val onWalk: (FavoriteLocation) -> Unit,
    private val onMessage: (String) -> Unit,
    private val onDismissed: () -> Unit,
) {
    private var dialog: AlertDialog? = null
    private var adapter: FavoriteLocationAdapter? = null
    private val childDialogs = mutableSetOf<AlertDialog>()

    fun show() {
        if (dialog?.isShowing == true) return

        val themedContext = ContextThemeWrapper(
            context,
            android.R.style.Theme_Material_Light_Dialog_Alert,
        )
        val listAdapter = FavoriteLocationAdapter(
            context = themedContext,
            currentPoint = currentPoint,
            onOpen = ::showActions,
            onDelete = ::showDelete,
        )
        val list = ListView(themedContext).apply {
            divider = null
            this.adapter = listAdapter
            setPadding(0, themedContext.dp(4), 0, 0)
        }
        val emptyView = TextView(themedContext).apply {
            gravity = Gravity.CENTER
            text = "No favorite locations yet\nTap Add to save one"
            textSize = 15f
            setTextColor(0xFF5F6368.toInt())
            setPadding(
                themedContext.dp(16),
                themedContext.dp(24),
                themedContext.dp(16),
                themedContext.dp(24),
            )
        }
        val listArea = FrameLayout(themedContext).apply {
            addView(list, FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
            ))
            addView(emptyView, FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
            ))
            list.emptyView = emptyView
        }
        val addButton = TextView(themedContext).apply {
            text = "+  Add favorite"
            textSize = 15f
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
            setTextColor(0xFF1967D2.toInt())
            background = themedContext.roundedBackground(0xFFE8F0FE.toInt(), 10)
            setPadding(
                themedContext.dp(12),
                themedContext.dp(10),
                themedContext.dp(12),
                themedContext.dp(10),
            )
            isClickable = true
            isFocusable = true
            contentDescription = "Add favorite location"
            setOnClickListener { showAdd() }
        }
        val content = LinearLayout(themedContext).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(themedContext.dp(12), 0, themedContext.dp(12), 0)
            addView(addButton, LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
            ))
            addView(listArea, LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                themedContext.dp(320),
            ).apply {
                topMargin = themedContext.dp(8)
            })
        }

        val newDialog = AlertDialog.Builder(themedContext)
            .setTitle("Favorite Locations")
            .setView(content)
            .setNegativeButton("Close", null)
            .create()
        dialog = newDialog
        adapter = listAdapter
        newDialog.setOnDismissListener {
            childDialogs.toList().forEach { child -> child.dismiss() }
            childDialogs.clear()
            if (dialog === newDialog) {
                dialog = null
                adapter = null
                onDismissed()
            }
        }
        newDialog.window?.setType(overlayWindowType())
        newDialog.setOnShowListener { refresh() }
        newDialog.show()
    }

    fun refresh() {
        if (dialog?.isShowing != true) return
        adapter?.replace(repository.read())
    }

    fun dismiss() {
        dialog?.dismiss()
    }

    private fun showActions(favorite: FavoriteLocation) {
        showChild(
            FavoriteLocationActionDialog(
                context = context,
                favorite = favorite,
                onTeleport = { onTeleport(favorite) },
                onWalk = { onWalk(favorite) },
            ).create(),
        )
    }

    private fun showAdd() {
        showChild(
            FavoriteLocationEditorDialog(
                context = context,
                currentPoint = currentPoint,
                onSave = repository::add,
                onSaved = {
                    refresh()
                    onMessage("Favorite saved")
                },
            ).create(),
        )
    }

    private fun showDelete(favorite: FavoriteLocation) {
        showChild(
            FavoriteLocationDeleteDialog(
                context = context,
                favorite = favorite,
                onDelete = { repository.delete(favorite.id) },
                onDeleted = {
                    refresh()
                    onMessage("Favorite deleted")
                },
            ).create(),
        )
    }

    private fun showChild(child: AlertDialog) {
        childDialogs += child
        child.setOnDismissListener { childDialogs.remove(child) }
        child.show()
    }
}
