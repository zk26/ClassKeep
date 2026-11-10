package top.ikee002.classkeep

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ClassKeepUnitTest {

    @Test
    fun tokenMatchesTeacherMachineReference() {
        // Standard SHA-256("veyon-classroom:ABC123:nonce-0001"); must match
        // ClassroomSecrets::sessionToken on the C++ side.
        assertEquals(
            "e198264293e2ee9d35aade33089e0f33e560cd60143d01292483f9f952330e04",
            ClassKeepCrypto.token("ABC123", "nonce-0001"),
        )
    }

    @Test
    fun tokenChangesWithLessonKey() {
        assertTrue(ClassKeepCrypto.token("old", "n") != ClassKeepCrypto.token("new", "n"))
        assertTrue(ClassKeepCrypto.token("key", "n1") != ClassKeepCrypto.token("key", "n2"))
    }

    @Test
    fun announcementIsParsed() {
        val state = Announcement.parse("CLASSKEEP:11880:abc123:42:nonce9")!!
        assertEquals(11880, state.port)
        assertEquals("abc123", state.teacherId)
        assertEquals(42L, state.revision)
        assertEquals("nonce9", state.nonce)
    }

    @Test
    fun foreignPayloadIsRejected() {
        assertNull(Announcement.parse("VEYON_CLASSROOM:11880:abc:1:n"))
        assertNull(Announcement.parse("CLASSKEEP:bad:abc:1:n"))
        assertNull(Announcement.parse("CLASSKEEP:11880::1:n"))
    }

    @Test
    fun announcementNeverCarriesTheLessonKey() {
        assertFalse(Announcement.MAGIC.contains("key", ignoreCase = true))
        assertFalse(Announcement.PREFIX.contains("key", ignoreCase = true))
    }
}
