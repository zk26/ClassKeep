# 课守 ClassKeep 协议规范

**协议版本：`1`（随本分支首次发布）**
仓库：<https://github.com/zk26/ClassKeep> · 许可 **GPL-2.0-or-later**（`SPDX-License-Identifier: GPL-2.0-or-later`）。

权威定义：`veyon/plugins/classroom/ClassroomProtocol.h`、`ClassroomBridge.cpp`、
`ClassroomSecrets.cpp`，以及 `mobile/app/src/main/java/top/ikee002/classkeep/Announcement.kt`。

> 本文件是**技术通信协议**（命令字、路由、认证、数据格式），不是软件许可协议。
> 软件许可与分发义务见 [`CLASSKEEP-LICENSE.md`](CLASSKEEP-LICENSE.md) 与 [`../../NOTICE.md`](../../NOTICE.md)；
> 其中描述的采集字段所对应的数据合规要求见 [`CLASSKEEP-PRIVACY.md`](CLASSKEEP-PRIVACY.md)。
> 第三方组件许可见 [`CLASSKEEP-THIRD-PARTY.md`](CLASSKEEP-THIRD-PARTY.md)。

> ### 兼容性铁律
>
> `Command` 与 `Argument` 的值会被转成十进制字符串作为 `FeatureMessage` 的参数键
> （`FeatureMessage::addArgument()` → `QString::number(int(index))`）。因此**枚举数值就是线上协议**。
>
> - **只允许在枚举末尾追加新值**，禁止插入、删除、重排或显式改数值。
> - 桌面端与手机端必须同时升级；旧端收到未知命令时 `handleFeatureMessage()` 返回 `false`，不得假装成功。
> - 新增 HTTP 路由可以追加，已有路由的路径、方法与字段含义不得变更。
> - 更改 `announcementPrefix()` / `discoveryMagic()` / 认证摘要公式属于破坏性变更，需提升协议版本。

---

## 1. Feature UIDs

| 名称 | UUID | 说明 |
|---|---|---|
| `pluginUid()` | `d3f1c0de-7a41-4b8e-9c12-000000000000` | 插件标识 |
| `consoleFeatureUid()` | `d3f1c0de-7a41-4b8e-9c12-000000000001` | 教师端工具栏「课守 ClassKeep」按钮（`Flag::Action \| Master`） |
| `clientFeatureUid()` | `d3f1c0de-7a41-4b8e-9c12-000000000002` | 学生端常驻 worker 通道（`Flag::Session \| Service \| Worker`） |

复用的上游 Feature UID：

| 用途 | UID |
|---|---|
| `InputDevicesLock`（禁 USB 输入，桌面可见） | `e4a77879-e544-4fec-bc18-e534f33b934c` |
| `RemoteControl` | `ca00ad68-1709-4abe-85e2-48dff6ccf8a2` |
| `FullScreenDemo`（demo client meta） | `7b6231bd-eb89-45d3-af32-f70663b2f878` |
| `Demo`（demo mode） | `6f4cd922-b63e-40bf-9451-536065c7cdf9` |
| `ShareOwnScreenFullScreen`（真·全屏广播） | `07b375e1-8ab6-4b48-bcb7-75fb3d56035b` |
| `ShareOwnScreenWindow`（真·窗口演示 / 边看边做） | `68c55fb9-127e-4c9f-9c90-28b998bf1a47` |
| `DemoAllScreens`（**仅选项，不启动演示**） | `2aca1e9f-25f9-4d0f-9729-01b03c80ab28` |
| `StartApp` | `da9ca56a-b2ad-4fff-8f8a-929b2927b442` |
| `DistributeFiles` | `4a70bd5a-fab2-4a4b-a92a-a1e81d2b75ed` |

> **2.0.0 修正**：旧版「广播」按钮对 `DemoAllScreens` 调 `controlFeature()`，
> 该 UID 只是多屏选项、`DemoFeaturePlugin::controlFeature()` 并不处理，实际什么也没发生。
> 现在通过 `FeatureManager::startFeature()` 启动 `ShareOwnScreenFullScreen` /
> `ShareOwnScreenWindow`，再叠加 `InputDevicesLock`（**桌面仍可见**，只禁键鼠输入）。

---

