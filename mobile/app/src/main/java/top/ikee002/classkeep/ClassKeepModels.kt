package top.ikee002.classkeep

import org.json.JSONObject

/** Mirrors the teacher machine's /api/classroom/state response. */
data class TeacherState(
    val teacherId: String = "",
    val revision: Long = 0,
    val port: Int = 11880,
    val className: String = "",
    val syncEnabled: Boolean = true,
    val classNames: List<String> = emptyList(),
    val nonce: String = "",
    val keyConfigured: Boolean = false,
)

data class ScoreRules(
    val published: Boolean = false,
    val baseScore: Int = 100,
    val html: String = "",
)

data class ScoreSnapshot(
    val className: String = "",
    val rules: ScoreRules = ScoreRules(),
    val lessons: List<Map<String, String>> = emptyList(),
    val entries: List<Map<String, Any>> = emptyList(),
    val raw: String = "{}",
)

/** One student card, already ordered like the teacher's seat grid. */
data class SeatStudent(
    val seat: String = "",
    val student: String = "",
    val ip: String = "",
    val score: Int = 100,
)

data class ActivityEvent(
    val timestamp: String = "",
    val student: String = "",
    val ip: String = "",
    val host: String = "",
    val detail: String = "",
) {
    fun label(): String = listOf(student, ip, host).firstOrNull { it.isNotEmpty() } ?: "-"
    fun time(): String = if (timestamp.length >= 19) timestamp.substring(11, 19) else ""
}

fun parseTeacherState(json: JSONObject): TeacherState = TeacherState(
    teacherId = json.optString("teacherId"),
    revision = json.optLong("revision"),
    port = json.optInt("port", Announcement.DEFAULT_PORT),
    className = json.optString("className"),
    syncEnabled = json.optBoolean("syncEnabled", true),
    classNames = json.optJSONArray("classNames")?.let { array ->
        (0 until array.length()).mapNotNull { array.optString(it) }
    } ?: emptyList(),
    nonce = json.optString("nonce"),
    keyConfigured = json.optBoolean("keyConfigured", false),
)

fun parseScoreSnapshot(raw: String): ScoreSnapshot {
    val json = JSONObject(raw)
    val rulesJson = json.optJSONObject("rules") ?: JSONObject()
    val lessons = json.optJSONArray("lessons")?.let { array ->
        (0 until array.length()).mapNotNull { index ->
            array.optJSONObject(index)?.let { obj ->
                mapOf(
                    "id" to obj.optString("id"),
                    "date" to obj.optString("date"),
                    "topic" to obj.optString("topic"),
                )
            }
        }
    } ?: emptyList()
    val entries = json.optJSONArray("entries")?.let { array ->
        (0 until array.length()).mapNotNull { index ->
            array.optJSONObject(index)?.let { obj ->
                mapOf<String, Any>(
                    "id" to obj.optString("id"),
                    "lessonId" to obj.optString("lessonId"),
                    "seat" to obj.optString("seat"),
                    "student" to obj.optString("student"),
                    "delta" to obj.optInt("delta"),
                    "label" to obj.optString("label"),
                    "richText" to obj.optString("richText"),
                )
            }
        }
    } ?: emptyList()
    return ScoreSnapshot(
        className = json.optString("className"),
        rules = ScoreRules(
            published = rulesJson.optBoolean("published"),
            baseScore = rulesJson.optInt("baseScore", 100),
            html = rulesJson.optString("html"),
        ),
        lessons = lessons,
        entries = entries,
        raw = raw,
    )
}

fun parseSeats(raw: String): List<SeatStudent> {
    val array = JSONObject(raw).optJSONArray("seats") ?: return emptyList()
    return (0 until array.length()).mapNotNull { index ->
        array.optJSONObject(index)?.let { obj ->
            SeatStudent(
                seat = obj.optString("seat"),
                student = obj.optString("student"),
                ip = obj.optString("ip"),
                score = obj.optInt("score", 100),
            )
        }
    }
}

fun parseEvents(raw: String): List<ActivityEvent> {
    val array = JSONObject(raw).optJSONArray("events") ?: return emptyList()
    return (0 until array.length()).mapNotNull { index ->
        array.optJSONObject(index)?.let { obj ->
            ActivityEvent(
                timestamp = obj.optString("timestamp"),
                student = obj.optString("student"),
                ip = obj.optString("ip"),
                host = obj.optString("host"),
                detail = obj.optString("detail"),
            )
        }
    }
}
