package dev.pogoroot.automation.overlay

import android.app.Fragment
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.BaseAdapter
import android.widget.LinearLayout
import android.widget.ListView
import android.widget.TextView
import android.graphics.Typeface

class AutomationSettingsListFragment : Fragment() {
    private var categoryAdapter: CategoryAdapter? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        val host = activity as AutomationSettingsActivity
        return ListView(host).apply {
            divider = null
            categoryAdapter = CategoryAdapter(host, categories(host))
            adapter = categoryAdapter
            setOnItemClickListener { _, _, position, _ ->
                categoryAdapter?.getItem(position)?.let { host.openCategory(it.id) }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        val host = activity as? AutomationSettingsActivity ?: return
        categoryAdapter?.replace(categories(host))
    }

    private fun categories(host: AutomationSettingsActivity): List<AutomationSettingsCategory> =
        AutomationSettingsCatalog.categories(host.repository, host.currentCooldownMode())

    private class CategoryAdapter(
        private val context: AutomationSettingsActivity,
        private var items: List<AutomationSettingsCategory>,
    ) : BaseAdapter() {
        override fun getCount(): Int = items.size

        override fun getItem(position: Int): AutomationSettingsCategory = items[position]

        override fun getItemId(position: Int): Long = position.toLong()

        fun replace(categories: List<AutomationSettingsCategory>) {
            items = categories
            notifyDataSetChanged()
        }

        override fun getView(position: Int, convertView: View?, parent: ViewGroup?): View {
            val category = getItem(position)
            return LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = android.view.Gravity.CENTER_VERTICAL
                minimumHeight = context.dp(64)
                setPadding(context.dp(16), context.dp(8), context.dp(12), context.dp(8))

                val textColumn = LinearLayout(context).apply {
                    orientation = LinearLayout.VERTICAL
                    gravity = android.view.Gravity.CENTER_VERTICAL
                }
                textColumn.addView(TextView(context).apply {
                    text = category.title
                    textSize = 16f
                    setTypeface(typeface, Typeface.BOLD)
                })
                textColumn.addView(TextView(context).apply {
                    text = category.summary
                    textSize = 12f
                    setTextColor(0xFF5F6368.toInt())
                    maxLines = 1
                })
                addView(
                    textColumn,
                    LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f),
                )
                addView(TextView(context).apply {
                    text = "›"
                    textSize = 28f
                    setTextColor(0xFF5F6368.toInt())
                    contentDescription = "Open ${category.title}"
                }, LinearLayout.LayoutParams(context.dp(32), ViewGroup.LayoutParams.WRAP_CONTENT))
            }
        }
    }
}
