# ikeback — 全部功能验收总表

本文件把需求讨论中**逐条列出过的每一项功能**与当前实现一一对照，作为验收总账。
仓库：<https://github.com/zk26/ikeback> · 许可 **GPL-2.0-or-later**。

状态说明：

| 标记 | 含义 |
|---|---|
| ✅ | 代码完成，且已被 Linux/Qt6/GCC 与 Windows/MinGW 编译覆盖 |
| ◐ | 代码完成，但依赖特定运行环境，尚未在真实机房联调 |
| ❌ | 未实现（明确列出，不含糊） |

---

## 2.2.0 三项遗留整改（第五轮）

| # | 审计结论 | 状态 | 实现位置与说明 |
|---|---|---|---|
| A2 | 工具栏「登陆/注销」仍是 Veyon 原生用户名+密码 | ✅ | `MainWindow.cpp::addFeaturesToToolBar()` 按 feature name 过滤掉 `UserLogin`/`UserLogoff`；新增 `ClassroomSignInDialog`（班级 / 姓名 / 座位号，无密码）+ 两个 `Action\|Master` feature；`ClassroomPlugin::startFeature()` 下发 `SetIdentity` / `ForceLogout`。**完全不触碰 `prepareLogon()` 与密码链路**，上游插件仍编译但不暴露入口 |
| A3/B3 | 托盘无图标、10+ 个 `ikeback-worker.exe` | ✅ | 托盘：`ClassroomPlugin::ensureTrayIcon()` 在 worker 会话内自建 `QSystemTrayIcon`（ikeback 图标 + 显示卡片/积分/AI/受保护退出菜单 + 班级座位姓名 tooltip），不再依赖 core builtin tray worker。进程收敛见下表 |
| B4 | 「将 ikeback 功能迁移至指定区域」 | ✅ | 采用**主界面内嵌**：`ClassroomMasterDialog` 由 `QDialog` 改为 `QWidget`，`ClassroomPlugin::startFeature()` 把它放进 `QDockWidget`（objectName `IkebackConsoleDock`）停靠在主窗口右侧；工具栏按钮改为显示/隐藏该面板；`ClassroomConsole`/`ClassroomSignIn`/`ClassroomSignOut` 一并移入主工具栏第一组。无 QMainWindow 时回退独立窗口 |

### Worker 进程收敛的具体改动

| 根因 | 修复 |
|---|---|
| 打开控制台连发 `SetSettings`/`SetRoster`/`SetSessionKey` 三条消息，每条都会走一次 worker 启动路径 | 合并为一条 `Command::ApplySession`（枚举末尾追加，保持线上协议兼容），worker 侧一次性应用设置/名册/密钥 |
| watchdog 每 5s 检查并在 worker 认证期间反复 `startClient()` | 间隔改为 30s，与 `WorkerAuthenticationTimeout` 对齐；保留 `m_restartPending` 单次重启闩 |
| `StopClient` 也被无条件转发给 `sendMessageToUnmanagedSessionWorker`，等于「为了停掉而新起一个 worker」 | 仅在 worker 确实运行时才转发 |
| `startUnmanagedSessionWorker()` 开头无条件 `stopWorker()` | 改为幂等：已在运行直接返回；新增 30s 启动冷却，重试定时器的消息进入队列而不是反复拉起进程 |
| `stopWorker()` 延迟 5s 才 terminate/kill | 立即 `terminate()`，1s 后 `kill()` 兜底；显式停止时清除冷却，教师端可立刻重新启动客户端 |
| `enqueueSessionWorkerMessage()` 在队列不存在时直接 return，而队列从未被创建 → 会话就绪前的消息全部丢失 | 用 `operator[]` 惰性创建队列，`retryPendingSessionWorkerMessages()` 在 worker 认证后统一投递 |

> 说明：Veyon 架构本身是「每个带 `Flag::Worker` 的 feature 一个 worker 进程」，
> 本轮不重写该架构（风险过高且无真实机房回归），而是消除 ikeback 侧造成的
> **重复启动与残留**，把进程数从持续增长的十几个收敛到稳定的一组。

---

## 2.1.0 逐项核对：审计发现与整改（第四轮）

