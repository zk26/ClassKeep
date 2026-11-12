package top.ikee002.classkeep

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.util.Calendar

/**
 * Timetable model aligned with the ClassBoard (G:\AndWork\ClassBoard) JSON format.
 *
 * Design rules borrowed from the reference project (concepts only, no code copied):
 *  - A period has ONE or TWO time ranges. Two ranges = two parallel school
 *    schedules ("小学" / "初高中"), NOT start/end split into two columns.
 *  - A course is drawn as a coloured rounded block that may span several rows.
 *  - Break rows (大课间 / 午休) span the whole week and never hold a course.
 *  - Dirty time strings must never crash the grid.
 *
 * Storage: local SharedPreferences AND (optionally) the teacher machine via
 * /api/classroom/timetable, see BridgeClient + ClassroomBridge.cpp.
 */
data class TimetablePeriod(
    val label: String,
    val times: List<String> = emptyList(),   // "8:20-9:00"; 1 entry = single, 2 = dual
    val isBreak: Boolean = false,
) {
    /** First schedule, kept for the "current period" calculation and legacy UIs. */
    val start: String get() = times.firstOrNull()?.substringBefore('-')?.trim() ?: ""
    val end: String get() = times.firstOrNull()?.substringAfter('-', "")?.trim() ?: ""
}

data class TimetableCourse(
    val name: String = "",
    val dayOfWeek: Int = 1,        // 1 = Monday .. 7 = Sunday
    val startSection: Int = 0,     // row index into periods
    val endSection: Int = 0,       // inclusive, may span several rows
    val colorIndex: Int = 0,
    val room: String = "",
    val teacher: String = "",
    val note: String = "",
    val className: String = "",    // which class this lesson belongs to
)

/** Parameters used by 「课表设置」 to regenerate the period rows. */
data class TimetableConfig(
    val lessonsPerDay: Int = 8,
    val lessonMinutes: Int = 40,
    val shortBreakMinutes: Int = 10,
    val dayStart: String = "08:20",
    val bigBreakAfter: Int = 2,       // 0 = none
    val bigBreakMinutes: Int = 30,
    val noonAfter: Int = 4,           // 0 = none
    val noonMinutes: Int = 130,
    val dualTime: Boolean = false,    // two parallel schedules
    val timeLabel1: String = "小学",
    val timeLabel2: String = "初高中",
    val secondShiftMinutes: Int = -20, // offset of the second schedule
)

data class Timetable(
    val name: String = "默认作息",
    val timeLabels: List<String> = emptyList(),   // empty = single, 2 = dual
    val periods: List<TimetablePeriod> = defaultPeriods(),
    val courses: List<TimetableCourse> = emptyList(),
    val config: TimetableConfig = defaultTimetableConfig(),
    val revision: Long = 0,
)

val WEEKDAYS = listOf("周一", "周二", "周三", "周四", "周五", "周六", "周日")
private val CN_NUMERALS = listOf("一", "二", "三", "四", "五", "六", "七", "八", "九", "十", "十一", "十二")

val SUBJECT_PRESETS = listOf(
    "语文", "数学", "英语", "物理", "化学", "生物",
    "政治", "历史", "地理", "体育", "音乐", "美术", "信息技术", "班会", "自习",
)

/** Candy colours, darkened so white text stays legible (ClassBoard palette). */
val COURSE_COLORS = intArrayOf(
    0xFF7C5CFF.toInt(), 0xFF6EA8FF.toInt(), 0xFF2FB8C6.toInt(), 0xFF16C47F.toInt(),
    0xFFE0A100.toInt(), 0xFFF2801B.toInt(), 0xFFFF7AA8.toInt(), 0xFFE0417B.toInt(),
    0xFF8B6CF0.toInt(), 0xFF7A7392.toInt(),
)

fun courseColor(index: Int): Int =
    COURSE_COLORS[((index % COURSE_COLORS.size) + COURSE_COLORS.size) % COURSE_COLORS.size]

