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

/**
 * 2.0.0 可调作息参数：一天几节课、每节时长、大课间/午休放在第几节之后、
 * 单列时间还是双列时间。改完由 [buildPeriods] 重新生成节次。
 */
data class TimetableConfig(
    val lessonsPerDay: Int = 8,
    val lessonMinutes: Int = 40,
    val shortBreakMinutes: Int = 10,
    val dayStart: String = "08:20",
    val bigBreakAfter: Int = 2,      // 0 = 不设大课间
    val bigBreakMinutes: Int = 30,
    val noonAfter: Int = 4,          // 0 = 不设午休
    val noonMinutes: Int = 130,
    val dualTimeColumn: Boolean = true, // true = 起/止两列，false = 一列显示 起-止
)

fun defaultTimetableConfig(): TimetableConfig = TimetableConfig()

private fun formatMinutes(total: Int): String {
    val t = ((total % 1440) + 1440) % 1440
    return "%02d:%02d".format(t / 60, t % 60)
}

/** Regenerate the period list from the configurable timetable parameters. */
fun buildPeriods(config: TimetableConfig): List<TimetablePeriod> {
    val result = mutableListOf<TimetablePeriod>()
    var cursor = minutes(config.dayStart) ?: (8 * 60 + 20)
    val count = config.lessonsPerDay.coerceIn(1, 12)
    val lesson = config.lessonMinutes.coerceIn(10, 120)
    val shortBreak = config.shortBreakMinutes.coerceIn(0, 60)
    for (i in 1..count) {
        val end = cursor + lesson
        result.add(TimetablePeriod("第${i}节", formatMinutes(cursor), formatMinutes(end)))
        cursor = end
        if (i >= count) break
        when {
            config.bigBreakAfter in 1..count && i == config.bigBreakAfter -> {
                val stop = cursor + config.bigBreakMinutes.coerceIn(5, 180)
                result.add(
                    TimetablePeriod("大课间", formatMinutes(cursor), formatMinutes(stop), isBreak = true)
                )
                cursor = stop
            }
            config.noonAfter in 1..count && i == config.noonAfter -> {
                val stop = cursor + config.noonMinutes.coerceIn(10, 300)
                result.add(
                    TimetablePeriod("午休", formatMinutes(cursor), formatMinutes(stop), isBreak = true)
                )
                cursor = stop
            }
            else -> cursor += shortBreak
        }
    }
    return result
}

data class Timetable(
    val name: String = "默认作息",
    val periods: List<TimetablePeriod> = defaultPeriods(),
    val courses: List<TimetableCourse> = emptyList(),
    val config: TimetableConfig = defaultTimetableConfig(),
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

/**
 * The default schedule is generated from [defaultTimetableConfig], so the
 * values shown in 「作息设置」 always describe the grid the teacher actually sees.
 */
fun defaultPeriods(): List<TimetablePeriod> = buildPeriods(defaultTimetableConfig())

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

    /** Rebuild the period rows from new settings, dropping courses beyond the new range. */
    fun updateConfig(config: TimetableConfig) {
        val current = load()
        val periods = buildPeriods(config)
        val courses = current.courses.filter { it.period in periods.indices }
        save(current.copy(periods = periods, courses = courses, config = config))
    }

    fun setDualTimeColumn(dual: Boolean) {
        val current = load()
        save(current.copy(config = current.config.copy(dualTimeColumn = dual)))
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

            val cfg = root.optJSONObject("config")
            val config = if (cfg != null) TimetableConfig(
                lessonsPerDay = cfg.optInt("lessonsPerDay", 8),
                lessonMinutes = cfg.optInt("lessonMinutes", 40),
                shortBreakMinutes = cfg.optInt("shortBreakMinutes", 10),
                dayStart = cfg.optString("dayStart").ifEmpty { "08:20" },
                bigBreakAfter = cfg.optInt("bigBreakAfter", 2),
                bigBreakMinutes = cfg.optInt("bigBreakMinutes", 30),
                noonAfter = cfg.optInt("noonAfter", 4),
                noonMinutes = cfg.optInt("noonMinutes", 130),
                dualTimeColumn = cfg.optBoolean("dualTimeColumn", true),
            ) else defaultTimetableConfig()

            return Timetable(
                name = root.optString("name").ifEmpty { nested?.optString("name") ?: "默认作息" },
                periods = effectivePeriods.ifEmpty { defaultPeriods() },
                courses = courses,
                config = config,
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
            val config = JSONObject()
                .put("lessonsPerDay", timetable.config.lessonsPerDay)
                .put("lessonMinutes", timetable.config.lessonMinutes)
                .put("shortBreakMinutes", timetable.config.shortBreakMinutes)
                .put("dayStart", timetable.config.dayStart)
                .put("bigBreakAfter", timetable.config.bigBreakAfter)
                .put("bigBreakMinutes", timetable.config.bigBreakMinutes)
                .put("noonAfter", timetable.config.noonAfter)
                .put("noonMinutes", timetable.config.noonMinutes)
                .put("dualTimeColumn", timetable.config.dualTimeColumn)
            return JSONObject()
                .put("name", timetable.name)
                .put("periods", periods)
                .put("courses", courses)
                .put("config", config)
                .toString()
        }
    }
}
