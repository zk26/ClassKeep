package top.ikee002.classkeep

/** Parsed from the UDP announcement: CLASSKEEP:<port>:<teacherId>:<revision>:<nonce>. */
object Announcement {
    const val PREFIX = "CLASSKEEP:"
    const val MAGIC = "CLASSKEEP_DISCOVER"
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