| # | 审计发现 | 状态 | 整改结果 |
|---|---|---|---|
| A1 | 主监控缩略图只有 `名字 · 100 points`，不是约定的 `🛡️学生1 [100]` | ✅ | `ComputerControlListModel.cpp:501-517` 改为 `盾 名字  [积分]`；`ClassroomMasterDialog.cpp:2073` 同时发布 `ikebackProtected` 属性，保护关闭时显示空心圆 |
| A2 | 工具栏「登陆/注销」仍是 Veyon 原生用户名+密码登录 | ❌ 未整改 | **仍未整改**：需替换 `usersessioncontrol` 的 `UserLoginDialog` 并改接 `Command::SetIdentity`，本轮未动 |
| A3 | 学生机悬浮卡片无实机截图证据、创建 4 个按钮 | ◐ | 代码存在且 2.0.0 已编译；本轮未改，待机房联调 |
| B1 | 安装/卸载详情满屏 `[SC] 1060`、`没有找到进程`、中文乱码 | ✅ | `ikeback-installer.nsi` 用 `SetDetailsPrint none/both` 包裹子进程清理段 |
| B2 | Quick tools / Configurator 全英文 | ✅ | 补齐 `veyon_zh_CN.ts` 428 条（Classroom* 上下文），lrelease 直接生效 |
| B3 | 托盘无图标、10+ 个 `ikeback Worker` | ❌ 未整改 | **仍未整改**：属 Veyon `FeatureWorkerManager` + 5s watchdog 的架构级竞态，需单独评估并回归测试，本轮不动 |
| B4 | 「将 ikeback 功能迁移至指定区域」目标区域不明确 | ❌ 未整改 | **等待你指定目标区域**（工具栏分组 / 主界面 / 配置器合并） |
| B5 | 教师端缺课表、当堂课表现评分 | ◐ | 新增「课表与课堂」页签：课表导入导出、讲授内容、全班表现评分、按班归档记录；与手机端共用 `timetable.json` |

## 2.1.0 手机端十项调整

| # | 需求 | 状态 | 实现位置 |
|---|---|---|---|
| M1 | `CK` → 真实图标 | ✅ | `MainActivity.brandHeader()` 用 `R.mipmap.ic_launcher_round` |
| M2 | 未配置时卡片收缩 + 提示去配置页 | ✅ | `refreshConnectionHint()`；隐藏概况/活动大卡，主区留给控制与名单 |
| M3 | 调整模块摆放 | ✅ | 概况/活动移入新增「班级」页；课堂页只留控制 + 名单 |
| M4 | 新增底部「班级」页并按班归档 | ✅ | 5 个 tab；`refreshClassRecords()` + `/api/classroom/classes` |
| M5 | 部分按钮收进设置 | ✅ | 「更多」只剩 2 个入口，7 个导入导出动作收进 `showMoreTools()`；课表 4 个操作收进「课表设置」 |
| M6 | 双列按 ClassBoard 样式 | ✅ | `TimetableGridView.kt` 冻结双作息时间轴、彩色跨节卡片、间歇行、今天高亮；`TimetableConfig.dualTime` + 偏移 |
| M7 | 本地 + 教师机双写 | ◐ | `BridgeClient.timetable()/pushTimetable()` + `ClassroomBridge` GET/POST；本地已验证编译，教师机路由待 CI + 联机验证 |
| M8 | 课表设置归入课表设置项 | ✅ | 配置页「课表显示」卡片移除，全部并入「课表设置」 |
| M9 | 修复按钮重叠 | ✅ | `Ui.button()` 显式 `MATCH_PARENT × WRAP_CONTENT` + `minHeight 44dp` + 8dp 上边距 |
| M10 | 课堂内容按班级自然记录，取消「查询这堂课」 | ✅ | 删除 `showLessonReview()`；`refreshClassRecords()` 按班列出讲了什么/干了什么/同学表现 |

> 手机端已用本机 JDK 23 + Android SDK 实测：`:app:assembleDebug` +
> `:app:testDebugUnitTest` **BUILD SUCCESSFUL，9 个单测零警告全绿**。
> 桌面端改动由 GitHub Actions 的 Linux Qt6/GCC 与 Windows MinGW 编译覆盖。

---

## 2.0.0 常用工具 / 座位视图 / 学生卡片（第三轮）

