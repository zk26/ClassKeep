# 课守 ClassKeep 发布产物规范

仓库：<https://github.com/zk26/ClassKeep> · 许可 **GPL-2.0-or-later**。

每个 Release 必须同时提供**教师端安装程序、学生端安装程序、APP** 三类产物，
另加校验文件与发布说明。

> **合规前置**：分发二进制前必须阅读 [`CLASSKEEP-LICENSE.md`](CLASSKEEP-LICENSE.md)、
> [`../../NOTICE.md`](../../NOTICE.md) 与 [`CLASSKEEP-THIRD-PARTY.md`](CLASSKEEP-THIRD-PARTY.md)，
> 随产物附 `LICENSE` / `NOTICE.md` / `COPYING` 并提供完整源码获取方式。
> 安装后的数据合规义务见 [`CLASSKEEP-PRIVACY.md`](CLASSKEEP-PRIVACY.md)。

---

## 1. 版本号与命名

版本格式 `vMAJOR.MINOR.PATCH`，与 `mobile/app/build.gradle.kts` 的 `versionName` 一致。

| 产物 | 文件名 | 格式 |
|---|---|---|
| 教师端安装程序 | `ClassKeep-Teacher-<ver>-win64-setup.exe` | NSIS 安装器（单文件 EXE） |
| 学生端安装程序 | `ClassKeep-Student-<ver>-win64-setup.exe` | NSIS 安装器（单文件 EXE） |
| APP | `ClassKeep-Mobile-<ver>.apk` | Android APK |
| 校验 | `SHA256SUMS.txt` | 覆盖上述三者 |
| 发布说明 | `RELEASE-NOTES.md` | 由 `scripts/release-notes-template.md` 生成 |

**三类产物都是真实编译的**，不含占位包、空包或伪造二进制。
仓库内不提交预编译二进制（`dist/`、`*.apk`、`*.exe` 安装包均由根 `.gitignore` 排除）。

> 历史：`v1.0.0` 曾发布 `-preset` 配置包，`v1.1.0`～`v1.2.0` 发布 ZIP 绿色包。
> 这些旧包因运行时 DLL 不全而无法正常启动，**已删除**。
> 自 `v1.3.0` 起改为 NSIS 安装 EXE，并用 `ntldd` 递归收集完整依赖闭包。

---

## 2. 两个安装程序的差别

沿用上游 `nsis/veyon.nsi.in` 的组件式安装流程：安装器通过原有的
`ClassKeep-wcli.exe` 注册/启动 Veyon Service、放行防火墙、启用 SAS。
该 CLI 在平台插件或 QCA 插件缺失时会 `qFatal`（UCRT `__fastfail`）并弹出
「快速异常检测失败」对话框，因此 payload 必须包含且硬校验
`windows-platform.dll` 与 Qt6 版 `libqca-ossl.dll`（见下节）。

| ROLE | 组件页 | 安装的额外文件 |
|---|---|---|
| `teacher` | 「服务端（必选，只读）」+「教师控制台（可选，默认勾选）」 | `ClassKeep-master.exe` + 控制台快捷方式 |
| `student` | **只有「服务端（必选，只读）」** | **不含 `ClassKeep-master.exe`，组件页也没有 Master 选项** |

两者共用同一份 `payload/`（Veyon 可执行文件、`veyon-core.dll`、全部 Veyon 插件、
Qt6/CRT 运行时 DLL、Qt 插件、QCA 加密插件、文档与 `ClassKeep.ico`），差别在编译期
决定——学生端安装器里根本没有 Master 段，等价于上游的 `/NoMaster` 行为。

### payload 目录结构

```
ClassKeep-server.exe / ClassKeep-service.exe / ClassKeep-worker.exe
ClassKeep-configurator.exe / ClassKeep-cli.exe / ClassKeep-wcli.exe
ClassKeep-master.exe                    （教师端安装器才释放）
veyon-core.dll + 全部 Qt6/CRT 运行时 DLL
plugins/classroom.dll + windows-platform.dll + 其它 Veyon 插件 DLL
platforms/qwindows.dll
styles/ tls/ imageformats/ crypto/libqca-ossl.dll
LICENSE.TXT / COPYING.TXT / README.TXT / ClassKeep.ico
```

