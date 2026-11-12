# ClassKeep {VER}

三种产物均由 GitHub Actions **真实编译**，不是占位包：

| 产物 | 文件 | 说明 |
|---|---|---|
| 教师端安装程序 | `ClassKeep-Teacher-{VER}-win64-setup.exe` | Windows + MSYS2 UCRT64 + Qt6/MinGW；NSIS 安装器，组件页含「服务端（必选）」+「教师控制台（可选）」 |
| 学生端安装程序 | `ClassKeep-Student-{VER}-win64-setup.exe` | 同一套运行时，组件页**只有「服务端（必选）」**，不安装也不出现 Master |
| APP | `ClassKeep-Mobile-{VER}.apk` | Ubuntu + JDK 21 + Android SDK，`:app:assembleDebug` + 单元测试 |
| 校验 | `SHA256SUMS.txt` | 覆盖上述三者 |

安装器自动注册并启动 `ClassKeepService`、放行防火墙、启用 SAS，
并把 `classroom.dll` 等插件、Qt6/CRT 运行时 DLL 与 Qt 插件安装到对应目录。
运行时依赖由 `ntldd` 递归解析 PE 导入闭包收集，**不使用手工 DLL 白名单**。
安装/卸载使用 `ClassKeep-wcli.exe`；该 CLI 启动必需
`plugins/windows-platform.dll` 与 Qt6 版 `crypto/libqca-ossl.dll`，
发布流水线已对两者做硬校验。

## {VER} 重要变更

> 本版本包含 2.1.0 的功能与体验修正，以及 2.2.0 的三项遗留整改。

### {VER} 三项遗留整改

- **自定义「姓名 + 座位号」登录**：教师端工具栏的 Veyon 原生「用户名 + 密码」登录按钮
  已移除，换成 ClassKeep 的班级 / 姓名 / 座位号对话框，下发 `SetIdentity` / `ForceLogout`。
  不收集、不加密、不传输任何 Windows 凭据，上游 `prepareLogon()` 路径不再被调用。
- **托盘图标与进程收敛**：学生机会话内自建 ClassKeep 托盘图标（含卡片 / 积分 / AI /
  受保护退出菜单）；控制台启动消息由三条合并为一条 `ApplySession`，watchdog 由 5s
  改为 30s，`StopClient` 不再新起 worker，`FeatureWorkerManager` 增加幂等启动、
  30s 冷却、立即回收进程与惰性消息队列。任务管理器里持续增长的十几个
  `ClassKeep-worker.exe` 已收敛为稳定的一组。
- **ClassKeep 控制台内嵌主界面**：由独立弹窗改为 `QDockWidget` 停靠在主窗口右侧，
  工具栏按钮显示/隐藏面板；登录、注销与控制台按钮一并移入主工具栏第一组。

### 2.1.0 已完成的改动

- **中文界面**：补齐 ClassKeep 插件全部 428 条界面字符串的简体中文翻译
  （Quick tools、配置器、座位视图、学生卡片此前全为英文源码回退）。
- **安装 / 卸载详情**：静默化清理阶段的 `sc stop` / `taskkill` 调用，不再满屏
  `[SC] OpenService 失败 1060`、`错误：没有找到进程 …` 与 GBK 中文乱码；
  安装成功进度仍正常显示。
- **教师端「课表与课堂」页签**：课表 JSON 导入/导出（与手机端共用同一文件）、
  当堂课讲授内容、当堂课全班表现评分、按班级归档的课堂自然记录。
- **手机端**：品牌头换成真实应用图标；未配置时状态卡收缩并提示前往配置页；
  新增底部「班级」页，课堂内容按班级自然展示（不再有「查询本堂课」）；
  导入导出与工具收进「更多 → 设置」；课表设置全部归入单一入口；按钮重叠修复。
- **课表重做**：参考 `ClassBoard` 的「北附」样式，双列 = 两套并行作息
  （小学 / 初高中），彩色跨节课程卡片、冻结时间轴、间歇行、今天与当前节高亮。
- **课表存储**：本地 `SharedPreferences` + 教师机
  `/api/classroom/timetable`（`%APPDATA%/ClassKeep/timetable.json`）双写；
  新增 `/api/classroom/classes` 供班级页读取。

### 2.0.0 已完成的命名统一

所有进程名、服务名与用户可见名称统一为英文 `ClassKeep` 前缀，
避免教师/管理员在任务管理器、服务控制台、防火墙规则里分不清程序归属：

| 旧（≤ 1.5.1） | 新（2.0.0） |
|---|---|
| `veyon-master.exe` / `veyon-configurator.exe` | `ClassKeep-master.exe` / `ClassKeep-configurator.exe` |
| `veyon-service.exe` / `veyon-server.exe` / `veyon-worker.exe` | `ClassKeep-service.exe` / `ClassKeep-server.exe` / `ClassKeep-worker.exe` |
| `veyon-cli.exe` / `veyon-wcli.exe` | `ClassKeep-cli.exe` / `ClassKeep-wcli.exe` |
| `VeyonService` | `ClassKeepService` |
| `课守 ClassKeep 教师端` 等中文产品名 | `ClassKeep Master` / `ClassKeep Student` |