| # | 需求 | 状态 | 实现位置 | 使用入口 |
|---|---|---|---|---|
| T1 | 教师端**常用工具放在页面上**（首个页签） | ◐ | `ClassroomMasterDialog.cpp` → `setupToolsTab()` | 教师端「Quick tools」页 |
| T2 | 全部广播 / 选中广播；广播默认禁 USB 但**屏幕正常显示** | ◐ | `runFeature(ShareOwnScreenFullScreen)` + `InputDevicesLock` | 全部广播 / 选中广播 |
| T3 | 边看边做（窗口演示，学生仍可操作） | ◐ | `runFeature(ShareOwnScreenWindow)`，不启 USB 锁 | 边看边做 |
| T4 | **启动/关闭保护**（防止学生端自行退出程序） | ◐ | `SetExitProtection` → `exitProtection`；`closeEvent()` 拒绝 + 守护定时器 | 客户端保护 启动 / 关闭 |
| T5 | **启动/关闭 UWF**（还原，防掉线 / 装软件前关闭） | ◐ | `UwfSetFilter`；工具面板**不自动重启** | UWF 还原 启动 / 关闭 / 查询 / 重启学生机 |
| T6 | 卸载保护（独立于以上两者） | ◐ | `SetUninstallProtection` + `HKLM\SOFTWARE\ikeback\Student` | 「系统设备」页 |
| T7 | 禁用 / 启用网关 | ◐ | `SetInternetBlock` | 网关 禁用 / 启用 |
| T8 | 肃静页面（文案可设置） | ◐ | `ShowSilencePage` / `HideSilencePage` + `silencePageText` | 肃静页面 文案框 + 显示 / 隐藏 |
| T9 | 禁用应用 / 禁用地址（**模糊匹配**） | ◐ | `SetAppBlockRules` / `SetWebBlockRules` → `ClassroomMonitor` 子串匹配 → `taskkill` + 事件上报 | 规则输入框 + 应用 / 清空 |
| T10 | 打开软件 / 打开网页 | ◐ | `OpenSoftware` / `OpenWebsite` | 路径 / 网址输入 + 打开按钮 |
| T11 | 全班加减分 | ✅ | `scoreWholeClass()` → `addEntryForSeat()` | +5/+1/-1/-5/自定义 |
| T12 | 座位**网格可拖动** + **列表视图**（不抓屏，含 UWF 保护情况） | ◐ | `ClassroomSeatGrid` `ViewMode::{Grid,List}`、`QTableWidget` 9 列 | 座位页「切换列表/网格视图」按钮 |
| T13 | 网格卡片下显示 **🛡 + 名字 + [积分]** | ◐ | `protectionIcon()` + `refresh()` caption | 座位网格 |
| T14 | 学生端悬浮卡片：姓名 + 座位号 + 积分/AI 两个按钮 | ◐ | `ClassroomClientWidget::updateStatus()` / `updateVisibility()` | 学生桌面 |
| T15 | 悬浮卡片**不置顶**，且 server 不退出就**关不掉** | ◐ | `keepFloating()`（无 `WindowStaysOnTopHint`）、`closeEvent()` + 1.5s 守护定时器；仅 `StopClient` 可隐藏 | 自动 |
| T16 | 点击按钮弹窗查询积分 / AI | ◐ | `ClassroomScoreView` / `ClassroomAiChatWidget` | 卡片按钮 |

> 「◐」表示代码已完成但**尚未在真实机房 / Windows UWF 环境联调**，且需由
> GitHub Actions 的 Qt6/MinGW 编译验证。本机无 Qt/MinGW 工具链，桌面端未做本地编译。

## 2.0.0 移动端（底部栏 / 课表 / 更多）

| # | 需求 | 状态 | 实现位置 | 使用入口 |
|---|---|---|---|---|
| M1 | 底部栏**图标 + 名称常显**（不再用单字假图标） | ✅ | `BottomBar.kt` + 4 个 `res/drawable/ck_tab_*.xml` 矢量图标 | 四个标签 |
| M2 | 课堂 = **课堂控制 + 加分减分** | ✅ | `MainActivity.kt` → `buildPages()` 的 `pages[0]` | 「课堂」 |
| M3 | 课表：一天几节课 / 大课间 / 中午 / 单列双列时间可调 | ✅ | `TimetableStore.kt` → `TimetableConfig` / `buildPeriods()`；`showTimetableSettings()` | 「课表」→ 作息设置 |
| M4 | 配置页（IP / 密钥 / 端口 / 时间列切换） | ✅ | `MainActivity.kt` → `pages[2]` | 「配置」 |
| M5 | 更多页：导入导出 + 查询这堂课讲了什么/干了什么/同学表现 | ✅ | `exportTimetable()` / `exportLocalData()` / `showLessonReview()` / `renderLessonReview()` | 「更多」 |
| M6 | 名称只用英文 `ikeback` | ✅ | `strings.xml` / `brandHeader()` / `mobile/README.md` | 应用名与品牌头 |

> 移动端已用本机 JDK 23 + Android SDK 实测：`:app:assembleDebug` +
> `:app:testDebugUnitTest` **BUILD SUCCESSFUL，7 个单测全绿**（含新增的
> 节次生成 / 大课间 / 午休 / 配置断言）。

---

## A. 课堂身份与连接（第一轮 ①②）

