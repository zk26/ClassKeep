package top.ikee002.classkeep

import android.app.AlertDialog
import android.content.Context
import android.graphics.Typeface
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.InputType
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.text.DateFormat
import java.util.Date
import java.util.concurrent.Executors

private data class Cmd(val name: String, val label: String, val enabled: Boolean = true)

class MainActivity : AppCompatActivity() {

    private lateinit var cacheStore: CacheStore
    private lateinit var timetableStore: TimetableStore
    private val bridge = BridgeClient()
    private val executor = Executors.newSingleThreadExecutor()
    private val handler = Handler(Looper.getMainLooper())
    private var discovery: Discovery? = null

    private var cache = Cache()
    private var seats: List<SeatStudent> = emptyList()
    private var connected = false
    private var busy = false
    private var lastAutoSync = 0L

    private lateinit var contentHost: FrameLayout
    private lateinit var bottomBar: BottomBar
    // 2.0.0: 课堂 / 课表 / 配置 / 更多 -- four tabs with icon + name.
    private val pages = arrayOfNulls<View>(4)

    private lateinit var statusBadge: TextView
    private lateinit var messageView: TextView
    private lateinit var revisionView: TextView
    private lateinit var snapshotView: TextView
    private lateinit var activityView: TextView
    private lateinit var seatGrid: LinearLayout
    private lateinit var seatHint: TextView
    private lateinit var scheduleGrid: LinearLayout
    private lateinit var scheduleHint: TextView

    private lateinit var ipField: Ui.Field
    private lateinit var keyField: Ui.Field
    private lateinit var portField: Ui.Field

    private lateinit var keyLauncher: ActivityResultLauncher<Array<String>>
    private lateinit var timetableLauncher: ActivityResultLauncher<Array<String>>

    private val commands = listOf(
        Cmd("mute", "静音"), Cmd("unmute", "取消静音", false),
        Cmd("internet", "断网"), Cmd("internet", "恢复网络", false),
        Cmd("silence", "肃静页"), Cmd("silence", "收起肃静", false),
        Cmd("usb", "禁用 USB 输入"), Cmd("usb", "恢复 USB 输入", false),
        Cmd("remote", "远程控制"), Cmd("demo", "分组投屏"),
        Cmd("uwfQuery", "查询 UWF"), Cmd("uwfDisable", "关闭还原"),
        Cmd("uwfEnable", "开启还原"),
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        cacheStore = CacheStore(this)
        timetableStore = TimetableStore(this)
        cache = cacheStore.load()
        seats = parseSeats(cache.seatsRaw)

        contentHost = findViewById(R.id.content)
        val barHost = findViewById<FrameLayout>(R.id.bottomBarHost)
        bottomBar = BottomBar(
            this,
            listOf(
                BottomBar.Tab(R.drawable.ck_tab_class, "课堂"),
                BottomBar.Tab(R.drawable.ck_tab_schedule, "课表"),
                BottomBar.Tab(R.drawable.ck_tab_config, "配置"),
                BottomBar.Tab(R.drawable.ck_tab_more, "更多"),
            ),
        ) { index -> showTab(index) }
        // Explicit height keeps the docked bar from ever eating the content area.
        // Must match BottomBar.barHeight (62dp) + its 6dp outer padding.
        val barHeight = Ui.dp(this, 62) + Ui.dp(this, 6) * 2
        barHost.addView(
            bottomBar,
            FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, barHeight),
        )
        // targetSdk 35+ draws edge-to-edge. Keep content below the status bar
        // instead of relying on fitsSystemWindows (whose behaviour varies).
        val root = findViewById<LinearLayout>(R.id.root)
        ViewCompat.setOnApplyWindowInsetsListener(root) { view, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.setPadding(0, bars.top, 0, 0)
            insets
        }
        ViewCompat.setOnApplyWindowInsetsListener(barHost) { view, insets ->
            val nav = insets.getInsets(WindowInsetsCompat.Type.navigationBars())
            view.setPadding(
                view.paddingLeft, Ui.dp(this, 6), view.paddingRight,
                nav.bottom + Ui.dp(this, 8),
            )
            insets
        }