改名清单、为兼容而保留的上游技术标识与 GPL 修改声明见 `NOTICE.md` §4.1。
CMake 构建目标仍为 `veyon-*`（仅编译期标识），实际产物名已全部改变。

## 修复

- **修复 1.5.1 升级安装失败**：报「无法打开要写入的文件
  `C:\Program Files\ClassKeep\Teacher\veyon-worker.exe`」的根因是
  `service unregister` 不会停止服务，旧 `VeyonService` 派生的 worker 仍占用文件句柄。
  现在安装/卸载前会 `sc stop` + `sc delete` 新旧服务、`taskkill` 新旧全部进程，
  最多等待 10 秒轮询文件句柄释放，仍被占用时给出可操作的中文提示而不是 NSIS 原始报错框。
- 自动清理 1.5.1 遗留的 `Teacher` 安装目录与旧卸载注册表项。
- 修正中文翻译目录与源码 `tr()` 源串不一致导致「关于」等界面回退英文的问题；
  中文译文中的 Veyon 品牌串统一为 ClassKeep。
- 统一版本号：`classkeep-windows.yml` 不再硬编码 1.3.0，APK `versionName` 同步为 {VER}。

## 教师端 / 移动端功能

- 教师端新增首个页签 **Quick tools**：全部/选中广播（默认禁 USB 输入但**屏幕正常显示**）、
  边看边做、启动/关闭客户端保护、启动/关闭 UWF（**不自动重启**，便于装软件）、
  禁用/启用网关、可设置文案的肃静页、模糊匹配的禁用应用/禁用地址、打开软件/网页、全班加减分等。
  **客户端保护（防止学生端自行退出）、卸载保护、UWF 还原是三个独立开关**。
- 座位视图支持**可拖拽网格 + 列表视图**：网格卡片显示 `🛡 姓名 [积分]`，
  列表视图不抓学生机屏、显示登录名 / IP / 在线 / 客户端保护 / UWF 状态 / 积分。
- 学生端悬浮卡片**不置顶、服务不退就关不掉**，显示姓名与座位号，只保留查询积分 / AI 两个弹窗按钮。
- APP 底部栏改为 **4 个标签：课堂 / 课表 / 配置 / 更多**，矢量图标 + 常显名称；
  「课堂」合并课堂控制与加分减分；课表可设置一天几节课、大课间/午休位置与时长、
  单列或双列时间；「更多」提供导入导出与课堂回顾（讲了什么 / 干了什么 / 同学表现）。

## 升级注意

- 2.0.0 安装程序会停止并移除旧服务与旧进程，安装过程可能需要十几秒，请耐心等待。
- `QCoreApplication` 组织名/应用名改为 `ClassKeep`，部分界面偏好会回到默认值；
  首次升级后请在 `ClassKeep Configurator` 中确认一次配置。
- 认证密钥与课堂数据目录刻意保持原路径（见 `NOTICE.md` §4.1），不会丢失。

## 校验

```powershell
Get-Content SHA256SUMS.txt | ForEach-Object {
    $hash, $file = $_ -split '\s+', 2
    if ((Get-FileHash $file -Algorithm SHA256).Hash.ToLower() -ne $hash) { throw "校验失败: $file" }
}
```

## 安装顺序

1. **学生机**：以管理员运行学生端安装 EXE，保持「服务端」勾选；安装完即已自动注册并启动服务。
2. **教师机**：以管理员运行教师端安装 EXE，保持「教师控制台」勾选；再用 `ClassKeep Configurator`
   导入/配置认证密钥。
3. **手机**：安装 APK，输入本堂课密钥与教师机 IP/端口。

安装目录内有 `README.TXT` 中文说明；完整部署流程见 `veyon/docs/CLASSKEEP-DEPLOY.md`。

## 已知限制

- Android APK 是 **debug 签名**（非发布签名），仅供内测；正式分发请自行签名。
- 构建产物来自 MSYS2 UCRT64/MinGW，未在真实 UWF 学生机上做安装联调；部署前请先在单台机器验证。
- 学生机须为 Windows 10/11 企业版、教育版或 IoT 企业版（UWF 仅这些版本支持）。
- 上游 `plugins/filetransfer` 在 Qt5/Qt6 下都无法编译，未包含在安装包中。
- 上游 CLI 帮助文本等少量英文字符串仍含 `Veyon` 字样（非产品名、非进程名），
  中文界面已统一显示为 ClassKeep。

## 合规

- 许可证 **GPL-2.0-or-later**（`LICENSE` / `NOTICE.md` / `COPYING`）
- 分发义务见 `veyon/docs/CLASSKEEP-LICENSE.md`；第三方依赖见 `veyon/docs/CLASSKEEP-THIRD-PARTY.md`
- 本软件采集学生行为数据，部署前**必须**阅读 `veyon/docs/CLASSKEEP-PRIVACY.md`
  并完成告知、目的限定、访问控制与保存期限评估

源码：<https://github.com/zk26/ClassKeep>