| # | 需求 | 状态 | 实现位置 | 使用入口 |
|---|---|---|---|---|
| A1 | 教师端**按 IP 快速登录学生**，每堂课/考试 IP 都不同 | ✅ | `ClassroomMasterDialog.cpp` → `assignByIp()` / `quickLogin()` / `batchLogin()` / `registerComputers()` | 「课堂与登录」页 → 按 IP 一键登录 / IP+座位+姓名快速登录 |
| A2 | 班级对应学生名单，按班级记录行为 | ✅ | `ClassroomRoster`（`ClassroomData.h/.cpp`）、`classes.json`、CSV 导入导出 | 班级下拉框 + 名单表格 |
| A3 | IP 不在 Veyon 电脑目录也能下发 | ✅ | `sendToIp()` → `m_directInterfaces` 临时直连接口 + 重试队列 | 自动；另有「同步 IP 到目录」按钮 |
| A4 | 无管理员权限时也能工作 | ✅ | 直连通道独立于 HKLM 目录写入；不可写时弹提示 | 自动 |
| A5 | 学生机**密钥绑定后不能退出**（超管登录环境） | ◐ | `ClassroomClient.cpp` → `setLocked()` / `requestQuit()`；`ClassroomPlugin.cpp` worker 看门狗（5 秒） | 悬浮窗右键「退出客户端」需超管凭据或密钥 |
| A6 | 退出需超级管理员账号 | ◐ | `PlatformUserFunctions::authenticate()` + 失败记日志 | 同上 |
| A7 | 学生自助登录（座位号 + 姓名 + 本堂密钥） | ✅ | `ClassroomClient.cpp` → `handleLogin()` | 学生端悬浮条 |
| A8 | 每堂课换密钥 | ✅ | `startNewSession()` 无条件 `generateKey()`（`QRandomGenerator::system()` 16 字节 / 128 bit 熵，32 位十六进制）；密钥只留在教师机与教师手机 | 「开始新课堂」 |
| A9 | 密钥加密落盘、兼容旧明文 | ✅ | `ClassroomSecrets.h/.cpp`（Veyon `CryptoCore` RSA） | 自动 |
| A10 | 教师发现不对可**强制退出 / 换学生 / 批量操作** | ✅ | `ForceLogout` / `SwitchUser` / 批量登录与锁定 | 「课堂与登录」页 |
| A11 | 换课清身份、补传离线事件、多教师端上下文 | ✅ | `startNewSession()` 先 `ForceLogout`；server 补传队列（上限 5000）；`QVector<MessageContext>` | 自动 |
| A12 | **批量登录后点「开始签到」**：学生桌面弹窗输入姓名，按座位号与名单比对，直接看出谁来了/谁没来 | ✅ | `StartCheckIn` / `CheckInReply`；`ClassroomCheckInDialog`；`ClassroomMasterDialog::startCheckIn()/recordCheckIn()/refreshCheckIn()` | 「Seats & check-in」页 → 开始签到 |
| A13 | 签到结果三态：已到 / 姓名不符 / 未到，含时间与汇总计数 | ✅ | 同上，教师机本地比对，学生机不判对错 | 签到表实时刷新 |

> A5/A6 标 ◐：纯用户态无法 100% 阻止 SYSTEM 权限的进程终止，当前靠「密钥 + 超管凭据 + 服务看门狗 + 留痕」四层。彻底防杀需驱动级保护，**本项目刻意不做**（Defender 友好）。

---

## B. 全数据监控、日志与分层（第一轮 ③、第二轮 ②）

| # | 需求 | 状态 | 实现位置 | 使用入口 |
|---|---|---|---|---|
| B1 | 全数据行为监控（前台窗口标题、进程名、浏览器判定、登录/退出/AI 提问等） | ✅ | `ClassroomMonitor`（`ClassroomClient.cpp`）、`ReportEvent` 链路 | 自动，5 秒采样 |
| B2 | **每台学生机独立日志** | ✅ | 教师机 `machines/<主机名>/<日期>.jsonl`、`students/<班级>/<座位-姓名>/`、`events/` | 「行为日志」页按机器/学生筛选 |
| B3 | **学生端一般不放数据**（UWF 下不丢） | ✅ | 学生端零落盘，全部经 `ReportEvent` 上报教师机 | 自动 |
| B4 | 多堂课**动态**决定分层（非一堂课定） | ✅ | `ClassroomData.cpp` 兴趣画像 + 半衰期加权；关键词表可编辑 | 「兴趣分析」页：兴趣分布、课堂次数、趋势、分层建议、CSV 导出 |
| B5 | **每个学生词云** | ✅ | `ClassroomWordCloud.h/.cpp`：中文 2–4gram、长词加权、停用词、螺旋布局；`toSvg()` / `toPixmap()` 零 QtSvg 依赖 | 「Score & reports」页选班级+座位，可导出 SVG |
| B6 | 日志筛选 / 导出 CSV / 清理过期 | ✅ | 「行为日志」页 | 日期/班级/学生/机器/关键词 |
| B7 | 状态索引与 IO 优化 | ✅ | `identityStates()` / `lastActivities()` 增量索引、目录缓存 | 自动 |

> 上网行为目前基于**前台窗口标题 + 进程名 + 浏览器识别**，不抓取 URL 或网络包。要精确 URL 需浏览器扩展或 UI Automation（未做）。

---

## C. AI 助手（第一轮 ④）

| # | 需求 | 状态 | 实现位置 | 使用入口 |
|---|---|---|---|---|
| C1 | 可配置**本地 AI 接口或网络地址** | ✅ | `ClassroomAiClient.h/.cpp`、`ClassroomConfiguration.h`、Configurator 页 | 「AI 设置」页 / Veyon Configurator → ikeback |
| C2 | OpenAI 兼容协议（`model`/`messages`/`stream:false`），可填密钥、模型、系统提示词、超时 | ✅ | 同上 | 同上，带**测试连接**与推送 |
| C3 | 学生端 **AI 悬浮按钮 → 直接聊天** | ✅ | `ClassroomAiChatWidget`（可拖动、置顶） | 学生端悬浮条「AI assistant」 |
| C4 | 登录状态悬浮 | ✅ | `ClassroomClientWidget` 悬浮条常驻 | 自动 |
| C5 | AI 提问进日志 | ✅ | `EventType::ai()` | 自动 |
| C6 | **保持原风格** | ✅ | Feature 动态注册到原工具栏、Configurator 插件页、`Configuration::Proxy`、`tr()`、`:/core/icon*.png`、ToolButton/QSplitter | — |