## 2. FeatureMessage 命令表

`M` = 教师机 Veyon Master，`S` = 学生机 Veyon Server，`W` = 学生机 Veyon Worker。

| 值 | Command | 方向 | 说明 | 主要参数 |
|---:|---|---|---|---|
| 1 | `StartClient` | S→W | 服务启动时拉起常驻 worker | `SettingsJson`、`SessionKey`、`SessionId` |
| 2 | `StopClient` | M→S→W | 停止 worker（同时暂停看门狗） | — |
| 3 | `SetSettings` | M→S→W | 下发全部设置 JSON | `SettingsJson` |
| 4 | `SetRoster` | M→S→W | 下发班级名单 JSON | `RosterJson` |
| 5 | `SetSessionKey` | M→S→W | 会话心跳（**2.3.0 起只传布尔，不再下发密钥**；枚举号保持不变） | `KeyConfigured`、`SessionId` |
| 6 | `SetIdentity` | M→S→W | 教师按 IP 直接登录学生 | `ClassName`、`Seat`、`Student`、`SessionId` |
| 7 | `ClearIdentity` | M→S→W | 清除学生身份 | — |
| 8 | `ForceLogout` | M→S→W | 强制退出并解除客户端锁定 | `Reason` |
| 9 | `SwitchStudent` | M→S→W | 换学生 | `NewSeat`、`NewStudent`、`SessionId` |
| 10 | `LockClient` | M→S→W | 锁定客户端 | `Enabled` |
| 11 | `UnlockClient` | M→S→W | 解锁客户端 | `Enabled` |
| 12 | `ToggleAi` | M→S→W | 开关学生端 AI | `Enabled` |
| 13 | `ReportEvent` | W→S→M | **行为/状态事件上报**（教师机离线时服务端缓存补传） | `Timestamp`、`EventType`、`Detail`、`App`、`Host`、`Ip`、`SessionId`、`ClassName`、`Seat`、`Student` |
| 14 | `ReportIdentity` | 保留 | 身份上报（当前未使用） | — |
| 15 | `RequestQuit` | W→S | 凭据验证通过后请求退出 worker | — |
| 16 | `Ping` | 保留 | 心跳占位 | — |
| 17 | `SetScoreRules` | 保留 / Bridge | 规则下发入口（当前由 `ScoreReply` 承载） | `RulesJson` |
| 18 | `QueryScore` | W→S→M | 学生端请求积分与明细 | `ClassName`、`Seat`、`Student`、`Host`、`Ip` |
| 19 | `ScoreReply` | M→S→W | 富文本积分页 HTML（规则未发布时不含规则区块） | `RichText` |
| 20 | `CallTeacher` | W→S→M | 学生呼叫老师 | `ClassName`、`Seat`、`Student`、`Host`、`Ip`、`Message` |
| 21 | `DismissCall` | 保留 | 清除呼叫提示 | `CallId` |
| 22 | `ShowSilencePage` | M→S→W | 全屏肃静页 | `Message`、`Enabled` |
| 23 | `HideSilencePage` | M→S→W | 收起肃静页 | `Enabled` |
| 24 | `SetMute` | M→S→W | Core Audio 静音/恢复 | `Enabled` |
| 25 | `SetInternetBlock` | M→S→W | 防火墙出站阻断/恢复 | `Enabled`、`Ip`（教师机 IP 例外） |
| 26 | `SetUsbInputBlock` | 保留 | 当前走上游 `InputDevicesLock` Feature | `Enabled` |
| 27 | `StartRemoteControl` | 保留 | 当前走上游 `RemoteControl` Feature / Ctrl+双击 | `TargetIps` |
| 28 | `StartGroupDemo` | 保留 | 当前走上游 Demo Feature | `GroupId` |
| 29 | `StartMultiDemo` | 保留 | 多选控屏 | `TargetIps` |
| 30 | `UwfQuery` | M→S→W | 查询 UWF 状态，结果以 `uwf` 事件回报 | — |
| 31 | `UwfSetFilter` | M→S→W | `uwfmgr filter enable/disable`（重启生效） | `Enabled` |
| 32 | `UwfRestart` | M→S→W | 重启学生机 | — |
| 33 | `UwfInstall` | 保留 | 当前走 `StartApp` Feature 执行静默安装 | `SoftwarePath`、`SoftwareArgs` |
| 34 | `UwfStatusReport` | W→S→M | UWF 状态回报（当前由 `ReportEvent` 承载） | `UwfStateJson` |
| 35 | `SyncRequest` | 保留 | Bridge 层同步走 HTTP，不经此通道 | `Revision` |
| 36 | `SyncReply` | 保留 | 同上 | `SyncDataJson` |
| 37 | `StartCheckIn` | M→S→W | 教师端「开始签到」，学生机弹出姓名输入框 | `ClassName` |
| 38 | `CheckInReply` | W→S→M | 学生提交姓名，教师端按「座位号 → 名单姓名」比对 | `ClassName`、`Seat`、`Student`、`CheckInName`、`Host`、`Ip` |
| 39 | `SetUninstallProtection` | M→S→W | 教师端开启/解除学生端**卸载**保护 | `Enabled` |
| 40 | `SetExitProtection` | M→S→W | **客户端保护**：禁止学生自行退出 ClassKeep worker / 悬浮卡片 | `Enabled` |
| 41 | `OpenSoftware` | M→S→W | 在学生机打开指定程序 | `SoftwarePath`、`SoftwareArgs` |
| 42 | `OpenWebsite` | M→S→W | 在学生机打开指定网址 | `Message`(URL) |
| 43 | `SetAppBlockRules` | M→S→W | 禁用应用（模糊匹配规则数组） | `RulesJson` |
| 44 | `SetWebBlockRules` | M→S→W | 禁用地址（模糊匹配规则数组） | `RulesJson` |
| 45 | `ProtectionStateReport` | W→S→M | 客户端保护状态回报 | `Enabled` |
| 46 | `ApplySession` | M→S→W | 合并的设置 / 名册 / 密钥 / 会话号，替代旧的三连发 | `SettingsJson`、`RosterJson`、`SessionKey`、`SessionId` |

