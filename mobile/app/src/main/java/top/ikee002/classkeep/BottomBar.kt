package top.ikee002.classkeep

import android.animation.ObjectAnimator
import android.content.Context
import android.view.Gravity
import android.view.View
import android.view.animation.DecelerateInterpolator
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView

/**
 * Bottom tab bar following the interaction spec:
 * a white capsule slides to the selected item (front arrives first, tail
 * follows), the selected icon bounces once, only the selected item grows a
 * text label, and the rest keep icons alone.
 */
class BottomBar(
    context: Context,
    tabs: List<Tab>,
    private val onSelected: (Int) -> Unit,
) : FrameLayout(context) {

    data class Tab(val icon: String, val label: String)

    private val indicator = View(context)
    private val items = mutableListOf<View>()
    private val icons = mutableListOf<TextView>()
    private val labels = mutableListOf<TextView>()
    private var selected = -1

    init {
        background = Ui.shape(
            context,
            context.getColor(R.color.ck_card),
            22,
            context.getColor(R.color.ck_border),
        )
        val outer = Ui.dp(context, 6)
        setPadding(outer, outer, outer, outer)
        elevation = 0f

        indicator.background = Ui.shape(context, context.getColor(R.color.ck_chip_bg), 16)
        addView(indicator, LayoutParams(0, LayoutParams.MATCH_PARENT))

        val row = LinearLayout(context).apply { orientation = LinearLayout.HORIZONTAL }
        tabs.forEachIndexed { index, tab ->
            val item = LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER
                val v = Ui.dp(context, 5)
                setPadding(v, v, v, v)
                isClickable = true
                setOnClickListener { select(index); onSelected(index) }
            }
            val icon = TextView(context).apply {
                text = tab.icon
                textSize = 18f
                gravity = Gravity.CENTER
                setTextColor(context.getColor(R.color.ck_secondary))
            }
            val label = TextView(context).apply {
                text = tab.label
                textSize = 10f
                gravity = Gravity.CENTER
                setTextColor(context.getColor(R.color.ck_primary))
                visibility = GONE
            }
            item.addView(icon)
            item.addView(label)
            row.addView(item, LinearLayout.LayoutParams(0, LayoutParams.MATCH_PARENT, 1f))
            items.add(item)
            icons.add(icon)
            labels.add(label)
        }
        addView(row, LayoutParams(LayoutParams.MATCH_PARENT, Ui.dp(context, 58)))

        post { select(0, animated = false) }
    }

    fun select(index: Int, animated: Boolean = true) {
        if (index !in items.indices) return
        selected = index
        post {
            val item = items[index]
            val width = item.width
            if (width <= 0) return@post
            if (indicator.layoutParams.width != width) {
                indicator.layoutParams = indicator.layoutParams.apply { this.width = width }
            }
            val target = (paddingLeft + item.left).toFloat()
            if (animated) {
                ObjectAnimator.ofFloat(indicator, "translationX", indicator.translationX, target).apply {
                    duration = 280
                    interpolator = DecelerateInterpolator()
                    start()
                }
                // Caterpillar stretch: grow along the travel direction, then settle.
                ObjectAnimator.ofFloat(indicator, "scaleX", 1f, 1.12f, 1f).apply {
                    duration = 280
                    start()
                }
                ObjectAnimator.ofFloat(icons[index], "scaleY", 1f, 0.82f, 1.08f, 1f).apply {
                    duration = 320
                    start()
                }
            } else {
                indicator.translationX = target
            }
            labels.forEachIndexed { i, view ->
                view.visibility = if (i == index) VISIBLE else GONE
            }
            icons.forEachIndexed { i, view ->
                view.setTextColor(
                    context.getColor(if (i == index) R.color.ck_primary else R.color.ck_secondary)
                )
            }
        }
    }

    fun current(): Int = selected
}