---

## D. 积分制与期末报表（第二轮 ⑤⑥）

| # | 需求 | 状态 | 实现位置 | 使用入口 |
|---|---|---|---|---|
| D1 | 基础分 **100** | ✅ | `ClassroomScore.h/.cpp` → `ClassroomScoreRules::baseScore` | 教师端可改 |
| D2 | 学生端**积分查询 + 积分明细**（直接查询，不需规则） | ✅ | `QueryScore` → `ScoreReply` → `ClassroomScoreView` | 学生端悬浮条「Score」 |
| D3 | **积分规则**：老师端设置好学生才能查看，否则是空的 | ✅ | `published` 门控，未发布时 HTML 连标题都不生成 | 「Score & reports」页 → 发布规则 |
| D4 | 明细格式：**加粗 / 下划线 / 改颜色 / 改字体 / 改字号 / 插入图片** | ✅ | `ClassroomScorePanel.cpp` `QTextEdit` 工具栏；`inlineImages()` 转 base64 data URI 精确内联 | 「Score & reports」页富文本编辑器 |
| D5 | 每天都能在教师端操作和查看 | ✅ | 节次按日期 + 教学内容建立，表格实时刷新 | 「Score & reports」页 |
| D6 | **期末导出每班情况表**（一节课一格，多次加分写同一格） | ✅ | `ClassroomTermReport.h/.cpp` → SpreadsheetML `.xls` + CSV | 「Score & reports」页导出 |
| D7 | 表格列式：`M.d 教学内容` + `目前总积分` + **独立出勤列** + `期末总积分` | ✅ | 同上（√ / 迟到 / 缺勤 / 请假 由留痕推断，不再把缺勤打勾） | 同上 |
| D8 | 累计分按**课程日期**而非录入顺序 | ✅ | `total()` 用 `std::stable_sort` | 自动 |
| D9 | Excel 工作表名合法化、规则富文本转纯文本 | ✅ | `sheetName()` + 正则去标签 | 自动 |
| D10 | **座位缩略图 + 下方「XX号：XX分」**，点击分数给分/减分 | ✅ | `ClassroomSeatGrid.h/.cpp`（插件内实现，不改 Veyon 未导出的 delegate）；缩略图取 `ComputerControlInterface::scaledFramebuffer()` | 「Seats & check-in」页座位网格 |
| D11 | 点分数弹出 **-5~+5 步长 1** 的选择，选后填**加减分原因**才生效 | ✅ | `chooseDelta()` → `QMenu` 11 项 → `QInputDialog` → `ClassroomScorePanel::addEntryForSeat()` | 点分数条 |
| D12 | **期末成绩详情表**：逐条列出每次加减分（时间/节次/座位/姓名/分值/原因/操作人） | ✅ | `ClassroomTermReport::toSpreadsheetMl()` 明细 Worksheet + `toDetailsCsv()`；CSV 导出时额外生成 `*-details.csv` | 「Score & reports」→ 期末报表（默认勾选「包含逐条明细」） |

---

## E. 系统控制与 UWF（第二轮 ①③⑧⑨）

| # | 需求 | 状态 | 实现位置 | 使用入口 |
|---|---|---|---|---|
| E1 | **原生 Win10 Defender 友好** | ✅ | 只用系统自带签名 API：`uwfmgr.exe`、WMI、WFP 用户态、Core Audio COM、SetupAPI；**无驱动、无进程注入、无临时 .ps1、无自签证书** | 设计约束 |
| E2 | UWF **状态查询** | ◐ | `ClassroomSystem.cpp` → `uwfState()`：`uwfmgr get-config` + WMI 交叉复核 | 「System & devices」→ UWF 查询 |
| E3 | UWF **关闭 / 打开** | ◐ | `setUwfFilter()`（`filter enable/disable`，重启生效） | UWF 关闭+重启 / 开启+重启 |
| E4 | UWF **状态监听** | ◐ | 30 秒低频轮询 + `UwfStatusReport` 上报到教师机日志 | 自动 |
| E5 | **远程装软件** | ◐ | 复用 Veyon `FileTransfer` + `StartApp`；`softwareInstallCommand()` 识别静默参数 | 「System & devices」→ 分发并安装 |
| E6 | 参考 `G:\UWFTool` 但**不照搬**、延续节约风格 | ✅ | 只借鉴 `uwfmgr` 子命令与「关保护→重启→安装→恢复」流程概念，**未复制任何代码/脚本/资源**，且全部收敛在一个插件 + 一条 FeatureMessage | 见 `IKEBACK-LICENSE.md` |
| E7 | **USB 禁止输入拦截**（禁用输入但保留桌面显示） | ✅ | 复用 Veyon `InputDevicesLock` + `LockWidget::Mode::DesktopVisible` | 「System & devices」→ USB 拦截/恢复 |
| E8 | 网关**静音 / 启用** | ◐ | Core Audio `IAudioEndpointVolume::SetMute` | 「System & devices」→ 静音/取消 |
| E9 | **WFP InternetGuard 断网**（保留教师机 IP 例外） | ◐ | WFP 用户态 `FWPM_LAYER_ALE_AUTH_CONNECT_V4` | 「System & devices」→ 断网/放行 |
| E10 | **Ctrl + 双击缩略图远控** | ✅ | `ComputerMonitoringWidget.cpp` 判 `Qt::ControlModifier` → `RemoteControl` | 教师端缩略图 |
| E11 | **分组投屏 / 多选控屏** | ✅ | 复用 demo 插件 `ShareUserScreen*` / `FullScreenDemo` UID | 「System & devices」→ 分组投屏 / 广播 |
| E12 | **肃静页面** | ✅ | `ClassroomSilencePage` 全屏置顶 | 「System & devices」→ 显示/隐藏 |
| E13 | 学生**呼叫老师** | ✅ | `CallTeacher` → 教师端 `showCall()` 提示 | 学生端悬浮条「Call teacher」 |