        keyLauncher = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            uri ?: return@registerForActivityResult
            val text = try {
                contentResolver.openInputStream(uri)?.bufferedReader(Charsets.UTF_8)
                    ?.use { it.readText() } ?: ""
            } catch (_: Exception) {
                ""
            }
            val key = extractKey(text)
            if (key.isEmpty()) {
                message("未从文件中读到密钥，请检查 .key 文件内容")
            } else {
                keyField.input.setText(key)
                applyKey(key)
                message("已从文件导入本堂课密钥")
            }
        }
        timetableLauncher = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            uri ?: return@registerForActivityResult
            val text = try {
                contentResolver.openInputStream(uri)?.bufferedReader(Charsets.UTF_8)
                    ?.use { it.readText() } ?: ""
            } catch (_: Exception) {
                ""
            }
            if (text.isBlank()) return@registerForActivityResult
            try {
                timetableStore.save(TimetableStore.fromJson(text))
                refreshSchedule()
                message("作息表已导入")
            } catch (_: Exception) {
                message("作息表格式无法识别")
            }
        }

        buildPages()
        showTab(0)
        startDiscovery()
    }

    // ------------------------------------------------------------------ pages

    private fun scrollPage(build: (LinearLayout) -> Unit): ScrollView {
        val scroll = ScrollView(this).apply {
            isFillViewport = true
            clipToPadding = false
            layoutParams = FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
            )
        }
        val column = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            val side = resources.getDimensionPixelSize(R.dimen.page_padding)
            val top = Ui.dp(this@MainActivity, 16)
            // Bottom gutter keeps the last row clear of the docked bar + gesture area.
            val bottom = Ui.dp(this@MainActivity, 110)
            setPadding(side, top, side, bottom)
        }
        build(column)
        scroll.addView(column)
        return scroll
    }

    private fun buildPages() {
        // 「课堂」= 状态概览 + 课堂控制 + 加分减分（原独立页签合并进来）
        pages[0] = scrollPage { column ->
            column.addView(brandHeader())

            val status = Ui.card(this)
            statusBadge = Ui.badge(this, "离线", false)
            val badgeRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
            badgeRow.addView(statusBadge)
            status.addView(badgeRow)
            messageView = Ui.body(this, "正在网段内查找教师机…")
            status.addView(messageView)
            val actions = Ui.row(this)
            val sync = Ui.button(this, "立即同步", primary = true)
            val offline = Ui.button(this, "离线工作")
            sync.setOnClickListener { doSync() }
            offline.setOnClickListener {
                connected = false
                setStatus(false)
                message("已断开，本地数据保留")
            }
            actions.addView(sync, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            actions.addView(offline, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            status.addView(actions)
            revisionView = Ui.helper(this, "本地版本 - - -")
            status.addView(revisionView)
            column.addView(status)

            val overview = Ui.card(this)
            overview.addView(Ui.section(this, "班级概况"))
            snapshotView = Ui.body(this)
            overview.addView(snapshotView)
            column.addView(overview)

            val activity = Ui.card(this)
            activity.addView(Ui.section(this, "最近活动"))
            activityView = Ui.body(this, "暂无活动记录", 12f)
            activity.addView(activityView)
            column.addView(activity)

            val control = Ui.card(this)
            control.addView(Ui.section(this, "课堂控制"))
            control.addView(Ui.body(this, "指令经教师机转发，需先在「配置」页填写 IP 与密钥。"))
            val columns = resources.getInteger(R.integer.grid_columns).coerceAtLeast(2)
            var controlRow: LinearLayout? = null
            commands.forEachIndexed { index, cmd ->
                if (index % columns == 0) {
                    controlRow = Ui.row(this)
                    control.addView(controlRow)
                }
                val button = Ui.button(this, cmd.label)
                button.setOnClickListener { sendCommand(cmd) }
                controlRow?.addView(
                    button,
                    LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f).apply {
                        marginEnd = Ui.dp(this@MainActivity, 8)
                    },
                )
            }
            column.addView(control)

            val score = Ui.card(this)
            score.addView(Ui.section(this, "加分减分"))
            score.addView(Ui.body(this, "卡片顺序与教师端座位网格拖拽后的顺序一致。点卡片选 -5…+5，再填原因。"))
            val refresh = Ui.button(this, "刷新学生列表", primary = true)
            refresh.setOnClickListener { loadSeats() }
            score.addView(refresh)
            seatHint = Ui.helper(this, "尚未同步学生名单")
            score.addView(seatHint)
            seatGrid = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
            score.addView(seatGrid)
            column.addView(score)

            refreshHome()
            refreshSeatCards()
        }

        // 「课表」= 可调作息 + 单列/双列时间 + 导入导出
        pages[1] = scrollPage { column ->
            val card = Ui.card(this)
            card.addView(Ui.section(this, "本周课表"))
            scheduleHint = Ui.body(this)
            card.addView(scheduleHint)

            val first = Ui.row(this)
            val setup = Ui.button(this, "作息设置", primary = true)
            val importJson = Ui.button(this, "导入作息")
            setup.setOnClickListener { showTimetableSettings() }
            importJson.setOnClickListener {
                timetableLauncher.launch(arrayOf("application/json", "text/plain", "*/*"))
            }
            first.addView(setup, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            first.addView(importJson, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            card.addView(first)

            val second = Ui.row(this)
            val exportJson = Ui.button(this, "导出作息")
            val reset = Ui.button(this, "恢复默认作息")
            exportJson.setOnClickListener { exportTimetable() }
            reset.setOnClickListener {
                timetableStore.save(Timetable())
                refreshSchedule()
                message("已恢复默认作息")
            }
            second.addView(exportJson, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            second.addView(reset, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            card.addView(second)
            card.addView(Ui.helper(this, "点课格改科目；当天列与当前节高亮。节数、大课间、午休与时间列样式在「作息设置」里调整。"))

            val horizontal = HorizontalScrollView(this).apply {
                isFillViewport = false
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                )
            }
            scheduleGrid = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
            horizontal.addView(scheduleGrid)
            card.addView(horizontal)
            column.addView(card)
            refreshSchedule()
        }

        // 「配置」= 连接参数 + 课表显示样式
        pages[2] = scrollPage { column ->
            val connection = Ui.card(this)
            connection.addView(Ui.section(this, "连接配置"))
            ipField = Ui.field(this, "教师机 IP", "与学生机、教师机处于同一局域网", initial = cache.teacherIp)
            keyField = Ui.field(
                this, "本堂课密钥", "与学生机相同；换课后需更新。支持手动输入或导入 .key 文件",
                password = true, initial = cache.sessionKey,
            )
            portField = Ui.field(
                this, "桥接端口", "默认 11880，一般无需修改",
                numeric = true, initial = cache.port.toString(),
            )
            connection.addView(ipField.container)
            connection.addView(keyField.container)
            connection.addView(portField.container)

            val keyRow = Ui.row(this)
            val showKey = Ui.button(this, "显示密钥")
            val importKey = Ui.button(this, "导入 .key 文件", primary = true)
            showKey.setOnClickListener {
                val visible = keyField.input.inputType !=
                    (InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD)
                keyField.input.inputType = if (visible)
                    InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
                else InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                keyField.input.setSelection(keyField.input.text.length)
                showKey.text = if (visible) "显示密钥" else "隐藏密钥"
            }
            importKey.setOnClickListener {
                keyLauncher.launch(arrayOf("text/plain", "application/json", "application/octet-stream", "*/*"))
            }
            keyRow.addView(showKey, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            keyRow.addView(importKey, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.4f))
            connection.addView(keyRow)

            val test = Ui.button(this, "立即同步并测试连接", primary = true)
            test.setOnClickListener { doSync() }
            connection.addView(test)
            connection.addView(Ui.helper(this, "密钥每堂课轮换；也可在「更多」页导入。"))
            column.addView(connection)

            val display = Ui.card(this)
            display.addView(Ui.section(this, "课表显示"))
            val toggle = Ui.button(this, "切换单列 / 双列时间", primary = true)
            toggle.setOnClickListener {
                val before = timetableStore.load().config.dualTimeColumn
                timetableStore.setDualTimeColumn(!before)
                refreshSchedule()
                message(if (before) "已切换为单列时间" else "已切换为双列时间")
            }
            display.addView(toggle)
            display.addView(Ui.helper(this, "双列：开始/结束各占一列；单列：一列内显示 08:20-09:00。"))
            column.addView(display)
        }

        // 「更多」= 导入导出 + 课堂回顾
        pages[3] = scrollPage { column ->
            val io = Ui.card(this)
            io.addView(Ui.section(this, "导入导出"))
            io.addView(Ui.body(this, "导出文件保存在 App 私有 Documents 目录，可用文件管理器或 adb 取走。"))
            val ioActions: List<Pair<String, () -> Unit>> = listOf(
                "导入本堂课密钥 (.key)" to {
                    keyLauncher.launch(arrayOf("text/plain", "application/json", "application/octet-stream", "*/*"))
                },
                "导入作息 JSON" to {
                    timetableLauncher.launch(arrayOf("application/json", "text/plain", "*/*"))
                },
                "导出课表 JSON" to { exportTimetable() },
                "导出本地缓存数据" to { exportLocalData() },
                "导出期末成绩表" to { exportReport() },
                "查看班级词云" to { showWordCloud() },
            )
            ioActions.forEach { (label, action) ->
                val button = Ui.button(this, label)
                button.setOnClickListener { action() }
                io.addView(button)
            }
            column.addView(io)

            val review = Ui.card(this)
            review.addView(Ui.section(this, "课堂回顾"))
            review.addView(Ui.body(this, "查询这堂课讲了什么、干了什么、同学表现怎么样。联网时读教师机，离线时读本地缓存。"))
            val query = Ui.button(this, "查询这堂课", primary = true)
            query.setOnClickListener { showLessonReview() }
            review.addView(query)
            column.addView(review)

            val about = Ui.card(this)
            about.addView(Ui.section(this, "关于"))
            about.addView(Ui.body(this, "ClassKeep · 教师机移动伴侣\n基于 Veyon 4.x 的教学管理定制分支\n许可：GPL-2.0-or-later"))
            about.addView(Ui.helper(this, "本软件采集学生行为数据，部署前请阅读 CLASSKEEP-PRIVACY.md"))
            column.addView(about)
        }
    }

    private fun brandHeader(): View {
        val header = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            background = Ui.shape(this@MainActivity, getColor(R.color.ck_primary), 20)
            val p = Ui.dp(this@MainActivity, 18)
            setPadding(p, p, p, p)
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
            ).apply { bottomMargin = Ui.dp(this@MainActivity, 14) }
            elevation = 0f
        }
        val mark = TextView(this).apply {
            text = "CK"
            textSize = 22f
            setTypeface(typeface, Typeface.BOLD)
            setTextColor(getColor(R.color.white))
            background = Ui.shape(this@MainActivity, 0x33FFFFFF, 12)
            val p = Ui.dp(this@MainActivity, 10)
            setPadding(p, p, p, p)
        }
        val text = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val name = TextView(this).apply {
            this.text = "ClassKeep"
            textSize = 22f
            setTypeface(typeface, Typeface.BOLD)
            setTextColor(getColor(R.color.white))
        }
        val sub = TextView(this).apply {
            this.text = "课堂看得见 · 成长记得下"
            textSize = 11f
            setTextColor(getColor(R.color.ck_brand_subtext))
        }
        text.addView(name)
        text.addView(sub)
        header.addView(mark)
        header.addView(text, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f).apply {
            marginStart = Ui.dp(this@MainActivity, 14)
        })
        return header
    }

    private fun showTab(index: Int) {
        contentHost.removeAllViews()
        pages[index]?.let {
            contentHost.addView(
                it,
                FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT,
                ),
            )
        }
        bottomBar.select(index)
        if (index == 1) refreshSchedule()
    }

    // ------------------------------------------------------------------ home

    private fun refreshHome() {
        if (::snapshotView.isInitialized.not()) return
        snapshotView.text = listOf(
            "班级：${cache.className.ifEmpty { "-" }}",
            "基础分：${cache.baseScore}",
            "规则已发布：${if (cache.rulesPublished) "是" else "否"}",
            "节次 ${cache.lessons} · 记录 ${cache.entries} · 学生 ${cache.students}",
        ).joinToString("\n")
        revisionView.text = "本地版本 ${if (cache.revision > 0) cache.revision else "-"} · " +
            if (cache.updatedAt > 0) DateFormat.getDateTimeInstance().format(Date(cache.updatedAt)) else "尚未同步"
        activityView.text = if (cache.events.isEmpty()) "暂无活动记录"
        else cache.events.takeLast(8).reversed().joinToString("\n") {
            "${it.time()}  ${it.label()}  ${it.detail}"
        }
    }

    private fun setStatus(ok: Boolean) {
        if (::statusBadge.isInitialized.not()) return
        statusBadge.text = if (ok) "已连接" else "离线"
        statusBadge.setTextColor(getColor(if (ok) R.color.ck_badge_ok_fg else R.color.ck_badge_off_fg))
        statusBadge.background = Ui.shape(
            this,
            getColor(if (ok) R.color.ck_badge_ok_bg else R.color.ck_badge_off_bg),
            12,
        )
    }

    private fun message(text: String) {
        if (::messageView.isInitialized) messageView.text = text
    }

    private fun currentIp(): String = ipField.input.text.toString().trim()
    private fun currentPort(): Int = portField.input.text.toString().trim().toIntOrNull() ?: 11880
    private fun currentKey(): String = keyField.input.text.toString().trim()

    private fun applyKey(key: String) {
        bridge.lessonKey = key
        cache = cache.copy(sessionKey = key)
        persist()
    }

    private fun persist() {
        cache = cache.copy(
            teacherIp = if (::ipField.isInitialized) currentIp() else cache.teacherIp,
            port = if (::portField.isInitialized) currentPort() else cache.port,
            sessionKey = if (::keyField.isInitialized) currentKey() else cache.sessionKey,
        )
        cacheStore.save(cache)
    }

    private fun startDiscovery() {
        discovery = Discovery(this) { ip, announced ->
            runOnUiThread {
                ipField.input.setText(ip)
                portField.input.setText(announced.port.toString())
                bridge.port = announced.port
                val now = System.currentTimeMillis()
                if (now - lastAutoSync > 30_000 && currentKey().isNotEmpty()) {
                    lastAutoSync = now
                    doSync()
                }
            }
        }
        discovery?.start(currentPort())
    }

    private fun doSync() {
        val host = currentIp()
        val key = currentKey()
        if (host.isEmpty()) { message("请先填写教师机 IP"); return }
        if (key.isEmpty()) { message("请先输入或导入本堂课密钥"); return }
        bridge.port = currentPort()
        applyKey(key)
        busy = true
        message("正在与 $host:${bridge.port} 同步…")
        background {
            try {
                val state = bridge.connect(host)
                if (!state.keyConfigured) {
                    ui { message("教师机尚未生成本堂课密钥，请先在教师端「开始新课堂」") }
                    return@background
                }
                val scores = bridge.scores(host)
                val events = bridge.events(host)
                val loadedSeats = try { bridge.seats(host) } catch (_: Exception) { emptyList() }
                val seatsJson = JSONObject().put(
                    "seats",
                    JSONArray().apply {
                        loadedSeats.forEach {
                            put(
                                JSONObject()
                                    .put("seat", it.seat)
                                    .put("student", it.student)
                                    .put("ip", it.ip)
                                    .put("score", it.score)
                            )
                        }
                    },
                ).toString()
                ui {
                    connected = true
                    setStatus(true)
                    cache = cache.copy(
                        revision = state.revision,
                        className = state.className.ifEmpty { scores.className },
                        baseScore = scores.rules.baseScore,
                        rulesPublished = scores.rules.published,
                        lessons = scores.lessons.size,
                        entries = scores.entries.size,
                        students = loadedSeats.size,
                        scoresRaw = scores.raw,
                        seatsRaw = seatsJson,
                        events = events,
                        updatedAt = System.currentTimeMillis(),
                    )
                    seats = loadedSeats
                    persist()
                    refreshHome()
                    refreshSeatCards()
                    message("同步完成 · 版本 ${state.revision} · ${cache.className}")
                }
            } catch (e: UnauthorizedException) {
                ui { setStatus(false); message("密钥被拒绝，换新课后请更新密钥") }
            } catch (e: Exception) {
                ui { setStatus(false); message("未找到教师机，保留本地数据：${e.message ?: ""}") }
            } finally {
                busy = false
            }
        }
    }

    // ------------------------------------------------------------------ seats

    private fun loadSeats() {
        val host = currentIp()
        if (host.isEmpty() || currentKey().isEmpty()) {
            message("请先在「配置」页填写 IP 与密钥")
            return
        }
        bridge.port = currentPort()
        applyKey(currentKey())
        background {
            try {
                val loaded = bridge.seats(host)
                ui {
                    seats = loaded
                    seatHint.text = "共 ${loaded.size} 名学生 · 顺序与教师端座位网格一致"
                    refreshSeatCards()
                }
            } catch (e: Exception) {
                ui { seatHint.text = "读取失败，显示本地缓存：${e.message ?: ""}" }
            }
        }
    }

    private fun refreshSeatCards() {
        if (::seatGrid.isInitialized.not()) return
        seatGrid.removeAllViews()
        val columns = resources.getInteger(R.integer.grid_columns).coerceAtLeast(2)
        var row: LinearLayout? = null
        seats.forEachIndexed { index, student ->
            if (index % columns == 0) {
                row = Ui.row(this)
                seatGrid.addView(row)
            }
            val card = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER
                background = Ui.shape(this@MainActivity, getColor(R.color.ck_card), 14, getColor(R.color.ck_border))
                val p = Ui.dp(this@MainActivity, 14)
                setPadding(p, p, p, p)
                isClickable = true
                setOnClickListener { chooseScore(student) }
            }
            val seat = TextView(this).apply {
                text = "${student.seat} 号"
                textSize = 12f
                setTextColor(getColor(R.color.ck_secondary))
            }
            val name = TextView(this).apply {
                text = student.student.ifEmpty { "—" }
                textSize = 15f
                setTypeface(typeface, Typeface.BOLD)
                setTextColor(getColor(R.color.ck_title))
            }
            val score = TextView(this).apply {
                text = "${student.score} 分"
                textSize = 18f
                setTypeface(typeface, Typeface.BOLD)
                setTextColor(getColor(R.color.ck_primary))
            }
            card.addView(seat)
            card.addView(name)
            card.addView(score)
            row?.addView(
                card,
                LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f).apply {
                    marginEnd = Ui.dp(this@MainActivity, 8)
                },
            )
        }
        if (seats.isEmpty()) {
            seatGrid.addView(Ui.body(this, "暂无学生数据，请先在「配置」页连接并同步。"))
        }
    }

    private fun chooseScore(student: SeatStudent) {
        val values = (5 downTo -5).toList()
        val labels = values.map { if (it > 0) "+$it" else it.toString() }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("${student.seat} 号 ${student.student} · 当前 ${student.score} 分")
            .setItems(labels) { _, which ->
                val delta = values[which]
                if (delta == 0) return@setItems
                askReason(student, delta)
            }
            .show()
    }

    private fun askReason(student: SeatStudent, delta: Int) {
        val input = EditText(this).apply {
            hint = "加分/扣分原因（可留空）"
            inputType = InputType.TYPE_CLASS_TEXT
            val p = Ui.dp(this@MainActivity, 12)
            setPadding(p, p, p, p)
        }
        AlertDialog.Builder(this)
            .setTitle("${if (delta > 0) "+" else ""}$delta 分")
            .setView(input)
            .setPositiveButton("确认") { _, _ ->
                submitScore(student, delta, input.text.toString().trim())
            }
            .setNegativeButton("取消", null)
            .show()
    }

    /** Optimistic update: show the new score first, roll back only on failure. */
    private fun submitScore(student: SeatStudent, delta: Int, reason: String) {
        val host = currentIp()
        if (host.isEmpty() || currentKey().isEmpty()) {
            message("请先填写教师机 IP 与本堂课密钥")
            return
        }
        bridge.port = currentPort()
        applyKey(currentKey())
        val index = seats.indexOfFirst { it.seat == student.seat }
        if (index >= 0) {
            seats = seats.toMutableList().also {
                it[index] = it[index].copy(score = it[index].score + delta)
            }.toList()
            refreshSeatCards()
        }
        background {
            val ok = try { bridge.score(host, student.seat, delta, reason) } catch (_: Exception) { false }
            ui {
                if (ok) {
                    message("${student.seat} 号 ${student.student} ${if (delta > 0) "+" else ""}$delta 分已记录")
                } else if (index >= 0) {
                    seats = seats.toMutableList().also {
                        it[index] = it[index].copy(score = it[index].score - delta)
                    }.toList()
                    refreshSeatCards()
                    message("加分未送达教师机，已恢复原分数")
                }
            }
        }
    }

    // ---------------------------------------------------------------- control

    private fun sendCommand(cmd: Cmd) {
        val host = currentIp()
        if (host.isEmpty() || currentKey().isEmpty()) {
            message("请先填写教师机 IP 与本堂课密钥")
            return
        }
        bridge.port = currentPort()
        applyKey(currentKey())
        background {
            try {
                val ok = bridge.command(host, cmd.name, cmd.enabled, "", host)
                ui { message(if (ok) "指令「${cmd.label}」已下发" else "指令「${cmd.label}」被拒绝") }
            } catch (e: Exception) {
                ui { message("指令失败：${e.message ?: ""}") }
            }
        }
    }

    // -------------------------------------------------------------- schedule

    private fun refreshSchedule() {
        if (::scheduleGrid.isInitialized.not()) return
        val timetable = timetableStore.load()
        scheduleGrid.removeAllViews()
        val cellWidth = resources.getDimensionPixelSize(R.dimen.timetable_cell)
        val day = todayDay()
        val now = currentPeriod(timetable)
        val next = nextPeriod(timetable)
        val nextText = next?.let { timetable.periods.getOrNull(it)?.label ?: "" } ?: "今天没有后续课程"
        scheduleHint.text = "今天 ${WEEKDAYS[day - 1]} · " +
            (now?.let { "正在上「${timetable.periods[it].label}」" } ?: "当前无课") +
            " · 下一节：$nextText"

        // 2.0.0: single column shows "08:20-09:00", dual column splits 起/止.
        val half = cellWidth / 2
        val dual = timetable.config.dualTimeColumn
        val header = Ui.row(this).apply { topMarginCompat(0) }
        if (dual) {
            header.addView(cellText("开始", isHeader = true),
                LinearLayout.LayoutParams(half, ViewGroup.LayoutParams.WRAP_CONTENT))
            header.addView(cellText("结束", isHeader = true),
                LinearLayout.LayoutParams(half, ViewGroup.LayoutParams.WRAP_CONTENT))
        } else {
            header.addView(cellText("时间", isHeader = true),
                LinearLayout.LayoutParams(cellWidth, ViewGroup.LayoutParams.WRAP_CONTENT))
        }
        WEEKDAYS.forEachIndexed { i, label ->
            header.addView(cellText(label, isHeader = true, highlighted = i + 1 == day),
                LinearLayout.LayoutParams(cellWidth, ViewGroup.LayoutParams.WRAP_CONTENT))
        }
        scheduleGrid.addView(header)

        timetable.periods.forEachIndexed { periodIndex, period ->
            val row = Ui.row(this).apply { topMarginCompat(0) }
            if (dual) {
                row.addView(
                    cellText("${period.label}\n${period.start}", isHeader = true, highlighted = periodIndex == now),
                    LinearLayout.LayoutParams(half, ViewGroup.LayoutParams.WRAP_CONTENT),
                )
                row.addView(
                    cellText(period.end, isHeader = true, highlighted = periodIndex == now),
                    LinearLayout.LayoutParams(half, ViewGroup.LayoutParams.WRAP_CONTENT),
                )
            } else {
                row.addView(
                    cellText("${period.label}\n${period.start}-${period.end}",
                        isHeader = true, highlighted = periodIndex == now),
                    LinearLayout.LayoutParams(cellWidth, ViewGroup.LayoutParams.WRAP_CONTENT),
                )
            }
            for (d in 1..7) {
                val subject = TimetableStore.subjectAt(timetable, d, periodIndex)
                val cell = cellText(
                    subject,
                    isHeader = false,
                    highlighted = d == day && periodIndex == now,
                    color = if (subject.isEmpty()) null else subjectColor(subject),
                )
                cell.setOnClickListener { editSubject(d, periodIndex, timetable) }
                row.addView(cell, LinearLayout.LayoutParams(cellWidth, ViewGroup.LayoutParams.WRAP_CONTENT))
            }
            scheduleGrid.addView(row)
        }
    }

    private fun LinearLayout.topMarginCompat(value: Int) {
        (layoutParams as? LinearLayout.LayoutParams)?.topMargin = value
    }

    private fun cellText(
        text: String,
        isHeader: Boolean,
        highlighted: Boolean = false,
        color: Int? = null,
    ): TextView = TextView(this).apply {
        this.text = text
        textSize = if (isHeader) 10f else 12f
        gravity = Gravity.CENTER
        minimumHeight = Ui.dp(this@MainActivity, if (isHeader) 34 else 46)
        val fill = color ?: when {
            highlighted -> getColor(R.color.ck_chip_pressed)
            isHeader -> getColor(R.color.ck_input_bg)
            else -> getColor(R.color.ck_card)
        }
        setTextColor(if (color != null) getColor(R.color.white) else getColor(R.color.ck_title))
        background = Ui.shape(this@MainActivity, fill, 10, getColor(R.color.ck_border))
        val p = Ui.dp(this@MainActivity, 4)
        setPadding(p, p, p, p)
        layoutParams = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.WRAP_CONTENT,
            ViewGroup.LayoutParams.WRAP_CONTENT,
        ).apply { setMargins(2, 2, 2, 2) }
    }

    private fun editSubject(day: Int, period: Int, timetable: Timetable) {
        val current = TimetableStore.subjectAt(timetable, day, period)
        val input = EditText(this).apply {
            setText(current)
            hint = "科目名称，留空则清除"
            val p = Ui.dp(this@MainActivity, 12)
            setPadding(p, p, p, p)
        }
        AlertDialog.Builder(this)
            .setTitle("${WEEKDAYS[day - 1]} · ${timetable.periods[period].label}")
            .setView(input)
            .setPositiveButton("保存") { _, _ ->
                timetableStore.setSubject(day, period, input.text.toString())
                refreshSchedule()
            }
            .setNeutralButton("常用科目") { _, _ ->
                AlertDialog.Builder(this)
                    .setItems(SUBJECT_PRESETS.toTypedArray()) { _, which ->
                        timetableStore.setSubject(day, period, SUBJECT_PRESETS[which])
                        refreshSchedule()
                    }.show()
            }
            .setNegativeButton("取消", null)
            .show()
    }

    // --------------------------------------------------- timetable settings & io

    /** 一天几节课 / 每节时长 / 大课间 / 午休 / 单列或双列时间，全部可调。 */
    private fun showTimetableSettings() {
        val config = timetableStore.load().config
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            val p = Ui.dp(this@MainActivity, 12)
            setPadding(p, p, p, p)
        }

        fun addField(label: String, value: String, numeric: Boolean = true): EditText {
            box.addView(Ui.body(this@MainActivity, label, 12f))
            val edit = EditText(this@MainActivity).apply {
                setText(value)
                inputType = if (numeric) InputType.TYPE_CLASS_NUMBER else InputType.TYPE_CLASS_TEXT
                val pad = Ui.dp(this@MainActivity, 10)
                setPadding(pad, pad, pad, pad)
                background = Ui.shape(this@MainActivity, this@MainActivity.getColor(R.color.ck_input_bg), 12)
            }
            box.addView(
                edit,
                LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                ),
            )
            return edit
        }

        val lessons = addField("一天几节课（1-12）", config.lessonsPerDay.toString())
        val lessonMinutes = addField("每节时长（分钟）", config.lessonMinutes.toString())
        val dayStart = addField("第一节开始时间（HH:mm）", config.dayStart, numeric = false)
        val shortBreak = addField("普通课间（分钟）", config.shortBreakMinutes.toString())
        val bigAfter = addField("大课间在第几节课后（0=不设）", config.bigBreakAfter.toString())
        val bigMinutes = addField("大课间时长（分钟）", config.bigBreakMinutes.toString())
        val noonAfter = addField("午休在第几节课后（0=不设）", config.noonAfter.toString())
        val noonMinutes = addField("午休时长（分钟）", config.noonMinutes.toString())
        val dual = android.widget.CheckBox(this).apply {
            text = "双列时间（开始/结束分两列）"
            isChecked = config.dualTimeColumn
            setTextColor(this@MainActivity.getColor(R.color.ck_title))
        }
        box.addView(dual)

        val scroll = ScrollView(this).apply { addView(box) }
        AlertDialog.Builder(this)
            .setTitle("作息设置")
            .setView(scroll)
            .setPositiveButton("保存并生成课表") { _, _ ->
                val next = TimetableConfig(
                    lessonsPerDay = lessons.text.toString().toIntOrNull() ?: config.lessonsPerDay,
                    lessonMinutes = lessonMinutes.text.toString().toIntOrNull() ?: config.lessonMinutes,
                    shortBreakMinutes = shortBreak.text.toString().toIntOrNull() ?: config.shortBreakMinutes,
                    dayStart = dayStart.text.toString().trim().ifEmpty { config.dayStart },
                    bigBreakAfter = bigAfter.text.toString().toIntOrNull() ?: config.bigBreakAfter,
                    bigBreakMinutes = bigMinutes.text.toString().toIntOrNull() ?: config.bigBreakMinutes,
                    noonAfter = noonAfter.text.toString().toIntOrNull() ?: config.noonAfter,
                    noonMinutes = noonMinutes.text.toString().toIntOrNull() ?: config.noonMinutes,
                    dualTimeColumn = dual.isChecked,
                )
                timetableStore.updateConfig(next)
                refreshSchedule()
                message("作息已更新：${next.lessonsPerDay} 节课")
            }
            .setNegativeButton("取消", null)
            .show()
    }

    private fun exportTimetable() {
        try {
            val dir = getExternalFilesDir(android.os.Environment.DIRECTORY_DOCUMENTS) ?: filesDir
            val file = File(dir, "ClassKeep-课表.json")
            file.writeText(TimetableStore.toJson(timetableStore.load()), Charsets.UTF_8)
            message("课表已导出：${file.absolutePath}")
        } catch (e: Exception) {
            message("导出失败：${e.message ?: ""}")
        }
    }

    private fun exportLocalData() {
        try {
            val obj = JSONObject()
                .put("exportedAt", DateFormat.getDateTimeInstance().format(Date()))
                .put("teacherIp", cache.teacherIp)
                .put("className", cache.className)
                .put("revision", cache.revision)
                .put("scores", JSONObject(cache.scoresRaw.ifBlank { "{}" }))
                .put("seats", JSONObject(cache.seatsRaw.ifBlank { "{}" }))
            val events = JSONArray()
            cache.events.forEach { event ->
                events.put(
                    JSONObject()
                        .put("timestamp", event.timestamp)
                        .put("student", event.student)
                        .put("ip", event.ip)
                        .put("host", event.host)
                        .put("detail", event.detail),
                )
            }
            obj.put("events", events)
            val dir = getExternalFilesDir(android.os.Environment.DIRECTORY_DOCUMENTS) ?: filesDir
            val file = File(dir, "ClassKeep-本地数据.json")
            file.writeText(obj.toString(2), Charsets.UTF_8)
            message("本地数据已导出：${file.absolutePath}")
        } catch (e: Exception) {
            message("导出失败：${e.message ?: ""}")
        }
    }

    /** 这堂课讲了什么 / 干了什么 / 同学表现怎么样。 */
    private fun showLessonReview() {
        val host = currentIp()
        if (host.isEmpty() || currentKey().isEmpty()) {
            message("请先在「配置」页填写 IP 与密钥")
            return
        }
        bridge.port = currentPort()
        applyKey(currentKey())
        background {
            try {
                bridge.connect(host)
                val scores = bridge.scores(host)
                val events: List<ActivityEvent> =
                    try { bridge.events(host) } catch (_: Exception) { emptyList() }
                ui { renderLessonReview(scores, events) }
            } catch (e: Exception) {
                ui {
                    try {
                        renderLessonReview(parseScoreSnapshot(cache.scoresRaw), cache.events)
                    } catch (_: Exception) {
                        message("查询失败，请先同步：${e.message ?: ""}")
                    }
                }
            }
        }
    }

    private fun renderLessonReview(scores: ScoreSnapshot, events: List<ActivityEvent>) {
        val lessons = scores.lessons
        if (lessons.isEmpty()) {
            AlertDialog.Builder(this)
                .setTitle("课堂回顾")
                .setMessage("还没有节次记录，请先在教师端「开始新课堂」并同步。")
                .setPositiveButton("关闭", null)
                .show()
            return
        }
        val labels = lessons.map { lesson ->
            val date = lesson["date"].orEmpty()
            val topic = lesson["topic"].orEmpty().ifEmpty { "未填写主题" }
            "$date  $topic"
        }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("选择一堂课")
            .setItems(labels) { _, which ->
                val lesson = lessons[which]
                val id = lesson["id"].orEmpty()
                val date = lesson["date"].orEmpty()
                val topic = lesson["topic"].orEmpty()
                val entries = scores.entries.filter { it["lessonId"]?.toString().orEmpty() == id }
                val totals = LinkedHashMap<String, Int>()
                entries.forEach { entry ->
                    val name = entry["student"]?.toString().orEmpty()
                        .ifEmpty { entry["seat"]?.toString().orEmpty() }
                    val delta = (entry["delta"] as? Number)?.toInt() ?: 0
                    totals[name] = (totals[name] ?: 0) + delta
                }
                val ranked = totals.entries.sortedByDescending { it.value }
                val performance = if (ranked.isEmpty()) {
                    "  本节暂无加减分记录"
                } else {
                    ranked.take(10).joinToString("\n") { item ->
                        val sign = if (item.value > 0) "+" else ""
                        "  ${item.key}：$sign${item.value} 分"
                    }
                }
                val happened = events
                    .filter { date.isEmpty() || it.timestamp.startsWith(date) }
                    .takeLast(12)
                    .reversed()
                    .joinToString("\n") { "  ${it.time()} ${it.label()} ${it.detail}" }
                    .ifEmpty { "  本节暂无活动记录" }
                val text = buildString {
                    append("讲了什么：").append(topic.ifEmpty { "未填写" }).append("\n\n")
                    append("这堂课干了什么（活动记录）：\n").append(happened).append("\n\n")
                    append("同学表现（本节加减分）：\n").append(performance)
                }
                AlertDialog.Builder(this)
                    .setTitle("$date 课堂回顾")
                    .setMessage(text)
                    .setPositiveButton("关闭", null)
                    .show()
            }
            .show()
    }

    // ------------------------------------------------------------------- more

    private fun exportReport() {
        val host = currentIp()
        if (host.isEmpty() || currentKey().isEmpty()) { message("请先填写 IP 与密钥"); return }
        bridge.port = currentPort()
        applyKey(currentKey())
        background {
            try {
                bridge.connect(host)
                val xml = bridge.report(host)
                val dir = getExternalFilesDir(android.os.Environment.DIRECTORY_DOCUMENTS)
                val file = File(dir, "ClassKeep-期末成绩表.xls")
                file.writeText(xml, Charsets.UTF_8)
                ui { message("期末表已保存：${file.absolutePath}") }
            } catch (e: Exception) {
                ui { message("导出失败：${e.message ?: ""}") }
            }
        }
    }

    private fun showWordCloud() {
        val host = currentIp()
        if (host.isEmpty() || currentKey().isEmpty()) { message("请先填写 IP 与密钥"); return }
        bridge.port = currentPort()
        applyKey(currentKey())
        background {
            try {
                bridge.connect(host)
                val svg = bridge.wordCloud(host, "")
                ui {
                    val web = android.webkit.WebView(this).apply {
                        settings.javaScriptEnabled = false
                        loadDataWithBaseURL(null, svg, "image/svg+xml", "utf-8", null)
                    }
                    AlertDialog.Builder(this)
                        .setTitle("班级兴趣词云")
                        .setView(web)
                        .setPositiveButton("关闭", null)
                        .show()
                }
            } catch (e: Exception) {
                ui { message("词云获取失败：${e.message ?: ""}") }
            }
        }
    }

    // ------------------------------------------------------------------ utils

    private fun background(block: () -> Unit) {
        executor.execute {
            try { block() } catch (_: Exception) { }
        }
    }

    private fun ui(block: () -> Unit) {
        handler.post {
            if (!isFinishing && !isDestroyed) block()
        }
    }

    private fun extractKey(raw: String): String {
        val text = raw.trim()
        if (text.isEmpty()) return ""
        if (text.startsWith("{")) {
            return try {
                val obj = JSONObject(text)
                listOf("sessionKey", "key", "lessonKey", "密钥")
                    .firstNotNullOfOrNull { obj.optString(it).takeIf { v -> v.isNotBlank() } }
                    ?.trim() ?: ""
            } catch (_: Exception) { "" }
        }
        return text.lineSequence().firstOrNull { it.isNotBlank() }?.trim()
            ?.removePrefix("sessionKey=")?.removePrefix("key=") ?: ""
    }

    override fun onDestroy() {
        discovery?.stop()
        executor.shutdownNow()
        super.onDestroy()
    }
}
