# ClassKeep · Android 教室端（权威工程）

这是仓库根 `mobile/`，**Android 教室端 App 的唯一工程**（历史副本已清理）。
请用 Android Studio 打开本目录。

- 仓库：<https://github.com/zk26/ClassKeep>
- 许可：**GPL-2.0-or-later**（见 [`../LICENSE`](../LICENSE)、[`../NOTICE.md`](../NOTICE.md)）
- 协议：[`../veyon/docs/CLASSKEEP-PROTOCOL.md`](../veyon/docs/CLASSKEEP-PROTOCOL.md)
- 数据合规：[`../veyon/docs/CLASSKEEP-PRIVACY.md`](../veyon/docs/CLASSKEEP-PRIVACY.md)

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

## 2.0.0 底部栏（4 个标签，图标 + 名称常显）

不再用单个汉字假装图标，也不再只给选中项显示文字。四个标签均为
`res/drawable/ck_tab_*.xml` 的**纯矢量图标 + 常驻名称**，选中时图标着色并回弹、
白色胶囊平滑跟随。图标全部自绘，未使用任何第三方图标库或会话凭据。

| 底部标签 | 内容 |
|---|---|
| 课堂 | 品牌头、连接状态、同步/离线、班级概况、最近活动，**课堂控制**（静音 / 断网 / 肃静 / USB / 远控 / 投屏 / UWF）+ **加分减分**（按教师端拖拽顺序生成卡片，`-5…+5` 选分 → 填原因 → 乐观更新失败回滚） |
| 课表 | **作息设置**（一天几节课、每节时长、第一节开始时间、普通课间、大课间在第几节后及多长、午休在第几节后及多长）、**单列 / 双列时间**、导入 / 导出作息 JSON、恢复默认、点格子填科目、当天列与当前节高亮 |
| 配置 | IP / 本堂课密钥 / 桥接端口、显示或导入 `.key`、同步测试、**单列 / 双列时间**快捷切换 |
| 更多 | **导入导出**：密钥、作息 JSON、课表 JSON、本地缓存数据、期末成绩表、班级词云；**课堂回顾**：选一堂课后查看「讲了什么 / 这堂课干了什么 / 同学表现怎么样」，联网读教师机、离线读本地缓存 |

设计遵循[交互与动效规范](../veyon/docs/CLASSKEEP-UI.md)的要点：
卡片**不使用投影阴影**、只用一层细描边；外圆角 20dp / 内圆角 14dp 的同心圆角；
浮动标签常驻框上方、帮助文字常驻框下方，不依赖会消失的 placeholder；
底部胶囊指示随选中项平滑移动、图标回弹、**所有标签始终显示名称**；
列表末尾留出底部栏 + 手势安全区。

导出文件写入 App 私有 `Documents` 目录（`getExternalFilesDir`），
文件名分别为 `ClassKeep-课表.json` / `ClassKeep-本地数据.json` / `ClassKeep-期末成绩表.xls`。

**平板适配**：`values-sw600dp` / `values-sw720dp` 提供更大的页面边距、课表格宽度
与学生卡片列数（手机 2 列 / 7 寸 3 列 / 10 寸 4 列），课表可横向滚动。

## 源码

```
app/src/main/java/top/ikee002/classkeep/
  MainActivity.kt       4 个标签页、课堂控制/加分、作息设置、导入导出、课堂回顾
  UiKit.kt             视觉规范组件（无阴影卡片、常驻标签、无边框按钮）
  BottomBar.kt         自定义底部导航（矢量图标 + 常显名称、胶囊跟随 + 图标回弹）
  TimetableStore.kt    零依赖周课表数据层（可配置节次/大课间/午休/单双列时间，兼容全校作息 JSON）
  BridgeClient.kt      HttpURLConnection 全部路由 + 挑战应答认证
  Discovery.kt         DatagramSocket 主动探测 + 周期广播 + MulticastLock
  ClassKeepCrypto.kt   SHA-256 挑战应答（与教师机逐字节一致）
  ClassKeepModels.kt   数据类与 org.json 解析
  Announcement.kt      UDP 广播报文常量与解析
  CacheStore.kt        SharedPreferences 离线缓存
```

**零第三方网络/JSON/加密库**：全部使用 Android / JDK 平台内置 API。
第三方构建依赖与许可见 [`../veyon/docs/CLASSKEEP-THIRD-PARTY.md`](../veyon/docs/CLASSKEEP-THIRD-PARTY.md)。

## 图标与界面

应用图标来自仓库根的 [`../ClassKeep_logo.png`](../ClassKeep_logo.png)（ClassKeep 品牌徽标）。
`tools/generate_icons.py` 从该图生成 `mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/` 下的
`ic_launcher.png`（圆角方形）与 `ic_launcher_round.png`（圆形），并清理 Android Studio 模板的
自适应图标 XML / webp。更换 Logo 后重跑：

```bash
python tools/generate_icons.py     # 需要 Pillow
```

界面采用与 Logo 一致的蓝→紫品牌渐变：头部展示 Logo 与中英文名称，卡片/输入框/状态胶囊/控制按钮
均为圆角样式（`res/drawable/bg_*.xml`），配色集中在 `res/values/colors.xml`，主题在 `themes.xml`。
夜间主题在 `values-night/themes.xml`。

## 权限

`INTERNET`、`ACCESS_NETWORK_STATE`、`ACCESS_WIFI_STATE`、`CHANGE_WIFI_MULTICAST_STATE`，
以及局域网明文 HTTP（`android:usesCleartextTraffic="true"`）。
Bridge 端口（默认 11880）**仅限受控教学网段**，切勿映射到公网。