> 新增命令一律追加在枚举末尾，因为枚举数值就是线上协议；旧版 worker 收到
> 未知命令时走 `default: return false`，不会崩溃。

### 2.2.0 新增 Feature（工具栏）

| Feature | UID 末段 | Flag | 行为 |
|---|---|---|---|
| `ClassroomSignIn` | `...0003` | `Action\|Master` | 弹出班级 + 姓名 + 座位号对话框，下发 `SetIdentity`；**不使用 Windows 用户名/密码** |
| `ClassroomSignOut` | `...0004` | `Action\|Master` | 下发 `ForceLogout` |

`MainWindow::addFeaturesToToolBar()` 过滤掉上游 `UserLogin` / `UserLogoff` 两个按钮，
因此教师端只暴露 ClassKeep 的姓名+座位号登录入口。上游 `usersessioncontrol` 插件
仍参与编译，但其 `prepareLogon()` / 密码加密路径不再被调用。

### 手机端 ↔ 教师机 HTTP 路由（2.1.0 新增）

| 路径 | 方法 | 鉴权 | 用途 |
|---|---|---|---|
| `/api/classroom/timetable` | GET | 需要 | 读取教师机 `%APPDATA%/ClassKeep/timetable.json`，返回 `{"timetable": {...}, "revision": n}`；从未上传时 `timetable` 为空对象 |
| `/api/classroom/timetable` | POST | 需要 | 请求体 `{"timetable": {...}}`，原样写入上述文件并返回 `{"ok": true, "revision": n}` |
| `/api/classroom/classes` | GET | 需要 | 按当前班级返回名单与当前积分，供手机端「班级」页按班归档 |

课表 JSON 采用 ClassBoard 单文档格式：`name`、`timeLabels`（0 或 2 个）、
`periods[{label, times[], isBreak}]`、`courses[{name, dayOfWeek, startSection,
endSection, colorIndex, room, teacher, note, className}]`，外加本分支的
`config`（节数 / 大课间 / 午休 / 双作息偏移）与 `revision`。
教师机端「课表与课堂」页签读写同一文件，两端始终一致。

