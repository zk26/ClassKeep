# 课守 ClassKeep 协议规范

**协议版本：`1`（随本分支首次发布）**
权威定义：`plugins/classroom/ClassroomProtocol.h`、`ClassroomBridge.cpp`、`ClassroomSecrets.cpp`、`mobile/app/src/main/java/top/ikee002/classkeep/Announcement.kt`。

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
| `FullScreenDemo` | `7b6231bd-eb89-45d3-af32-f70663b2f878` |
| `DemoAllScreens` | `2aca1e9f-25f9-4d0f-9729-01b03c80ab28` |
| `StartApp` | `da9ca56a-b2ad-4fff-8f8a-929b2927b442` |
| `DistributeFiles` | `4a70bd5a-fab2-4a4b-a92a-a1e81d2b75ed` |

---

## 2. FeatureMessage 命令表

`M` = 教师机 Veyon Master，`S` = 学生机 Veyon Server，`W` = 学生机 Veyon Worker。

| 值 | Command | 方向 | 说明 | 主要参数 |
|---:|---|---|---|---|
| 1 | `StartClient` | S→W | 服务启动时拉起常驻 worker | `SettingsJson`、`SessionKey`、`SessionId` |
| 2 | `StopClient` | M→S→W | 停止 worker（同时暂停看门狗） | — |
| 3 | `SetSettings` | M→S→W | 下发全部设置 JSON | `SettingsJson` |
| 4 | `SetRoster` | M→S→W | 下发班级名单 JSON | `RosterJson` |
| 5 | `SetSessionKey` | M→S→W | 下发本堂课密钥与会话号 | `SessionKey`、`SessionId` |
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

> 标注「保留」的命令已在协议中占位但尚未接线；收到时 `default` 分支返回 `false`，不会静默吞掉。

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
| 24 | `Label` | QString | | | |

---

## 4. 事件类型（`EventType`）

日志 `type` 字段的取值，词云与分层分析只统计 `app / window / web / ai` 四类。

```
login, logout, switch, assign, app, window, web, ai, lock, unlock,
key-failed, quit-attempt, quit-allowed, connection,
score, call, silence, mute, internet, usb, uwf, install, sync, remote, demo
```

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

地址：`http://<教师机IP>:<bridgePort>/api/classroom/...`，默认端口 `11880`。
响应统一带 `Access-Control-Allow-Origin: *` 与 `Connection: close`；错误为 JSON `{"error": ...}`。

| 方法 | 路径 | 认证 | 响应 |
|---|---|---|---|
| `GET` | `/state` | **公开** | `TeacherState`（见下），仅元数据与当前挑战值 |
| `GET` | `/roster` | 需要 | 班级名单 JSON |
| `GET` | `/scores` | 需要 | 当前班级积分快照 |
| `GET` | `/events` | 需要 | `{"events":[...]}`，近 30 天最多 2000 条 |
| `GET` | `/wordcloud?seat=<seat>` | 需要 | `image/svg+xml`，SVG 文本 |
| `GET` | `/report` | 需要 | `application/vnd.ms-excel`，Excel 2003 SpreadsheetML |
| `POST` | `/command` | 需要 | `{"ok": bool}` |
| `POST` | `/sync` | 需要 | `{"ok": bool, "revision": int}` |
| `OPTIONS` | 任意 | 免 | 空 200（CORS 预检） |

`/state` 响应字段：

```json
{
  "teacherId": "12位随机ID",
  "revision": 1730000000000,
  "port": 11880,
  "className": "高一3班",
  "syncEnabled": true,
  "nonce": "16位挑战值",
  "keyConfigured": true,
  "classNames": ["高一3班", "..."]
}
```

**`/state` 永不返回密钥或 token。**

`POST /command` 请求体：

```json
{ "name": "mute", "enabled": true, "text": "", "teacherIp": "192.168.1.2" }
```

`name` 取值：`mute`、`internet`、`silence`、`uwfDisable`、`uwfEnable`、`uwfQuery`、`scoreRules`、`usb`、`remote`、`demo`。
其中 `usb / remote / demo` 由 Bridge 转成上游 Feature UID 控制，其余转成对应的 `Classroom::Command` 广播给全部学生机。

---

## 7. 认证协议（课堂密钥复用）

凭据就是本堂课学生端输入的 `sessionKey`，**不新增 token、不做登录流程**。

```
token = lowercase_hex( SHA-256( UTF8("veyon-classroom:" + sessionKey + ":" + nonce) ) )
```

每个需要认证的请求带两个头：

```
X-Classroom-Nonce: <nonce>
X-Classroom-Token: <token>
```

服务端 `ClassroomSecrets::sessionToken()` / Android `ClassKeepCrypto.token()` 实现完全一致。
基准向量（双端单元测试共用）：

```
sessionKey = "ABC123", nonce = "nonce-0001"
token = e198264293e2ee9d35aade33089e0f33e560cd60143d01292483f9f952330e04
```

规则：

- **密钥永不上网**：既不在 HTTP 头/正文，也不在 UDP 广播里，只参与本地摘要。
- `nonce` 每 60 秒轮换，服务端同时接受当前值与上一个值；客户端每次 `POST` 前先 `GET /state` 刷新。
- 服务端常量时间比较；密钥为空时 **fail-closed**（所有数据/控制接口 401）。
- 60 秒内累计 10 次认证失败 → 锁定 60 秒。
- 教师端点「开始新课堂」会无条件生成新密钥（16 位十六进制，64 bit 熵）并立即轮换 nonce，**上一堂课的设备立即失效**。

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
