# 课守 ClassKeep 部署与使用说明

仓库：<https://github.com/zk26/ClassKeep> · 许可 **GPL-2.0-or-later**。
本文件只讲部署与操作；许可见 [`CLASSKEEP-LICENSE.md`](CLASSKEEP-LICENSE.md) 与 [`../../NOTICE.md`](../../NOTICE.md)，
第三方依赖见 [`CLASSKEEP-THIRD-PARTY.md`](CLASSKEEP-THIRD-PARTY.md)。

> ⚠️ **部署前置条件**：本软件会采集学生行为数据。部署方必须先阅读
> [`CLASSKEEP-PRIVACY.md`](CLASSKEEP-PRIVACY.md)，完成告知/同意、目的限定、访问控制与保存期限评估，
> 并确认符合所在地法律与学校制度。项目作者不接触实际数据，也不对部署行为承担责任。

## 1. 环境要求

| 角色 | 要求 |
|---|---|
| 教师机 | Windows 10/11，Veyon Master + Configurator + WebAPI（可选），Qt 运行时 |
| 学生机 | **Windows 10/11 企业版、教育版或 IoT 企业版**（UWF 仅这三个版本有）；使用超级管理员账户登录并启用 UWF 还原 |
| 杀毒 | 仅 Windows Defender（原生）即可。本分支不写内核驱动、不做进程注入、不在 `Temp` 落临时脚本、不装自签名根证书 |
| 手机 | Android 11+（minSdk 30），与教师机同一局域网、可发/收 UDP 广播 |
| 网络 | 教师机 `bridgePort`（默认 TCP+UDP `11880`）放行；Veyon 原有控制端口正常 |

> 关闭 UWF 后安装软件必须**重启生效**；「关机再开机」会因 Windows 快速启动而不应用排队配置。

---

## 2. 构建与安装

### 桌面端

桌面端**只能用 MinGW-w64 / GCC 编译**（Veyon CMake 硬编码 GCC 参数，MSVC 不可行）。
完整依赖与命令见 [`CLASSKEEP-BUILD.md`](CLASSKEEP-BUILD.md)。最简形式：

```bash
cmake -S veyon -B build -G Ninja -DCMAKE_PREFIX_PATH=/ucrt64 \
  -DWITH_QT6=ON -DWITH_WEBAPI=OFF -DWITH_CORE_ONLY=OFF
cmake --build build --target classroom veyon-master veyon-configurator \
                    veyon-cli veyon-wcli veyon-service veyon-server veyon-worker
bash veyon/scripts/collect-windows-runtime.sh
```

> 必须显式列出目标，**不要**跑全量 `cmake --build build`：
> 上游 `plugins/filetransfer` 在 Qt5/Qt6 下都编不过（缺 `<QUuid>`、`qApp` 类型）。
> 详见 [`CLASSKEEP-BUILD.md`](CLASSKEEP-BUILD.md)。

若只想先验证代码能否编译，直接用 GitHub Actions 的 **ClassKeep Build Check**（Linux/Qt6/GCC），
无需在本机装整套 MinGW 工具链。

### 安装（推荐使用 Release 里的安装程序）

1. **学生机**：以管理员运行 `ClassKeep-Student-<ver>-win64-setup.exe`。组件页**只有
   「服务端（必选）」**，不会安装也不会出现 Master；安装器会自动注册并启动
   Veyon Service、放行防火墙、启用 SAS。
2. **教师机**：以管理员运行 `ClassKeep-Teacher-<ver>-win64-setup.exe`，组件页另含可选项
   「教师控制台」（`veyon-master.exe`），保持勾选。
3. 两端再分别用 `veyon-configurator.exe` 完成 Veyon 认证配置（教师机导出、学生机导入）。

安装器沿用上游 `nsis/veyon.nsi.in` 的安装流程；`classroom.dll` 等插件安装在
`plugins/` 子目录，运行时 DLL 由 `ntldd` 递归收集，**不要**自己手工拷贝 DLL 白名单。
Veyon Service 以 `LocalSystem` 运行，因此 `uwfmgr.exe`、`netsh.exe`、`shutdown.exe` 无需额外提权或 UAC。

### 手机端

```powershell
cd mobile
.\gradlew.bat :app:assembleDebug
```

生成的 APK 在 `app/build/outputs/apk/debug/`。首次用 Android Studio 打开时会生成 `local.properties`（`sdk.dir`），该文件**不应提交**。

---

## 3. 学生机首次准备

1. 安装带课守插件的 Veyon，确认 `VeyonService` 正在运行。
2. 用 Veyon Configurator 打开「课守 ClassKeep」页，按需设置：
   - 退出保护 / 退出需超管账号 / 超管账户名
   - 允许学生自助登录
   - AI 接口地址与模型
   - 行为采集开关与采样间隔
   - **Bridge 端口与开关（教师机才需要开）**
3. 确认 UWF 保护卷包含系统盘；给需要持久化的目录（如 `D:\作业`）加 `uwfmgr file add-exclusion`。
4. 重启，确认学生机右上角出现「课守 ClassKeep」悬浮条。

> 学生机**默认零落盘**：行为事件只通过 `ReportEvent` 上报到教师机。教师机未连接时，服务端内存队列最多缓存 5000 条，连接后自动补传；学生机重启会清空该队列。

---

## 4. 教师机每堂课流程