> `SetUninstallProtection` 收到后，**运行在登录用户交互会话中的**学生端 worker
> （不是 `LocalSystem`）尝试写 `HKLM\SOFTWARE\ClassKeep\Student\UninstallProtection` = 1/0。
> 写入后读回 DWORD 并核对 `QSettings::status()`，失败会写一条活动日志并以
> `ProtectionStateReport{failed}` 回报教师端——不会再静默显示「已开启」。
> 因此该保护只在登录账户具备管理员权限时生效。独立的 `uninstall.exe` 启动时读同一 DWORD，
> 为 1 则弹提示并 `Abort`；卸载器不联网，教师端离线时保护仍然有效。

> ### 三种「保护」互不相同，请勿混淆
>
> | 名称 | 开关 | 作用 |
> |---|---|---|
> | **客户端保护** | `SetExitProtection` / `exitProtection` | 防止学生端**进程/悬浮卡片**被自行退出；开启时关闭请求被 `closeEvent()` 拒绝，1.5s 守护定时器会把它重新显示；**关闭保护后允许隐藏卡片**（worker 仍由服务看门狗保活） |
> | **卸载保护** | `SetUninstallProtection` | 防止学生把安装好的 ClassKeep **卸载**掉 |
> | **UWF 还原** | `UwfSetFilter` / `UwfQuery` / `UwfRestart` | **系统级写过滤**，重启后还原；用于防学生机改动后掉线，以及**安装软件前先关闭** |
>
> 启用/关闭 UWF 只改过滤器状态（重启生效），工具面板**不会自动重启**，
> 便于教师先关闭 UWF → 安装软件 → 再启用 UWF。需要重启时单独点「重启学生机」。
>
> **模糊匹配规则**：`SetAppBlockRules` / `SetWebBlockRules` 下发 `QStringList` JSON 数组，
> 规则为不区分大小写的子串。学生端 `ClassroomMonitor` 每次采样前台进程名 / 浏览器窗口标题，
> 命中即以 `app-block` / `web-block` 事件上报教师机，并用用户态 `taskkill` 结束该进程
> （无驱动、无注入）。规则同时持久化在教师机设置 `blockedApps` / `blockedWebsites`。

> 标注「保留」的命令已在协议中占位但尚未接线；收到时 `default` 分支返回 `false`，不会静默吞掉。
>
> **签到判定规则**（全部在教师机完成，学生机不判断对错）：名单里每个座位，
> `CheckInName == 名单姓名` → 已到；有回复但不相等 → 姓名不符；无回复 → 未到。
> 每次 `StartCheckIn` 清空上一轮结果；换班也清空。所有回复同时写入 `check-in` 行为日志。

---

## 3. Argument 参数表

参数键为 `QString::number(值)`，值就是下表序号。类型以 `QVariant` 承载。

| 值 | Argument | 类型 | 值 | Argument | 类型 |
|---:|---|---|---:|---|---|
| 0 | `SettingsJson` | QString(JSON) | 25 | `RichText` | QString(HTML) |
| 1 | `RosterJson` | QString(JSON) | 26 | `ImageRefs` | QString |
| 2 | `SessionKey` | QString | 27 | `Author` | QString |
| 3 | `ClassName` | QString | 28 | `BaseScore` | int |
| 4 | `Seat` | QString | 29 | `Published` | bool |
| 5 | `Student` | QString | 30 | `TotalScore` | int |
| 6 | `SessionId` | QString | 31 | `EntryId` | QString |
| 7 | `Host` | QString | 32 | `CallId` | QString |
| 8 | `Ip` | QString | 33 | `Message` | QString |
| 9 | `EventType` | QString | 34 | `GroupId` | QString |
| 10 | `Category` | QString | 35 | `Mode` | QString |
| 11 | `Detail` | QString | 36 | `TargetIps` | QString |
| 12 | `App` | QString | 37 | `SoftwarePath` | QString |
| 13 | `Timestamp` | QString(ISO8601ms) | 38 | `SoftwareArgs` | QString |
| 14 | `Enabled` | bool | 39 | `InstallerType` | QString |
| 15 | `Reason` | QString | 40 | `UwfStateJson` | QString(JSON) |
| 16 | `NewSeat` | QString | 41 | `Progress` | int |
| 17 | `NewStudent` | QString | 42 | `Result` | QString |
| 18 | `ScoreJson` | QString(JSON) | 43 | `Revision` | qint64 |
| 19 | `RulesJson` | QString(JSON) | 44 | `SyncDataJson` | QString(JSON) |
| 20 | `LessonId` | QString | 45 | `TeacherId` | QString |
| 21 | `LessonTopic` | QString | 46 | `BridgePort` | int |
| 22 | `LessonDate` | QString(ISO date) | 47 | `DeviceClass` | QString |
| 23 | `Delta` | int | 48 | `BlockMinutes` | int |
| 24 | `Label` | QString | 49 | `CheckInName` | QString |

