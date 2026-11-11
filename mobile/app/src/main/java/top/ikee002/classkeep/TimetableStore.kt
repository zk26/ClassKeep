package top.ikee002.classkeep

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.util.Calendar

/**
 * A deliberately small, dependency-free weekly timetable.
 *
 * Design灵感来自 ClassBoard 的「periods + 周一到周日网格」，但做了简化：
 * - 单份 JSON 存在 SharedPreferences，不引入 Room / Compose / Retrofit；
 * - 节次时间用 `HH:mm-HH:mm` 字符串，脏数据不会让网格崩；
 * - 只保留教学需要的能力：今天列高亮、当前节高亮、下一节课提示、
 *   点格子直接改科目、支持导入全校作息 JSON。
 * 未复制 ClassBoard 的任何代码、界面或资源。
 */
data class TimetablePeriod(
    val label: String,
    val start: String,
    val end: String,
    val isBreak: Boolean = false,
)

data class TimetableCourse(
    val day: Int,          // 1 = Monday .. 7 = Sunday
    val period: Int,       // index into periods
    val subject: String,
)

data class Timetable(
    val name: String = "默认作息",
    val periods: List<TimetablePeriod> = defaultPeriods(),
    val courses: List<TimetableCourse> = emptyList(),
)

val WEEKDAYS = listOf("周一", "周二", "周三", "周四", "周五", "周六", "周日")

val SUBJECT_PRESETS = listOf(
    "语文", "数学", "英语", "物理", "化学", "生物",
    "政治", "历史", "地理", "体育", "音乐", "美术", "信息技术", "班会", "自习",
)

/** Muted candy colours, darkened enough to stay legible under white text. */
val SUBJECT_COLORS = intArrayOf(
    0xFF6C63E6.toInt(), 0xFF3E86D8.toInt(), 0xFF1E9C9C.toInt(), 0xFF149A6A.toInt(),
    0xFFC88A00.toInt(), 0xFFD96A1A.toInt(), 0xFFD9568A.toInt(), 0xFF7A7392.toInt(),
)

fun subjectColor(subject: String): Int {
    if (subject.isEmpty()) return 0xFFF1F4FA.toInt()
    val index = ((subject.hashCode() % SUBJECT_COLORS.size) + SUBJECT_COLORS.size) % SUBJECT_COLORS.size
    return SUBJECT_COLORS[index]
}

fun defaultPeriods(): List<TimetablePeriod> = listOf(
    TimetablePeriod("早读", "07:50", "08:20"),
    TimetablePeriod("第一节", "08:20", "09:00"),
    TimetablePeriod("第二节", "09:10", "09:50"),
    TimetablePeriod("大课间", "09:50", "10:20", isBreak = true),
    TimetablePeriod("第三节", "10:20", "11:00"),
    TimetablePeriod("第四节", "11:10", "11:50"),
    TimetablePeriod("午休", "11:50", "14:00", isBreak = true),
    TimetablePeriod("第五节", "14:00", "14:40"),
    TimetablePeriod("第六节", "14:50", "15:30"),
    TimetablePeriod("第七节", "15:40", "16:20"),
    TimetablePeriod("第八节", "16:30", "17:10"),
)

fun todayDay(): Int {
    val value = Calendar.getInstance().get(Calendar.DAY_OF_WEEK) // 1=Sun
    return if (value == Calendar.SUNDAY) 7 else value - 1
}

fun nowMinutes(): Int {
    val c = Calendar.getInstance()
    return c.get(Calendar.HOUR_OF_DAY) * 60 + c.get(Calendar.MINUTE)
}

private fun minutes(text: String): Int? {
    val parts = text.trim().split(':')
    val h = parts.getOrNull(0)?.toIntOrNull() ?: return null
    val m = parts.getOrNull(1)?.toIntOrNull() ?: 0
    return if (h in 0..23 && m in 0..59) h * 60 + m else null
}

/** Currently running period index; breaks and after-school return null. */
fun currentPeriod(timetable: Timetable, at: Int = nowMinutes()): Int? {
    timetable.periods.forEachIndexed { index, period ->
        if (period.isBreak) return@forEachIndexed
        val start = minutes(period.start) ?: return@forEachIndexed
        val end = minutes(period.end) ?: return@forEachIndexed
        if (at in start until end) return index
    }
    return null
}

