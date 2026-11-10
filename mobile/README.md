# 课守 ClassKeep · Android 教室端

教师机侧的移动伴侣 App（**不是学生端**）。在局域网内发现教师机后同步班级/积分/日志数据，
并把控制指令交给教师机执行。Kotlin + 传统 XML View + Material3，**零第三方网络/JSON/加密库**。

完整说明见仓库根目录 `docs/`：

- [`CLASSKEEP.md`](../docs/CLASSKEEP.md) — 功能总览与架构
- [`CLASSKEEP-PROTOCOL.md`](../docs/CLASSKEEP-PROTOCOL.md) — HTTP/UDP 协议、认证算法与数据格式
- [`CLASSKEEP-DEPLOY.md`](../docs/CLASSKEEP-DEPLOY.md) — 部署与每堂课操作流程
- [`CLASSKEEP-LICENSE.md`](../docs/CLASSKEEP-LICENSE.md) — GPL-2.0-or-later 许可证声明

## 构建

```powershell
.\gradlew.bat :app:assembleDebug
.\gradlew.bat :app:testDebugUnitTest
```

- AGP 9.1.0 / Gradle 9.3.1 / JDK 21，`minSdk 30`、`targetSdk 36`
- 首次打开由 Android Studio 生成 `local.properties`（`sdk.dir`），**不要提交**
- 单元测试验证前后端共用的 SHA-256 基准向量与 UDP 广播解析，不依赖 Android 运行时

## 源码

```
app/src/main/java/top/ikee002/classkeep/
  MainActivity.kt        UI、自动发现节流、同步、控制指令、报表/词云
  BridgeClient.kt        HttpURLConnection 全部路由 + 认证头
  Discovery.kt           DatagramSocket 主动探测 + 周期广播 + MulticastLock
  ClassKeepCrypto.kt     SHA-256 挑战应答（与教师机逐字节一致）
  ClassKeepModels.kt     数据类与 org.json 解析
  Announcement.kt        UDP 广播报文常量与解析
  CacheStore.kt          SharedPreferences 离线缓存
```

## 权限与网络

| 权限 | 用途 |
|---|---|
| `INTERNET` / `ACCESS_NETWORK_STATE` | HTTP 访问教师机 Bridge |
| `ACCESS_WIFI_STATE` / `CHANGE_WIFI_MULTICAST_STATE` | 接收 UDP 广播（MulticastLock） |

教师机 Bridge 是局域网明文 HTTP，Manifest 已设 `android:usesCleartextTraffic="true"`。
**不要把 11880 端口映射到公网**：虽然认证已启用，仍应在受控教学网段使用。

## 使用

1. 教师机「系统与设备」页勾选 Start mobile bridge（默认 11880）。
2. App 自动发现教师机；**找到才同步，找不到不操作**，并保留本地缓存。
3. 首次输入**本堂课密钥**（与学生机悬浮条输入的是同一个），以及教师机 IP/端口。
4. 之后自动同步，也可点「立即同步」；换课后密钥变化会提示重新输入。
5. 可用：班级概况、13 项控制指令、最近活动、导出期末表、查看每生词云。

密钥只参与本地 SHA-256 摘要，**不会出现在 HTTP 头或 UDP 广播里**。
