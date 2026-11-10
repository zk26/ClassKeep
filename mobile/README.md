# 课守 ClassKeep · Android 教室端（权威工程）

这是仓库根 `mobile/`，**Android 教室端 App 的唯一权威工程**。
请用 Android Studio 打开本目录；`../veyon/mobile/` 是早期推送到旧 fork 时留下的历史副本，**请勿在此修改**。

- 仓库：<https://github.com/zk26/ClassKeep>
- 许可：**GPL-2.0-or-later**（见 [`../LICENSE`](../LICENSE)、[`../NOTICE.md`](../NOTICE.md)）
- 完整工程说明：[`../veyon/mobile/README.md`](../veyon/mobile/README.md)（内容相同，属历史副本）
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

## 源码

```
app/src/main/java/top/ikee002/classkeep/
  MainActivity.kt       UI、自动发现节流、同步、控制指令、报表/词云
  BridgeClient.kt       HttpURLConnection 全部路由 + 认证头
  Discovery.kt          DatagramSocket 主动探测 + 周期广播 + MulticastLock
  ClassKeepCrypto.kt    SHA-256 挑战应答（与教师机逐字节一致）
  ClassKeepModels.kt    数据类与 org.json 解析
  Announcement.kt       UDP 广播报文常量与解析
  CacheStore.kt         SharedPreferences 离线缓存
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
