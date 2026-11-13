# ikeback · Android 教室端（权威工程）

这是仓库根 `mobile/`，**Android 教室端 App 的唯一工程**（历史副本已清理）。
请用 Android Studio 打开本目录。

- 仓库：<https://github.com/zk26/ikeback>
- 许可：**GPL-2.0-or-later**（见 [`../LICENSE`](../LICENSE)、[`../NOTICE.md`](../NOTICE.md)）
- 协议：[`../veyon/docs/IKEBACK-PROTOCOL.md`](../veyon/docs/IKEBACK-PROTOCOL.md)
- 数据合规：[`../veyon/docs/IKEBACK-PRIVACY.md`](../veyon/docs/IKEBACK-PRIVACY.md)

## 构建

```powershell
$env:JAVA_HOME = 'E:\jdk-23.0.2'      # 必须指向 JDK 根目录，不要带 \bin
$env:ANDROID_HOME = 'G:\Andorid'
.\gradlew.bat :app:assembleDebug :app:testDebugUnitTest
```

要求 JDK 21+、Android SDK（`compileSdk 36.1`）、Gradle 9.3.1 / AGP 9.1.0。
`local.properties`（含本机 `sdk.dir`）**不提交**；CI 用 `ANDROID_HOME` 环境变量。

本机已实测：`:app:assembleDebug` 与 `:app:testDebugUnitTest` 全部 `BUILD SUCCESSFUL`，
产物 `app/build/outputs/apk/debug/app-debug.apk`。

## 功能

## 2.1.0 底部栏（5 个标签，图标 + 名称常显）

不再用单个汉字假装图标，也不再只给选中项显示文字。五个标签均为
`res/drawable/ikeback_tab_*.xml` 的**纯矢量图标 + 常驻名称**，选中时图标着色并回弹、
白色胶囊平滑跟随。图标全部自绘，未使用任何第三方图标库或会话凭据。

| 底部标签 | 内容 |
|---|---|
| 课堂 | 品牌头（真实 ikeback 图标）、紧凑连接提示；未配置时收缩成一行并提示前往配置页，主要区域留给**课堂控制**与**加分减分**名单 |
| 班级 | 按班级自然归档的**课堂记录**：讲了什么 / 干了什么 / 同学表现；班级概况与最近活动也归入此页 |
| 课表 | **ClassBoard 样式网格**：冻结的双作息时间轴（小学 / 初高中两套并行时间）、彩色跨节课程卡片、间歇行、今天列与当前节高亮；所有设置集中在单一「课表设置」入口 |
| 配置 | IP / 密钥 / 端口、显示或导入 `.key`、同步测试；课表相关设置已移出本页 |
| 更多 | 导入导出与工具**收进一个设置弹窗**（密钥、作息 JSON、课表 JSON、本地缓存、期末表、词云、刷新班级记录）、从教师机同步课表、关于 |

设计遵循[交互与动效规范](../veyon/docs/IKEBACK-UI.md)的要点：
卡片**不使用投影阴影**、只用一层细描边；外圆角 20dp / 内圆角 14dp 的同心圆角；
浮动标签常驻框上方、帮助文字常驻框下方，不依赖会消失的 placeholder；
底部胶囊指示随选中项平滑移动、图标回弹、**所有标签始终显示名称**；按钮全部显式尺寸，消除重叠；
列表末尾留出底部栏 + 手势安全区。

导出文件写入 App 私有 `Documents` 目录（`getExternalFilesDir`），
文件名分别为 `ikeback-课表.json` / `ikeback-本地数据.json` / `ikeback-期末成绩表.xls`。

### 课表数据保存位置：本地 + 教师机

课表以 ClassBoard 兼容的单文档 JSON 存于本机 `SharedPreferences`，并在
「课表设置」保存后、编辑课程后、导入作息后自动 `POST /api/classroom/timetable`
推送到教师机 `%APPDATA%/ikeback/timetable.json`；换手机或重装后可用
「从教师机读取课表」拉回。教师机不可达时仍以本地为准，并提示稍后重试。

### 双列作息（不是起止时间分列）

