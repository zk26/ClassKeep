package top.ikee002.classkeep

import java.security.MessageDigest

/** Shared with the teacher machine: SHA-256("veyon-classroom:<key>:<nonce>"), lowercase hex. */
object ClassKeepCrypto {
    fun sha256Hex(input: String): String {
        val digest = MessageDigest.getInstance("SHA-256").digest(input.toByteArray(Charsets.UTF_8))
        return digest.joinToString("") { byte -> "%02x".format(byte.toInt() and 0xff) }
    }

    fun token(lessonKey: String, nonce: String): String =
        sha256Hex("veyon-classroom:$lessonKey:$nonce")
}
