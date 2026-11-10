# 课守 ClassKeep

基于 [Veyon](https://veyon.io/) 4.x 的教学管理定制分支。中文名「课守」，英文名 `ClassKeep`。
仓库：<https://github.com/zk26/ClassKeep> · 许可 **GPL-2.0-or-later**（`SPDX-License-Identifier: GPL-2.0-or-later`）。
本分支不是 Veyon 官方版本，与上游项目无隶属、背书或合作关系。

> **部署前必读**：本项目会采集学生行为数据。请先阅读
> [`CLASSKEEP-PRIVACY.md`](CLASSKEEP-PRIVACY.md) 履行告知与数据保护义务，
> 再阅读 [`CLASSKEEP-LICENSE.md`](CLASSKEEP-LICENSE.md)、[`../../NOTICE.md`](../../NOTICE.md)
> 与 [`CLASSKEEP-THIRD-PARTY.md`](CLASSKEEP-THIRD-PARTY.md) 确认许可与分发要求。
> 完整文档索引见仓库根 [`../../README.md`](../../README.md)。

---

## 1. 它解决什么

普通 Veyon 解决「看得见、控得住」，课守在此基础上补齐「这堂课是谁、他做了什么、怎么长期评价」：

| 能力 | 说明 |
|---|---|
| 班级与 IP 绑定 | 教师端选择班级、按 IP 一键登录/退出/换人；名单支持 CSV 导入导出，IP 可自动注册进 Veyon 电脑目录，也可直连不在目录内的 IP |
| 课堂密钥 | 每堂课自动生成新密钥；学生端悬浮条输入「座位号 + 姓名 + 密钥」完成认证；认证后锁定退出，退出需超级管理员凭据或密钥，进程被结束由服务端看门狗自动拉起 |
| 全量行为日志 | 前台窗口标题、进程名、浏览器判定、登录/退出/换人/AI 提问/退出尝试等；教师机按「日期 / 主机 / 班级 / 座位」分别落盘，每台学生机都有独立日志 |
| 兴趣词云与分层 | 中文分词 + 关键词表 + 半衰期加权，跨多堂课动态计算兴趣分布与分层建议；每生可生成 SVG 词云 |
| 积分制 | 基础分 100；每日按节次录入，同一节课多次加扣分合并进同一格；积分明细支持加粗、下划线、颜色、字体、字号、插图；规则由教师发布后学生端才可见 |
| 期末报表 | 导出「一节课一列 + 目前总积分」的班级总表，`.xls`（Excel 2003 XML，零依赖）或 CSV |
| UWF 与装软件 | 教师端查询/关闭/开启 UWF、重启确认、分发安装包并静默安装；全程只用 `uwfmgr.exe` / `netsh.exe` 等系统自带二进制 |
| 课堂控制 | 静音、WFP 断网（保留教师机例外）、USB 输入拦截（桌面可见）、Ctrl+双击远控、分组投屏、多选控屏、肃静页、学生呼叫老师 |
| 本地 AI | 教师端配置本地或网络 OpenAI 兼容接口；学生端 AI 悬浮窗聊天，提问进入日志 |
| 手机教室端 | Android 原生 App，网段内发现教师机后同步数据并下发控制；找不到教师机就不操作，另有手动同步 |

---

## 2. 架构与通道

```
Android 教室端 App
   │  UDP 发现（只带挑战值）  ┌──────────────────────────────┐
   │  HTTP/JSON（密钥认证）   │ 教师机 Veyon Master           │
   └─────────────────────────>│  ├ 课守控制台（Qt 对话框）    │
                              │  ├ ClassroomBridge（HTTP/UDP）│
                              │  └ 数据主库 %APPDATA%/ClassKeep│
                              └───────────┬──────────────────┘
                                          │ Veyon FeatureMessage（原有加密 VNC 通道）
                                          v
                              ┌──────────────────────────────┐
                              │ 学生机 Veyon Service (SYSTEM) │
                              │  ├ Server：UWF / WFP / 重启    │
                              │  └ Worker：悬浮窗/静音/肃静页 │
                              └──────────────────────────────┘
```

设计原则（延续 Veyon 与 UWFTool 的节约风格）：

- **不新增常驻服务 / 端口**：教师机 ↔ 学生机复用 Veyon 已有的 `FeatureMessage`；只有手机端需要额外的 Bridge HTTP 端点。
- **不新增第三方库**：C++ 侧只用 Qt + 系统 API；Android 侧只用 `HttpURLConnection` / `org.json` / `MessageDigest` / `DatagramSocket` / `SharedPreferences`。
- **不引入内核驱动 / 进程注入 / 临时脚本**：所有敏感操作都调系统签名二进制（`uwfmgr.exe`、`netsh.exe`、`shutdown.exe`）或用户态 COM/WFP，Windows Defender 友好。
- **学生端零落盘**：行为事件只经 `ReportEvent` 上报，教师机统一按主机分目录保存；教师机离线时服务端有补传队列（上限 5000 条）。

---

## 3. 目录结构

仓库根为 `G:\veyon`（=<https://github.com/zk26/ClassKeep>），本文件位于 `veyon/docs/`。
下列路径中，带 `veyon/` 前缀的是桌面端子目录，带 `mobile/` 前缀的是仓库根下的 Android 工程。

```
veyon/plugins/classroom/      课守插件（全部新增）
  ClassroomPlugin.*             Master / Server / Worker 三端消息分发与生命周期
  ClassroomProtocol.h           Feature UID、Command / Argument 枚举、事件类型、配置键、产品名
  ClassroomConfiguration.h      配置项宏
  ClassroomConfigurationPage.*  Veyon Configurator 配置页
  ClassroomMasterDialog.*       教师端控制台（6 个页签）
  ClassroomScorePanel.*         积分/规则/词云/期末报表页
  ClassroomClient.*             学生端悬浮条、AI 聊天、积分明细、肃静页、行为采集
  ClassroomAiClient.*           OpenAI 兼容 HTTP 客户端
  ClassroomBridge.*             教师机 HTTP/JSON + UDP 发现 + 密钥认证
  ClassroomData.*               名单、日志存储、兴趣分析
  ClassroomScore.*              积分册（节次、富文本明细、规则、学生端 HTML）
  ClassroomTermReport.*         期末 .xls / CSV 导出
  ClassroomWordCloud.*          中文分词与 SVG / 位图词云
  ClassroomSystem.*             UWF / Core Audio / 防火墙 / 安装命令
  ClassroomSecrets.*            密钥加密落盘与挑战应答摘要
  ClassroomSelfTest.cpp         零依赖自检用例（WITH_TESTS=ON）

mobile/                       Android 教室端（Kotlin，零第三方网络库）— 唯一权威工程
  app/src/main/java/top/ikee002/classkeep/
veyon/mobile/                 历史提交副本，仅为保留旧 fork 结构，**不再维护**

核心扩展点（对上游的最小改动）
  veyon/core/src/FeatureProviderInterface.h    增加 startSessionFeature() 钩子
  veyon/core/src/FeatureManager.h/.cpp         增加 startSessionFeatures() 分发
  veyon/server/src/ComputerControlServer.cpp   start() 时调用该钩子
  veyon/master/src/ComputerMonitoringWidget.cpp  Ctrl+双击缩略图直接远控

仓库根级
  README.md / LICENSE / NOTICE.md / .gitignore
```

---

> **关于两份 `mobile/` 目录**：仓库根 `mobile/` 是 **Android 唯一权威工程**（Android Studio 打开它）；
> `veyon/mobile/` 是早期推送到旧 fork `zk26/veyon` 时留下的**历史副本，不再维护**。
> 两者互不联动，请勿在 `veyon/mobile/` 中修改代码。
> 生成物、本机 SDK 配置与发布产物已由仓库根 `.gitignore` 排除。

## 4. 构建

### 4.1 桌面端（Windows）

> **完整构建方案见 [`CLASSKEEP-BUILD.md`](CLASSKEEP-BUILD.md)**。下面只是要点摘要。

Veyon 4.11.4 的 CMake **只能用 MinGW-w64 / GCC 编译**（硬编码 `-Wno-attributes`、
`-fno-exceptions`、`-Wl,-no-undefined`、`-mwindows`、`-lws2_32`，
且 `windows-binaries` target 依赖 MinGW 前缀与 Unix shell 命令）。
**MSVC + vcpkg 不可行**，QCA 也没有 vcpkg port（`find_package(Qca-qt6 REQUIRED)`）。

需要：Qt6-MinGW（含 `Core5Compat`、`GuiPrivate`、`HttpServer`、`WebSockets`、`LinguistTools`）、
自编的 QCA-Qt6（`Qca-qt6Config.cmake`）、LibVNC ≥ 0.9.14、
OpenSSL / zlib / libjpeg / libpng / LZO / SASL / LDAP / interception、CMake ≥ 3.18、Ninja、NSIS。
上游 Windows 构建是在 Linux 容器里用 `x86_64-w64-mingw32` 交叉编译，其镜像私有且无 Dockerfile。

最快获得「真实编译结果」的方式不是本机装这一整套，而是
**GitHub Actions 的 [`.github/workflows/classkeep-build.yml`](../../.github/workflows/classkeep-build.yml)**
（Linux/Qt5/GCC 全量编译，能验证除 Windows 专属分支外的全部 C++ 文件）。

`plugins/classroom/` 会被 `plugins/CMakeLists.txt` 的 GLOB 自动发现，无需手工注册；
但 `-DWITH_CORE_ONLY=ON` 会跳过整个 `plugins/`，必须保持 OFF。
自检 `-DWITH_TESTS=ON` 生成 `ClassroomSelfTest`；注意 `-DWITH_BUNDLED_LIBVNC=ON` 会强制关闭 `WITH_TESTS`。

### 4.2 Android

```powershell
cd mobile
.\gradlew.bat :app:assembleDebug
.\gradlew.bat :app:testDebugUnitTest
```

`minSdk 30`、`targetSdk 36`、AGP 9.1.0 / Gradle 9.3.1 / JDK 21。首次构建会在 `local.properties` 里需要 `sdk.dir`（Android Studio 自动生成，不需要提交）。

---

## 5. 数据流

1. 教师在控制台点「开始新课堂」→ 生成新 `sessionId` 与 16 位十六进制密钥 → 强制退出上一堂课所有学生 → 推送设置、名单、密钥。
2. 学生端悬浮条输入座位号 + 姓名 + 密钥，或教师端按 IP 直接下发身份 → 认证成功后客户端锁定。
3. 行为采集（前台窗口/进程）与积分、呼叫、UWF 状态等事件经 `ReportEvent` 上报；教师机 `ClassroomLogStore` 写 `events/<日期>.jsonl`、`machines/<主机>/<日期>.jsonl`、`students/<班级>/<座位-姓名>/<日期>.jsonl`。
4. 教师机 Bridge 每 5 秒广播一次挑战值；App 发现后用本堂课密钥做 SHA-256 摘要请求数据/控制接口。
5. 期末导出按节次生成列，积分明细富文本经 base64 图片内联后随 `ScoreReply` 下发到学生端 `QTextBrowser`。

完整的命令字、路由、认证算法与文件格式见 [CLASSKEEP-PROTOCOL.md](CLASSKEEP-PROTOCOL.md)。

---

## 6. 状态与限制

- **Android 端已实测编译通过**：`:app:assembleDebug` 与 `:app:testDebugUnitTest` 均 `BUILD SUCCESSFUL`，
  产出 `app-debug.apk`；本机需 JDK 21+（实测 JDK 23）与 Android SDK。
- **桌面端尚未真实编译**：开发机缺 Qt 6 / QCA / LibVNC / CMake，代码仅经静态复核。
  首次在有完整工具链的机器上编译如遇报错请通过 Issues 反馈。
- UWF 仅存在于 Windows 10/11 **企业版、教育版、IoT 企业版**；专业版/家庭版不支持。
- `uwfmgr filter enable/disable` 是「排队到下次会话」，必须**重启**才生效，且不能用「关机再开机」代替（快速启动）。
- Bridge 认证依赖每堂课密钥；教师机未生成密钥时所有数据/控制接口 fail-closed 返回 401。
- iOS 端尚未提供工程；协议为标准 HTTP/JSON + UDP + SHA-256，可直接复用。
- 本软件处理学生行为数据，部署前必须阅读 [`CLASSKEEP-PRIVACY.md`](CLASSKEEP-PRIVACY.md)；
  分发二进制前必须阅读 [`CLASSKEEP-LICENSE.md`](CLASSKEEP-LICENSE.md) 与
  [`CLASSKEEP-THIRD-PARTY.md`](CLASSKEEP-THIRD-PARTY.md)。