参考 `G:\AndWork\ClassBoard` 的「北附」课表：左轴显示两套并行作息
（`小学 8:20-9:00` / `初高中 8:00-8:40`），两栏标题、偏移分钟数、节数、
普通课间、大课间与午休位置均可在「课表设置」里调整；单列模式只显示一套。
课程卡片可跨节、按颜色区分，点击课格可填写科目 / 班级 / 地点 / 连堂节数。
节数、大课间、午休、双列、导入导出、恢复默认、教师机同步全部归入
「课表 → 课表设置」一个入口。

**平板适配**：`values-sw600dp` / `values-sw720dp` 提供更大的页面边距、课表格宽度
与学生卡片列数（手机 2 列 / 7 寸 3 列 / 10 寸 4 列），课表可横向滚动。

## 源码

```
app/src/main/java/top/ikee002/ikeback/
  MainActivity.kt       5 个标签页、班级记录、课表设置、导入导出、教师机同步
  UiKit.kt             视觉规范组件（无阴影卡片、常驻标签、无边框按钮）
  BottomBar.kt         自定义底部导航（矢量图标 + 常显名称、胶囊跟随 + 图标回弹）
  TimetableGridView.kt  ClassBoard 样式网格（冻结时间轴 + 彩色跨节课程卡片）
  TimetableStore.kt    零依赖课表数据层（双作息、跨节课程、班级归属，兼容 ClassBoard 全校作息 JSON）
  BridgeClient.kt      HttpURLConnection 全部路由 + 挑战应答认证
  Discovery.kt         DatagramSocket 主动探测 + 周期广播 + MulticastLock
  IkebackCrypto.kt   HMAC-SHA256 请求签名（与教师机逐字节一致）
  IkebackModels.kt   数据类与 org.json 解析
  Announcement.kt      UDP 广播报文常量与解析
  CacheStore.kt        SharedPreferences 离线缓存
```

**零第三方网络/JSON/加密库**：全部使用 Android / JDK 平台内置 API。
第三方构建依赖与许可见 [`../veyon/docs/IKEBACK-THIRD-PARTY.md`](../veyon/docs/IKEBACK-THIRD-PARTY.md)。

## 图标与界面

应用图标来自仓库根的 [`../ikeback_logo.png`](../ikeback_logo.png)（ikeback 品牌徽标）。
`tools/generate_icons.py` 从该图生成 `mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/` 下的
`ic_launcher.png`（圆角方形）与 `ic_launcher_round.png`（圆形），并清理 Android Studio 模板的
自适应图标 XML / webp。更换 Logo 后重跑：

```bash
python tools/generate_icons.py     # 需要 Pillow
```

界面采用与 Logo 一致的蓝→紫品牌渐变：头部展示 Logo 与产品名称，卡片/输入框/状态胶囊/控制按钮
均为圆角样式（`res/drawable/bg_*.xml`），配色集中在 `res/values/colors.xml`，主题在 `themes.xml`。
夜间主题在 `values-night/themes.xml`。

## 权限

`INTERNET`、`ACCESS_NETWORK_STATE`、`ACCESS_WIFI_STATE`、`CHANGE_WIFI_MULTICAST_STATE`，
以及局域网明文 HTTP（`android:usesCleartextTraffic="true"`）。
Bridge 端口（默认 11880）**仅限受控教学网段**，切勿映射到公网。

### 密钥与凭据

- 课堂密钥由教师在教师机生成（128 bit 系统随机数），**只保存在教师机与教师手机**；
  设置下发到学生机前会删除 `sessionKey`、`aiApiKey` 等全部秘密字段，学生机只收到一个
  `keyConfigured` 布尔值。学生端退出保护只认管理员凭据。
- App 把密钥存在 `SharedPreferences("ikeback", MODE_PRIVATE)`。
  `android:allowBackup="false"`，且 `backup_rules.xml` / `data_extraction_rules.xml`
  显式排除 `ikeback.xml`，避免随云备份或设备迁移泄露。
- 密钥本身永不上网：每个请求用 `HMAC-SHA256(method + path + nonce + SHA256(body))`
  证明持有密钥；`/state` 返回的 nonce **一次性**，抓到的请求无法重放。
- Bridge 是**明文 HTTP**：token 与全部 JSON 在网段内可读，因此必须限定在隔离的教学网，
  不要在可被学生抓包的网络上使用同一密钥控制真实课堂。
