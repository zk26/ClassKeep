# 课守 ClassKeep — 第三方组件与许可清单

本文件列明本仓库**构建期 / 运行期**涉及的第三方组件及其许可，用于分发合规核对。
**本仓库不附带、不重新分发任何下列组件的二进制文件**；它们由使用者自行安装或由包管理器提供。
如有遗漏，以各组件官方发布时随附的许可证为准。

---

## 1. 上游代码（直接包含在本仓库内）

| 组件 | 来源 | 许可 | 在本仓库中的位置 |
|---|---|---|---|
| Veyon 4.x | <https://github.com/veyon/veyon> | **GPL-2.0-or-later** | `veyon/`（除本分支新增/修改文件） |
| Veyon 自带图标、翻译与文档 | 同上 | GPL-2.0-or-later | `veyon/core/resources/`、`veyon/translations/` |

Veyon 的 GPL v2 全文见仓库根 [`LICENSE`](../../LICENSE) 与 [`veyon/COPYING`](../COPYING)。
本仓库相对上游的修改声明见 [`NOTICE.md`](../../NOTICE.md) 第 4 节。

## 2. 桌面端构建依赖（不随本仓库分发）

| 组件 | 用途 | 许可（以官方为准） |
|---|---|---|
| Qt 6 / Qt 5.15（Core, Gui, Widgets, Network, Concurrent） | UI 与基础库 | LGPL-3.0 / GPL-3.0 / 商业许可（Qt 双许可） |
| QCA（Qt Cryptographic Architecture） | 密钥加密落盘 | LGPL-2.1-or-later |
| LibVNCClient / LibVNCServer | Veyon 远程屏幕通道 | GPL-2.0-or-later |
| OpenSSL | TLS / 加密 | Apache-2.0（3.x）或 OpenSSL License（1.x） |
| zlib | 压缩 | zlib License |
| libjpeg-turbo | 屏幕图像编解码 | BSD-3-Clause / IJG |
| LZO | 压缩 | GPL-2.0-or-later |
| OpenLDAP / Cyrus SASL | 目录与认证（Linux/可选） | OpenLDAP License / CMU 风格许可 |
| PAM | 认证（Linux/可选） | 各实现许可 |
| libX11 / libXtst / libfakekey | X11 输入（Linux/可选） | MIT / X11 风格许可 |
| NSIS | Windows 安装包（可选） | zlib License |
| CMake、编译器（MSVC / MinGW / GCC / Clang） | 构建工具 | 各自许可 |

## 3. 桌面端运行时系统接口（Windows，不分发）

以下为 Windows 操作系统自带的公开 API 或系统自带签名程序，本仓库只调用、不分发：

| 接口 / 程序 | 用途 |
|---|---|
| `uwfmgr.exe`、UWF WMI 类（`root\standardcimv2\embedded`） | UWF 还原开关与状态 |
| `netsh.exe advfirewall`（Windows 防火墙 / WFP 用户态配置接口） | 断网与教师机例外规则 |
| Core Audio COM（`IMMDeviceEnumerator` / `IAudioEndpointVolume`） | 静音 |
| SetupAPI / `WindowsDeviceFunctions`（Veyon 上游已有） | USB 输入设备禁用 |
| `shutdown.exe` | 重启 |
| CryptoAPI / DPAPI（经 Veyon `CryptoCore`） | 密钥加密落盘 |

**本仓库不包含内核驱动、不安装驱动、不做进程注入、不落临时脚本。**

## 4. Android 教室端依赖（`mobile/`，不随本仓库分发二进制）

| 组件 | 版本 | 许可（以官方为准） |
|---|---|---|
| AndroidX `core-ktx` | 1.10.1 | Apache-2.0 |
| AndroidX `appcompat` | 1.6.1 | Apache-2.0 |
| AndroidX `activity` | 1.8.0 | Apache-2.0 |
| AndroidX `constraintlayout` | 2.1.4 | Apache-2.0 |
| Material Components for Android | 1.10.0 | Apache-2.0 |
| JUnit | 4.13.2 | Eclipse Public License 1.0 |
| AndroidX Test Ext JUnit / Espresso | 1.1.5 / 3.5.1 | Apache-2.0 |
| Kotlin 标准库 | 随 AGP/Kotlin 插件 | Apache-2.0 |
| Android Gradle Plugin / Gradle | 9.1.0 / 9.3.1 | Apache-2.0 |
| Android SDK / JDK (21+) | 平台 | 各自许可 |
| `org.json`、`HttpURLConnection`、`MessageDigest`、`DatagramSocket`、`SharedPreferences` | Android / JDK 内置 | 平台许可 |

> 本分支的 Android 工程**未引入任何第三方网络、JSON、加密或图片库**（无 Retrofit、OkHttp、
> Gson、Moshi、Room、Compose、协程、Glide 等）；上述 HTTP/JSON/加密能力全部使用平台内置 API。

## 5. 可选外部服务（由使用者自行部署与授权）

| 服务 | 用途 | 说明 |
|---|---|---|
| Ollama（默认示例端点） | 本地 AI 推理 | MIT，非本仓库组件 |
| 任意 OpenAI 兼容推理服务 | 本地或内网 AI 接口 | 由使用者自行确认其条款与数据政策 |
| Veyon WebAPI（上游插件） | 可选：手机端获取缩略图 / 控制 Feature | 上游 GPL-2.0-or-later |

## 6. 明确排除的代码来源

| 项目 | 关系 |
|---|---|
| `G:/UWFTool` | 仅参考 `uwfmgr` 命令与状态机思路，**未复制任何代码 / 脚本 / 资源** |
| `candiedoperation/Monitoring-Center` | 仅参考「移动端驱动 Veyon」理念，**未复制任何代码 / 界面 / 资源** |

Android 应用图标为 Android Studio 模板默认资源；桌面端图标沿用 Veyon 自带资源。
本仓库不含第三方字体、图片素材或音频资源。

---

## 7. 分发时的核对清单

分发二进制产物前请确认：

1. 随附 `LICENSE`（GPL v2 全文）与 `NOTICE.md`；
2. 提供对应完整源码或有效的源码获取方式（GPL 第 3 条）；
3. 若捆绑了 Qt / QCA / OpenSSL 等第三方 DLL 或 `.so`，逐一核对其许可证是否允许该分发方式
   （尤其 LGPL 动态链接与 GPL 静态链接的差异）；
4. 若捆绑了 Android 依赖，随附相应的 Apache-2.0 许可文本与归属声明；
5. 保留所有文件头的版权声明，并保留本仓库的修改声明（git 历史 + `NOTICE.md`）；
6. 不附加任何与 GPL 冲突的额外限制（如禁止再分发、禁止修改、要求保密等）。
