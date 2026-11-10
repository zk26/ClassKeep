# 课守 ClassKeep {VER}

三种产物均由 GitHub Actions **真实编译**，不是占位包：

| 产物 | 文件 | 构建 |
|---|---|---|
| APP | `ClassKeep-Mobile-{VER}.apk` | Ubuntu + JDK 21 + Android SDK，`:app:assembleDebug` + 单元测试 |
| 教师端 | `ClassKeep-Teacher-{VER}-win64.zip` | Windows + MSYS2 UCRT64 + Qt6/MinGW，含 `veyon-master.exe` / `veyon-configurator.exe` |
| 学生端 | `ClassKeep-Student-{VER}-win64.zip` | 同上，含 `veyon-service.exe` / `veyon-server.exe` / `veyon-worker.exe` |
| 校验 | `SHA256SUMS.txt` | 覆盖全部产物 |

教师端与学生端使用同一套 Veyon 二进制（含 `classroom.dll`、Qt6/CRT 运行时 DLL 与 Qt 插件），
差别只在注入的预设配置和安装辅助脚本。

## 校验

```powershell
Get-Content SHA256SUMS.txt | ForEach-Object {
    $hash, $file = $_ -split '\s+', 2
    if ((Get-FileHash $file -Algorithm SHA256).Hash.ToLower() -ne $hash) { throw "校验失败: $file" }
}
```

## 使用

1. 学生机：以管理员运行 `install-student.ps1`，安装 Veyon Service，按 `config/uwf-exclusions.txt`
   添加 UWF 排除项后**重启**（不是关机再开机）。
2. 教师机：以管理员运行 `install-teacher.ps1`，打开 ClassKeep 控制台点「开始新课堂」。
3. 手机：安装 APK，输入本堂课密钥与教师机 IP/端口。

详见包内 `README.txt` 与 `docs/CLASSKEEP-DEPLOY.md`。

## 已知限制

- Android APK 是 **debug 签名**（非发布签名），仅供内测；正式分发请自行签名。
- 本构建产物来自 MSYS2 UCRT64/MinGW，未在真实 UWF 学生机上做安装联调；部署前请先在单台机器验证。
- 学生机须为 Windows 10/11 企业版、教育版或 IoT 企业版（UWF 仅这些版本支持）。

## 合规

- 许可证 **GPL-2.0-or-later**（`LICENSE` / `NOTICE.md` / `COPYING`）
- 分发义务见 `veyon/docs/CLASSKEEP-LICENSE.md`；第三方依赖见 `veyon/docs/CLASSKEEP-THIRD-PARTY.md`
- 本软件采集学生行为数据，部署前**必须**阅读 `veyon/docs/CLASSKEEP-PRIVACY.md`
  并完成告知、目的限定、访问控制与保存期限评估

源码：<https://github.com/zk26/ClassKeep>
