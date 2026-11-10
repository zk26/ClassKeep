package top.ikee002.classkeep

import android.content.res.ColorStateList
import android.os.Bundle
import android.os.Environment
import android.view.View
import android.webkit.WebView
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import org.json.JSONObject
import java.io.File
import java.text.DateFormat
import java.util.Date
import java.util.concurrent.Executors

class MainActivity : AppCompatActivity() {

    private data class Cmd(val label: Int, val name: String, val enabled: Boolean)

    private lateinit var badge: TextView
    private lateinit var message: TextView
    private lateinit var progress: ProgressBar
    private lateinit var ipInput: EditText
    private lateinit var keyInput: EditText
    private lateinit var portInput: EditText
    private lateinit var revisionView: TextView
    private lateinit var snapshot: TextView
    private lateinit var activityView: TextView

    private val bridge = BridgeClient()
    private val executor = Executors.newSingleThreadExecutor()
    private lateinit var cacheStore: CacheStore
    private var discovery: Discovery? = null
    private var cache = Cache()
    private var lastAutoSync = 0L
    private var firstSeat = ""

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        badge = findViewById(R.id.badge)
        message = findViewById(R.id.message)
        progress = findViewById(R.id.progress)
        ipInput = findViewById(R.id.ipInput)
        keyInput = findViewById(R.id.keyInput)
        portInput = findViewById(R.id.portInput)
        revisionView = findViewById(R.id.revision)
        snapshot = findViewById(R.id.snapshot)
        activityView = findViewById(R.id.activity)

        cacheStore = CacheStore(this)
        cache = cacheStore.load()
        ipInput.setText(cache.teacherIp)
        keyInput.setText(cache.sessionKey)
        portInput.setText(cache.port.toString())
        render()

        addGroup(
            findViewById(R.id.groupAudio),
            listOf(
                Cmd(R.string.cmd_mute, "mute", true),
                Cmd(R.string.cmd_unmute, "mute", false),
                Cmd(R.string.cmd_block_internet, "internet", true),
                Cmd(R.string.cmd_allow_internet, "internet", false),
            ),
        )
        addGroup(
            findViewById(R.id.groupScreen),
            listOf(
                Cmd(R.string.cmd_silence, "silence", true),
                Cmd(R.string.cmd_hide_silence, "silence", false),
                Cmd(R.string.cmd_usb_block, "usb", true),
                Cmd(R.string.cmd_usb_restore, "usb", false),
                Cmd(R.string.cmd_remote, "remote", true),
                Cmd(R.string.cmd_demo, "demo", true),
            ),
        )
        addGroup(
            findViewById(R.id.groupUwf),
            listOf(
                Cmd(R.string.cmd_uwf_query, "uwfQuery", true),
                Cmd(R.string.cmd_uwf_disable, "uwfDisable", false),
                Cmd(R.string.cmd_uwf_enable, "uwfEnable", true),
                Cmd(R.string.cmd_report, "report", true),
                Cmd(R.string.cmd_wordcloud, "wordcloud", true),
            ),
        )

        findViewById<Button>(R.id.syncButton).setOnClickListener {
            sync(ipInput.text.toString().trim(), null)
        }
        findViewById<Button>(R.id.offlineButton).setOnClickListener {
            setConnected(false)
            setMessage(getString(R.string.msg_offline))
        }

