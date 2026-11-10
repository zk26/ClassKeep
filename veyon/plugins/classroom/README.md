# `plugins/classroom` — 课守 ClassKeep 插件

本目录是课守的全部桌面端实现，作为 Veyon 插件编译（`plugins/CMakeLists.txt` 的 GLOB 自动发现，无需注册）。

- 仓库：<https://github.com/zk26/ClassKeep> · 本目录位于 `veyon/plugins/classroom/`
- 许可：**GPL-2.0-or-later**（`SPDX-License-Identifier: GPL-2.0-or-later`），
  见仓库根 [`../../../LICENSE`](../../../LICENSE) 与 [`../../../NOTICE.md`](../../../NOTICE.md)
- 总览：[`../../docs/CLASSKEEP.md`](../../docs/CLASSKEEP.md)
- 协议（命令字、参数、认证）：[`../../docs/CLASSKEEP-PROTOCOL.md`](../../docs/CLASSKEEP-PROTOCOL.md)
- 部署与数据合规：[`../../docs/CLASSKEEP-DEPLOY.md`](../../docs/CLASSKEEP-DEPLOY.md)、[`../../docs/CLASSKEEP-PRIVACY.md`](../../docs/CLASSKEEP-PRIVACY.md)

## 文件职责

| 文件 | 职责 |
|---|---|
| `ClassroomProtocol.h` | **协议权威定义**：Feature UID、`Command`/`Argument` 枚举、事件类型、配置键、产品名与广播常量 |
| `ClassroomPlugin.h/.cpp` | 插件入口；Master / Server / Worker 三端消息分发、worker 生命周期与看门狗 |
| `ClassroomConfiguration.h` | 配置项宏（`Classroom` 注册表段） |
| `ClassroomConfigurationPage.h/.cpp` | Veyon Configurator 配置页 |
| `ClassroomMasterDialog.h/.cpp` | 教师端控制台（课堂/日志/分析/AI/监控/系统设备/积分报表/座位签到 8 个页签） |
| `ClassroomScorePanel.h/.cpp` | 积分录入、规则发布、每生词云、期末报表导出（含逐条明细） |
| `ClassroomSeatGrid.h/.cpp` | **座位网格**：实时缩略图、`XX号:XX分` 点击条、-5~+5 选择与原因输入 |
| `ClassroomClient.h/.cpp` | 学生端悬浮条、AI 聊天、积分明细、肃静页、**签到弹窗**、行为采集 |
| `ClassroomAiClient.h/.cpp` | OpenAI 兼容 HTTP 客户端 |
| `ClassroomBridge.h/.cpp` | 教师机 HTTP/JSON + UDP 发现 + 课堂密钥挑战应答认证 |
| `ClassroomData.h/.cpp` | 名单、`ClassroomLogStore` 日志存储、兴趣分析、内存状态索引 |
| `ClassroomScore.h/.cpp` | 积分册：节次、富文本明细、规则发布、学生端 HTML |
| `ClassroomTermReport.h/.cpp` | 期末 `.xls`（Excel 2003 SpreadsheetML，含明细工作表）/ CSV / 明细 CSV 导出 |
| `ClassroomWordCloud.h/.cpp` | 中文分词、螺旋布局、SVG 与位图词云 |
| `ClassroomSystem.h/.cpp` | UWF / Core Audio / 防火墙 / 安装命令（仅 Windows 系统 API） |
| `ClassroomSecrets.h/.cpp` | 密钥加密落盘与 SHA-256 挑战应答摘要 |
| `ClassroomSelfTest.cpp` | 零依赖自检用例（`WITH_TESTS=ON`） |
| `CMakeLists.txt` | 构建与自检目标 |

## 三端职责

| 组件 | 进程 | 主要工作 |
|---|---|---|
| Master | `veyon-master` | 教师端控制台、名单/密钥下发、日志与积分主库、Bridge 服务 |
| Server | `veyon-server`（SYSTEM） | FeatureMessage 转发、离线事件补传队列、worker 看门狗、UWF/网络/音频系统操作入口 |
| Worker | `veyon-worker`（登录用户） | 悬浮条、登录认证、AI/积分/肃静窗口、行为采集、上报事件 |

## 修改约束

1. **`ClassroomProtocol.h` 中的枚举数值就是线上协议**（`FeatureMessage` 用 `QString::number(int)` 作参数键），
   **只允许在末尾追加**，禁止插入、删除、重排或改数值。详见协议文档第 1 节「兼容性铁律」。
2. 认证公式、UDP 广播前缀、数据文件字段属于破坏性变更，需提升协议版本并同步
   `ClassroomSelfTest.cpp` 与 Android `ClassKeepUnitTest.kt` 的断言。
3. 新增能力应复用本插件的 `FeatureMessage` 通道，不新增常驻服务、端口或第三方库。
4. 学生端保持**零落盘**；教师机离线时的事件走 Server 补传队列。
5. 任何涉及采集字段的改动，须同步更新 `CLASSKEEP-PROTOCOL.md` 与 `CLASSKEEP-PRIVACY.md`。

## 构建与自检

完整构建方案见 [`../../docs/CLASSKEEP-BUILD.md`](../../docs/CLASSKEEP-BUILD.md)。
Veyon 4.11.4 **只能用 MinGW-w64/GCC 编译**（MSVC 不可行）。

```bash
# Windows / MinGW
cmake -S veyon -B build -G Ninja -DWITH_QT6=ON -DWITH_TESTS=ON -DWITH_CORE_ONLY=OFF
cmake --build build
```

最快的编译验证是 GitHub Actions 的 `ClassKeep Build Check`（Linux/Qt5/GCC 全量编译）。

Windows 下 `ClassroomSystem.cpp` 需要 Windows SDK（`mmdeviceapi.h` / `endpointvolume.h`）；
Linux/macOS 下相关功能走 `#else` 分支返回 `false`，不影响编译——
**注意这也意味着 Linux 编译无法验证 UWF / Core Audio / WFP 这些 Windows 专属实现**。
另外 `-DWITH_BUNDLED_LIBVNC=ON` 会强制关闭 `WITH_TESTS`，跑 `ClassroomSelfTest` 时需保持 OFF
并使用系统 LibVNC ≥ 0.9.14。
