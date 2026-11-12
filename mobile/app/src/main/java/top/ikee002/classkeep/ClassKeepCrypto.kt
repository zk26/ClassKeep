package top.ikee002.classkeep

import java.security.MessageDigest
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

/**
 * Shared with the teacher machine (ClassroomSecrets::requestToken).
 *
 * token = HMAC-SHA256(
 *     key   = lessonKey,
 *     msg   = method + "\n" + path + "\n" + nonce + "\n" + SHA256(body).hex
 * )
 *
 * Binding the method, full path (including the query string) and the request
 * body means a captured token cannot be replayed against a different call.
 */
object ClassKeepCrypto {
    fun sha256Hex(bytes: ByteArray): String {
        val digest = MessageDigest.getInstance("SHA-256").digest(bytes)
        return digest.joinToString("") { byte -> "%02x".format(byte.toInt() and 0xff) }
    }

    fun token(
        lessonKey: String,
        method: String,
        path: String,
        nonce: String,
        body: String?,
    ): String {
        val bodyHash = sha256Hex((body ?: "").toByteArray(Charsets.UTF_8))
        val message = "$method\n$path\n$nonce\n$bodyHash"
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(lessonKey.toByteArray(Charsets.UTF_8), "HmacSHA256"))
        return mac.doFinal(message.toByteArray(Charsets.UTF_8))
            .joinToString("") { byte -> "%02x".format(byte.toInt() and 0xff) }
    }
}
