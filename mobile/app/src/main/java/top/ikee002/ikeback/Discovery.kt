package top.ikee002.ikeback

import android.content.Context
import android.net.wifi.WifiManager
import android.os.Handler
import android.os.Looper
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.SocketTimeoutException

/**
 * Finds the teacher machine on the LAN. Sends one plain-text probe and also
 * listens for the periodic announcement. "Found -> sync, not found -> do nothing".
 */
class Discovery(
    context: Context,
    private val onFound: (String, TeacherState) -> Unit,
) {
    private val appContext = context.applicationContext
    private val mainHandler = Handler(Looper.getMainLooper())

    @Volatile
    private var running = false
    private var worker: Thread? = null

    fun start(port: Int, timeoutMs: Long = 8_000) {
        stop()
        running = true
        worker = Thread {
            var socket: DatagramSocket? = null
            var lock: WifiManager.MulticastLock? = null
            try {
                val wifi = appContext.getSystemService(Context.WIFI_SERVICE) as? WifiManager
                lock = wifi?.createMulticastLock("ikeback")?.also { it.acquire() }

                socket = DatagramSocket(InetSocketAddress(port)).apply {
                    reuseAddress = true
                    soTimeout = 500
                }
                val magic = Announcement.MAGIC.toByteArray(Charsets.UTF_8)
                val target = InetAddress.getByName("255.255.255.255")
                val probe = DatagramPacket(magic, magic.size, target, port)
                socket.send(probe)

                val buffer = ByteArray(1024)
                val deadline = System.currentTimeMillis() + timeoutMs
                while (running && System.currentTimeMillis() < deadline) {
                    try {
                        val packet = DatagramPacket(buffer, buffer.size)
                        socket.receive(packet)
                        val payload = String(packet.data, 0, packet.length, Charsets.UTF_8)
                        val state = Announcement.parse(payload) ?: continue
                        val ip = packet.address?.hostAddress ?: continue
                        if (ip.startsWith("0.") || ip == "255.255.255.255") continue
                        mainHandler.post { if (running) onFound(ip, state) }
                    } catch (_: SocketTimeoutException) {
                        // Re-probe so a teacher machine that came online late still answers.
                        if (running) socket.send(probe)
                    }
                }
            } catch (_: Exception) {
                // Discovery is best effort. The manual IP field always remains usable.
            } finally {
                try { socket?.close() } catch (_: Exception) { }
                try { if (lock?.isHeld == true) lock.release() } catch (_: Exception) { }
            }
        }.apply {
            isDaemon = true
            start()
        }
    }

    fun stop() {
        running = false
        worker?.interrupt()
        worker = null
    }
}