fun nextPeriod(timetable: Timetable, at: Int = nowMinutes()): Int? {
    timetable.periods.forEachIndexed { index, period ->
        if (period.isBreak) return@forEachIndexed
        val start = minutes(period.start) ?: return@forEachIndexed
        if (start > at) return index
    }
    return null
}

class TimetableStore(context: Context) {
    private val prefs = context.applicationContext
        .getSharedPreferences("classkeep_timetable", Context.MODE_PRIVATE)

    fun load(): Timetable = try {
        val raw = prefs.getString("json", "") ?: ""
        if (raw.isBlank()) Timetable() else fromJson(raw)
    } catch (_: Exception) {
        Timetable()
    }

    fun save(timetable: Timetable) {
        prefs.edit().putString("json", toJson(timetable)).apply()
    }

    fun setSubject(day: Int, period: Int, subject: String) {
        val current = load()
        val courses = current.courses.filterNot { it.day == day && it.period == period }.toMutableList()
        if (subject.isNotBlank()) courses.add(TimetableCourse(day, period, subject.trim()))
        save(current.copy(courses = courses))
    }

    companion object {
        fun subjectAt(timetable: Timetable, day: Int, period: Int): String =
            timetable.courses.firstOrNull { it.day == day && it.period == period }?.subject ?: ""

        /** Accepts both our own format and ClassBoard's school-wide 作息 JSON. */
        fun fromJson(raw: String): Timetable {
            val root = JSONObject(raw)
            val periodsArray = root.optJSONArray("periods")
            val periods = if (periodsArray != null) {
                (0 until periodsArray.length()).mapNotNull { i ->
                    val obj = periodsArray.optJSONObject(i) ?: return@mapNotNull null
                    val label = obj.optString("label")
                    val start: String
                    val end: String
                    if (obj.has("start")) {
                        start = obj.optString("start")
                        end = obj.optString("end")
                    } else {
                        // ClassBoard: times = ["8:20-9:00", "8:00-8:40"]
                        val range = obj.optJSONArray("times")?.optString(0) ?: ""
                        start = range.substringBefore('-').trim()
                        end = range.substringAfter('-', "").trim()
                    }
                    TimetablePeriod(
                        label = label.ifEmpty { "第${i + 1}节" },
                        start = start,
                        end = end,
                        isBreak = obj.optBoolean("isBreak", false),
                    )
                }
            } else defaultPeriods()

            val coursesArray = root.optJSONArray("courses")
            val courses = if (coursesArray != null) {
                (0 until coursesArray.length()).mapNotNull { i ->
                    val obj = coursesArray.optJSONObject(i) ?: return@mapNotNull null
                    TimetableCourse(
                        day = obj.optInt("day"),
                        period = obj.optInt("period"),
                        subject = obj.optString("subject"),
                    )
                }.filter { it.day in 1..7 && it.period >= 0 }
            } else emptyList()

            // ClassBoard nests the periods inside timetables[0].
            val nested = root.optJSONArray("timetables")?.optJSONObject(0)
            val effectivePeriods = if (periods.isEmpty() && nested != null) {
                val arr = nested.optJSONArray("periods") ?: JSONArray()
                (0 until arr.length()).mapNotNull { i ->
                    val obj = arr.optJSONObject(i) ?: return@mapNotNull null
                    val range = obj.optJSONArray("times")?.optString(0) ?: ""
                    TimetablePeriod(
                        label = obj.optString("label"),
                        start = range.substringBefore('-').trim(),
                        end = range.substringAfter('-', "").trim(),
                        isBreak = obj.optBoolean("isBreak", false),
                    )
                }
            } else periods

            return Timetable(
                name = root.optString("name").ifEmpty { nested?.optString("name") ?: "默认作息" },
                periods = effectivePeriods.ifEmpty { defaultPeriods() },
                courses = courses,
            )
        }

        fun toJson(timetable: Timetable): String {
            val periods = JSONArray()
            timetable.periods.forEach {
                periods.put(
                    JSONObject()
                        .put("label", it.label)
                        .put("start", it.start)
                        .put("end", it.end)
                        .put("isBreak", it.isBreak)
                )
            }
            val courses = JSONArray()
            timetable.courses.forEach {
                courses.put(
                    JSONObject()
                        .put("day", it.day)
                        .put("period", it.period)
                        .put("subject", it.subject)
                )
            }
            return JSONObject()
                .put("name", timetable.name)
                .put("periods", periods)
                .put("courses", courses)
                .toString()
        }
    }
}
