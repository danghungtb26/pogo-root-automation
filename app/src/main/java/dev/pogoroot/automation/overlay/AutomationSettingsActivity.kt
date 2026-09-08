package dev.pogoroot.automation.overlay

import android.app.Activity
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import dev.pogoroot.automation.core.time.TeleportCooldownMode
import dev.pogoroot.automation.headless.AutomationConfigRepository

class AutomationSettingsActivity : Activity() {
    companion object {
        private const val SETTINGS_CONTAINER_ID = 0x2001
    }

    lateinit var repository: AutomationConfigRepository
        private set

    private lateinit var positionStore: OverlayPositionStore
    private lateinit var titleView: TextView
    private lateinit var backView: TextView
    private lateinit var saveView: TextView
    private var cooldownMode = TeleportCooldownMode.CURRENT_POSITION

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        repository = AutomationConfigRepository(this)
        positionStore = OverlayPositionStore(this)
        cooldownMode = positionStore.loadCooldownMode()
        setContentView(buildContent())

        fragmentManager.addOnBackStackChangedListener { updateToolbar() }
        if (savedInstanceState == null) {
            fragmentManager.beginTransaction()
                .replace(SETTINGS_CONTAINER_ID, AutomationSettingsListFragment())
                .commit()
        }
        updateToolbar()
    }

    override fun onBackPressed() {
        if (fragmentManager.backStackEntryCount > 0) {
            fragmentManager.popBackStack()
        } else {
            super.onBackPressed()
        }
    }

    fun openCategory(categoryId: String) {
        fragmentManager.beginTransaction()
            .replace(
                SETTINGS_CONTAINER_ID,
                AutomationCategoryFragment.newInstance(categoryId),
            )
            .addToBackStack(categoryId)
            .commit()
    }

    fun currentCooldownMode(): TeleportCooldownMode = cooldownMode

    fun setCooldownMode(mode: TeleportCooldownMode) {
        cooldownMode = mode
        positionStore.persistCooldownMode(mode)
    }

    private fun saveCurrentCategory() {
        val fragment = fragmentManager.findFragmentById(SETTINGS_CONTAINER_ID)
            as? AutomationCategoryFragment ?: return
        fragment.save()
        fragmentManager.popBackStack()
    }

    private fun buildContent(): View = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL

        addView(buildToolbar())
        addView(FrameLayout(context).apply {
            id = SETTINGS_CONTAINER_ID
        }, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            0,
            1f,
        ))
    }

    private fun buildToolbar(): View = LinearLayout(this).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        minimumHeight = dp(56)
        setPadding(dp(8), 0, dp(8), 0)

        backView = TextView(context).apply {
            text = "‹"
            textSize = 34f
            gravity = Gravity.CENTER
            contentDescription = "Back"
            setOnClickListener { onBackPressed() }
        }
        addView(backView, LinearLayout.LayoutParams(dp(48), dp(56)))

        titleView = TextView(context).apply {
            textSize = 20f
            gravity = Gravity.CENTER_VERTICAL
        }
        addView(titleView, LinearLayout.LayoutParams(0, dp(56), 1f))

        saveView = TextView(context).apply {
            text = "Save"
            textSize = 16f
            gravity = Gravity.CENTER
            contentDescription = "Save settings"
            setOnClickListener { saveCurrentCategory() }
        }
        addView(saveView, LinearLayout.LayoutParams(dp(64), dp(56)))
    }

    private fun updateToolbar() {
        if (!::titleView.isInitialized) return
        val current = fragmentManager.findFragmentById(SETTINGS_CONTAINER_ID)
        val isCategory = current is AutomationCategoryFragment
        titleView.text = if (isCategory) {
            AutomationSettingsCatalog.titleFor(current.categoryId)
        } else {
            "Automation Settings"
        }
        backView.visibility = if (isCategory) View.VISIBLE else View.INVISIBLE
        saveView.visibility = if (isCategory) View.VISIBLE else View.INVISIBLE
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()
}
