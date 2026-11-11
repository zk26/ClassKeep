package top.ikee002.classkeep

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.StateListDrawable
import android.text.InputType
import android.view.Gravity
import android.view.ViewGroup
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView

/**
 * Shared visual language, following the project's UI interaction spec:
 *
 * - Visual restraint: at most ONE frame between the eye and the content. Cards
 *   carry a hairline inner border and NO drop shadow; inner controls are filled
 *   shapes without borders.
 * - Concentric radii: outer card 20dp, inner controls 14dp, 6dp gap.
 * - Information is permanent: floating labels sit ABOVE fields and helper text
 *   sits BELOW them. Nothing disappears into a placeholder.
 * - Bottom bar leaves a safe-area gutter; lists pad their last item.
 */
object Ui {

    fun dp(context: Context, value: Int): Int =
        (value * context.resources.displayMetrics.density).toInt()

    private fun c(context: Context, id: Int): Int = context.getColor(id)

    fun shape(context: Context, fill: Int, radiusDp: Int, strokeColor: Int? = null): GradientDrawable {
        val drawable = GradientDrawable()
        drawable.setColor(fill)
        drawable.cornerRadius = dp(context, radiusDp).toFloat()
        if (strokeColor != null) drawable.setStroke(dp(context, 1), strokeColor)
        return drawable
    }

    /** Outer container: one hairline border, no elevation (no muddy shadows). */
    fun card(context: Context): LinearLayout = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        background = shape(context, c(context, R.color.ck_card), 20, c(context, R.color.ck_border))
        val pad = dp(context, 16)
        setPadding(pad, dp(context, 18), pad, dp(context, 18))
        layoutParams = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { bottomMargin = dp(context, 12) }
        // Deliberately no elevation.
    }

    fun section(context: Context, text: String): TextView = TextView(context).apply {
        this.text = text
        textSize = 16f
        setTextColor(c(context, R.color.ck_title))
        setTypeface(typeface, android.graphics.Typeface.BOLD)
        letterSpacing = 0.02f
        layoutParams = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { bottomMargin = dp(context, 12) }
    }

    fun body(context: Context, text: String = "", size: Float = 13f): TextView = TextView(context).apply {
        this.text = text
        textSize = size
        setTextColor(c(context, R.color.ck_body))
        setLineSpacing(dp(context, 3).toFloat(), 1f)
    }

    fun helper(context: Context, text: String): TextView = TextView(context).apply {
        this.text = text
        textSize = 11f
        setTextColor(c(context, R.color.ck_hint))
        layoutParams = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { topMargin = dp(context, 6) }
    }

    data class Field(val container: LinearLayout, val input: EditText, val label: TextView)

    /**
     * Floating label (always visible) + filled input + permanent helper line.
     * Placeholder hints are intentionally avoided.
     */
    fun field(
        context: Context,
        label: String,
        helper: String,
        password: Boolean = false,
        numeric: Boolean = false,
        initial: String = "",
    ): Field {
        val holder = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = dp(context, 12) }
        }
        val labelView = TextView(context).apply {
            text = label
            textSize = 12f
            setTextColor(c(context, R.color.ck_secondary))
            setTypeface(typeface, android.graphics.Typeface.BOLD)
        }
        val edit = EditText(context).apply {
            setText(initial)
            setTextColor(c(context, R.color.ck_title))
            setHintTextColor(c(context, R.color.ck_hint))
            textSize = 14f
            background = shape(context, c(context, R.color.ck_input_bg), 14)
            val p = dp(context, 12)
            setPadding(p, dp(context, 11), p, dp(context, 11))
            isSingleLine = true
            inputType = when {
                password -> InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
                numeric -> InputType.TYPE_CLASS_NUMBER
                else -> InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
            }
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = dp(context, 6) }
        }
        holder.addView(labelView)
        holder.addView(edit)
        holder.addView(helper(context, helper))
        return Field(holder, edit, labelView)
    }

    /** Borderless filled button; press state darkens the fill instead of a shadow. */
    fun button(context: Context, text: String, primary: Boolean = false): android.widget.Button =
        android.widget.Button(context).apply {
            this.text = text
            textSize = 13f
            isAllCaps = false
            stateListAnimator = null
            val normal = if (primary) c(context, R.color.ck_primary) else c(context, R.color.ck_chip_bg)
            val pressed = if (primary) c(context, R.color.ck_primary_dark) else c(context, R.color.ck_chip_pressed)
            val fg = if (primary) c(context, R.color.white) else c(context, R.color.ck_primary)
            setTextColor(fg)
            background = StateListDrawable().apply {
                addState(intArrayOf(android.R.attr.state_pressed), shape(context, pressed, 14))
                addState(intArrayOf(), shape(context, normal, 14))
            }
            val p = dp(context, 12)
            setPadding(p, p, p, p)
            gravity = Gravity.CENTER
        }

    fun row(context: Context): LinearLayout = LinearLayout(context).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        layoutParams = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { topMargin = dp(context, 12) }
    }

    fun spacer(context: Context, weight: Float = 1f): android.view.View =
        android.view.View(context).apply {
            layoutParams = LinearLayout.LayoutParams(0, 0, weight)
        }

    /** A compact status pill, no shadow. */
    fun badge(context: Context, text: String, ok: Boolean): TextView = TextView(context).apply {
        this.text = text
        textSize = 11f
        setTypeface(typeface, android.graphics.Typeface.BOLD)
        setTextColor(c(context, if (ok) R.color.ck_badge_ok_fg else R.color.ck_badge_off_fg))
        background = shape(
            context,
            c(context, if (ok) R.color.ck_badge_ok_bg else R.color.ck_badge_off_bg),
            12
        )
        val h = dp(context, 12)
        val v = dp(context, 4)
        setPadding(h, v, h, v)
    }
}
