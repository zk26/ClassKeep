# `plugins/classroom` — ikeback 插件

本目录是 ikeback 的全部桌面端实现，作为 Ikeback 插件编译（`plugins/CMakeLists.txt` 的 GLOB 自动发现，无需注册）。

- 仓库：<https://github.com/zk26/ikeback> · 本目录位于 `desktop/plugins/classroom/`
- 许可：**GPL-2.0-or-later**（`SPDX-License-Identifier: GPL-2.0-or-later`），
  见仓库根 [`../../../LICENSE`](../../../LICENSE) 与 [`../../../NOTICE.md`](../../../NOTICE.md)
- 总览：[`../../docs/IKEBACK.md`](../../docs/IKEBACK.md)
- 协议（命令字、参数、认证）：[`../../docs/IKEBACK-PROTOCOL.md`](../../docs/IKEBACK-PROTOCOL.md)
- 部署与数据合规：[`../../docs/IKEBACK-DEPLOY.md`](../../docs/IKEBACK-DEPLOY.md)、[`../../docs/IKEBACK-PRIVACY.md`](../../docs/IKEBACK-PRIVACY.md)

## 文件职责

| 文件 | 职责 |
|---|---|
| `ClassroomProtocol.h` | **协议权威定义**：Feature UID、`Command`/`Argument` 枚举、事件类型、配置键、产品名与广播常量 |
| `ClassroomPlugin.h/.cpp` | 插件入口；Master / Server / Worker 三端消息分发、worker 生命周期与看门狗 |
| `ClassroomConfiguration.h` | 配置项宏（`Classroom` 注册表段） |
| `ClassroomConfigurationPage.h/.cpp` | Ikeback Configurator 配置页 |
| `ClassroomMasterDialog.h/.cpp` | 教师端控制台（**常用工具** / 课堂 / 日志 / 分析 / AI / 监控 / 系统设备 / 积分报表 / 座位签到 / **课表与课堂**） |
| `ClassroomScorePanel.h/.cpp` | 积分录入、规则发布、每生词云、期末报表导出（含逐条明细） |
| `ClassroomSeatGrid.h/.cpp` | **座位网格 + 列表视图**：可拖拽网格显示实时缩略图与 `🛡 姓名 [积分]`；列表视图不抓屏、显示登录名/IP/在线/客户端保护/UWF/积分等全部信息 |
| `ClassroomClient.h/.cpp` | 学生端**不置顶、不可关闭**的悬浮卡片（姓名+座位号+查询积分/AI 两个按钮）、AI 聊天、积分弹窗、肃静页、签到弹窗、行为采集与模糊规则拦截 |
| `ClassroomAiClient.h/.cpp` | OpenAI 兼容 HTTP 客户端 |
| `ClassroomBridge.h/.cpp` | 教师机 HTTP/JSON + UDP 发现 + 课堂密钥挑战应答认证 |
| `ClassroomData.h/.cpp` | 名单、`ClassroomLogStore` 日志存储、兴趣分析、内存状态索引 |
| `ClassroomScore.h/.cpp` | 积分册：节次、富文本明细、规则发布、学生端 HTML |
| `ClassroomTermReport.h/.cpp` | 期末 `.xls`（Excel 2003 SpreadsheetML，含明细工作表）/ CSV / 明细 CSV 导出 |
| `ClassroomWordCloud.h/.cpp` | 中文分词、螺旋布局、SVG 与位图词云 |
| `ClassroomSystem.h/.cpp` | UWF / Core Audio / 防火墙 / 安装命令（仅 Windows 系统 API） |
| `ClassroomSecrets.h/.cpp` | 密钥 DPAPI 落盘与 HMAC-SHA256 请求签名 |
| `ClassroomSelfTest.cpp` | 零依赖自检用例（`WITH_TESTS=ON`） |
| `CMakeLists.txt` | 构建与自检目标 |

## 三端职责

| 组件 | 进程 | 主要工作 |
|---|---|---|
| Master | `ikeback-master` | 教师端控制台、名单/设置下发（**密钥不下发**）、日志与积分主库、Bridge 服务、AI 模型代理 |
| Server | `ikeback-server`（SYSTEM） | FeatureMessage 转发、离线事件补传队列、worker 看门狗、UWF/网络/音频系统操作入口 |
| Worker | `ikeback-worker`（登录用户） | 悬浮条、登录认证、AI/积分/肃静窗口、行为采集、上报事件 |

## 5.2.0 常用工具直接在一级工具栏

