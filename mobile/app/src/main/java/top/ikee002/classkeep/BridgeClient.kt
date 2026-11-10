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
 * every request proves knowledge of it with SHA-256("veyon-classroom:<key>:<nonce>"),
 * where nonce is a rotating challenge obtained from the public /state route.
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
        val connection = url(host, route).openConnection() as HttpURLConnection
        try {
            connection.connectTimeout = 5_000
            connection.readTimeout = 15_000
            connection.requestMethod = method
            connection.setRequestProperty("Accept", "application/json, */*")
            if (auth) {
                if (lessonKey.isEmpty() || nonce.isEmpty()) throw UnauthorizedException()
                connection.setRequestProperty("X-Classroom-Nonce", nonce)
                connection.setRequestProperty("X-Classroom-Token", ClassKeepCrypto.token(lessonKey, nonce))
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

    fun events(host: String): List<ActivityEvent> =
        parseEvents(request(host, "events", "GET", null, auth = true))

    fun roster(host: String): String =
        request(host, "roster", "GET", null, auth = true)

    fun wordCloud(host: String, seat: String): String =
        request(host, "wordcloud?seat=${java.net.URLEncoder.encode(seat, "UTF-8")}", "GET", null, auth = true)

    fun report(host: String): String =
        request(host, "report", "GET", null, auth = true)

    /** Every control command refreshes the challenge first, so a rotation never causes a 401. */
    fun command(
        host: String,
        name: String,
        enabled: Boolean,
        text: String = "",
        teacherIp: String = "",
    ): Boolean {
        connect(host)
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
        connect(host)
        val body = JSONObject().put("scores", JSONObject(snapshotRaw)).toString()
        return JSONObject(request(host, "sync", "POST", body, auth = true)).optBoolean("ok")
    }

}
