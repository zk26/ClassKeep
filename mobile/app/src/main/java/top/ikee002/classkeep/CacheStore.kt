package top.ikee002.classkeep

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

/** Offline-first snapshot. Best effort: storage failures never block the UI. */
data class Cache(
    val teacherIp: String = "",
    val sessionKey: String = "",
    val port: Int = Announcement.DEFAULT_PORT,
    val revision: Long = 0,
    val className: String = "",
    val baseScore: Int = 100,
    val rulesPublished: Boolean = false,
    val lessons: Int = 0,
    val entries: Int = 0,
    val students: Int = 0,
    val scoresRaw: String = "{}",
    val seatsRaw: String = "{}",
    val events: List<ActivityEvent> = emptyList(),
    val updatedAt: Long = 0,
)

class CacheStore(context: Context) {
    private val prefs = context.applicationContext
        .getSharedPreferences("classkeep", Context.MODE_PRIVATE)

    fun load(): Cache = try {
        val events = JSONArray(prefs.getString("events", "[]") ?: "[]")
        Cache(
            teacherIp = prefs.getString("teacherIp", "") ?: "",
            sessionKey = prefs.getString("sessionKey", "") ?: "",
            port = prefs.getInt("port", Announcement.DEFAULT_PORT),
            revision = prefs.getLong("revision", 0),
            className = prefs.getString("className", "") ?: "",
            baseScore = prefs.getInt("baseScore", 100),
            rulesPublished = prefs.getBoolean("rulesPublished", false),
            lessons = prefs.getInt("lessons", 0),
            entries = prefs.getInt("entries", 0),
            students = prefs.getInt("students", 0),
            scoresRaw = prefs.getString("scoresRaw", "{}") ?: "{}",
            seatsRaw = prefs.getString("seatsRaw", "{}") ?: "{}",
            events = (0 until events.length()).mapNotNull { index ->
                events.optJSONObject(index)?.let { obj ->
                    ActivityEvent(
                        timestamp = obj.optString("timestamp"),
                        student = obj.optString("student"),
                        ip = obj.optString("ip"),
                        host = obj.optString("host"),
                        detail = obj.optString("detail"),
                    )
                }
            },
            updatedAt = prefs.getLong("updatedAt", 0),
        )
    } catch (_: Exception) {
        Cache()
    }

    fun save(cache: Cache) {
        try {
            val events = JSONArray()
            cache.events.takeLast(40).forEach { event ->
                events.put(
                    JSONObject()
                        .put("timestamp", event.timestamp)
                        .put("student", event.student)
                        .put("ip", event.ip)
                        .put("host", event.host)
                        .put("detail", event.detail),
                )
            }
            prefs.edit()
                .putString("teacherIp", cache.teacherIp)
                .putString("sessionKey", cache.sessionKey)
                .putInt("port", cache.port)
                .putLong("revision", cache.revision)
                .putString("className", cache.className)
                .putInt("baseScore", cache.baseScore)
                .putBoolean("rulesPublished", cache.rulesPublished)
                .putInt("lessons", cache.lessons)
                .putInt("entries", cache.entries)
                .putInt("students", cache.students)
                .putString("scoresRaw", cache.scoresRaw)
                .putString("seatsRaw", cache.seatsRaw)
                .putString("events", events.toString())
                .putLong("updatedAt", cache.updatedAt)
                .apply()
        } catch (_: Exception) {
            // Offline cache is best effort.
        }
    }
}
