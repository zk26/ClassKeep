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
    fun singleScheduleGeneratesPeriodsAndBreaks() {
        val config = TimetableConfig(
            lessonsPerDay = 4,
            lessonMinutes = 40,
            shortBreakMinutes = 10,
            dayStart = "08:00",
            bigBreakAfter = 2,
            bigBreakMinutes = 30,
            noonAfter = 0,
            dualTime = false,
        )
        val periods = buildPeriods(config)
        // 一 08:00-08:40, 二 08:50-09:30, 大课间 09:30-10:00,
        // 三 10:00-10:40, 四 10:50-11:30
        assertEquals("一", periods[0].label)
        assertEquals(1, periods[0].times.size)
        assertEquals("08:00-08:40", periods[0].times[0])
        assertEquals("08:00", periods[0].start)
        assertEquals("08:40", periods[0].end)
        assertEquals("大课间", periods[2].label)
        assertTrue(periods[2].isBreak)
        assertEquals("09:30-10:00", periods[2].times[0])
        assertEquals("四", periods.last().label)
        assertEquals("11:30", periods.last().end)
        assertEquals(5, periods.size)
    }

    @Test
    fun dualScheduleHasTwoParallelTimeRangesPerRow() {
        val config = TimetableConfig(
            lessonsPerDay = 4,
            lessonMinutes = 40,
            shortBreakMinutes = 10,
            dayStart = "08:20",
            bigBreakAfter = 2,
            bigBreakMinutes = 30,
            noonAfter = 4,
            dualTime = true,
            timeLabel1 = "小学",
            timeLabel2 = "初高中",
            secondShiftMinutes = -20,
        )
        val periods = buildPeriods(config)
        assertEquals(2, periods[0].times.size)
        // 小学 08:20-09:00, 初高中 08:00-08:40 (shifted -20 min)
        assertEquals("08:20-09:00", periods[0].times[0])
        assertEquals("08:00-08:40", periods[0].times[1])
        assertTrue(periods[2].isBreak)
        assertEquals(2, periods[2].times.size)
        val labels = if (config.dualTime) listOf(config.timeLabel1, config.timeLabel2) else emptyList()
        assertEquals(listOf("小学", "初高中"), labels)
        assertNull(currentPeriod(Timetable(periods = periods), 7 * 60))
        assertEquals(0, currentPeriod(Timetable(periods = periods), 8 * 60 + 30))
    }

    @Test
    fun noonBreakAndColourPaletteAreTolerant() {
        // org.json is a stub in plain JVM unit tests, so only pure-Kotlin
        // helpers are asserted here.
        val config = TimetableConfig(
            lessonsPerDay = 4,
            lessonMinutes = 40,
            shortBreakMinutes = 10,
            dayStart = "08:00",
            bigBreakAfter = 0,
            noonAfter = 2,
            noonMinutes = 120,
            dualTime = false,
        )
        val periods = buildPeriods(config)
        assertEquals("午休", periods[2].label)
        assertTrue(periods[2].isBreak)
        assertEquals("09:30-11:30", periods[2].times[0])
        assertEquals("13:00", periods.last().end)
        assertFalse(config.dualTime)
        assertTrue(config.copy(dualTime = true).dualTime)
        assertEquals(4, config.lessonsPerDay)
        // Out-of-range / negative colour indices wrap instead of crashing.
        assertTrue(courseColor(-1) == courseColor(COURSE_COLORS.size - 1))
        assertTrue(courseColor(999) == courseColor(999 % COURSE_COLORS.size))
        assertNull(parseTimeRange("not-a-time"))
        assertEquals(480..540, parseTimeRange("8:00-9:00"))
    }

    @Test
    fun courseAtFindsSpanningCourses() {
        val course = TimetableCourse(
            name = "通用技术", dayOfWeek = 3, startSection = 1, endSection = 2,
            className = "高一4",
        )
        val table = Timetable(periods = buildPeriods(TimetableConfig()), courses = listOf(course))
        assertEquals(course, courseAt(table, 3, 1))
        assertEquals(course, courseAt(table, 3, 2))
        assertNull(courseAt(table, 3, 3))
        assertNull(courseAt(table, 2, 1))
    }
}
