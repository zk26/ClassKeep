package top.ikee002.ikeback

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
import android.widget.ImageView
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
    // 2.0.x: 课堂 / 班级 / 课表 / 配置 / 更多 -- five tabs with icon + name.
    private val pages = arrayOfNulls<View>(5)

    private lateinit var statusBadge: TextView
    private lateinit var messageView: TextView
    private lateinit var revisionView: TextView
    private lateinit var setupHint: TextView
    private lateinit var goConfigButton: android.widget.Button
    private lateinit var statusActions: LinearLayout
    private lateinit var snapshotView: TextView
    private lateinit var activityView: TextView
    private lateinit var seatGrid: LinearLayout
    private lateinit var seatHint: TextView
    private lateinit var scheduleHint: TextView
    private lateinit var timetableAxis: TimetableAxisView
    private lateinit var timetableGrid: TimetableCourseGridView
    private lateinit var classHost: LinearLayout
    private lateinit var classHint: TextView
    private var classNames: List<String> = emptyList()

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
                BottomBar.Tab(R.drawable.ikeback_tab_class, "课堂"),
                BottomBar.Tab(R.drawable.ikeback_tab_classroom, "班级"),
                BottomBar.Tab(R.drawable.ikeback_tab_schedule, "课表"),
                BottomBar.Tab(R.drawable.ikeback_tab_config, "配置"),
                BottomBar.Tab(R.drawable.ikeback_tab_more, "更多"),
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
                pushTimetableToTeacher()
                message("作息表已导入（兼容 ClassBoard 全校作息 JSON）")
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
        // 「课堂」= 紧凑连接提示 + 课堂控制 + 加分减分。
        // 未配置时状态卡收缩成一行提示，主要区域留给控制与学生名单。
        pages[0] = scrollPage { column ->
            column.addView(brandHeader())

            val status = Ui.card(this)
            statusBadge = Ui.badge(this, "离线", false)
            val badgeRow = Ui.row(this)
            badgeRow.addView(statusBadge)
            badgeRow.addView(Ui.spacer(this))
            status.addView(badgeRow)
            messageView = Ui.body(this, "正在网段内查找教师机…")
            status.addView(messageView)
            revisionView = Ui.helper(this, "本地版本 - - -")
            status.addView(revisionView)
            setupHint = Ui.helper(this, "")
            status.addView(setupHint)
            val goConfig = Ui.button(this, "前往「配置」填写教师机 IP 与密钥", primary = true)
            goConfig.setOnClickListener { showTab(3) }
            goConfigButton = goConfig
            status.addView(goConfig)
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
            statusActions = actions
            status.addView(actions)
            column.addView(status)

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
            refreshConnectionHint()
        }

        // 「班级」= 按班级归档的课堂自然记录 + 概况 + 最近活动
        pages[1] = scrollPage { column ->
            val classes = Ui.card(this)
            classes.addView(Ui.section(this, "班级记录"))
            classes.addView(Ui.body(this, "每堂课讲了什么、干了什么、同学表现如何，按班级自然归档，不需要单独查询。"))
            classHint = Ui.helper(this, "尚未同步班级数据")
            classes.addView(classHint)
            classHost = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
            classes.addView(classHost)
            column.addView(classes)

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
            refreshClassRecords()
            refreshHome()
        }

        // 「课表」= ClassBoard 样式网格 + 单一「课表设置」入口。
        // 所有课表相关操作（作息、双列、导入导出、教师机同步）都收进设置里。
        pages[2] = scrollPage { column ->
            val card = Ui.card(this)
            card.addView(Ui.section(this, "本周课表"))
            scheduleHint = Ui.body(this)
            card.addView(scheduleHint)
            val setup = Ui.button(this, "课表设置", primary = true)
            setup.setOnClickListener { showTimetableSettings() }
            card.addView(setup)
            card.addView(Ui.helper(this, "点课格编辑课程；今天列与当前节高亮；大课间 / 午休横跨整行。"))

            // Frozen time axis + horizontally scrollable day grid, both inside a
            // common vertical scroll so rows stay aligned.
            val table = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
            timetableAxis = TimetableAxisView(this)
            timetableGrid = TimetableCourseGridView(this)
            timetableGrid.onCellClick = { day, row -> editCourse(day, row) }
            val horizontal = HorizontalScrollView(this).apply {
                isFillViewport = false
                layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
            }
            horizontal.addView(timetableGrid)
            table.addView(timetableAxis, LinearLayout.LayoutParams(Ui.dp(this, 112), ViewGroup.LayoutParams.WRAP_CONTENT))
            table.addView(horizontal)
            card.addView(table)
            column.addView(card)
            refreshSchedule()
        }

        // 「配置」= 连接参数（课表设置已移出到课表页）
        pages[3] = scrollPage { column ->
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

            column.addView(Ui.helper(this, "课表的节数、大课间、午休、双列作息与导入导出，统一在「课表 → 课表设置」里。"))
        }

        // 「更多」= 少量设置入口 + 关于（课堂记录已移入「班级」页自然展示）
        pages[4] = scrollPage { column ->
            val settings = Ui.card(this)
            settings.addView(Ui.section(this, "设置"))
            settings.addView(Ui.body(this, "导入导出、同步与工具集中在这里，不再铺满整页。"))
            val tools = Ui.button(this, "导入导出与工具", primary = true)
            tools.setOnClickListener { showMoreTools() }
            settings.addView(tools)
            val syncTimetable = Ui.button(this, "从教师机同步课表")
            syncTimetable.setOnClickListener { pullTimetableFromTeacher() }
            settings.addView(syncTimetable)
            column.addView(settings)

            val about = Ui.card(this)
            about.addView(Ui.section(this, "关于"))
            about.addView(Ui.body(this, "点可课堂 · 教师机移动伴侣\n基于 Veyon 4.x 的教学管理定制分支\n许可：GPL-2.0-or-later"))
            about.addView(Ui.helper(this, "本软件采集学生行为数据，部署前请阅读 IKEBACK-PRIVACY.md"))
            column.addView(about)
        }
    }

    private fun brandHeader(): View {
        val header = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            background = Ui.shape(this@MainActivity, getColor(R.color.ikeback_primary), 20)
            val p = Ui.dp(this@MainActivity, 18)
            setPadding(p, p, p, p)
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
            ).apply { bottomMargin = Ui.dp(this@MainActivity, 14) }
            elevation = 0f
        }
        // Real ikeback launcher icon instead of the "CK" text placeholder.
        val mark = ImageView(this).apply {
            setImageResource(R.mipmap.ic_launcher_round)
            scaleType = ImageView.ScaleType.FIT_CENTER
            background = Ui.shape(this@MainActivity, 0x33FFFFFF, 12)
            val p = Ui.dp(this@MainActivity, 6)
            setPadding(p, p, p, p)
        }
        header.addView(mark, LinearLayout.LayoutParams(Ui.dp(this, 52), Ui.dp(this, 52)))
        val text = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val name = TextView(this).apply {
            this.text = getString(R.string.app_name)
            textSize = 22f
            setTypeface(typeface, Typeface.BOLD)
            setTextColor(getColor(R.color.white))
        }
        val sub = TextView(this).apply {
            this.text = "课堂看得见 · 成长记得下"
            textSize = 11f
            setTextColor(getColor(R.color.ikeback_brand_subtext))
        }
        text.addView(name)
        text.addView(sub)
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
        if (index == 0) refreshConnectionHint()
        if (index == 1) refreshClassRecords()
        if (index == 2) refreshSchedule()
    }

    /** Unconfigured: shrink the status card to a hint, leave the page to control + roster. */
    private fun refreshConnectionHint() {
        if (::setupHint.isInitialized.not()) return
        val configured = ::ipField.isInitialized && currentIp().isNotEmpty() && currentKey().isNotEmpty()
        setupHint.text = if (configured) ""
        else "尚未配置：请到「配置」页填写教师机 IP 与本堂课密钥，才能下发指令和读取名单。"
        setupHint.visibility = if (configured) View.GONE else View.VISIBLE
        goConfigButton.visibility = if (configured) View.GONE else View.VISIBLE
        statusActions.visibility = if (configured) View.VISIBLE else View.GONE
        revisionView.visibility = if (configured) View.VISIBLE else View.GONE
        messageView.visibility = if (configured) View.VISIBLE else View.GONE
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
        statusBadge.setTextColor(getColor(if (ok) R.color.ikeback_badge_ok_fg else R.color.ikeback_badge_off_fg))
        statusBadge.background = Ui.shape(
            this,
            getColor(if (ok) R.color.ikeback_badge_ok_bg else R.color.ikeback_badge_off_bg),
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
        if (host.isEmpty()) {
            message("请先填写教师机 IP")
            toast("请先填写教师机 IP")
            showTab(3)
            return
        }
        if (key.isEmpty()) {
            message("请先输入或导入本堂课密钥")
            toast("请先输入或导入本堂课密钥")
            showTab(3)
            return
        }
        if (busy) {
            toast("正在同步，请稍候…")
            return
        }
        bridge.port = currentPort()
        applyKey(key)
        busy = true
        message("正在与 $host:${bridge.port} 同步…")
        toast("正在与教师机同步…")
        background {
            try {
                val state = bridge.connect(host)
                if (!state.keyConfigured) {
                    ui {
                        message("教师机尚未生成本堂课密钥，请先在教师端「开始新课堂」")
                        toast("教师机尚未生成本堂课密钥")
                    }
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
                    classNames = state.classNames
                    refreshHome()
                    refreshSeatCards()
                    refreshClassRecords()
                    refreshConnectionHint()
                    message("同步完成 · 版本 ${state.revision} · ${cache.className}")
                    toast("同步完成 · ${cache.className}")
                }
            } catch (e: UnauthorizedException) {
                ui {
                    setStatus(false)
                    message("密钥被拒绝，换新课后请更新密钥")
                    toast("密钥被拒绝，请更新本堂课密钥")
                }
            } catch (e: Exception) {
                val reason = e.message ?: ""
                ui {
                    setStatus(false)
                    message("未找到教师机，保留本地数据：$reason")
                    toast("未找到教师机，请检查 IP、端口与同一网络")
                }
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
            toast("请先在「配置」页填写 IP 与密钥")
            showTab(3)
            return
        }
        bridge.port = currentPort()
        applyKey(currentKey())
        toast("正在读取学生名单…")
        background {
            try {
                val loaded = bridge.seats(host)
                ui {
                    seats = loaded
                    seatHint.text = "共 ${loaded.size} 名学生 · 顺序与教师端座位网格一致"
                    refreshSeatCards()
                    toast(if (loaded.isEmpty()) "教师端还没有学生名单" else "已读取 ${loaded.size} 名学生")
                }
            } catch (e: UnauthorizedException) {
                ui {
                    seatHint.text = "密钥被拒绝，请更新本堂课密钥"
                    toast("密钥被拒绝，请更新本堂课密钥")
                }
            } catch (e: Exception) {
                val reason = e.message ?: ""
                ui {
                    seatHint.text = "读取失败，显示本地缓存：$reason"
                    toast("读取学生名单失败")
                }
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
                background = Ui.shape(this@MainActivity, getColor(R.color.ikeback_card), 14, getColor(R.color.ikeback_border))
                val p = Ui.dp(this@MainActivity, 14)
                setPadding(p, p, p, p)
                isClickable = true
                setOnClickListener { chooseScore(student) }
            }
            val seat = TextView(this).apply {
                text = "${student.seat} 号"
                textSize = 12f
                setTextColor(getColor(R.color.ikeback_secondary))
            }
            val name = TextView(this).apply {
                text = student.student.ifEmpty { "—" }
                textSize = 15f
                setTypeface(typeface, Typeface.BOLD)
                setTextColor(getColor(R.color.ikeback_title))
            }
            val score = TextView(this).apply {
                text = "${student.score} 分"
                textSize = 18f
                setTypeface(typeface, Typeface.BOLD)
                setTextColor(getColor(R.color.ikeback_primary))
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
        val key = currentKey()
        if (host.isEmpty() || key.isEmpty()) {
            // A silent no-op is what made the buttons feel broken: always say
            // why nothing happened, both in the status card and as a toast.
            message("请先填写教师机 IP 与本堂课密钥")
            toast("请先在「配置」页填写教师机 IP 与密钥")
            showTab(3)
            return
        }
        bridge.port = currentPort()
        applyKey(key)
        // Immediate feedback on tap, before the network round-trip finishes.
        toast("正在下发「${cmd.label}」…")
        message("正在下发「${cmd.label}」…")
        background {
            try {
                val ok = bridge.command(host, cmd.name, cmd.enabled, "", host)
                val text = if (ok) "指令「${cmd.label}」已下发" else "指令「${cmd.label}」被拒绝"
                ui { message(text); toast(text) }
            } catch (e: UnauthorizedException) {
                ui {
                    message("密钥被拒绝：请在「配置」页更新本堂课密钥")
                    toast("密钥被拒绝，请更新本堂课密钥")
                }
            } catch (e: Exception) {
                val reason = e.message ?: ""
                ui {
                    message("指令失败：无法连接教师机（$reason）")
                    toast("指令失败：无法连接教师机")
                }
            }
        }
    }

    private fun toast(text: String) {
        Toast.makeText(this, text, Toast.LENGTH_SHORT).show()
    }

    // -------------------------------------------------------------- schedule

    private fun refreshSchedule() {
        if (::timetableGrid.isInitialized.not()) return
        val timetable = timetableStore.load()
        val now = currentPeriod(timetable)
        val next = nextPeriod(timetable)
        timetableAxis.timetable = timetable
        timetableAxis.nowRow = now
        timetableGrid.timetable = timetable
        timetableGrid.nowRow = now
        // Re-measure the frozen axis when the number of time columns changes.
        val axisWidth = Ui.dp(this, if (timetable.timeLabels.size > 1) 112 else 66)
        (timetableAxis.layoutParams as? LinearLayout.LayoutParams)?.width = axisWidth
        timetableAxis.requestLayout()
        val nextText = next?.let { timetable.periods.getOrNull(it)?.label ?: "" } ?: "今天没有后续课程"
        val mode = if (timetable.timeLabels.size > 1)
            "双作息 ${timetable.timeLabels.joinToString(" / ")}" else "单作息"
        scheduleHint.text = "今天 ${WEEKDAYS[todayDay() - 1]} · $mode · " +
            (now?.let { "正在上「${timetable.periods[it].label}」" } ?: "当前无课") +
            " · 下一节：$nextText"
    }

    /** Click a cell -> edit the course (subject, class, room, span, colour). */
    private fun editCourse(day: Int, row: Int) {
        val timetable = timetableStore.load()
        val existing = courseAt(timetable, day, row)
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            val p = Ui.dp(this@MainActivity, 12)
            setPadding(p, p, p, p)
        }

        fun field(label: String, value: String): EditText {
            box.addView(Ui.body(this@MainActivity, label, 12f))
            val edit = EditText(this@MainActivity).apply {
                setText(value)
                textSize = 14f
                val pad = Ui.dp(this@MainActivity, 10)
                setPadding(pad, pad, pad, pad)
                background = Ui.shape(this@MainActivity, getColor(R.color.ikeback_input_bg), 12)
            }
            box.addView(edit, LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
            return edit
        }

        val name = field("科目 / 内容", existing?.name ?: "")
        val className = field("班级（用于班级页归档）", existing?.className ?: "")
        val room = field("地点 / 备注", existing?.room ?: "")
        val span = field("连堂节数（1 = 单节）",
            ((existing?.endSection ?: row) - (existing?.startSection ?: row) + 1).toString())
        val color = android.widget.CheckBox(this).apply {
            text = "换一个颜色"
            setTextColor(getColor(R.color.ikeback_title))
        }
        box.addView(color)

        AlertDialog.Builder(this)
            .setTitle("${WEEKDAYS.getOrElse(day - 1) { "" }} · ${timetable.periods.getOrNull(row)?.label ?: ""}")
            .setView(ScrollView(this).apply { addView(box) })
            .setPositiveButton("保存") { _, _ ->
                val subject = name.text.toString().trim()
                if (subject.isEmpty()) {
                    timetableStore.removeCourse(day, row)
                } else {
                    val length = span.text.toString().trim().toIntOrNull()?.coerceIn(1, 6) ?: 1
                    val lastRow = (row + length - 1).coerceAtMost(timetable.periods.size - 1)
                    timetableStore.setCourse(
                        TimetableCourse(
                            name = subject,
                            dayOfWeek = day,
                            startSection = row,
                            endSection = lastRow,
                            colorIndex = if (color.isChecked)
                                ((existing?.colorIndex ?: 0) + 1) % COURSE_COLORS.size
                            else existing?.colorIndex ?: 0,
                            room = room.text.toString().trim(),
                            className = className.text.toString().trim(),
                        )
                    )
                }
                refreshSchedule()
                pushTimetableToTeacher()
            }
            .setNeutralButton("常用科目") { _, _ ->
                AlertDialog.Builder(this)
                    .setItems(SUBJECT_PRESETS.toTypedArray()) { _, which ->
                        timetableStore.setCourse(
                            TimetableCourse(
                                name = SUBJECT_PRESETS[which],
                                dayOfWeek = day,
                                startSection = row,
                                endSection = row,
                                className = existing?.className ?: "",
                            )
                        )
                        refreshSchedule()
                        pushTimetableToTeacher()
                    }.show()
            }
            .setNegativeButton("删除") { _, _ ->
                timetableStore.removeCourse(day, row)
                refreshSchedule()
                pushTimetableToTeacher()
            }
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
                background = Ui.shape(this@MainActivity, this@MainActivity.getColor(R.color.ikeback_input_bg), 12)
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
            text = "双列作息（两套并行时间，如 小学 / 初高中）"
            isChecked = config.dualTime
            setTextColor(this@MainActivity.getColor(R.color.ikeback_title))
        }
        box.addView(dual)
        val label1 = addField("第一栏标题", config.timeLabel1, numeric = false)
        val label2 = addField("第二栏标题", config.timeLabel2, numeric = false)
        val shift = addField("第二套作息相对偏移（分钟，可为负）", config.secondShiftMinutes.toString())
        val name = addField("课表名称", timetableStore.load().name, numeric = false)

        // 課表相关操作全部归入这一个设置面板，不再散落在配置页 / 更多页。
        val actionLabels = arrayOf(
            "导入作息 JSON（兼容 ClassBoard 全校作息）",
            "导出课表 JSON",
            "从教师机读取课表",
            "推送课表到教师机",
            "恢复默认作息",
        )
        box.addView(Ui.body(this, "导入导出与同步", 13f))
        val scroll = ScrollView(this).apply { addView(box) }
        AlertDialog.Builder(this)
            .setTitle("课表设置")
            .setView(scroll)
            .setItems(actionLabels) { _, which ->
                when (which) {
                    0 -> timetableLauncher.launch(arrayOf("application/json", "text/plain", "*/*"))
                    1 -> exportTimetable()
                    2 -> pullTimetableFromTeacher()
                    3 -> pushTimetableToTeacher(showResult = true)
                    4 -> {
                        timetableStore.save(Timetable())
                        refreshSchedule()
                        message("已恢复默认作息")
                    }
                }
            }
            .setPositiveButton("保存作息") { _, _ ->
                val next = TimetableConfig(
                    lessonsPerDay = lessons.text.toString().toIntOrNull() ?: config.lessonsPerDay,
                    lessonMinutes = lessonMinutes.text.toString().toIntOrNull() ?: config.lessonMinutes,
                    shortBreakMinutes = shortBreak.text.toString().toIntOrNull() ?: config.shortBreakMinutes,
                    dayStart = dayStart.text.toString().trim().ifEmpty { config.dayStart },
                    bigBreakAfter = bigAfter.text.toString().toIntOrNull() ?: config.bigBreakAfter,
                    bigBreakMinutes = bigMinutes.text.toString().toIntOrNull() ?: config.bigBreakMinutes,
                    noonAfter = noonAfter.text.toString().toIntOrNull() ?: config.noonAfter,
                    noonMinutes = noonMinutes.text.toString().toIntOrNull() ?: config.noonMinutes,
                    dualTime = dual.isChecked,
                    timeLabel1 = label1.text.toString().trim().ifEmpty { config.timeLabel1 },
                    timeLabel2 = label2.text.toString().trim().ifEmpty { config.timeLabel2 },
                    secondShiftMinutes = shift.text.toString().trim().toIntOrNull()
                        ?: config.secondShiftMinutes,
                )
                timetableStore.updateConfig(next, name.text.toString().trim().ifEmpty { null })
                refreshSchedule()
                pushTimetableToTeacher()
                message("作息已更新：${next.lessonsPerDay} 节课")
            }
            .setNegativeButton("关闭", null)
            .show()
    }

    /** Local copy is authoritative; also mirror it onto the teacher machine. */
    private fun pushTimetableToTeacher(showResult: Boolean = false) {
        val host = currentIp()
        if (host.isEmpty() || currentKey().isEmpty()) {
            if (showResult) message("请先在「配置」页填写 IP 与密钥")
            return
        }
        val json = TimetableStore.toJson(timetableStore.load())
        bridge.port = currentPort()
        applyKey(currentKey())
        background {
            val ok = try { bridge.pushTimetable(host, json) } catch (_: Exception) { false }
            ui { message(if (ok) "课表已同步到教师机" else "课表已存本地，教师机同步失败（稍后重试）") }
        }
    }

    private fun pullTimetableFromTeacher() {
        val host = currentIp()
        if (host.isEmpty() || currentKey().isEmpty()) {
            message("请先在「配置」页填写 IP 与密钥")
            return
        }
        bridge.port = currentPort()
        applyKey(currentKey())
        background {
            try {
                val doc = bridge.timetable(host)
                if (doc.length() == 0) {
                    ui { message("教师机上还没有课表，可先「推送课表到教师机」") }
                    return@background
                }
                val t = TimetableStore.fromJson(doc.toString())
                ui {
                    timetableStore.save(t)
                    refreshSchedule()
                    message("已从教师机读取课表：${t.name}")
                }
            } catch (e: Exception) {
                ui { message("读取教师机课表失败：${e.message ?: ""}") }
            }
        }
    }

    private fun exportTimetable() {
        try {
            val dir = getExternalFilesDir(android.os.Environment.DIRECTORY_DOCUMENTS) ?: filesDir
            val file = File(dir, "ikeback-课表.json")
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
            val file = File(dir, "ikeback-本地数据.json")
            file.writeText(obj.toString(2), Charsets.UTF_8)
            message("本地数据已导出：${file.absolutePath}")
        } catch (e: Exception) {
            message("导出失败：${e.message ?: ""}")
        }
    }

    // ------------------------------------------------------------------ classes

    /** Pull class rosters so the 班级 page can group records by class. */
    private fun loadClasses() {
        val host = currentIp()
        if (host.isEmpty() || currentKey().isEmpty()) {
            refreshClassRecords()
            return
        }
        bridge.port = currentPort()
        applyKey(currentKey())
        background {
            try {
                val groups = parseClasses(bridge.classes(host))
                ui {
                    classNames = groups.map { it.name }.filter { it.isNotBlank() }
                    refreshClassRecords()
                }
            } catch (_: Exception) {
                ui { refreshClassRecords() }
            }
        }
    }

    /**
     * Natural per-class lesson records: what was taught, what happened and how
     * the students did. This replaces the old one-off 「查询这堂课」 dialog.
     */
    private fun refreshClassRecords() {
        if (::classHost.isInitialized.not()) return
        classHost.removeAllViews()
        val scores = try { parseScoreSnapshot(cache.scoresRaw) } catch (_: Exception) { ScoreSnapshot() }
        val classes = (classNames + listOf(cache.className) +
            timetableStore.load().courses.map { it.className })
            .filter { it.isNotBlank() }.distinct()
        if (classes.isEmpty()) {
            classHint.text = "尚未同步班级数据，同步后这里会按班级展示课堂记录。"
            classHost.addView(Ui.body(this, "暂无班级记录。连接教师机并同步后，每堂课的内容、活动与表现会自动归入对应班级。"))
            return
        }
        classHint.text = "共 ${classes.size} 个班级 · ${scores.lessons.size} 堂课"
        classes.forEach { className ->
            val card = Ui.card(this)
            card.addView(Ui.section(this, className))

            val lessons = scores.lessons.takeLast(5).reversed()
            if (lessons.isEmpty()) {
                card.addView(Ui.body(this, "暂无课堂记录"))
            } else {
                lessons.forEach { lesson ->
                    val date = lesson["date"].orEmpty()
                    val topic = lesson["topic"].orEmpty().ifEmpty { "未填写主题" }
                    val id = lesson["id"].orEmpty()
                    val rows = scores.entries.filter { it["lessonId"]?.toString().orEmpty() == id }
                    val deltas = rows.sumOf { (it["delta"] as? Number)?.toInt() ?: 0 }
                    val happened = cache.events
                        .filter { date.isEmpty() || it.timestamp.startsWith(date) }
                        .takeLast(3).reversed()
                        .joinToString("；") { "${it.time()} ${it.detail}" }
                        .ifEmpty { "无活动记录" }
                    val item = Ui.body(
                        this,
                        "$date · 讲了什么：$topic\n" +
                            "干了什么：$happened\n" +
                            "同学表现：${rows.size} 条加减分，合计 ${if (deltas > 0) "+" else ""}$deltas 分",
                        12f,
                    )
                    card.addView(item)
                }
            }
            classHost.addView(card)
        }
    }

    /** Import/export & tools collapsed into one settings dialog. */
    private fun showMoreTools() {
        val items: List<Pair<String, () -> Unit>> = listOf(
            "导入本堂课密钥 (.key)" to {
                keyLauncher.launch(arrayOf("text/plain", "application/json", "application/octet-stream", "*/*"))
            },
            "导入作息 JSON（ClassBoard 格式）" to {
                timetableLauncher.launch(arrayOf("application/json", "text/plain", "*/*"))
            },
            "导出课表 JSON" to { exportTimetable() },
            "导出本地缓存数据" to { exportLocalData() },
            "导出期末成绩表" to { exportReport() },
            "查看班级词云" to { showWordCloud() },
            "刷新班级记录" to { loadClasses() },
        )
        AlertDialog.Builder(this)
            .setTitle("导入导出与工具")
            .setItems(items.map { it.first }.toTypedArray()) { _, which -> items[which].second() }
            .setNegativeButton("关闭", null)
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
                val file = File(dir, "ikeback-期末成绩表.xls")
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
