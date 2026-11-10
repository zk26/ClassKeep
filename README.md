# 课守 ClassKeep

> 基于 [Veyon](https://veyon.io/) 4.x 的教学管理定制分支。中文名「课守」，英文名 `ClassKeep`。
> 仓库：<https://github.com/zk26/ClassKeep> · 许可：**GPL-2.0-or-later**
> 本分支不是 Veyon 官方版本，与上游项目无隶属关系。

一个课堂上真正能用的「看得见、管得住、记得下、评得准」的整套方案：
教师机控制台 + 学生机常驻客户端 + 教师机数据主库 + Android 教室端 App + 本地 AI。

---

## 1. 仓库目录

本仓库根就是整个 `G:\veyon`，全部项目都在同一个仓库里：

```
G:\veyon\  (= 仓库根)
├── README.md            ← 本文件：项目总入口
├── LICENSE              GNU GPL v2 全文（与 veyon/COPYING 相同）
├── NOTICE.md            版权、许可、修改声明、商标、隐私与无担保声明
├── .gitignore           排除生成物 / 本机配置 / 发布产物
│
├── veyon\               桌面端（上游 Veyon 源码 + 课守插件）
│   ├── README.md         仓库首页总入口（桌面端视角）
│   ├── COPYING           上游 GPL v2 全文
│   ├── plugins\classroom\ 课守插件源码及说明
│   ├── docs\             全套协议与说明文档
│   ├── scripts\          发布包打包脚本及说明
│   ├── .github\workflows\ 发布构建流水线
│   ├── mobile\           Android 工程的**历史提交副本**（请勿在此修改）
│   └── core\, master\, server\  对上游的最小扩展点
│
└── mobile\              ← Android 教室端 App（**唯一权威工程**，Android Studio 打开此目录）
```

> **`mobile/` 与 `veyon/mobile/` 的关系**：后者是先前推送到旧 fork 时留下的副本，
> 仅为保留历史而存在，**不再维护**；所有 Android 改动请在根 `mobile/` 进行。
> 本仓库保留了上游 Veyon 的完整提交历史（详见 `git log`）。

---

## 2. 三类发布产物

| 产物 | 运行位置 | 内容 |
|---|---|---|
| **教师端** | 教师机 Windows | Veyon Master + Configurator + 课守插件 + Bridge 服务 + 预设配置 |
| **学生端** | 学生机 Windows（UWF 还原） | Veyon Service / Server / Worker + 课守插件 + 预设安全配置 |
| **APP** | Android 11+ 手机/平板 | 课守教室端（网段发现、密钥同步、班级数据、控制指令、期末表、词云） |

产物命名、内容清单、校验与安装步骤见
[`veyon/docs/CLASSKEEP-RELEASE.md`](veyon/docs/CLASSKEEP-RELEASE.md)。

> 仓库内**不提交预编译二进制**。三类产物由
> [发布工作流](veyon/.github/workflows/classkeep-release.yml) 在推送 `v*` tag 时于云端构建并发布；
> 也可在有工具链的机器上用 [`scripts/package-classkeep.ps1`](veyon/scripts/package-classkeep.ps1) 本地打包。
>
> **[v1.0.0 Release](https://github.com/zk26/ClassKeep/releases/tag/v1.0.0) 三类产物全部有内容**：
> 本机实测编译的 `ClassKeep-Mobile-1.0.0.apk`（debug 内测包）、
> `ClassKeep-Teacher-1.0.0-win64-preset.zip`、`ClassKeep-Student-1.0.0-win64-preset.zip`，
> 以及 `SHA256SUMS.txt` / `RELEASE-NOTES.md`。
>
> 两个 `-preset` 包是**部署预设包**（配置模板、安装脚本、全套文档与许可文件），
> 包内首行明确标注**不含 Veyon / 课守二进制**——开发机缺 Qt 6 / QCA / LibVNC / CMake，
> 无法本地编译桌面端。完整二进制 ZIP 由 CI 或 `scripts/package-classkeep.ps1` 在有工具链的机器上补上。
> **不含任何占位包、空包或伪造二进制**，Release 说明中逐项标注了每类产物的状态。

---

## 3. 快速开始

### 桌面端（**只能 MinGW-w64/GCC**，MSVC 不可行）

完整方案见 [`veyon/docs/CLASSKEEP-BUILD.md`](veyon/docs/CLASSKEEP-BUILD.md)。
本机装 Qt6-MinGW + 自编 QCA-Qt6 + LibVNC 等依赖较重；**推荐先推代码到 `main`，
由 [GitHub Actions 编译工作流](.github/workflows/classkeep-build.yml) 做真实编译验证**
（<https://github.com/zk26/ClassKeep/actions> → ClassKeep Build Check → Run workflow）。

```powershell
# 有完整 MinGW 工具链时（简写，详见 CLASSKEEP-BUILD.md）
cmake -S veyon -B build -G Ninja -DWITH_QT6=ON -DWITH_WEBAPI=OFF -DWITH_CORE_ONLY=OFF
cmake --build build
```

### Android（JDK 21+ 与 Android SDK；本机 `JAVA_HOME` 需指向 JDK 根目录，不要带 `\bin`）

```powershell
$env:JAVA_HOME = 'E:\jdk-23.0.2'
$env:ANDROID_HOME = 'G:\Andorid'
cd mobile
.\gradlew.bat :app:assembleDebug :app:testDebugUnitTest
```

### 打发布包

```powershell
cd veyon
.\scripts\package-classkeep.ps1 -Version 1.0.0 `
    -TeacherDir <教师端编译输出> -StudentDir <学生端编译输出> `
    -ApkPath <app-debug.apk> -OutputDir dist
```

---

## 4. 文档索引（全部说明文件）

### 法律与合规（部署前请先读）

| 文档 | 内容 |
|---|---|
| [`LICENSE`](LICENSE) | GNU GPL v2 全文 |
| [`NOTICE.md`](NOTICE.md) | 版权归属、许可、修改声明、商标、隐私与无担保声明 |
| [`veyon/docs/CLASSKEEP-LICENSE.md`](veyon/docs/CLASSKEEP-LICENSE.md) | 衍生作品与分发义务详解 |
| [`veyon/docs/CLASSKEEP-THIRD-PARTY.md`](veyon/docs/CLASSKEEP-THIRD-PARTY.md) | 第三方组件与许可清单、分发核对清单 |
| [`veyon/docs/CLASSKEEP-PRIVACY.md`](veyon/docs/CLASSKEEP-PRIVACY.md) | **数据采集范围与合规义务**（监控类软件必读） |

### 技术与使用

| 文档 | 内容 |
|---|---|
| [`veyon/README.md`](veyon/README.md) | 桌面端仓库首页总入口 |
| [`veyon/docs/CLASSKEEP.md`](veyon/docs/CLASSKEEP.md) | 功能总览、架构通道图、目录结构、构建、数据流 |
| [`veyon/docs/CLASSKEEP-BUILD.md`](veyon/docs/CLASSKEEP-BUILD.md) | **真实编译方案**：GitHub Actions 编译验证、本机 MinGW 编译、CI 出 Windows 产物、排错表 |
| [`veyon/docs/CLASSKEEP-PROTOCOL.md`](veyon/docs/CLASSKEEP-PROTOCOL.md) | **协议规范**：命令/参数全量数值表、HTTP 路由、认证算法、UDP 发现、数据格式、兼容性铁律 |
| [`veyon/docs/CLASSKEEP-DEPLOY.md`](veyon/docs/CLASSKEEP-DEPLOY.md) | 部署、每堂课流程、UWF 与 Defender 注意、数据迁移、排障 |
| [`veyon/docs/CLASSKEEP-RELEASE.md`](veyon/docs/CLASSKEEP-RELEASE.md) | 发布产物清单、命名、校验、安装 |
| [`veyon/mobile/README.md`](veyon/mobile/README.md) | Android 教室端工程说明 |
| [`veyon/plugins/classroom/README.md`](veyon/plugins/classroom/README.md) | 课守插件源码结构 |
| [`veyon/scripts/README.md`](veyon/scripts/README.md) | 发布包打包脚本 |

---

## 5. 关键约束

- 学生机须为 **Windows 10/11 企业版、教育版或 IoT 企业版**（UWF 仅这些版本支持）。
- 只用系统自带签名 API（`uwfmgr.exe` / `netsh.exe` / Core Audio COM / SetupAPI），
  不写内核驱动、不做进程注入、不落临时脚本，Windows Defender 友好。
- 课堂密钥每堂课轮换，同时用于学生端认证与手机端 Bridge 认证；密钥永不上网，只参与本地摘要。
- 学生端默认零落盘，全部行为数据集中到教师机，按主机/日期/学生分别保存。
- **本软件会采集学生行为数据**，部署方须先阅读 `CLASSKEEP-PRIVACY.md` 并履行告知、最小必要、
  访问控制与定期删除义务；项目作者不接触任何实际数据，也不对部署行为承担责任。

---

## 6. 状态

- Android 端：`assembleDebug` 与 `testDebugUnitTest` 已在本机实测通过。
- 桌面端：代码经静态复核，**尚未真实编译**（开发机缺 Qt/CMake 等工具链）。
- 首次在有完整工具链的机器上编译如遇报错，请通过仓库 Issues 反馈。

---

GitHub：<https://github.com/zk26/ClassKeep> · 上游：<https://github.com/veyon/veyon>
许可与归属疑问见 [`NOTICE.md`](NOTICE.md)。