1. 打开 Veyon Master，点工具栏「课守 ClassKeep」。
2. 在「课堂与登录」页选择班级；名单可直接编辑 IP，或用 CSV 导入（列：`seat,name,ip`）。
3. 点 **开始新课堂**：
   - 生成新 `sessionId` 与 16 位密钥
   - 强制退出上一堂课所有学生
   - 推送设置、名单、密钥
   - **Bridge 旧认证令牌立即失效**
4. 「按 IP 一键登录」或逐台「快速登录」；名单外的 IP 会自动临时直连，也可点「同步 IP 到目录」写入 Veyon 电脑目录（需教师机有写全局配置权限）。
5. 学生也可在自己机器上输入「座位号 + 姓名 + 本堂课密钥」自助认证。
6. 上课期间可随时：静音/断网/USB 拦截/肃静页/Ctrl+双击远控/分组投屏/多选控屏，学生可「呼叫老师」。
7. 「积分与报表」页每日录入加扣分（同一节课多次加分合并进同一格）、编辑富文本明细、发布规则、生成每生词云、期末导出 `.xls`/CSV。

### 手机端

1. 教师机上「系统与设备」页勾选 **Start mobile bridge**（默认端口 11880）。
2. App 会自动在网段内发现教师机；**找到才同步，找不到不操作**。
3. 首次需在 App 输入**本堂课密钥**（与学生端完全相同）和教师机 IP/端口（自动发现会填好）。
4. 之后每次打开 App 自动同步；也可点「立即同步」。密钥换课后需重新输入，App 会明确提示「密钥被拒绝」。
5. 「导出期末表」写入 App 私有文档目录；「查看词云」用内嵌 WebView 展示 SVG。

---

## 5. UWF 与远程装软件

「系统与设备」页的 UWF 是**分步式**（保持克制，不做隐式状态机）：

```
查询状态 → 关闭还原 + 重启 → 确认每台已真正关闭 → 分发并安装 → 开启还原 + 重启
```

- 「查询」会向所有学生机下发 `UwfQuery`，状态以 `uwf` 事件写进教师机行为日志；请在日志页确认**每台**保护都已关闭再安装。
- 「分发并安装」通过 Veyon `StartApp` Feature 执行静默参数（MSI `/quiet`、Inno `/VERYSILENT`、NSIS `/S` 等自动识别）。
- **保护没真正关闭就安装，内容会全写进 UWF 缓存 → 缓存写满 → 重启黑屏**；这是 UWF 最常见的严重故障。
- **维修模式（`servicing`）只给 Windows 更新用**，进出会自动重启，装软件禁止使用。
- 缓存建议 DISK ≥ 8192 MB，阈值 75% / 95%；RAM/DISK 两套都要配好。
- 装完**必须恢复还原**，否则机器会一直不还原。

断网功能用 `netsh advfirewall`（Windows 防火墙 = WFP 用户态配置接口）加出站阻断规则，并自动为教师机 IP 加放行例外，避免把控制通道一起断掉。

---

## 6. 数据位置与迁移

数据主库：教师机 `%APPDATA%/ClassKeep/`。

```
settings.json            配置（密钥加密）
classes.json             班级名单
scores/<班级>.json       积分册
teacher/events/          汇总事件（按天）
teacher/machines/<主机>/ 每台学生机独立日志
teacher/students/        每生独立日志
```

> 本分支早期版本使用 `%APPDATA%/Classroom`。若已有数据，把该目录**重命名为 `ClassKeep`** 即可，内部结构与字段名未变。

日志保留天数默认 180 天，教师端「行为日志」页可手动清理。

---

## 7. 故障排查

| 现象 | 检查 |
|---|---|
| 学生端悬浮条不出现 | Veyon Service 是否运行；`enabled` 是否为 true；Server 日志里的 watchdog 与 worker 启动记录 |
| 学生无法自助登录 | 教师端是否已「开始新课堂」生成密钥；密钥是否与学生端一致；名单座位号/姓名是否匹配 |
| 手机端一直 401 | 密钥是否换课；教师机是否生成过密钥；Bridge 状态是否显示 `lesson key active`；60 秒内失败 10 次会锁 60 秒 |
| 手机端找不到教师机 | 是否同一网段、UDP 广播是否被 AP 隔离、教师机防火墙是否放行 UDP 11880；可直接手填 IP 并点「立即同步」 |
| 行为日志缺数据 | 教师机是否在线（离线时靠服务端补传队列）；`monitoringEnabled` 是否开启；采样间隔 |
| 导出 `.xls` 打不开 | 用 Excel/WPS 打开；文件为 Excel 2003 XML（SpreadsheetML），不是二进制 xls |
| UWF 命令无效 | 系统版本是否企业版/教育版/IoT；是否重启；`uwfmgr get-config` 输出与日志里的 `uwf` 事件 |

---

## 8. 安全建议

- 课堂密钥每堂课自动轮换；不要长期复用，不要把密钥写进公开的名单 CSV。
- Bridge 认证已启用（挑战应答 + 失败限流），但仍建议在**受控教学网段**使用，不要把 11880 端口映射到公网。
- `aiApiKey` 与 `sessionKey` 在 `settings.json` 里是加密的，但教师机本身需按普通管理员权限保护。
- 行为日志包含学生上网/窗口标题信息，属于敏感数据；按学校规定设置保留天数并限制访问。
- 本仓库不是 git 上游官方仓库，推送前请确认 remote 指向自己的 fork（见 `docs/CLASSKEEP-LICENSE.md` 的仓库说明）。