fun defaultTimetableConfig(): TimetableConfig = TimetableConfig()

private fun formatMinutes(total: Int): String {
    val t = ((total % 1440) + 1440) % 1440
    return "%02d:%02d".format(t / 60, t % 60)
}

private fun parseClock(text: String): Int? {
    val parts = text.trim().split(':')
    val h = parts.getOrNull(0)?.toIntOrNull() ?: return null
    val m = parts.getOrNull(1)?.toIntOrNull() ?: 0
    return if (h in 0..23 && m in 0..59) h * 60 + m else null
}

/** "08:20-09:00" -> 500..540, tolerant of single-digit hours and blanks. */
fun parseTimeRange(text: String): IntRange? {
    val parts = text.split('-')
    if (parts.size != 2) return null
    val a = parseClock(parts[0]) ?: return null
    val b = parseClock(parts[1]) ?: return null
    return if (b >= a) a..b else null
}

private fun rangeText(start: Int, end: Int): String = "${formatMinutes(start)}-${formatMinutes(end)}"

/** Regenerate the rows. Column 2 is a parallel schedule shifted by N minutes. */
fun buildPeriods(config: TimetableConfig): List<TimetablePeriod> {
    val result = mutableListOf<TimetablePeriod>()
    var cursor = parseClock(config.dayStart) ?: (8 * 60 + 20)
    val count = config.lessonsPerDay.coerceIn(1, 12)
    val lesson = config.lessonMinutes.coerceIn(10, 120)
    val shortBreak = config.shortBreakMinutes.coerceIn(0, 60)
    val shift = config.secondShiftMinutes.coerceIn(-180, 180)
    val dual = config.dualTime

    fun add(label: String, from: Int, to: Int, isBreak: Boolean) {
        val first = rangeText(from, to)
        val times = if (dual) listOf(first, rangeText(from + shift, to + shift)) else listOf(first)
        result.add(TimetablePeriod(label, times, isBreak))
    }

    for (i in 1..count) {
        val end = cursor + lesson
        add(CN_NUMERALS.getOrElse(i - 1) { "第${i}节" }, cursor, end, false)
        cursor = end
        if (i >= count) break
        when {
            config.bigBreakAfter in 1..count && i == config.bigBreakAfter -> {
                val stop = cursor + config.bigBreakMinutes.coerceIn(5, 180)
                add("大课间", cursor, stop, true)
                cursor = stop
            }
            config.noonAfter in 1..count && i == config.noonAfter -> {
                val stop = cursor + config.noonMinutes.coerceIn(10, 300)
                add("午休", cursor, stop, true)
                cursor = stop
            }
            else -> cursor += shortBreak
        }
    }
    return result
}

fun defaultPeriods(): List<TimetablePeriod> = buildPeriods(defaultTimetableConfig())

fun todayDay(): Int {
    val value = Calendar.getInstance().get(Calendar.DAY_OF_WEEK) // 1 = Sunday
    return if (value == Calendar.SUNDAY) 7 else value - 1
}

fun nowMinutes(): Int {
    val c = Calendar.getInstance()
    return c.get(Calendar.HOUR_OF_DAY) * 60 + c.get(Calendar.MINUTE)
}

/** Row index currently in progress; break rows never count. */
fun currentPeriod(timetable: Timetable, at: Int = nowMinutes()): Int? {
    timetable.periods.forEachIndexed { index, period ->
        if (period.isBreak) return@forEachIndexed
        if (period.times.any { parseTimeRange(it)?.contains(at) == true }) return index
    }
    return null
}

fun nextPeriod(timetable: Timetable, at: Int = nowMinutes()): Int? {
    timetable.periods.forEachIndexed { index, period ->
        if (period.isBreak) return@forEachIndexed
        for (range in period.times) {
            val r = parseTimeRange(range) ?: continue
            if (r.first > at) return index
        }
    }
    return null
}