> `CheckInName` 是学生签到时**自己输入**的姓名，与 `Student`（名单姓名）语义不同；
> 教师端用 `Seat` 查名单后再比对两者。

---

## 4. 事件类型（`EventType`）

日志 `type` 字段的取值，词云与分层分析只统计 `app / window / web / ai` 四类。

```
login, logout, switch, assign, app, window, web, ai, lock, unlock,
key-failed, quit-attempt, quit-allowed, connection,
score, call, silence, mute, internet, usb, uwf, install, sync, remote, demo,
check-in, quick-score
```

`check-in` 由学生签到回复与教师端「开始签到」共同产生；`quick-score` 记录座位网格上的一次快捷加减分（含分值与原因）。

---

## 5. 配置项（`SettingKey`）

Veyon 注册表段名固定为 `Classroom`（**不要改**，否则旧配置丢失）。

| 键 | 类型 | 默认 |
|---|---|---|
| `enabled` | bool | `true` |
| `dataDirectory` | QString | `%APPDATA%/ClassKeep` |
| `sessionKey` | QString（加密落盘） | 空 |
| `exitProtection` / `exitRequiresAdmin` | bool | `true` |
| `exitAdminUsername` | QString | `Administrator` |
| `allowSelfLogin` | bool | `true` |
| `aiEnabled` | bool | `true` |
| `aiEndpoint` | QString | `http://127.0.0.1:11434/v1/chat/completions` |
| `aiApiKey` | QString（加密落盘） | 空 |
| `aiModel` | QString | `qwen2.5:7b` |
| `aiSystemPrompt` / `aiTimeout` | QString / int | 空 / `60` |
| `monitoringEnabled` / `captureWindowTitle` / `captureProcess` | bool | `true` |
| `captureInterval` | int | `5`（秒） |
| `scoreBaseScore` | int | `100` |
| `uwfEnabled` / `internetGuard` / `usbInputBlock` | bool | 见配置页 |
| `bridgeEnabled` | bool | `false` |
| `bridgePort` | int | `11880` |
| `syncEnabled` | bool | `true` |
| `silencePageText` / `wordCloudStopWords` / `interestCategories` | QString | 空 |
| `retentionDays` | int | `180` |

`sessionKey` 与 `aiApiKey` 通过 `ClassroomSecrets::protect()` 以 `enc:` 前缀加密写入 `settings.json`；`reveal()` 兼容旧明文。

---

## 6. Bridge HTTP 协议（教师机 → 手机端）

地址：`http://<教师机局域网IP>:<bridgePort>/api/classroom/...`，默认端口 `11880`。Bridge **只绑定一个已校验的局域网单播 IPv4 地址**，不监听 `0.0.0.0`。
响应统一带 `Cache-Control: no-store` 与 `Connection: close`；不再返回 `Access-Control-Allow-Origin: *`。错误为 JSON `{"error": ...}` 并带真实 HTTP 状态码（401 / 404 / 413）。