        discovery = Discovery(this) found@{ ip, announced ->
            // The announced port wins over the cached/default one.
            ipInput.setText(ip)
            portInput.setText(announced.port.toString())
            val now = System.currentTimeMillis()
            if (now - lastAutoSync < 30_000) return@found
            lastAutoSync = now
            sync(ip, announced)
        }
        discovery?.start(currentPort())
    }

    override fun onDestroy() {
        discovery?.stop()
        executor.shutdownNow()
        super.onDestroy()
    }

    private fun currentPort(): Int =
        portInput.text.toString().toIntOrNull() ?: Announcement.DEFAULT_PORT

    private fun prepare(host: String): String? {
        if (host.isEmpty()) {
            setMessage(getString(R.string.msg_no_ip))
            return null
        }
        val key = keyInput.text.toString().trim()
        if (key.isEmpty()) {
            setMessage(getString(R.string.msg_no_key))
            return null
        }
        bridge.lessonKey = key
        bridge.port = currentPort()
        return key
    }

    private fun sync(host: String, announced: TeacherState?) {
        val port = announced?.port ?: currentPort()
        bridge.port = port
        val key = keyInput.text.toString().trim()
        if (key.isEmpty()) {
            setConnected(false)
            setMessage(getString(R.string.msg_found_need_key))
            return
        }
        bridge.lessonKey = key
        setBusy(true)
        setMessage(getString(R.string.msg_syncing, host, port))

        background {
            val state = bridge.connect(host)
            if (!state.keyConfigured) {
                ui {
                    setConnected(false)
                    setMessage(getString(R.string.msg_no_key_server))
                }
            } else if (!state.syncEnabled && announced == null) {
                ui {
                    setConnected(false)
                    setMessage(getString(R.string.msg_sync_disabled))
                }
            } else {
                val scores = bridge.scores(host)
                val events = bridge.events(host)
                firstSeat = try {
                    val roster = JSONObject(bridge.roster(host))
                    val classes = roster.optJSONArray("classes")
                    val students = classes?.optJSONObject(0)?.optJSONArray("students")
                    students?.optJSONObject(0)?.optString("seat") ?: ""
                } catch (_: Exception) {
                    ""
                }
                val students = scores.entries.mapNotNull { it["seat"] as? String }.distinct().size
                cache = cache.copy(
                    teacherIp = host,
                    sessionKey = key,
                    port = port,
                    revision = state.revision,
                    className = state.className.ifEmpty { scores.className },
                    baseScore = scores.rules.baseScore,
                    rulesPublished = scores.rules.published,
                    lessons = scores.lessons.size,
                    entries = scores.entries.size,
                    students = students,
                    scoresRaw = scores.raw,
                    events = events,
                    updatedAt = System.currentTimeMillis(),
                )
                cacheStore.save(cache)
                ui {
                    render()
                    setConnected(true)
                    setMessage(getString(R.string.msg_synced, state.revision, cache.className))
                }
            }
        }
    }

    private fun sendCommand(name: String, enabled: Boolean) {
        val host = ipInput.text.toString().trim()
        if (prepare(host) == null) return
        if (name == "report") {
            exportReport(host)
            return
        }
        if (name == "wordcloud") {
            showWordCloud(host)
            return
        }
        background {
            val ok = bridge.command(host, name, enabled, "", host)
            ui {
                setMessage(
                    getString(
                        if (ok) R.string.msg_command_ok else R.string.msg_command_rejected,
                        name,
                    ),
                )
            }
        }
    }

    private fun exportReport(host: String) {
        background {
            bridge.connect(host)
            val xml = bridge.report(host)
            val directory = getExternalFilesDir(Environment.DIRECTORY_DOCUMENTS) ?: cacheDir
            val file = File(directory, "课守-期末表.xls")
            file.writeText(xml, Charsets.UTF_8)
            ui { setMessage(getString(R.string.msg_report_saved, file.absolutePath)) }
        }
    }

    private fun showWordCloud(host: String) {
        val input = EditText(this).apply {
            hint = "座位号"
            setText(firstSeat)
        }
        AlertDialog.Builder(this)
            .setTitle(R.string.cmd_wordcloud)
            .setView(input)
            .setNegativeButton(android.R.string.cancel, null)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                background {
                    bridge.connect(host)
                    val svg = bridge.wordCloud(host, input.text.toString().trim())
                    ui {
                        val web = WebView(this@MainActivity).apply {
                            loadDataWithBaseURL(null, svg, "image/svg+xml", "UTF-8", null)
                        }
                        AlertDialog.Builder(this@MainActivity)
                            .setTitle(R.string.cmd_wordcloud)
                            .setView(web)
                            .setPositiveButton(android.R.string.ok, null)
                            .show()
                    }
                }
            }
            .show()
    }

    private fun addGroup(container: LinearLayout, commands: List<Cmd>) {
        val gap = (8 * resources.displayMetrics.density).toInt()
        commands.chunked(2).forEach { pair ->
            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                )
            }
            pair.forEach { cmd ->
                val button = Button(this).apply {
                    text = getString(cmd.label)
                    textSize = 12f
                    backgroundTintList = ColorStateList.valueOf(
                        ContextCompat.getColor(this@MainActivity, R.color.ck_chip_bg),
                    )
                    setTextColor(ContextCompat.getColor(this@MainActivity, R.color.ck_primary))
                    layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
                        .apply { marginEnd = gap }
                    setOnClickListener { sendCommand(cmd.name, cmd.enabled) }
                }
                row.addView(button)
            }
            container.addView(row)
        }
    }

    private fun render() {
        snapshot.text = listOf(
            getString(R.string.label_class, cache.className.ifEmpty { getString(R.string.dash) }),
            getString(R.string.label_base_score, cache.baseScore.toString()),
            getString(
                R.string.label_rules,
                getString(if (cache.rulesPublished) R.string.yes else R.string.no),
            ),
            getString(R.string.label_stats, cache.lessons, cache.entries, cache.students),
        ).joinToString("\n")

        activityView.text = if (cache.events.isEmpty()) {
            getString(R.string.msg_no_activity)
        } else {
            cache.events.takeLast(40).asReversed().joinToString("\n") { event ->
                "${event.time()}  ${event.label()} · ${event.detail}"
            }
        }

        val version = if (cache.revision > 0) cache.revision.toString() else getString(R.string.dash)
        val updated = if (cache.updatedAt > 0) {
            DateFormat.getDateTimeInstance().format(Date(cache.updatedAt))
        } else {
            getString(R.string.dash)
        }
        revisionView.text = getString(R.string.label_revision, version, updated)
    }

    private fun setConnected(connected: Boolean) {
        badge.text = getString(if (connected) R.string.status_connected else R.string.status_offline)
        badge.setTextColor(
            ContextCompat.getColor(this, if (connected) R.color.ck_badge_ok_fg else R.color.ck_badge_off_fg),
        )
        badge.setBackgroundColor(
            ContextCompat.getColor(this, if (connected) R.color.ck_badge_ok_bg else R.color.ck_badge_off_bg),
        )
    }

    private fun setMessage(text: String) {
        message.text = text
    }

    private fun setBusy(busy: Boolean) {
        progress.visibility = if (busy) View.VISIBLE else View.GONE
    }

    private fun ui(block: () -> Unit) {
        runOnUiThread {
            if (!isFinishing && !isDestroyed) block()
        }
    }

    private fun background(block: () -> Unit) {
        executor.execute {
            try {
                block()
            } catch (_: UnauthorizedException) {
                ui {
                    setConnected(false)
                    setMessage(getString(R.string.msg_denied))
                }
            } catch (error: Exception) {
                ui {
                    setConnected(false)
                    setMessage("${getString(R.string.msg_not_found)} ${error.message ?: ""}")
                }
            } finally {
                ui { setBusy(false) }
            }
        }
    }
}
