# ikeback {VER}

三种产物均由 GitHub Actions **真实编译**，不是占位包：

| 产物 | 文件 | 说明 |
|---|---|---|
| 教师端安装程序 | `ikeback-teacher-{VER}-win64-setup.exe` | Windows + MSYS2 UCRT64 + Qt6/MinGW；NSIS 安装器，组件页含「服务端（必选）」+「教师控制台（可选）」 |
| 学生端安装程序 | `ikeback-student-{VER}-win64-setup.exe` | 同一套运行时，组件页**只有「服务端（必选）」**，不安装也不出现 Master |
| APP | `ikeback-Mobile-{VER}.apk` | Ubuntu + JDK 21 + Android SDK，`:app:assembleDebug` + 单元测试 |
| 校验 | `SHA256SUMS.txt` | 覆盖上述三者 |

安装器自动注册并启动 `ikebackService`、放行防火墙（含手机桥接端口 11880
TCP+UDP）、启用 SAS，并把 `classroom.dll` 等插件、Qt6/CRT 运行时 DLL 与 Qt 插件
安装到对应目录。安装结束前会**校验 `ikeback-server.exe` 是否真的起来**，
缺失时自动重启服务重试一次，仍失败则弹窗提示并写入 `install.log`。
运行时依赖由 `ntldd` 递归解析 PE 导入闭包收集，**不使用手工 DLL 白名单**。
安装/卸载使用 `ikeback-wcli.exe`；该 CLI 启动必需
`plugins/windows-platform.dll` 与 Qt6 版 `crypto/libqca-ossl.dll`，
发布流水线已对两者做硬校验。

## {VER} 重要变更

> **5.2.1 是经过 7 个内部测试版本打磨后的最终节点，也是 ikeback 的第一个正式版本。**

### 常用工具：一个功能一个按钮

工具栏上每个按钮都带中文名称与专属图标（图标下方就有名字）：
**保护、UWF、外网、肃静页、禁用应用、禁用网址、静音**都是单个开关按钮——
点一下切换，不再出现“开/关”两个按钮；**全班 +1 / 全班 -1** 直接打分；
**远程装软件**一键选择安装包并下发到学生机安装。
广播类功能不再重复实现，直接使用 ikeback Master 自带的「屏幕演示」与
「远程控制」。一行放不下时自动换行。「常用工具」下拉与「签到」按钮同时保留。

### 班级名单批量导入

- 「导入表格」支持带「班级」列的 CSV/TSV：一次导入多个班，班级 + 座位号 + 姓名 + IP
  一步到位；无表头时按 4 列 / 3 列自动推断布局。
- 「从 Excel 粘贴」：在 Excel/WPS 里选中区域复制，直接粘贴导入。
- 「模板」按钮导出可填写的 CSV。
- 解析器支持带引号字段、UTF-8 BOM 与 GBK（Excel 直接另存的中文 CSV 可用），
  导出再导入不会把 `1` 当成座位号。
- 同一班级内重复座位号、重复 IP 会被明确拒绝并指出冲突座位；
  不同班级可以使用相同 IP。

### 按 IP 快速登录 / 批量登录

登录学生时会把学生**真正写入当前班级名单**并刷新表格，不再出现
「输入了学生，表格里却没有」。批量登录同样补齐名单与座位号。

### 连通性自检与安装可靠性

- 「系统与设备」页新增：网卡地址下拉（避免多网卡选错 IP）、
  **连接自检**（探测本机 `127.0.0.1:11100+会话`、显示手机应填写的 IP:端口、
  说明红图标含义）、**一键重启 ikeback 服务**。
- 手机桥接默认开启，切换网卡或端口会真正重新绑定，不再停留在旧地址。
- Worker 看门狗连续失败 5 次后停止自动重启（下次教师操作重新武装），
  不再每 30 秒拉起一个新的 `ikeback-worker.exe`。
- 修正安装器把 `SoftwareSASEnabled` 写到不存在的 `Windows/` 配置段的问题
  （改为 `Service/SoftwareSASEnabled`）。

### 手机端

所有指令、同步、读取名单按钮点击即有反馈：正在下发 → 成功 / 被拒绝 /
无法连接；IP 或密钥缺失时直接提示并跳到「配置」页，不再「点了没反应」。

## 质量与可靠性