> E2–E5、E8–E9 标 ◐：代码已编译，但 `uwfmgr` / Core Audio / WFP 依赖真实 Windows 企业版/教育版/IoT + UWF 环境，未在机房验证。

---

## F. 手机端与数据同步（第二轮 ④⑦ + 密钥认证）

| # | 需求 | 状态 | 实现位置 | 使用入口 |
|---|---|---|---|---|
| F1 | **Android 教室端 App**（只做教室端，控制教师机内容） | ✅ | `mobile/` 原生 Kotlin + XML View + Material3，**零第三方网络/JSON/加密库** | APK，见 Releases |
| F2 | iOS 工程 | ❌ | — | 协议为标准 HTTP/JSON + UDP + SHA-256，可直接复用 |
| F3 | 参考 `Monitoring-Center` 但不复制 | ✅ | 只借鉴「移动驱动 Veyon Feature」理念，**从零编写**，未复制代码/界面/资源 | 见 `IKEBACK-LICENSE.md` |
| F4 | **网段内发现教师机**，找到才同步、没找到不操作 | ✅ | `Discovery.kt`：UDP 主动探测 + 周期广播 + MulticastLock；30 秒节流 | App 自动 |
| F5 | **手动同步按钮** | ✅ | `MainActivity.kt` → 立即同步；另有手动 IP / 端口 / 密钥输入 | App |
| F6 | 手机端也存数据（离线副本） | ✅ | `CacheStore.kt` → SharedPreferences，保存密钥、端口、统计、最近活动 | 自动 |
| F7 | 数据主库在教师机、按 `revision` 增量同步 | ✅ | `ClassroomBridge.cpp` → `revision()` / `/api/classroom/sync` | 自动 + 手动 |
| F8 | **教师机专用密钥做 Bridge 认证**（不是裸奔） | ✅ | C++ `ClassroomSecrets::requestToken()` ↔ Kotlin `IkebackCrypto.token()`，`HMAC-SHA256(method+path+nonce+SHA256(body))`；nonce 单次使用；双端同一基准断言；密钥不下发学生机、落盘走 DPAPI | App 输入「本堂课密钥」 |
| F9 | 广播**不携带密钥**，只带 60 秒轮换的挑战值 | ✅ | `ClassroomProtocol.h` → `IKEBACK:` 前缀；`announcementPayload()` | 自动 |
| F10 | 换课旧设备立即失效、10 次失败锁 60 秒、常量时间比较 | ✅ | `setSettings()` 轮换 nonce、`noteAuthFailure()`、`authorized()` | 自动 |
| F11 | App 控制指令（静音/断网/肃静/USB/远控/投屏/UWF 等 13 项） | ✅ | `/api/classroom/command` → `handleCommand()` → FeatureMessage / 原生 Feature | App 控制网格 |
| F12 | 期末表导出 `.xls`、词云 WebView 查看 | ✅ | `BridgeClient.kt` → `report()` / `wordCloud()` | App |
| F13 | 401 明确提示「可能换了新课，检查密钥」 | ✅ | `UnauthorizedError` / `MainActivity.kt` | App |
| F14 | **手机端美化**：品牌渐变头部（Logo + ikeback）、圆角卡片/输入框/胶囊状态、控制按钮按下态、行距与状态栏配色 | ✅ | `activity_main.xml`、`bg_brand.xml`、`bg_input.xml`、`bg_badge_*.xml`、`bg_button_chip.xml`、`colors.xml`、`themes.xml` | App 全界面 |
| F15 | **更换图标为 `ikeback_logo.png`**（全密度方形 + 圆形） | ✅ | `mobile/tools/generate_icons.py` 生成 `mipmap-{mdpi..xxxhdpi}/ic_launcher*.png`；删除模板自适应 XML 与 webp | 桌面图标 |

