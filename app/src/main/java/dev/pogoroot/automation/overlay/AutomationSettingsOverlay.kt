package dev.pogoroot.automation.overlay

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.view.ContextThemeWrapper
import android.view.Gravity
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.view.inputmethod.InputMethodManager
import android.widget.BaseAdapter
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ListView
import android.widget.TextView
import dev.pogoroot.automation.config.AutomationConfigRepository

internal class AutomationSettingsOverlay(
    context: Context,
    private val windowManager: WindowManager,
    private val repository: AutomationConfigRepository,
    private val positionStore: OverlayPositionStore,
    private val onSaved: () -> Unit,
    private val onClosed: () -> Unit,
) {
    private enum class Screen {
        CLOSED,
        LIST,
        CATEGORY,
    }

    private val viewContext = ContextThemeWrapper(
        context,
        android.R.style.Theme_Material_Light_NoActionBar,
    )
    private val marginPx = viewContext.dp(PANEL_MARGIN_DP)
    private var screen = Screen.CLOSED
    private var categoryId: String? = null
    private var editor: AutomationSettingsEditor? = null

    private lateinit var rootView: FrameLayout
    private lateinit var panel: LinearLayout
    private lateinit var contentView: FrameLayout
    private lateinit var titleView: TextView
    private lateinit var backView: TextView
    private lateinit var saveView: TextView
    private lateinit var closeView: TextView
    private lateinit var windowParams: WindowManager.LayoutParams

    fun ensure() {
        if (::rootView.isInitialized) return

        rootView = FrameLayout(viewContext).apply {
            isFocusableInTouchMode = true
            setBackgroundColor(Color.TRANSPARENT)
            setOnClickListener { dismiss() }
            setOnKeyListener { _, keyCode, event ->
                if (keyCode == KeyEvent.KEYCODE_BACK &&
                    event.action == KeyEvent.ACTION_UP
                ) {
                    handleBack()
                } else {
                    false
                }
            }
        }
        panel = LinearLayout(viewContext).apply {
            orientation = LinearLayout.VERTICAL
            isClickable = true
            background = viewContext.roundedBackground(0xF7FFFFFF.toInt(), 12)
            setOnClickListener { }
        }
        panel.addView(buildToolbar())
        contentView = FrameLayout(viewContext)
        panel.addView(contentView, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            0,
            1f,
        ))
        rootView.addView(panel, FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT,
        ).apply {
            setMargins(marginPx, marginPx, marginPx, marginPx)
        })

        windowParams = newFocusableOverlayParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT,
        ).apply {
            gravity = Gravity.TOP or Gravity.START
        }
        windowManager.addView(rootView, windowParams)
        rootView.visibility = View.GONE
    }

    fun open() {
        ensure()
        if (screen == Screen.CLOSED) renderList()
        rootView.visibility = View.VISIBLE
        rootView.requestFocus()
        updateWindowLayout()
    }

    fun openCategory(nextCategoryId: String) {
        ensure()
        screen = Screen.CATEGORY
        categoryId = nextCategoryId
        editor = AutomationSettingsEditorFactory(
            context = viewContext,
            repository = repository,
            currentCooldownMode = positionStore::loadCooldownMode,
            setCooldownMode = positionStore::persistCooldownMode,
        ).build(nextCategoryId)
        titleView.text = AutomationSettingsCatalog.titleFor(nextCategoryId)
        backView.visibility = View.VISIBLE
        saveView.visibility = View.VISIBLE
        contentView.removeAllViews()
        contentView.addView(android.widget.ScrollView(viewContext).apply {
            isFillViewport = true
            addView(editor?.view)
        }, FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT,
        ))
        rootView.visibility = View.VISIBLE
        rootView.requestFocus()
        updateWindowLayout()
    }

    fun handleBack(): Boolean {
        if (screen == Screen.CLOSED) return false
        if (screen == Screen.CATEGORY) {
            clearEditorAndInput()
            renderList()
        } else {
            dismiss()
        }
        return true
    }

    fun setVisible(visible: Boolean) {
        if (visible) {
            open()
        } else {
            dismiss()
        }
    }

    fun onConfigurationChanged() {
        if (screen != Screen.CLOSED) dismiss()
    }

    fun dismiss() {
        if (screen == Screen.CLOSED && rootView.visibility != View.VISIBLE) return
        clearEditorAndInput()
        screen = Screen.CLOSED
        categoryId = null
        rootView.visibility = View.GONE
        onClosed()
    }

    fun dispose() {
        clearEditorAndInput()
        screen = Screen.CLOSED
        categoryId = null
        if (::rootView.isInitialized) {
            runCatching { windowManager.removeView(rootView) }
        }
    }

    private fun buildToolbar(): View = LinearLayout(viewContext).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        minimumHeight = viewContext.dp(56)
        setPadding(viewContext.dp(8), 0, viewContext.dp(8), 0)

        backView = TextView(viewContext).apply {
            text = "‹"
            textSize = 34f
            gravity = Gravity.CENTER
            contentDescription = "Back"
            setOnClickListener { handleBack() }
        }
        addView(backView, LinearLayout.LayoutParams(viewContext.dp(48), viewContext.dp(56)))

        titleView = TextView(viewContext).apply {
            textSize = 20f
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER_VERTICAL
            setTextColor(Color.BLACK)
        }
        addView(titleView, LinearLayout.LayoutParams(0, viewContext.dp(56), 1f))

        saveView = TextView(viewContext).apply {
            text = "Save"
            textSize = 16f
            gravity = Gravity.CENTER
            contentDescription = "Save settings"
            setOnClickListener { saveCategory() }
        }
        addView(saveView, LinearLayout.LayoutParams(viewContext.dp(64), viewContext.dp(56)))

        closeView = TextView(viewContext).apply {
            text = "×"
            textSize = 28f
            gravity = Gravity.CENTER
            contentDescription = "Close settings"
            setOnClickListener { dismiss() }
        }
        addView(closeView, LinearLayout.LayoutParams(viewContext.dp(48), viewContext.dp(56)))
    }

    private fun renderList() {
        screen = Screen.LIST
        categoryId = null
        editor = null
        titleView.text = "Automation Settings"
        backView.visibility = View.INVISIBLE
        saveView.visibility = View.INVISIBLE
        contentView.removeAllViews()

        val categories = AutomationSettingsCatalog.categories(
            repository = repository,
            cooldownMode = positionStore.loadCooldownMode(),
        )
        val list = ListView(viewContext).apply {
            divider = null
            adapter = CategoryAdapter(categories)
            setOnItemClickListener { _, _, position, _ ->
                openCategory(categories[position].id)
            }
        }
        contentView.addView(list, FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT,
        ))
    }

    private fun saveCategory() {
        val activeEditor = editor ?: return
        activeEditor.save()
        clearEditorAndInput()
        onSaved()
        renderList()
    }

    private fun clearEditorAndInput() {
        editor = null
        categoryId = null
        if (::contentView.isInitialized) contentView.removeAllViews()
        if (::rootView.isInitialized) {
            rootView.clearFocus()
            val inputMethodManager = viewContext.getSystemService(InputMethodManager::class.java)
            inputMethodManager?.hideSoftInputFromWindow(rootView.windowToken, 0)
        }
    }

    private fun updateWindowLayout() {
        if (!::rootView.isInitialized || !::windowParams.isInitialized) return
        runCatching { windowManager.updateViewLayout(rootView, windowParams) }
    }

    private inner class CategoryAdapter(
        private val items: List<AutomationSettingsCategory>,
    ) : BaseAdapter() {
        override fun getCount(): Int = items.size

        override fun getItem(position: Int): AutomationSettingsCategory = items[position]

        override fun getItemId(position: Int): Long = position.toLong()

        override fun getView(position: Int, convertView: View?, parent: ViewGroup?): View {
            val category = getItem(position)
            return LinearLayout(viewContext).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                minimumHeight = viewContext.dp(72)
                setPadding(viewContext.dp(16), viewContext.dp(8), viewContext.dp(12), viewContext.dp(8))
                isClickable = true
                isFocusable = true
                contentDescription = "Open ${category.title}"

                val textColumn = LinearLayout(viewContext).apply {
                    orientation = LinearLayout.VERTICAL
                    gravity = Gravity.CENTER_VERTICAL
                }
                textColumn.addView(TextView(viewContext).apply {
                    text = category.title
                    textSize = 16f
                    typeface = Typeface.DEFAULT_BOLD
                    setTextColor(Color.BLACK)
                })
                textColumn.addView(TextView(viewContext).apply {
                    text = category.summary
                    textSize = 12f
                    setTextColor(0xFF5F6368.toInt())
                    maxLines = 1
                })
                addView(textColumn, LinearLayout.LayoutParams(
                    0,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    1f,
                ))
                addView(TextView(viewContext).apply {
                    text = "›"
                    textSize = 28f
                    setTextColor(0xFF5F6368.toInt())
                    contentDescription = "Open ${category.title}"
                }, LinearLayout.LayoutParams(
                    viewContext.dp(32),
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                ))
            }
        }
    }

    private companion object {
        const val PANEL_MARGIN_DP = 16
    }
}
