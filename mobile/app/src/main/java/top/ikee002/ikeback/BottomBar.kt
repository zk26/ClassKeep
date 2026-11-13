package top.ikee002.ikeback

import android.animation.ObjectAnimator
import android.content.Context
import android.graphics.PorterDuff
import android.graphics.Typeface
import android.view.Gravity
import android.view.View
import android.view.ViewTreeObserver
import android.view.animation.DecelerateInterpolator
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView

/**
 * Bottom tab bar with REAL vector icons + a permanently visible name.
 *
 * 5.2.0: the old bar used a single Chinese character as a fake icon and only
 * revealed the label for the selected tab, which read as "ugly text-only".
 * Every tab now shows [icon + label] at all times; the white capsule still
 * slides to the selected item and that icon bounces once.
 *
 * Phone-fit note: every child has an EXPLICIT height. Using MATCH_PARENT for
 * the indicator makes a wrap_content FrameLayout stretch to the whole screen
 * and squeeze the content area to zero (that was the "竖条占满全屏" bug).
 */
class BottomBar(
    context: Context,
    tabs: List<Tab>,
    private val onSelected: (Int) -> Unit,
) : FrameLayout(context) {

    data class Tab(val iconRes: Int, val label: String)

    private val indicator = View(context)
    private val items = mutableListOf<View>()
    private val icons = mutableListOf<ImageView>()
    private val labels = mutableListOf<TextView>()
    private var selected = -1

    private val barHeight = Ui.dp(context, 62)
    private val outer = Ui.dp(context, 6)

    init {
        background = Ui.shape(
            context,
            context.getColor(R.color.ikeback_card),
            22,
            context.getColor(R.color.ikeback_border),
        )
        setPadding(outer, outer, outer, outer)
        elevation = 0f
        // Fixed height: never let this bar consume the content area's space.
        layoutParams = LayoutParams(LayoutParams.MATCH_PARENT, barHeight + outer * 2)

        indicator.background = Ui.shape(context, context.getColor(R.color.ikeback_chip_bg), 16)
        addView(indicator, LayoutParams(0, barHeight).apply { gravity = Gravity.TOP })

        val row = LinearLayout(context).apply { orientation = LinearLayout.HORIZONTAL }
        tabs.forEachIndexed { index, tab ->
            val item = LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER
                val v = Ui.dp(context, 2)
                setPadding(v, v, v, v)
                isClickable = true
                isFocusable = true
                contentDescription = tab.label
                setOnClickListener { select(index); onSelected(index) }
            }
            val icon = ImageView(context).apply {
                setImageResource(tab.iconRes)
                scaleType = ImageView.ScaleType.FIT_CENTER
                setColorFilter(context.getColor(R.color.ikeback_secondary), PorterDuff.Mode.SRC_IN)
            }
            item.addView(
                icon,
                LinearLayout.LayoutParams(Ui.dp(context, 22), Ui.dp(context, 22)),
            )
            val label = TextView(context).apply {
                text = tab.label
                textSize = 10.5f
                gravity = Gravity.CENTER
                maxLines = 1
                setTextColor(context.getColor(R.color.ikeback_secondary))
            }
            item.addView(
                label,
                LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT),
            )
            row.addView(item, LinearLayout.LayoutParams(0, barHeight, 1f))
            items.add(item)
            icons.add(icon)
            labels.add(label)
        }
        addView(row, LayoutParams(LayoutParams.MATCH_PARENT, barHeight).apply { gravity = Gravity.TOP })

        // select() needs measured item widths. Re-run once after the first real
        // layout so a zero-width early call cannot leave the capsule invisible.
        viewTreeObserver.addOnGlobalLayoutListener(object : ViewTreeObserver.OnGlobalLayoutListener {
            override fun onGlobalLayout() {
                viewTreeObserver.removeOnGlobalLayoutListener(this)
                select(if (selected < 0) 0 else selected, animated = false)
            }
        })
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
            // The indicator already starts at paddingLeft, just like the row.
            val target = item.left.toFloat()
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
                indicator.scaleX = 1f
            }
            icons.forEachIndexed { i, view ->
                view.setColorFilter(
                    context.getColor(if (i == index) R.color.ikeback_primary else R.color.ikeback_secondary),
                    PorterDuff.Mode.SRC_IN,
                )
            }
            labels.forEachIndexed { i, view ->
                view.setTextColor(
                    context.getColor(if (i == index) R.color.ikeback_primary else R.color.ikeback_secondary)
                )
                view.setTypeface(
                    view.typeface,
                    if (i == index) Typeface.BOLD else Typeface.NORMAL,
                )
            }
        }
    }

    fun current(): Int = selected
}
