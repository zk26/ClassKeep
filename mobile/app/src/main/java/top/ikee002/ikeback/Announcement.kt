package top.ikee002.ikeback

/** Parsed from the UDP announcement: IKEBACK:<port>:<teacherId>:<revision>:<nonce>. */
object Announcement {
    const val PREFIX = "IKEBACK:"
    const val MAGIC = "IKEBACK_DISCOVER"
    const val DEFAULT_PORT = 11880

    fun parse(payload: String): TeacherState? {
        if (!payload.startsWith(PREFIX)) return null
        val parts = payload.removePrefix(PREFIX).split(":")
        if (parts.size < 4) return null
        val port = parts[0].toIntOrNull() ?: return null
        if (parts[1].isEmpty()) return null
        return TeacherState(
            teacherId = parts[1],
            revision = parts[2].toLongOrNull() ?: 0,
            port = port,
            nonce = parts.getOrElse(3) { "" },
            keyConfigured = true,
        )
    }
}
