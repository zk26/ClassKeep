# 课守 ClassKeep {VER}

三种产物均由 GitHub Actions **真实编译**，不是占位包：

| 产物 | 文件 | 说明 |
|---|---|---|
| 教师端安装程序 | `ClassKeep-Teacher-{VER}-win64-setup.exe` | Windows + MSYS2 UCRT64 + Qt6/MinGW；NSIS 安装器，组件页含「服务端（必选）」+「教师控制台（可选）」 |
| 学生端安装程序 | `ClassKeep-Student-{VER}-win64-setup.exe` | 同一套运行时，组件页**只有「服务端（必选）」**，不安装也不出现 Master |
| APP | `ClassKeep-Mobile-{VER}.apk` | Ubuntu + JDK 21 + Android SDK，`:app:assembleDebug` + 单元测试 |
| 校验 | `SHA256SUMS.txt` | 覆盖上述三者 |

安装器沿用 Veyon 官方安装流程：自动注册并启动 Veyon Service、放行防火墙、启用 SAS，
并把 `classroom.dll` 等插件、Qt6/CRT 运行时 DLL 与 Qt 插件安装到对应目录。
自 {VER} 起运行时依赖由 `ntldd` 递归解析 PE 导入闭包收集，**不再使用手工 DLL 白名单**
（旧版会报 `libbz2-1.dll` / `libmd4c.dll` 缺失）。
安装/卸载沿用 Veyon 原有的 `veyon-wcli.exe`；该 CLI 启动必需
`plugins/windows-platform.dll` 与 Qt6 版 `crypto/libqca-ossl.dll`，
发布流水线已对两者做硬校验，不再出现「快速异常检测失败」崩溃框。

## 本版新增

- 教师端座位网格支持**拖拽排序**，顺序即时保存、下次开机不变；
  Bridge 按该顺序返回学生，APP 加分页据此生成卡片。
- APP 重做为 5 个底部标签：课堂 / 加分 / 控制 / 课表 / 更多。
  密钥可手动输入或导入 `.key` 文件；学生加减分 `-5…+5` 步长 1，
  填原因后乐观更新、失败回滚；新增周课表与平板适配（`sw600dp` / `sw720dp`）。
- UI 按交互规范重做：卡片去阴影、同心圆角、常驻标签与帮助文字、
  底部胶囊指示随选中项平滑移动。详见 `veyon/docs/CLASSKEEP-UI.md`。
- {VER} 修复手机端适配：底部导航栏改为固定高度、所有子控件使用显式高度，
  解决窄屏上胶囊被竖向拉满全屏、内容区变白屏的问题。

## 校验

```powershell
Get-Content SHA256SUMS.txt | ForEach-Object {
    $hash, $file = $_ -split '\s+', 2
    if ((Get-FileHash $file -Algorithm SHA256).Hash.ToLower() -ne $hash) { throw "校验失败: $file" }
}
```

## 安装顺序

1. **学生机**：以管理员运行学生端安装 EXE，保持「服务端」勾选；安装完即已自动注册并启动服务。
2. **教师机**：以管理员运行教师端安装 EXE，保持「教师控制台」勾选；再用 Veyon Configurator
   导入/配置认证密钥。
3. **手机**：安装 APK，输入本堂课密钥与教师机 IP/端口。

安装目录内有 `README.TXT` 中文说明；完整部署流程见 `veyon/docs/CLASSKEEP-DEPLOY.md`。

## 已知限制

- Android APK 是 **debug 签名**（非发布签名），仅供内测；正式分发请自行签名。
- 构建产物来自 MSYS2 UCRT64/MinGW，未在真实 UWF 学生机上做安装联调；部署前请先在单台机器验证。
- 学生机须为 Windows 10/11 企业版、教育版或 IoT 企业版（UWF 仅这些版本支持）。
- 上游 `plugins/filetransfer` 在 Qt5/Qt6 下都无法编译，未包含在安装包中；
  远程装软件功能请通过课守控制台的「分发并安装」或系统自带部署方式完成。

## 合规

- 许可证 **GPL-2.0-or-later**（`LICENSE` / `NOTICE.md` / `COPYING`）
- 分发义务见 `veyon/docs/CLASSKEEP-LICENSE.md`；第三方依赖见 `veyon/docs/CLASSKEEP-THIRD-PARTY.md`
- 本软件采集学生行为数据，部署前**必须**阅读 `veyon/docs/CLASSKEEP-PRIVACY.md`
  并完成告知、目的限定、访问控制与保存期限评估

源码：<https://github.com/zk26/ClassKeep>
