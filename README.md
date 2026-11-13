# ikeback

> 基于 [Veyon](https://veyon.io/) 4.x 的教学管理定制分支。**产品名统一为英文小写 `ikeback`**。
> 仓库：<https://github.com/zk26/ikeback> · 许可：**GPL-2.0-or-later**
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
├── veyon\               桌面端（上游 Veyon 源码 + ikeback 插件）
│   ├── README.md         桌面端视角的总入口
│   ├── COPYING           上游 GPL v2 全文
│   ├── plugins\classroom\ ikeback 插件源码及说明
│   ├── docs\             全套协议与说明文档
│   ├── scripts\          Windows 依赖收集、NSIS 安装器、图标生成脚本
│   └── core\, master\, server\  对上游的最小扩展点
│
└── mobile\              ← Android 教室端 App（**唯一工程**，Android Studio 打开此目录）
```

`ikeback_logo.png` 是唯一的品牌图标来源，桌面端 `.ico` 与 Android `mipmap` 图标均由它生成。

**2.0.0 命名**：所有进程均为 `ikeback-master.exe` / `ikeback-server.exe` /
`ikeback-worker.exe` / `ikeback-service.exe` / `ikeback-configurator.exe` /
`ikeback-cli.exe` / `ikeback-wcli.exe`，Windows 服务名为 `ikebackService`，
安装目录为 `%PROGRAMFILES64%\ikeback\Master`（教师机）与 `...\Student`（学生机）。
CMake 构建目标、核心库 `veyon-core.dll`、插件 DLL 与配置数据目录仍沿用上游标识，
详见 `NOTICE.md` §4.1。

本仓库保留了上游 Veyon 的完整提交历史（详见 `git log`）；早期留下的
`veyon/mobile/` 历史副本与废弃的 ZIP 打包脚本已清理。

---

## 2. 三类发布产物

| 产物 | 形态 | 内容 |
|---|---|---|
| **教师端** | `ikeback-teacher-<ver>-win64-setup.exe` | NSIS 安装程序：组件页「服务端(必选) + 教师控制台(可选)」，自动注册并启动服务 |
| **学生端** | `ikeback-student-<ver>-win64-setup.exe` | NSIS 安装程序：组件页**只有服务端(必选)**，不安装也不出现 Master |
| **APP** | `ikeback-Mobile-<ver>.apk` | Android 11+ 教室端（网段发现、密钥同步、班级数据、控制指令、期末表、词云） |

产物命名、内容清单、校验与安装步骤见
[`veyon/docs/IKEBACK-RELEASE.md`](veyon/docs/IKEBACK-RELEASE.md)。

> 仓库内**不提交预编译二进制**。三类产物由
> [发布工作流](.github/workflows/ikeback-release.yml) 在推送 `v*` tag 或手动触发时于云端**真实编译并发布**：
>
> | Job | 环境 | 产出 |
> |---|---|---|
> | `android` | Ubuntu 24.04 + JDK 21 | `ikeback-Mobile-<ver>.apk` |
> | `windows` | Windows + MSYS2 UCRT64 + Qt6/MinGW + ntldd + NSIS | 教师端 / 学生端两个 `setup.exe` |
> | `package` | Windows | 组装三类产物 + `SHA256SUMS.txt` + 发布说明 |
>
> 三类产物全部真实编译，不含占位包或伪造二进制。
> **当前版本 5.2.0（首个 ikeback 正式版）**：改名后的全部旧构建（ClassKeep 时代）
> 一律作废，请只使用 5.2.0 及以后的产物。
> **教师端工具栏**：常用工具（广播 / 边看边做 / 停止广播、客户端保护开关、
> UWF 开关、网关开关、肃静页、禁用应用/网址、全班加减分、静音）不再是下拉菜单里的
> 二级项，而是与「屏幕演示」「远程控制」同级的一级按钮，放不下时自动换行成两列。
> **名单批量导入**：可直接粘贴 Excel/WPS 选中区域，或导入带「班级」列的 CSV，
> 一次完成「班级 + 座位号 + 姓名 + IP」，并内置模板下载；
> 同一班级内的重复座位号、重复 IP 会被明确拒绝（不同班级可用相同 IP）。
> **按 IP 快速登录**会把学生真正写入名单表格，不再“输入了却看不到”。
> **连通性自检**：系统与设备页可一键检测本机 ikeback Server 与学生机可达性、
> 选择正确的网卡地址、重启 ikeback 服务；安装器额外校验
> `ikeback-server.exe` 是否真的起来（失败会自动重启尝试一次）并写入 `install.log`，
> 同时放行手机桥接端口 11880（TCP+UDP）。
> **手机端**：所有指令/同步/读取名单按钮点击即有 Toast 反馈（处理中、成功、失败、
> 未配置时直接跳到配置页），不再出现“点了没反应”。
> 桌面端**默认简体中文**（ikeback 插件界面串全部补齐 `veyon_zh_CN.ts`）。
> APP 为 **5 个底部标签（课堂 / 班级 / 课表 / 配置 / 更多）**：真实应用图标、
> 未配置时收缩提示、ClassBoard 样式双作息课表（彩色跨节卡片 + 今天高亮）、
> 按班级自然展示课堂记录、导入导出收进设置、课表本地 + 教师机双写同步。
> 旧版 ZIP / preset 包与有缺陷的 Release 已删除。
> Windows 编译路线见 [`IKEBACK-BUILD.md` 路线 D](veyon/docs/IKEBACK-BUILD.md)。

---

## 3. 快速开始

### 桌面端（**只能 MinGW-w64/GCC**，MSVC 不可行）

完整方案见 [`veyon/docs/IKEBACK-BUILD.md`](veyon/docs/IKEBACK-BUILD.md)。
本机装 Qt6-MinGW + 自编 QCA-Qt6 + LibVNC 等依赖较重；**推荐先推代码到 `main`，
由 [GitHub Actions 编译工作流](.github/workflows/ikeback-build.yml) 做真实编译验证**
（<https://github.com/zk26/ikeback/actions> → ikeback Build Check → Run workflow）。

```powershell
# 有完整 MinGW 工具链时（简写，详见 IKEBACK-BUILD.md）
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
{ printf '\xEF\xBB\xBF'; cat veyon/scripts/ikeback-installer.nsi; } > installer.nsi
makensis -DVERSION=2.2.0 -DROLE=teacher installer.nsi
makensis -DVERSION=2.2.0 -DROLE=student installer.nsi
```

详见 [`veyon/scripts/README.md`](veyon/scripts/README.md)。

---

## 4. 文档索引（全部说明文件）

### 法律与合规（部署前请先读）

| 文档 | 内容 |
|---|---|
| [`LICENSE`](LICENSE) | GNU GPL v2 全文 |
| [`NOTICE.md`](NOTICE.md) | 版权归属、许可、修改声明、商标、隐私与无担保声明 |
| [`veyon/docs/IKEBACK-LICENSE.md`](veyon/docs/IKEBACK-LICENSE.md) | 衍生作品与分发义务详解 |
| [`veyon/docs/IKEBACK-THIRD-PARTY.md`](veyon/docs/IKEBACK-THIRD-PARTY.md) | 第三方组件与许可清单、分发核对清单 |
| [`veyon/docs/IKEBACK-PRIVACY.md`](veyon/docs/IKEBACK-PRIVACY.md) | **数据采集范围与合规义务**（监控类软件必读） |

### 技术与使用

| 文档 | 内容 |
|---|---|
| [`veyon/README.md`](veyon/README.md) | 桌面端仓库首页总入口 |
| [`veyon/docs/IKEBACK-FEATURES.md`](veyon/docs/IKEBACK-FEATURES.md) | **全部功能验收总表**：两轮需求逐条对照、实现位置、使用入口、未完成项 |
| [`veyon/docs/IKEBACK.md`](veyon/docs/IKEBACK.md) | 功能总览、架构通道图、目录结构、构建、数据流 |
| [`veyon/docs/IKEBACK-BUILD.md`](veyon/docs/IKEBACK-BUILD.md) | **真实编译方案**：GitHub Actions 编译验证、本机 MinGW 编译、CI 出 Windows 产物、排错表 |
| [`veyon/docs/IKEBACK-PROTOCOL.md`](veyon/docs/IKEBACK-PROTOCOL.md) | **协议规范**：命令/参数全量数值表、HTTP 路由、认证算法、UDP 发现、数据格式、兼容性铁律 |
| [`veyon/docs/IKEBACK-DEPLOY.md`](veyon/docs/IKEBACK-DEPLOY.md) | 部署、每堂课流程、UWF 与 Defender 注意、数据迁移、排障 |
| [`veyon/docs/IKEBACK-RELEASE.md`](veyon/docs/IKEBACK-RELEASE.md) | 发布产物清单、命名、校验、安装 |
| [`veyon/docs/IKEBACK-UI.md`](veyon/docs/IKEBACK-UI.md) | APP 交互与视觉规范（去阴影、同心圆角、动效顺序、平板适配验收清单） |
| [`mobile/README.md`](mobile/README.md) | Android 教室端工程说明 |
| [`veyon/plugins/classroom/README.md`](veyon/plugins/classroom/README.md) | ikeback 插件源码结构 |
| [`veyon/scripts/README.md`](veyon/scripts/README.md) | 依赖闭包收集、NSIS 安装器与图标生成脚本 |

---

## 5. 关键约束

- 学生机须为 **Windows 10/11 企业版、教育版或 IoT 企业版**（UWF 仅这些版本支持）。
- 只用系统自带签名 API（`uwfmgr.exe` / `netsh.exe` / Core Audio COM / SetupAPI），
  不写内核驱动、不做进程注入、不落临时脚本，Windows Defender 友好。
- 课堂密钥每堂课轮换，同时用于学生端认证与手机端 Bridge 认证；密钥永不上网，只参与本地摘要。
- 学生端默认零落盘，全部行为数据集中到教师机，按主机/日期/学生分别保存。
- **本软件会采集学生行为数据**，部署方须先阅读 `IKEBACK-PRIVACY.md` 并履行告知、最小必要、
  访问控制与定期删除义务；项目作者不接触任何实际数据，也不对部署行为承担责任。

---

## 6. 状态

- Android 端：本机与 GitHub Actions 均实测通过（`assembleDebug` + `testDebugUnitTest`）。
- 桌面端 Linux/Qt6/GCC：**ikeback Build Check 全绿**，`classroom.so` 编译链接成功。
- 桌面端 Windows/Qt6/MinGW：**ikeback Windows Build 全绿**，`classroom.dll` 与
  `ikeback-master.exe` / `ikeback-service.exe` 等全部编译成功，UWF / Core Audio / WFP 专属代码已过编译器。
- 仍未完成：在真实 UWF 学生机上的**安装联调**与功能验证（需实际机房环境）。
- 编译或部署如遇报错，请通过仓库 Issues 反馈。

---

GitHub：<https://github.com/zk26/ikeback> · 上游：<https://github.com/veyon/veyon>
许可与归属疑问见 [`NOTICE.md`](NOTICE.md)。
