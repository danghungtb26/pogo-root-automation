package dev.pogoroot.automation.overlay

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.BaseAdapter
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.TextView
import dev.pogoroot.automation.core.location.GeoMath
import dev.pogoroot.automation.core.time.TeleportCooldownService
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.headless.FavoriteLocation
import java.util.Locale

internal class FavoriteLocationAdapter(
    private val context: Context,
    private val currentPoint: () -> GeoPoint?,
    private val onOpen: (FavoriteLocation) -> Unit,
    private val onDelete: (FavoriteLocation) -> Unit,
) : BaseAdapter() {
    private val cooldownService = TeleportCooldownService()
    private var items: List<FavoriteLocation> = emptyList()

    fun replace(next: List<FavoriteLocation>) {
        items = next
        notifyDataSetChanged()
    }

    override fun getCount(): Int = items.size

    override fun getItem(position: Int): FavoriteLocation = items[position]

    override fun getItemId(position: Int): Long = getItem(position).id.hashCode().toLong()

    override fun getView(position: Int, convertView: View?, parent: ViewGroup?): View {
        val favorite = getItem(position)
        val point = currentPoint()
        val distanceMeters = point?.let { GeoMath.distanceMeters(it, favorite.point) }
        val cooldownMillis = distanceMeters?.let {
            cooldownService.cooldownMillisForDistance(it / METERS_PER_KILOMETER)
        }

        val textColumn = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_VERTICAL
        }
        textColumn.addView(TextView(context).apply {
            text = favorite.name
            textSize = 16f
            typeface = Typeface.DEFAULT_BOLD
            setTextColor(Color.BLACK)
        })
        textColumn.addView(TextView(context).apply {
            text = buildString {
                append(String.format(
                    Locale.US,
                    "%.6f, %.6f",
                    favorite.point.latitude,
                    favorite.point.longitude,
                ))
                append("\nDistance: ")
                append(formatDistance(distanceMeters))
                append("  •  Cooldown: ")
                append(formatCooldown(cooldownMillis))
            }
            textSize = 12f
            setTextColor(0xFF5F6368.toInt())
        })

        val deleteButton = ImageButton(context).apply {
            setImageResource(android.R.drawable.ic_menu_delete)
            setBackgroundColor(Color.TRANSPARENT)
            setPadding(context.dp(10), context.dp(10), context.dp(10), context.dp(10))
            contentDescription = "Delete ${favorite.name}"
            setOnClickListener { onDelete(favorite) }
        }

        return LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            minimumHeight = context.dp(78)
            setPadding(context.dp(12), context.dp(8), context.dp(4), context.dp(8))
            isClickable = true
            isFocusable = true
            contentDescription = "Open ${favorite.name}"
            setOnClickListener { onOpen(favorite) }
            addView(
                textColumn,
                LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f),
            )
            addView(deleteButton, LinearLayout.LayoutParams(context.dp(52), context.dp(52)))
        }
    }

    private fun formatDistance(distanceMeters: Double?): String {
        distanceMeters ?: return "—"
        if (!distanceMeters.isFinite() || distanceMeters < 0.0) return "—"
        return if (distanceMeters < METERS_PER_KILOMETER) {
            String.format(Locale.US, "%.0f m", distanceMeters)
        } else {
            String.format(Locale.US, "%.2f km", distanceMeters / METERS_PER_KILOMETER)
        }
    }

    private fun formatCooldown(cooldownMillis: Long?): String {
        cooldownMillis ?: return "—"
        if (cooldownMillis <= 0L) return "Ready"
        val totalMinutes = (cooldownMillis + MILLIS_PER_MINUTE - 1L) / MILLIS_PER_MINUTE
        val hours = totalMinutes / MINUTES_PER_HOUR
        val minutes = totalMinutes % MINUTES_PER_HOUR
        return String.format(Locale.US, "~%02d:%02d (estimate)", hours, minutes)
    }

    private companion object {
        const val METERS_PER_KILOMETER = 1_000.0
        const val MILLIS_PER_MINUTE = 60_000L
        const val MINUTES_PER_HOUR = 60L
    }
}