| 方法 | 完整路径 | 认证 | 响应 |
|---|---|---|---|
| `GET` | `/api/classroom/state` | **公开** | `TeacherState`（见下），仅元数据与一次性挑战值 |
| `GET` | `/api/classroom/roster` | 需要 | 班级名单 JSON |
| `GET` | `/api/classroom/scores` | 需要 | 当前班级积分快照 |
| `GET` | `/api/classroom/seats` | 需要 | `{"className":..., "seats":[{"seat","student","ip","score"}]}`，**顺序与教师端座位网格拖拽后的顺序一致** |
| `POST` | `/api/classroom/score` | 需要 | 请求 `{"seat":"12","delta":3,"reason":"发言积极"}`（`delta` 仅 `-5..5`），响应 `{"ok":bool}` |
| `GET` | `/api/classroom/events` | 需要 | `{"events":[...]}`，近 30 天最多 2000 条（线程池扫描，不阻塞界面） |
| `GET` | `/api/classroom/wordcloud?seat=<seat>` | 需要 | `image/svg+xml`，SVG 文本（线程池扫描，不阻塞界面） |
| `GET` | `/api/classroom/report` | 需要 | `application/vnd.ms-excel`，Excel 2003 SpreadsheetML |
| `GET` / `POST` | `/api/classroom/timetable` | 需要 | 课表 JSON / `{"ok":bool}` |
| `GET` | `/api/classroom/classes` | 需要 | 班级列表 |
| `POST` | `/api/classroom/command` | 需要 | `{"ok": bool}` |
| `POST` | `/api/classroom/sync` | 需要 | `{"ok": bool, "revision": int}`，**只按 id 追加新积分条目，不覆盖规则/课堂/既有条目** |
| `OPTIONS` | 任意 | 免 | 空 204 |

`/api/classroom/state` 响应字段：

```json
{
  "teacherId": "12位随机ID",
  "revision": 1730000000000,
  "port": 11880,
  "bindAddress": "192.168.1.2",
  "className": "高一3班",
  "syncEnabled": true,
  "nonce": "32位一次性挑战值",
  "keyConfigured": true,
  "classNames": ["高一3班", "..."]
}
```

**`/api/classroom/state` 永不返回密钥或 token；其中的 `nonce` 只能用于一次请求。**

`POST /command` 请求体：

```json
{ "name": "mute", "enabled": true, "text": "", "teacherIp": "192.168.1.2" }
```

`name` 取值：`mute`、`internet`、`silence`、`uwfDisable`、`uwfEnable`、`uwfQuery`、`scoreRules`、`usb`、`remote`、`demo`。
其中 `usb / remote / demo` 由 Bridge 转成上游 Feature UID 控制，其余转成对应的 `Classroom::Command` 广播给全部学生机。

---

## 7. 认证协议（教师机专用 Bridge 密钥）

凭据是 `sessionKey`，**只存在于教师机与教师手机**。设置下发用 `ClassroomSettings::forStudents()` 删掉 `sessionKey`、`aiApiKey` 等所有秘密字段；学生端只收到 `keyConfigured` 布尔值，退出保护只认管理员凭据。AI 请求由学生端提交 prompt，教师机代理调用模型。

教师端点「开始新课堂」或「生成新密钥」时用 `QRandomGenerator::system()` 生成 **16 字节 / 128 bit** 熵，十六进制编码为 32 个大写字符。`settings.json` 里的密钥用当前 Windows 用户的 DPAPI 保护（`dpapi:` 前缀），旧的 `enc:` 值在教师端启动时迁移一次。

`GET /api/classroom/state` 是唯一公开路由，返回一个**单次使用**的挑战 `nonce`（300 秒内有效，用后即从服务端移除）。每个需要认证的请求带：

```
X-Classroom-Nonce: <nonce>
X-Classroom-Token: <token>
```

```
bodyHash = lowercase_hex( SHA-256( body ) )      // 无正文时 body 为空字节串
message  = method + "\n" + path + "\n" + nonce + "\n" + bodyHash
path     = 请求行里的完整路径（含查询串，例如 /api/classroom/wordcloud?seat=1）
token    = lowercase_hex( HMAC-SHA256( key = UTF8(sessionKey), msg = UTF8(message) ) )
```

服务端 `ClassroomSecrets::requestToken()` / Android `ClassKeepCrypto.token()` 实现一致。基准向量（双端单元测试共用）：

```
sessionKey = "ABC123", method = "GET", path = "/api/classroom/state"
nonce = "nonce-0001", body = ""
token = bd41d1e94c8c04a9ae857dce47c9f1d6907b65cfcd5c1bcbd8d08711a7c14e50
```

规则：

