# ClassKeep

> 基于 [Veyon](https://veyon.io/) 4.x 的教学管理定制分支。**自 2.0.0 起产品名只使用英文 `ClassKeep`**。
> 仓库：<https://github.com/zk26/ClassKeep> · 许可：**GPL-2.0-or-later**
> 本分支不是 Veyon 官方版本，与上游项目无隶属关系。
> 全部进程名 / 服务名 / 窗口名的改名清单与 GPL 修改声明见 [`NOTICE.md` §4.1](NOTICE.md)。

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
├── .github\workflows\   三条 GitHub Actions 流水线（编译验证 / Windows 构建 / 发布）
├── veyon\               桌面端（上游 Veyon 源码 + 课守插件）
│   ├── README.md         桌面端视角的总入口
│   ├── COPYING           上游 GPL v2 全文
│   ├── plugins\classroom\ 课守插件源码及说明
│   ├── docs\             全套协议与说明文档
│   ├── scripts\          Windows 依赖收集、NSIS 安装器、图标生成脚本
│   └── core\, master\, server\  对上游的最小扩展点
│
└── mobile\              ← Android 教室端 App（**唯一工程**，Android Studio 打开此目录）
```

`ClassKeep_logo.png` 是唯一的品牌图标来源，桌面端 `.ico` 与 Android `mipmap` 图标均由它生成。

**2.0.0 命名**：所有进程均为 `ClassKeep-master.exe` / `ClassKeep-server.exe` /
`ClassKeep-worker.exe` / `ClassKeep-service.exe` / `ClassKeep-configurator.exe` /
`ClassKeep-cli.exe` / `ClassKeep-wcli.exe`，Windows 服务名为 `ClassKeepService`，
安装目录为 `%PROGRAMFILES64%\ClassKeep\Master`（教师机）与 `...\Student`（学生机）。
CMake 构建目标、核心库 `veyon-core.dll`、插件 DLL 与配置数据目录仍沿用上游标识，
详见 `NOTICE.md` §4.1。

本仓库保留了上游 Veyon 的完整提交历史（详见 `git log`）；早期留下的
`veyon/mobile/` 历史副本与废弃的 ZIP 打包脚本已清理。

---

## 2. 三类发布产物

| 产物 | 形态 | 内容 |
|---|---|---|
| **教师端** | `ClassKeep-Teacher-<ver>-win64-setup.exe` | NSIS 安装程序：组件页「服务端(必选) + 教师控制台(可选)」，自动注册并启动服务 |
| **学生端** | `ClassKeep-Student-<ver>-win64-setup.exe` | NSIS 安装程序：组件页**只有服务端(必选)**，不安装也不出现 Master |
| **APP** | `ClassKeep-Mobile-<ver>.apk` | Android 11+ 教室端（网段发现、密钥同步、班级数据、控制指令、期末表、词云） |

产物命名、内容清单、校验与安装步骤见
[`veyon/docs/CLASSKEEP-RELEASE.md`](veyon/docs/CLASSKEEP-RELEASE.md)。

> 仓库内**不提交预编译二进制**。三类产物由
> [发布工作流](.github/workflows/classkeep-release.yml) 在推送 `v*` tag 或手动触发时于云端**真实编译并发布**：
>
> | Job | 环境 | 产出 |
> |---|---|---|
> | `android` | Ubuntu 24.04 + JDK 21 | `ClassKeep-Mobile-<ver>.apk` |
> | `windows` | Windows + MSYS2 UCRT64 + Qt6/MinGW + ntldd + NSIS | 教师端 / 学生端两个 `setup.exe` |
> | `package` | Windows | 组装三类产物 + `SHA256SUMS.txt` + 发布说明 |
>
> 三类产物全部真实编译，不含占位包或伪造二进制。
> **当前最新为 [v2.0.0](https://github.com/zk26/ClassKeep/releases/tag/v2.0.0)**：
> 全量统一英文 `ClassKeep` 命名；**修复 1.5.1 升级安装时
> 「无法打开要写入的文件 … veyon-worker.exe」**——安装前先停止并清理
> 新/旧服务与全部客户端进程，等待文件句柄释放后再写入；
> 安装器仍会自检服务 RUNNING 并把返回码写入 `install.log`。
> 桌面端**默认简体中文**（Configurator → General → Language 可切换），
> 界面与中文译文中的 Veyon 品牌串统一为 ClassKeep。
> 教师端支持座位拖拽排序（下次开机不变）、监控缩略图显示姓名+当前分数、
> 学生端卸载保护、常用功能置顶的小图标工具栏；APP 为 **4 个底部标签
> （课堂 / 课表 / 配置 / 更多）**，矢量图标 + 常显名称，课表作息可调、更多页含导入导出与课堂回顾。
> 旧版 ZIP / preset 包与有缺陷的 Release 已删除。
> Windows 编译路线见 [`CLASSKEEP-BUILD.md` 路线 D](veyon/docs/CLASSKEEP-BUILD.md)。

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

### 打安装程序（有 MSYS2 UCRT64 工具链时）

```bash
bash veyon/scripts/collect-windows-runtime.sh
mkdir -p dist
{ printf '\xEF\xBB\xBF'; cat veyon/scripts/classkeep-installer.nsi; } > installer.nsi
makensis -DVERSION=2.0.0 -DROLE=teacher installer.nsi
makensis -DVERSION=2.0.0 -DROLE=student installer.nsi
```

详见 [`veyon/scripts/README.md`](veyon/scripts/README.md)。

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
| [`veyon/docs/CLASSKEEP-FEATURES.md`](veyon/docs/CLASSKEEP-FEATURES.md) | **全部功能验收总表**：两轮需求逐条对照、实现位置、使用入口、未完成项 |
| [`veyon/docs/CLASSKEEP.md`](veyon/docs/CLASSKEEP.md) | 功能总览、架构通道图、目录结构、构建、数据流 |
| [`veyon/docs/CLASSKEEP-BUILD.md`](veyon/docs/CLASSKEEP-BUILD.md) | **真实编译方案**：GitHub Actions 编译验证、本机 MinGW 编译、CI 出 Windows 产物、排错表 |
| [`veyon/docs/CLASSKEEP-PROTOCOL.md`](veyon/docs/CLASSKEEP-PROTOCOL.md) | **协议规范**：命令/参数全量数值表、HTTP 路由、认证算法、UDP 发现、数据格式、兼容性铁律 |
| [`veyon/docs/CLASSKEEP-DEPLOY.md`](veyon/docs/CLASSKEEP-DEPLOY.md) | 部署、每堂课流程、UWF 与 Defender 注意、数据迁移、排障 |
| [`veyon/docs/CLASSKEEP-RELEASE.md`](veyon/docs/CLASSKEEP-RELEASE.md) | 发布产物清单、命名、校验、安装 |
| [`veyon/docs/CLASSKEEP-UI.md`](veyon/docs/CLASSKEEP-UI.md) | APP 交互与视觉规范（去阴影、同心圆角、动效顺序、平板适配验收清单） |
| [`mobile/README.md`](mobile/README.md) | Android 教室端工程说明 |
| [`veyon/plugins/classroom/README.md`](veyon/plugins/classroom/README.md) | 课守插件源码结构 |
| [`veyon/scripts/README.md`](veyon/scripts/README.md) | 依赖闭包收集、NSIS 安装器与图标生成脚本 |

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

- Android 端：本机与 GitHub Actions 均实测通过（`assembleDebug` + `testDebugUnitTest`）。
- 桌面端 Linux/Qt6/GCC：**ClassKeep Build Check 全绿**，`classroom.so` 编译链接成功。
- 桌面端 Windows/Qt6/MinGW：**ClassKeep Windows Build 全绿**，`classroom.dll` 与
  `ClassKeep-master.exe` / `ClassKeep-service.exe` 等全部编译成功，UWF / Core Audio / WFP 专属代码已过编译器。
- 仍未完成：在真实 UWF 学生机上的**安装联调**与功能验证（需实际机房环境）。
- 编译或部署如遇报错，请通过仓库 Issues 反馈。

---

GitHub：<https://github.com/zk26/ClassKeep> · 上游：<https://github.com/veyon/veyon>
许可与归属疑问见 [`NOTICE.md`](NOTICE.md)。
