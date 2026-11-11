# 课守 ClassKeep · Android 教室端（权威工程）

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

| 底部标签 | 内容 |
|---|---|
| 课堂 | 品牌头、连接状态、IP / 密钥 / 端口，**密钥可手动输入或导入 `.key` 文件**、立即同步、离线工作、班级概况、最近活动 |
| 加分 | 按教师端座位网格**拖拽后的顺序**生成学生卡片；点卡片选 `-5…+5`（步长 1）→ 填原因 → 提交；乐观更新，失败回滚 |
| 控制 | 静音 / 断网 / 肃静 / USB / 远控 / 投屏 / UWF 等指令 |
| 课表 | 周课表网格、当天列与当前节高亮、下一节课提示、点格子填科目、常用科目预设、导入作息 JSON |
| 更多 | 期末成绩表导出、班级词云、许可与隐私说明 |

设计遵循[交互与动效规范](../veyon/docs/CLASSKEEP-UI.md)的要点：
卡片**不使用投影阴影**、只用一层细描边；外圆角 20dp / 内圆角 14dp 的同心圆角；
浮动标签常驻框上方、帮助文字常驻框下方，不依赖会消失的 placeholder；
底部胶囊指示随选中项平滑移动、图标回弹、仅选中项显示文字；
列表末尾留出底部栏 + 手势安全区。

**平板适配**：`values-sw600dp` / `values-sw720dp` 提供更大的页面边距、课表格宽度
与学生卡片列数（手机 2 列 / 7 寸 3 列 / 10 寸 4 列），课表可横向滚动。

## 源码

```
app/src/main/java/top/ikee002/classkeep/
  MainActivity.kt       5 个标签页、密钥导入、座位加分、课表、报表/词云
  UiKit.kt             视觉规范组件（无阴影卡片、常驻标签、无边框按钮）
  BottomBar.kt         自定义底部导航（胶囊跟随 + 图标回弹）
  TimetableStore.kt    零依赖周课表数据层（兼容全校作息 JSON 导入）
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

应用图标来自仓库根的 [`../ClassKeep_logo.png`](../ClassKeep_logo.png)（课守 ClassKeep 品牌徽标）。
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