- **密钥永不上网**：不在 HTTP 头/正文，不在 UDP 广播里，也不下发学生机。
- token 绑定方法、路径、正文与 nonce；抓到的请求**无法原样重放**，也无法改路径/正文后复用。
- 客户端每个认证请求先取一次 `/state`；同一个 nonce 只允许成功一次。
- 服务端常量时间比较；密钥为空时 **fail-closed**（所有数据/控制接口 401）。
- 认证失败按**来源 IP** 计数：10 次锁定该 IP 60 秒，一台学生机不能锁住全班手机。
- Bridge 只绑定一个已校验的局域网单播 IPv4 地址（不监听 `0.0.0.0`）；请求头上限 16 KiB、正文上限 1 MiB，超限返回 413；未知路由返回 404。
- `/sync` 只按条目 id **追加**，不会整本覆盖积分册；`/score` 的 `delta` 只允许 `-5..5`；`teacherIp` 只接受单播 IPv4。

---

## 8. UDP 发现协议

- 端口：与 `bridgePort` 相同，默认 `11880`。
- 广播报文（教师机每 5 秒一次，UTF-8 明文）：

```
CLASSKEEP:<port>:<teacherId>:<revision>:<nonce>
```

- 主动探测（手机端发往 `255.255.255.255:<port>`，UTF-8 明文）：

```
CLASSKEEP_DISCOVER
```

- 教师机收到探测后**单播回复同格式的广播报文**（不是 JSON），因此手机端只需一套解析器。
- 常量：`announcementPrefix() = "CLASSKEEP:"`、`discoveryMagic() = "CLASSKEEP_DISCOVER"`。
- 手机端策略：**找到才同步，找不到不操作**；发现结果 30 秒节流；手动 IP + 手动同步始终可用。

---

## 9. 数据文件格式

根目录为 `dataDirectory`（`%APPDATA%/ClassKeep`），按 `scope` 分 `teacher/`、`client/`。

| 文件 | 格式 | 说明 |
|---|---|---|
| `settings.json` | JSON | 全部配置；密钥字段带 `enc:` 前缀 |
| `classes.json` | JSON | `{"classes":[{"name","students":[{"seat","name","ip"}]}]}` |
| `scores/<班级>.json` | JSON | `rules` / `lessons` / `entries`，见下 |
| `teacher/events/<YYYY-MM-DD>.jsonl` | JSON Lines | 教师机汇总事件 |
| `teacher/machines/<主机>/<日期>.jsonl` | JSON Lines | **每台学生机独立日志** |
| `teacher/students/<班级>/<座位-姓名>/<日期>.jsonl` | JSON Lines | 每生独立日志 |

事件行字段：`timestamp, sessionId, className, seat, student, host, ip, type, app, detail, category`。

`scores/<班级>.json`：

```json
{
  "className": "高一3班",
  "rules": {
    "published": false,
    "baseScore": 100,
    "html": "<b>规则</b>",
    "updated": "2026-11-10T08:00:00.000",
    "quickActions": [["做的不错", 1], ["迟到", -5]]
  },
  "lessons": [{ "id": "...", "date": "2026-11-03", "topic": "11.3 画图软件" }],
  "entries": [{
    "id": "...", "timestamp": "...", "lessonId": "...", "className": "...",
    "seat": "1", "student": "张三", "delta": 3,
    "label": "做的最快+3", "richText": "<b>做的最快</b>", "author": ""
  }]
}
```

`rules.published = false` 时学生端 HTML **不输出规则区块**。图片在 `studentHtml()` 里转成 `data:image/...;base64,...` 内联，因此单条 `ScoreReply` 即可携带带图明细，学生端无需落盘。

期末报表为 **Excel 2003 SpreadsheetML**（`.xls`，纯文本 XML，无第三方库）：
表头「序号 | 姓名 | `<M.d 教学内容>` | 目前总积分 | … | 期末总积分」，可选独立出勤列；
一节课一个格子，多次加分以换行合并；正分绿、负分红；含边框、冻结表头与自动换行。CSV 导出带 UTF-8 BOM。

---

## 10. 版本演进

1. 新增能力一律**追加** `Command` / `Argument` 枚举值与 HTTP 路由，不改已有序号。
2. 若必须改变认证公式、广播格式或字段语义，提升协议版本，并让 Bridge 在 `/state` 返回 `protocol` 字段，双端按版本协商。
3. 每次改动同步更新本文件、`ClassroomSelfTest.cpp` 与 `ClassKeepUnitTest.kt` 的常量断言。
