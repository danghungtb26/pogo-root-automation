package dev.pogoroot.automation.overlay

import android.app.Fragment
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ScrollView

class AutomationCategoryFragment : Fragment() {
    companion object {
        private const val ARG_CATEGORY_ID = "category_id"

        fun newInstance(categoryId: String): AutomationCategoryFragment =
            AutomationCategoryFragment().apply {
                arguments = Bundle().apply { putString(ARG_CATEGORY_ID, categoryId) }
            }
    }

    val categoryId: String
        get() = arguments?.getString(ARG_CATEGORY_ID).orEmpty()

    private var saveAction: (() -> Unit)? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        val host = activity as AutomationSettingsActivity
        val editor = AutomationSettingsEditorFactory(
            context = host,
            repository = host.repository,
            currentCooldownMode = host::currentCooldownMode,
            setCooldownMode = host::setCooldownMode,
        ).build(categoryId)
        saveAction = editor.save
        return ScrollView(host).apply {
            isFillViewport = true
            addView(editor.view)
        }
    }

    fun save() {
        saveAction?.invoke()
    }
}
