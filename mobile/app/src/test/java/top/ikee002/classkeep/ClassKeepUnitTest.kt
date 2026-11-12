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

    @Test
    fun timetableConfigGeneratesPeriodsAndBreaks() {
        val config = TimetableConfig(
            lessonsPerDay = 4,
            lessonMinutes = 40,
            shortBreakMinutes = 10,
            dayStart = "08:00",
            bigBreakAfter = 2,
            bigBreakMinutes = 30,
            noonAfter = 0,
            noonMinutes = 0,
            dualTimeColumn = true,
        )
        val periods = buildPeriods(config)
        assertEquals("第1节", periods[0].label)
        assertEquals("08:00", periods[0].start)
        assertEquals("08:40", periods[0].end)
        // 1: 08:00-08:40, 2: 08:50-09:30, 大课间 09:30-10:00,
        // 3: 10:00-10:40, 4: 10:50-11:30
        assertEquals("大课间", periods[2].label)
        assertTrue(periods[2].isBreak)
        assertEquals("09:30", periods[2].start)
        assertEquals("10:00", periods[2].end)
        assertEquals("第4节", periods.last().label)
        assertEquals("11:30", periods.last().end)
        assertEquals(5, periods.size)
    }

    @Test
    fun timetableNoonBreakAndSingleColumnConfig() {
        // org.json is a stub in plain JVM unit tests, so only the pure-Kotlin
        // period generator and config copy are asserted here.
        val config = TimetableConfig(
            lessonsPerDay = 4,
            lessonMinutes = 40,
            shortBreakMinutes = 10,
            dayStart = "08:00",
            bigBreakAfter = 0,
            noonAfter = 2,
            noonMinutes = 120,
            dualTimeColumn = false,
        )
        val periods = buildPeriods(config)
        // 1: 08:00-08:40, 2: 08:50-09:30, 午休 09:30-11:30,
        // 3: 11:30-12:10, 4: 12:20-13:00
        assertEquals("午休", periods[2].label)
        assertTrue(periods[2].isBreak)
        assertEquals("09:30", periods[2].start)
        assertEquals("11:30", periods[2].end)
        assertEquals("13:00", periods.last().end)
        assertFalse(config.dualTimeColumn)
        val toggled = config.copy(dualTimeColumn = true)
        assertTrue(toggled.dualTimeColumn)
        assertEquals(4, toggled.lessonsPerDay)
    }
}