fun courseAt(timetable: Timetable, day: Int, row: Int): TimetableCourse? =
    timetable.courses.firstOrNull {
        it.dayOfWeek == day && row in it.startSection..it.endSection
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

    /** Regenerate rows from new settings, keeping courses that still fit. */
    fun updateConfig(config: TimetableConfig, name: String? = null) {
        val current = load()
        val periods = buildPeriods(config)
        val labels = if (config.dualTime) listOf(config.timeLabel1, config.timeLabel2) else emptyList()
        val courses = current.courses.filter { it.startSection < periods.size }
            .map { it.copy(endSection = it.endSection.coerceAtMost(periods.size - 1)) }
        save(
            current.copy(
                name = name ?: current.name,
                periods = periods,
                timeLabels = labels,
                courses = courses,
                config = config,
                revision = current.revision + 1,
            )
        )
    }

    fun setCourse(course: TimetableCourse) {
        val current = load()
        val kept = current.courses.filterNot {
            it.dayOfWeek == course.dayOfWeek && it.startSection == course.startSection
        }.toMutableList()
        if (course.name.isNotBlank()) kept.add(course)
        save(current.copy(courses = kept, revision = current.revision + 1))
    }

    fun removeCourse(day: Int, row: Int) {
        val current = load()
        save(
            current.copy(
                courses = current.courses.filterNot { it.dayOfWeek == day && it.startSection == row },
                revision = current.revision + 1,
            )
        )
    }

    companion object {
        /**
         * Accepts three shapes:
         *  1. ClassBoard bundle  { "timetables": [ {...} ] }
         *  2. ClassBoard single  { "name", "timeLabels", "periods":[{label,times,isBreak}], "courses":[...] }
         *  3. Legacy ClassKeep   { "name", "periods":[{label,start,end,isBreak}], "courses":[{day,period,subject}], "config":{...} }
         */
        fun fromJson(raw: String): Timetable {
            val root = JSONObject(raw)
            val array = root.optJSONArray("timetables")
            val doc = if (array != null && array.length() > 0) array.optJSONObject(0) ?: root else root

            val labelsArray = doc.optJSONArray("timeLabels")
            val timeLabels = if (labelsArray != null) {
                (0 until labelsArray.length()).mapNotNull { labelsArray.optString(it).takeIf { s -> s.isNotBlank() } }
            } else emptyList()

            var sawLegacyConfig = false
            val periodsArray = doc.optJSONArray("periods")
            val periods = if (periodsArray != null) {
                (0 until periodsArray.length()).mapNotNull { i ->
                    val obj = periodsArray.optJSONObject(i) ?: return@mapNotNull null
                    val label = obj.optString("label").ifEmpty { "第${i + 1}节" }
                    val timesArray = obj.optJSONArray("times")
                    val times = if (timesArray != null) {
                        (0 until timesArray.length()).map { timesArray.optString(it) ?: "" }
                    } else {
                        val start = obj.optString("start")
                        val end = obj.optString("end")
                        if (start.isNotBlank()) sawLegacyConfig = true
                        listOf(if (start.isNotBlank() && end.isNotBlank()) "$start-$end" else "")
                    }
                    TimetablePeriod(label, times, obj.optBoolean("isBreak", false))
                }
            } else defaultPeriods()

            val coursesArray = doc.optJSONArray("courses")
            val courses = if (coursesArray != null) {
                (0 until coursesArray.length()).mapNotNull { i ->
                    val obj = coursesArray.optJSONObject(i) ?: return@mapNotNull null
                    val legacySubject = obj.optString("subject")
                    if (legacySubject.isNotBlank()) {
                        TimetableCourse(
                            name = legacySubject,
                            dayOfWeek = obj.optInt("day", 1),
                            startSection = obj.optInt("period", 0),
                            endSection = obj.optInt("period", 0),
                            className = obj.optString("className"),
                        )
                    } else {
                        TimetableCourse(
                            name = obj.optString("name"),
                            dayOfWeek = obj.optInt("dayOfWeek", 1),
                            startSection = obj.optInt("startSection", 0),
                            endSection = obj.optInt("endSection", obj.optInt("startSection", 0)),
                            colorIndex = obj.optInt("colorIndex", 0),
                            room = obj.optString("room", ""),
                            teacher = obj.optString("teacher", ""),
                            note = obj.optString("note", ""),
                            className = obj.optString("className", ""),
                        )
                    }
                }.filter { it.dayOfWeek in 1..7 && it.startSection >= 0 }
            } else emptyList()

            val cfg = doc.optJSONObject("config")
            val config = if (cfg != null) {
                sawLegacyConfig = true
                TimetableConfig(
                    lessonsPerDay = cfg.optInt("lessonsPerDay", 8),
                    lessonMinutes = cfg.optInt("lessonMinutes", 40),
                    shortBreakMinutes = cfg.optInt("shortBreakMinutes", 10),
                    dayStart = cfg.optString("dayStart").ifEmpty { "08:20" },
                    bigBreakAfter = cfg.optInt("bigBreakAfter", 2),
                    bigBreakMinutes = cfg.optInt("bigBreakMinutes", 30),
                    noonAfter = cfg.optInt("noonAfter", 4),
                    noonMinutes = cfg.optInt("noonMinutes", 130),
                    dualTime = cfg.optBoolean("dualTime", cfg.optBoolean("dualTimeColumn", false)),
                    timeLabel1 = cfg.optString("timeLabel1", "小学"),
                    timeLabel2 = cfg.optString("timeLabel2", "初高中"),
                    secondShiftMinutes = cfg.optInt("secondShiftMinutes", -20),
                )
            } else if (sawLegacyConfig) defaultTimetableConfig() else defaultTimetableConfig()

            return Timetable(
                name = doc.optString("name").ifEmpty { "默认作息" },
                timeLabels = timeLabels.ifEmpty {
                    if (config.dualTime) listOf(config.timeLabel1, config.timeLabel2) else emptyList()
                },
                periods = periods.ifEmpty { defaultPeriods() },
                courses = courses,
                config = config,
                revision = doc.optLong("revision", 0),
            )
        }

        /** ClassBoard-compatible single-document JSON, plus our own settings fields. */
        fun toJson(timetable: Timetable): String {
            val periods = JSONArray()
            timetable.periods.forEach { p ->
                val times = JSONArray()
                p.times.forEach { times.put(it) }
                periods.put(
                    JSONObject()
                        .put("label", p.label)
                        .put("times", times)
                        .put("isBreak", p.isBreak)
                )
            }
            val courses = JSONArray()
            timetable.courses.forEach { c ->
                courses.put(
                    JSONObject()
                        .put("name", c.name)
                        .put("teacher", c.teacher)
                        .put("room", c.room)
                        .put("dayOfWeek", c.dayOfWeek)
                        .put("startSection", c.startSection)
                        .put("endSection", c.endSection)
                        .put("colorIndex", c.colorIndex)
                        .put("note", c.note)
                        .put("className", c.className)
                )
            }
            val labels = JSONArray()
            timetable.timeLabels.forEach { labels.put(it) }
            val c = timetable.config
            val config = JSONObject()
                .put("lessonsPerDay", c.lessonsPerDay)
                .put("lessonMinutes", c.lessonMinutes)
                .put("shortBreakMinutes", c.shortBreakMinutes)
                .put("dayStart", c.dayStart)
                .put("bigBreakAfter", c.bigBreakAfter)
                .put("bigBreakMinutes", c.bigBreakMinutes)
                .put("noonAfter", c.noonAfter)
                .put("noonMinutes", c.noonMinutes)
                .put("dualTime", c.dualTime)
                .put("timeLabel1", c.timeLabel1)
                .put("timeLabel2", c.timeLabel2)
                .put("secondShiftMinutes", c.secondShiftMinutes)
            return JSONObject()
                .put("name", timetable.name)
                .put("timeLabels", labels)
                .put("periods", periods)
                .put("courses", courses)
                .put("config", config)
                .put("revision", timetable.revision)
                .toString()
        }
    }
}