---

## G. 工程、协议与合规（后续补充要求）

| # | 需求 | 状态 | 实现位置 |
|---|---|---|---|
| G1 | 统一更名为 **ikeback** | ✅ | 插件元信息、标题、Android `app_name`、包名 `top.ikee002.ikeback`、数据目录 `%APPDATA%/ikeback`、广播前缀 |
| G2 | 删除 RN 残留，**移植到你的 Kotlin 项目** | ✅ | `mobile/` 只剩 Android 工程；7 个 Kotlin 文件 |
| G3 | **每一个说明文件都写清楚、不要有纠纷** | ✅ | 根 `README.md`、`NOTICE.md`、`LICENSE`、`veyon/README.md`、8 份 docs、`plugins/classroom/README.md`、`scripts/README.md`、`mobile/README.md` |
| G4 | 法律与合规文档 | ✅ | `LICENSE`(GPL v2 全文)、`NOTICE.md`（含 Logo 归属）、`IKEBACK-LICENSE.md`、`IKEBACK-THIRD-PARTY.md`、`IKEBACK-PRIVACY.md` |
| G5 | **协议规范**写死兼容铁律 | ✅ | `IKEBACK-PROTOCOL.md`：全量枚举数值表、HTTP 路由、认证公式与基准向量、UDP 报文、数据格式、「只追加不插入/重排」 |
| G6 | 清理冗余、只保留一份 Android 工程 | ✅ | 删除 `veyon/mobile/` 历史副本与两个废弃 ZIP 打包脚本；所有文档引用已同步 |
| G7 | **Releases 三类产物全都有** | ✅ | 当前 `v1.3.0`：教师端 `setup.exe` + 学生端 `setup.exe` + APP `apk` + 校验 + 说明，**GitHub Actions 真实编译**；旧 ZIP / preset 包已删除 |
| G8 | GitHub 真实编译路线 | ✅ | `ikeback-build.yml`(Linux/Qt6)、`ikeback-windows.yml`(MinGW)、`ikeback-release.yml`(三产物)；`IKEBACK-BUILD.md` |
| G9 | 安装器、依赖闭包与自检 | ✅ | `collect_mingw_deps.py`(ntldd 递归)、`collect-windows-runtime.sh`、`ikeback-installer.nsi`、`generate_windows_icons.py`、`ClassroomSelfTest.cpp`、`IkebackUnitTest.kt` |
| G10 | **桌面端统一改名与换图标** | ✅ | `WindowsBuildHelpers.cmake` 产品名改 ikeback；Logo 生成 master/configurator/installer/uninstaller `.ico`；安装器与快捷方式显示「ikeback 教师端/学生端」 |
| G11 | **沿用上游组件式安装**，学生端不出现 Master | ✅ | `ikeback-installer.nsi` 基于 `nsis/veyon.nsi.in`：教师端「服务端(RO) + 教师控制台(可选)」，学生端只有「服务端(RO)」，自动注册/启动服务 |
| G12 | 安装/卸载 **不再弹 `__fastfail` 崩溃框**（沿用原有 `ikeback-wcli.exe`） | ✅ | 补齐并硬校验启动必需的 `windows-platform.dll` 与 Qt6 版 `libqca-ossl.dll`；用自制 `interception.h` 桩与 MinGW 头文件兼容修复让平台插件可编译；安装流程保持上游 wcli 不变 |
| G13 | APP 密钥支持**手动输入 + 上传 `.key` 文件** | ✅ | `ActivityResultContracts.OpenDocument`，支持纯文本 / `key=…` / JSON（`sessionKey`/`key`/`lessonKey`/`密钥`）三种格式 |
| G14 | APP **底部导航栏 5 项** + 学生加减分 | ✅ | 课堂 / 加分 / 控制 / 课表 / 更多；加分页点卡片选 -5…+5、填原因、乐观更新失败回滚 |
| G15 | **教师端网格拖拽排序**（下次开机不变）+ 手机按序生成卡片 | ✅ | `ClassroomSeatGrid` 拖放交换 → `seat-order-<班级>.json` 即时持久化 → Bridge `/seats` 按序返回 → APP 按序渲染 |
| G16 | APP **课表功能**（参考 ClassBoard，不照搬）+ **平板适配** | ✅ | 零依赖 `TimetableStore`（周网格、当前节高亮、下一节提示、导入作息 JSON）；`sw600dp`/`sw720dp` 调整边距、格宽与卡片列数 |
| G17 | UI 按交互规范重做（去阴影、同心圆角、常驻标签） | ✅ | `IKEBACK-UI.md` 记录验收清单；`UiKit.kt` / `BottomBar.kt` 落地，胶囊指示随选中项平滑移动 |
| G18 | **教师端/学生端默认中文**，配置里可切换 | ✅ | CI 用 `lrelease` 生成 `veyon_zh_CN.qm` + Qt `qt*_zh_CN.qm` 装到 `translations/`；`TranslationLoader` 未配置时默认 `zh_CN`；Configurator → General → Language 可改 |
| G19 | 窗口标题、任务栏、启动页、Configurator、关于、服务名统一改名换图标 | ✅ | `MainWindow.ui`×2、`AboutDialog`、`VeyonServiceControl`、`ServiceControlCommands`；`generate_windows_icons.py` 生成 `core/resources/icon*.png` 与 `master/resources/splash.png`（任务栏取 qrc 图，只换 .ico 无效） |
| G20 | **安装后服务自检**：wcli 返回码 + 安装日志 + `sc query` 验证 + 失败兜底 | ✅ | `ikeback-installer.nsi` 捕获 `service register/start` 退出码写 `$INSTDIR\install.log`，未 RUNNING 时用 `sc.exe` 兜底并明确提示 |
| G21 | Veyon Master 缩略图下显示 **姓名 + 当前分数** | ✅ | 插件按 IP 写 `ikebackScore` 动态属性到 `ComputerControlInterface`；`ComputerControlListModel` 拼接标签，零 ABI 改动 |
| G22 | 学生端**卸载保护**：教师端可开启/解除 | ✅ | `SetUninstallProtection`(39) → worker 写 HKLM → `uninstall.exe` 检测并 Abort；教师端「System & devices」页两个按钮 |
| G23 | Master 工具栏：图标缩小、常用置顶、不常用折叠 | ✅ | `MainToolBar` 32→24 并恢复 Qt `>>` 溢出按钮；`MainWindow::addFeaturesToToolBar` 按白名单分组 + 分隔线 |