- **修复“服务已注册但 ikeback-server 没有起来、连不上”的问题**：
  根因是 VNC 服务端插件（`builtin-ultravnc-server`）从未被构建（构建目标名写错，
  被当作“跳过”），导致 `ikeback-server` 启动后打印
  `no VNC server plugins found!` 并退出。现在该插件是必需构建项，
  其它依赖（libsodium、旧版 Windows 常量映射等）也一并补齐。
- 安装器不仅检查服务状态，还校验 `ikeback-server.exe` 是否真正运行：
  缺失时自动重启服务重试一次，仍失败则明确提示并写入 `install.log`，
  不再出现「服务 RUNNING 但本机红图标、谁也连不上」的静默失败。
- 中文界面串与源码 `tr()` 源串保持一致，「关于」等界面不再回退英文。
- 安装器版本由推送的 tag 决定，APK `versionName` 为 {VER}、`versionCode` 为 10。

## 教师端 / 移动端功能

- 教师端首个页签 **Quick tools** 保留作用域与参数设置：全部/选中广播（默认禁 USB 输入
  但**屏幕正常显示**）、边看边做、客户端保护、UWF（**不自动重启**，便于装软件）、
  网关、可设置文案的肃静页、模糊匹配的禁用应用/禁用地址、打开软件/网页、全班加减分。
  **客户端保护（防止学生端自行退出）、卸载保护、UWF 还原是三个独立开关**。
- 座位视图支持**可拖拽网格 + 列表视图**：网格卡片显示 `🛡 姓名 [积分]`，
  列表视图不抓学生机屏、显示登录名 / IP / 在线 / 客户端保护 / UWF 状态 / 积分。
- 学生端悬浮卡片**不置顶、服务不退就关不掉**，显示姓名与座位号。
- 教师端「课表与课堂」页签：课表导入导出、当堂课讲授内容、当堂课表现评分、
  按班级归档的课堂记录。
- APP 为 **5 个底部标签（课堂 / 班级 / 课表 / 配置 / 更多）**：
  未配置时收缩提示、双作息课表、按班级展示课堂记录、导入导出收进设置、
  课表本地 + 教师机双写同步。

## 升级注意

- 安装程序会先停止并移除可能残留的服务与进程，安装过程可能需要十几秒，请耐心等待。
- 首次安装后请在 `ikeback Configurator` 中确认一次本机配置。
- 认证密钥与课堂数据目录见 `NOTICE.md` §4.1。

## 校验

```powershell
Get-Content SHA256SUMS.txt | ForEach-Object {
    $hash, $file = $_ -split '\s+', 2
    if ((Get-FileHash $file -Algorithm SHA256).Hash.ToLower() -ne $hash) { throw "校验失败: $file" }
}
```

## 安装顺序

1. **学生机**：以管理员运行学生端安装 EXE，保持「服务端」勾选；安装完即已自动注册并启动服务。
2. **教师机**：以管理员运行教师端安装 EXE，保持「教师控制台」勾选；再用 `ikeback Configurator`
   导入/配置认证密钥。
3. **手机**：安装 APK，输入本堂课密钥与教师机 IP/端口。

安装目录内有 `README.TXT` 中文说明；完整部署流程见 `desktop/docs/IKEBACK-DEPLOY.md`。

## 已知限制

- Android APK 是 **debug 签名**（非发布签名），仅供内测；正式分发请自行签名。
- 构建产物来自 MSYS2 UCRT64/MinGW，未在真实 UWF 学生机上做安装联调；部署前请先在单台机器验证。
- 学生机须为 Windows 10/11 企业版、教育版或 IoT 企业版（UWF 仅这些版本支持）。
- 上游 `plugins/filetransfer` 在 Qt5/Qt6 下都无法编译，未包含在安装包中。
- 上游 CLI 帮助文本等少量英文字符串仍含 `Ikeback` 字样（非产品名、非进程名），
  中文界面已统一显示为 ikeback。

## 合规

- 许可证 **GPL-2.0-or-later**（`LICENSE` / `NOTICE.md` / `COPYING`）
- 分发义务见 `desktop/docs/IKEBACK-LICENSE.md`；第三方依赖见 `desktop/docs/IKEBACK-THIRD-PARTY.md`
- 本软件采集学生行为数据，部署前**必须**阅读 `desktop/docs/IKEBACK-PRIVACY.md`
  并完成告知、目的限定、访问控制与保存期限评估

源码：<https://github.com/zk26/ikeback>
