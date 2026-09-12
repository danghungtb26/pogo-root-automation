package dev.pogoroot.automation.overlay

import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.provider.Settings
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.TextView
import android.util.Log
import java.lang.ref.WeakReference
import java.util.ArrayDeque

/**
 * App-owned transient message surface. It is a real View hosted either in the
 * current Activity or in an overlay window; android.widget.Toast is never used.
 */
internal object CustomToast {
    const val SHORT_DURATION_MS = 2_200L
    const val LONG_DURATION_MS = 3_800L

    private const val ANIMATION_MS = 180L
    private const val BOTTOM_MARGIN_DP = 96
    private const val HORIZONTAL_MARGIN_DP = 24
    private const val MAX_QUEUE_SIZE = 32
    private const val LOG_TAG = "PogoRootAutomation"

    private val mainHandler = android.os.Handler(android.os.Looper.getMainLooper())
    private val pending = ArrayDeque<Request>()
    private var active = false
    private var activeView: View? = null
    private var activeParent: ViewGroup? = null
    private var activeWindowManager: WindowManager? = null
    private var finishRunnable: Runnable? = null

    fun show(
        context: Context,
        message: String,
        durationMs: Long = SHORT_DURATION_MS,
        accentColor: Int = 0xFF8AB4F8.toInt(),
    ) {
        if (message.isBlank()) return
        mainHandler.post {
            while (pending.size >= MAX_QUEUE_SIZE) pending.removeFirst()
            val activity = context as? Activity
            pending.addLast(
                Request(
                    activity = activity?.let { WeakReference<Activity>(it) },
                    applicationContext = context.applicationContext,
                    message = message,
                    durationMs = durationMs.coerceAtLeast(0L),
                    accentColor = accentColor,
                ),
            )
            showNextIfIdle()
        }
    }

    private fun showNextIfIdle() {
        if (active) return
        if (pending.isEmpty()) return
        val request = pending.removeFirst()
        val activity = request.activity?.get()?.let { candidate ->
            candidate.takeUnless { it.isFinishing || it.isDestroyed }
        }
        val card = createCard(request.applicationContext, request.message, request.accentColor)
        val mounted = if (activity != null) {
            mountInActivity(activity, card)
        } else {
            mountInOverlay(request.applicationContext, card)
        }
        if (!mounted) {
            Log.w(LOG_TAG, "custom toast could not mount message=${request.message}")
            mainHandler.post(::showNextIfIdle)
            return
        }

        active = true
        activeView = card
        card.alpha = 0f
        card.translationY = card.context.dp(12).toFloat()
        card.animate()
            .alpha(1f)
            .translationY(0f)
            .setDuration(ANIMATION_MS)
            .withEndAction {
                finishRunnable = Runnable { dismissActive(card) }.also { runnable ->
                    mainHandler.postDelayed(runnable, request.durationMs)
                }
            }
            .start()
    }

    private fun createCard(context: Context, message: String, accentColor: Int): FrameLayout =
        FrameLayout(context).apply {
            background = context.roundedBackground(0xEE202124.toInt(), 16)
            elevation = context.dp(8).toFloat()
            setPadding(context.dp(14), context.dp(10), context.dp(16), context.dp(10))
            addView(
                View(context).apply {
                    setBackgroundColor(accentColor)
                },
                FrameLayout.LayoutParams(context.dp(3), context.dp(24)).apply {
                    gravity = Gravity.CENTER_VERTICAL
                },
            )
            addView(
                TextView(context).apply {
                    this.text = message
                    textSize = 14f
                    typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
                    setTextColor(Color.WHITE)
                    maxLines = 4
                    setPadding(context.dp(10), 0, 0, 0)
                },
                FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                ),
            )
        }

    private fun mountInActivity(activity: Activity, card: FrameLayout): Boolean {
        val content = activity.findViewById<View>(android.R.id.content) as? ViewGroup ?: return false
        return runCatching {
            content.addView(
                card,
                FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                ).apply {
                    gravity = Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
                    bottomMargin = activity.dp(BOTTOM_MARGIN_DP)
                    leftMargin = activity.dp(HORIZONTAL_MARGIN_DP)
                    rightMargin = activity.dp(HORIZONTAL_MARGIN_DP)
                },
            )
            activeParent = content
            true
        }.getOrDefault(false)
    }

    private fun mountInOverlay(context: Context, card: FrameLayout): Boolean {
        if (!Settings.canDrawOverlays(context)) return false
        val manager = context.getSystemService(WindowManager::class.java) ?: return false
        val params = newOverlayParams(
            ViewGroup.LayoutParams.WRAP_CONTENT,
            ViewGroup.LayoutParams.WRAP_CONTENT,
        ).apply {
            gravity = Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
            flags = flags or WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
            y = context.dp(BOTTOM_MARGIN_DP)
        }
        return runCatching {
            manager.addView(card, params)
            activeWindowManager = manager
            true
        }.getOrDefault(false)
    }

    private fun dismissActive(card: View) {
        if (!active || activeView !== card) return
        finishRunnable?.let(mainHandler::removeCallbacks)
        finishRunnable = null
        card.animate()
            .alpha(0f)
            .translationY(card.context.dp(12).toFloat())
            .setDuration(ANIMATION_MS)
            .withEndAction {
                runCatching { activeParent?.removeView(card) }
                runCatching { activeWindowManager?.removeView(card) }
                activeParent = null
                activeWindowManager = null
                activeView = null
                active = false
                showNextIfIdle()
            }
            .start()
    }

    private data class Request(
        val activity: WeakReference<Activity>?,
        val applicationContext: Context,
        val message: String,
        val durationMs: Long,
        val accentColor: Int,
    )
}