---

## H. 明确未完成 / 限制（不含糊）

| # | 事项 | 原因 / 下一步 |
|---|---|---|
| H1 | **iOS 工程** | 只提供了 Kotlin/Android 工程。协议纯标准，后续建 SwiftUI + URLSession + NWUDP 可直接复用，无需改教师机 |
| H2 | **真实机房联调** | UWF 开关/监听、Core Audio 静音、WFP 断网、远程装软件、超管防退出均只经过编译验证，需在企业版/教育版 + UWF 学生机上实测 |
| H3 | **精确 URL 采集** | 当前基于窗口标题/进程名/浏览器判定；如需 URL 需浏览器扩展或 UI Automation |
| H4 | **UWF 自动状态机** | 目前是分步手动（查询 → 关闭+重启 → 分发安装 → 开启+重启），刻意不做全自动，避免 UWFTool 文档里「忘恢复保护」的严重坑 |
| H5 | APK 为 **debug 签名** | 内测包；正式分发需自行签名 |
| H6 | 桌面端「完整全量编译」不可行 | 上游 `plugins/filetransfer` 在 Qt5/Qt6 下都编不过（缺 `<QUuid>`、`qApp` 类型），CI 用 `--target classroom ...` 精确列出目标绕开 |
| H7 | 学生机是超管登录，纯用户态无法 100% 防杀进程 | 见 A5 备注 |

---

## I. 使用入口速查

**教师端**（工具栏「ikeback」→ 控制台）

| 页签 | 能做什么 |
|---|---|
| 课堂与登录 | 班级名单 / CSV / 按 IP 登录 / 批量登录 / 换密钥 / 同步 IP 到目录 |
| 行为日志 | 按日期·班级·学生·机器·关键词筛选、导出 CSV、清理过期 |
| 兴趣分析 | 多堂课动态兴趣分布、趋势、分层建议、导出 |
| AI 设置 | 本地/网络地址、密钥、模型、系统提示词、超时、测试连接、推送 |
| Score & reports | 每日积分录入、富文本明细、规则发布、每生词云、期末 `.xls`/`.csv` 导出、呼叫处理 |
| System & devices | 静音 / 断网 / USB / 肃静 / 远控 / 分组投屏 / 广播 / UWF / 装软件 / Bridge / 手动同步 |
| Seats & check-in | **开始签到 / 清空结果**、名单比对（已到·姓名不符·未到）、实时座位缩略图、点击 `XX号:XX分` 选 -5~+5 并填原因 |

**学生端悬浮条**：登录（座位号+姓名+密钥）、AI assistant、Score、Call teacher、受保护退出。

**手机 App**：自动发现 / 手动 IP·端口·本堂课密钥 / 立即同步 / 班级快照 / 13 项控制 / 期末表导出 / 词云 / 活动日志 / 离线缓存。

**编译与发布**：见 [`IKEBACK-BUILD.md`](IKEBACK-BUILD.md) 与 [`IKEBACK-RELEASE.md`](IKEBACK-RELEASE.md)。
部署与合规：见 [`IKEBACK-DEPLOY.md`](IKEBACK-DEPLOY.md) 与 [`IKEBACK-PRIVACY.md`](IKEBACK-PRIVACY.md)。