> **不变式（必读）**：`sessionKey` / `aiApiKey` 只允许存在于教师机与教师手机。
> 任何发给 `ikeback-server` / `ikeback-worker` 的设置都必须先经过
> `ClassroomSettings::forStudents()`；学生端只收 `keyConfigured` 布尔值，
> 退出保护只认管理员凭据，AI 只提交 prompt 由教师机代理调用模型。

5.2.0 起，常用工具是 Ikeback 主工具栏上的**一级按钮**（与「屏幕演示」「远程控制」
同级）：`ClassroomPlugin::buildQuickActionFeatures()` 为白名单里的每个动作生成一个
`Action|Master` Feature，工具栏放不下时自动换行，成对的开 / 关按钮保持相邻。
「Quick tools」下拉菜单与「Check-in」按钮仍保留，并复用同一份动作实现
（`ClassroomMasterDialog::quickToolGroups()` / `runQuickTool()`）；控制台第一个
页签只保留作用域与参数（肃静文案、模糊规则、软件路径、网址）。
下列动作按「全部学生 / 选中学生」作用域下发：

| 分组 | 按钮 |
|---|---|
| 广播 | 全部广播、选中广播（默认禁 USB 输入、**屏幕正常显示**）、边看边做（窗口演示，学生可操作）、停止广播 |
| 客户端保护 | 启动 / 关闭（防止学生端自行退出程序） |
| UWF 还原 | 启动 UWF / 关闭 UWF（**不自动重启**，便于装软件）/ 查询 UWF / 重启学生机 |
| 网关 | 禁用 / 启用 |
| 肃静页面 | 文案可设置、显示 / 隐藏 |
| 禁用应用 / 禁用地址 | 逗号或换行分隔的**模糊匹配**规则、应用 / 清空 |
| 打开软件 / 打开网页 | 路径 / 网址输入后下发 |
| 全班加减分 | +5 / +1 / -1 / -5 / 自定义 |
| 其他 | 全员静音、锁定/解锁、开始签到、刷新 |

> **三个开关互不相同**：`SetExitProtection`（客户端保护，学生退不出 worker）、
> `SetUninstallProtection`（卸载保护，学生卸不掉程序）、
> `UwfSetFilter`（系统还原，重启生效，装软件前关闭）。
> 详见 [`../../docs/IKEBACK-PROTOCOL.md`](../../docs/IKEBACK-PROTOCOL.md) 第 2 节。

## 修改约束

1. **`ClassroomProtocol.h` 中的枚举数值就是线上协议**（`FeatureMessage` 用 `QString::number(int)` 作参数键），
   **只允许在末尾追加**，禁止插入、删除、重排或改数值。详见协议文档第 1 节「兼容性铁律」。
2. 认证公式、UDP 广播前缀、数据文件字段属于破坏性变更，需提升协议版本并同步
   `ClassroomSelfTest.cpp` 与 Android `IkebackUnitTest.kt` 的断言。
3. 新增能力应复用本插件的 `FeatureMessage` 通道，不新增常驻服务、端口或第三方库。
4. 学生端保持**零落盘**；教师机离线时的事件走 Server 补传队列。
5. 任何涉及采集字段的改动，须同步更新 `IKEBACK-PROTOCOL.md` 与 `IKEBACK-PRIVACY.md`。

## 构建与自检

完整构建方案见 [`../../docs/IKEBACK-BUILD.md`](../../docs/IKEBACK-BUILD.md)。
Ikeback 4.11.4 **只能用 MinGW-w64/GCC 编译**（MSVC 不可行）。

```bash
# Windows / MinGW
cmake -S ikeback -B build -G Ninja -DWITH_QT6=ON -DWITH_TESTS=ON -DWITH_CORE_ONLY=OFF
cmake --build build
```

最快的编译验证是 GitHub Actions 的 `ikeback Build Check`（Linux/Qt6/GCC）。

Windows 下 `ClassroomSystem.cpp` 需要 Windows SDK（`mmdeviceapi.h` / `endpointvolume.h`）；
Linux/macOS 下相关功能走 `#else` 分支返回 `false`，不影响编译——
**注意这也意味着 Linux 编译无法验证 UWF / Core Audio / WFP 这些 Windows 专属实现**。
另外 `-DWITH_BUNDLED_LIBVNC=ON` 会强制关闭 `WITH_TESTS`，跑 `ClassroomSelfTest` 时需保持 OFF
并使用系统 LibVNC ≥ 0.9.14。
