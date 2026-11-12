package top.ikee002.classkeep

import org.json.JSONObject
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

class UnauthorizedException : IOException("课堂密钥被教师机拒绝")

/**
 * Talks to the teacher machine's ClassKeep bridge.
 *
 * Reuses the per-lesson connection key. The key itself is never transmitted:
 * every request proves knowledge of it with an HMAC-SHA256 over the method,
 * path, nonce and body hash. /state issues a single-use challenge, so one
 * nonce can never authorize two requests (see ClassKeepCrypto).
 * Uses only the JDK/Android built-ins - no Retrofit, OkHttp or crypto library.
 */
class BridgeClient {
    var port: Int = Announcement.DEFAULT_PORT
    var lessonKey: String = ""
        set(value) {
            field = value.trim()
        }

    private var nonce: String = ""

    fun hasKey(): Boolean = lessonKey.isNotEmpty()

    private fun url(host: String, route: String): URL =
        URL("http://$host:$port/api/classroom/$route")

    private fun request(host: String, route: String, method: String, body: String?, auth: Boolean): String {
        // Challenges are single-use on the teacher machine, so every
        // authenticated call obtains a fresh /state nonce first.
        val challenge = if (auth) {
            if (lessonKey.isEmpty()) throw UnauthorizedException()
            val issued = connect(host).nonce
            if (issued.isEmpty()) throw UnauthorizedException()
            nonce = issued
            issued
        } else {
            ""
        }
        val connection = url(host, route).openConnection() as HttpURLConnection
        try {
            connection.connectTimeout = 5_000
            connection.readTimeout = 15_000
            connection.requestMethod = method
            connection.setRequestProperty("Accept", "application/json, */*")
            if (auth) {
                connection.setRequestProperty("X-Classroom-Nonce", challenge)
                connection.setRequestProperty("X-Classroom-Token",
                    ClassKeepCrypto.token(lessonKey, method, "/api/classroom/$route", challenge, body))
            }
            if (body != null) {
                connection.doOutput = true
                connection.setRequestProperty("Content-Type", "application/json; charset=utf-8")
                connection.outputStream.use { it.write(body.toByteArray(Charsets.UTF_8)) }
            }
            val code = connection.responseCode
            if (code == HttpURLConnection.HTTP_UNAUTHORIZED) throw UnauthorizedException()
            if (code !in 200..299) throw IOException("$route: HTTP $code")
            return connection.inputStream.bufferedReader(Charsets.UTF_8).use { it.readText() }
        } finally {
            connection.disconnect()
        }
    }

    /** Public metadata endpoint. Always returns the current challenge. */
    fun connect(host: String): TeacherState {
        val state = parseTeacherState(JSONObject(request(host, "state", "GET", null, auth = false)))
        nonce = state.nonce
        return state
    }

    fun scores(host: String): ScoreSnapshot =
        parseScoreSnapshot(request(host, "scores", "GET", null, auth = true))

    /** Students in the exact order the teacher dragged them on the seat grid. */
    fun seats(host: String): List<SeatStudent> =
        parseSeats(request(host, "seats", "GET", null, auth = true))

    /** One-tap scoring from the phone (delta in -5..+5, reason optional). */
    fun score(host: String, seat: String, delta: Int, reason: String): Boolean {
        val body = JSONObject()
            .put("seat", seat)
            .put("delta", delta)
            .put("reason", reason)
            .toString()
        return JSONObject(request(host, "score", "POST", body, auth = true)).optBoolean("ok")
    }

    fun events(host: String): List<ActivityEvent> =
        parseEvents(request(host, "events", "GET", null, auth = true))

    fun roster(host: String): String =
        request(host, "roster", "GET", null, auth = true)

    fun wordCloud(host: String, seat: String): String =
        request(host, "wordcloud?seat=${java.net.URLEncoder.encode(seat, "UTF-8")}", "GET", null, auth = true)

    fun report(host: String): String =
        request(host, "report", "GET", null, auth = true)

    /** request() refreshes the single-use challenge before sending. */
    fun command(
        host: String,
        name: String,
        enabled: Boolean,
        text: String = "",
        teacherIp: String = "",
    ): Boolean {
        val body = JSONObject()
            .put("name", name)
            .put("enabled", enabled)
            .put("text", text)
            .put("teacherIp", teacherIp)
            .toString()
        val reply = JSONObject(request(host, "command", "POST", body, auth = true))
        return reply.optBoolean("ok")
    }

    fun pushScores(host: String, snapshotRaw: String): Boolean {
        val body = JSONObject().put("scores", JSONObject(snapshotRaw)).toString()
        return JSONObject(request(host, "sync", "POST", body, auth = true)).optBoolean("ok")
    }

    /** Teacher-machine copy of the timetable. Empty object when never uploaded. */
    fun timetable(host: String): JSONObject {
        return JSONObject(request(host, "timetable", "GET", null, auth = true))
            .optJSONObject("timetable") ?: JSONObject()
    }

    fun pushTimetable(host: String, timetableJson: String): Boolean {
        val body = JSONObject().put("timetable", JSONObject(timetableJson)).toString()
        return JSONObject(request(host, "timetable", "POST", body, auth = true)).optBoolean("ok")
    }

    /** Per-class roster used by the 「班级」 page. */
    fun classes(host: String): String {
        return request(host, "classes", "GET", null, auth = true)
    }

}