> 运行时 DLL 由 `scripts/collect_mingw_deps.py` 用 `ntldd` 递归解析 PE 导入闭包收集，
> **不再使用文件名白名单**——白名单曾漏掉 `libbz2-1.dll`、`libmd4c.dll`、
> `libbrotli*.dll` 等，导致安装后所有 EXE 报「找不到 DLL」。
>
> `collect-windows-runtime.sh` 还会**硬校验** `plugins/windows-platform.dll`、
> `crypto/libqca-ossl.dll`（Qt6 版）、`libqca-qt6.dll`、`platforms/qwindows.dll`。
> 前两者是 VeyonCore 启动必需件：缺失时 `PlatformPluginManager` / `CryptoCore`
> 会 `qFatal`，表现为所有程序 `__fastfail` 崩溃（`v1.3.0` 的安装弹窗即由此引起）。

---

## 3. 构建来源

由 [`../../.github/workflows/classkeep-release.yml`](../../.github/workflows/classkeep-release.yml)
在推送 `v*` tag 或手动 `Run workflow` 时构建：

| Job | Runner | 产出 |
|---|---|---|
| `android` | `ubuntu-24.04` + JDK 21 + Android SDK | `ClassKeep-Mobile.apk` |
| `windows` | `windows-latest` + MSYS2 **UCRT64** + Qt6/MinGW + ntldd + NSIS | 两个 `*-setup.exe` |
| `package` | `windows-latest` | 组装三类产物 + `SHA256SUMS.txt` + 发布说明 |
| `release` | `ubuntu-24.04` | 校验后创建 Release 并上传全部 assets |

Windows 依赖、3rdparty 锁定 SHA 与编译目标详见
[`CLASSKEEP-BUILD.md`](CLASSKEEP-BUILD.md) 路线 D。
另有 [`../../.github/workflows/classkeep-windows.yml`](../../.github/workflows/classkeep-windows.yml)
可单独手动触发，只做 Windows 编译与安装器验证。

---

## 4. 本地生成安装程序

在装有 MSYS2 UCRT64（含 `ntldd` / `nsis` / `python`）的机器上，仓库根执行：

```bash
bash veyon/scripts/collect-windows-runtime.sh
mkdir -p dist
{ printf '\xEF\xBB\xBF'; cat veyon/scripts/classkeep-installer.nsi; } > installer.nsi
makensis -DVERSION=2.0.0 -DROLE=teacher installer.nsi
makensis -DVERSION=2.0.0 -DROLE=student installer.nsi
```

`payload/` 由第一条命令从 `build/` 组装；图标 `.ico` 已提交进仓库，CI 不重新生成。

---

## 5. 校验

`SHA256SUMS.txt` 由 `package` job 自动生成，覆盖三类产物：

```
<sha256小写>  ClassKeep-Mobile-1.3.0.apk
<sha256小写>  ClassKeep-Student-1.3.0-win64-setup.exe
<sha256小写>  ClassKeep-Teacher-1.3.0-win64-setup.exe
```

```powershell
Get-Content SHA256SUMS.txt | ForEach-Object {
    $hash, $file = $_ -split '\s+', 2
    if ((Get-FileHash $file -Algorithm SHA256).Hash.ToLower() -ne $hash) { throw "校验失败: $file" }
}
```

`release` job 会用 `sha256sum -c` 再校验一次，任一不符即失败。

---

## 6. 安装顺序

1. **学生机**：以管理员运行学生端 `setup.exe`，保持「服务端」勾选；安装完成后
   Veyon Service 已自动注册并启动。再以管理员运行 `ClassKeep-configurator.exe`
   导入教师机导出的认证配置。
2. **教师机**：以管理员运行教师端 `setup.exe`，保持「教师控制台」勾选；
   用 Veyon Configurator 完成认证配置，再运行 `ClassKeep-master.exe`。
3. **手机**：安装 APK，输入本堂课密钥与教师机 IP/端口。

安装目录内的 `README.TXT` 有中文说明；完整流程见
[`CLASSKEEP-DEPLOY.md`](CLASSKEEP-DEPLOY.md)。

> 学生机须为 Windows 10/11 **企业版、教育版或 IoT 企业版**（UWF 仅这些版本支持）。
> UWF 排除项添加后必须**重启**，不能用「关机再开机」代替快速启动。

---

## 7. 已知限制

- Android APK 是 **debug 签名**内测包，正式分发需自行签名。
- 上游 `plugins/filetransfer` 在 Qt5/Qt6 下都编不过，未随包分发；
  课守控制台的「分发并安装」依赖该功能，请改用系统自带部署方式。
- 构建产物未在真实 UWF 学生机上做安装联调，部署前请先在单台机器验证。
