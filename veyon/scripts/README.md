# `scripts/` — Windows 构建、依赖收集与安装器

本目录只保留**当前发布流程实际使用**的脚本。旧的 ZIP 打包脚本
（`package-classkeep.ps1` / `package-presets.ps1`）已删除——发布产物改为
两个 NSIS 安装 EXE + 一个 Android APK。

| 文件 | 职责 |
|---|---|
| `collect-windows-runtime.sh` | 在 MSYS2 UCRT64 下组装 `payload/`：编译产物、Veyon 插件、Qt/QCA 插件、文档与图标 |
| `collect_mingw_deps.py` | 用 `ntldd` 递归解析 PE 导入闭包，把所有需要的运行时 DLL 复制进 `payload/` |
| `classkeep-installer.nsi` | NSIS 安装器模板，按 `-DROLE=teacher\|student` 生成两个 setup EXE |
| `generate_windows_icons.py` | 从根 `ClassKeep_logo.png` 生成 `.ico`（需 Pillow，本机执行后提交） |
| `generate_icons.py`（`mobile/tools/`） | 从同一 Logo 生成 Android 全密度启动图标 |
| `windows-install-readme.txt` | 安装目录内的中文部署说明 |
| `release-notes-template.md` | Release 正文模板（`{VER}` 占位符） |
| `ClassKeep.ico` | 生成的图标，供安装器快捷方式与注册表显示使用 |

## 为什么用递归依赖收集

早期版本用「DLL 文件名前缀白名单」手工复制运行时库，漏掉了
`libbz2-1.dll`、`libmd4c.dll`、`libbrotli*.dll` 等间接依赖，安装后
`ClassKeep-server.exe` 直接报「找不到 DLL」。`collect_mingw_deps.py` 改为：

1. 遍历 `payload/` 下所有 `.exe` / `.dll`（含子目录）；
2. 对每个文件跑 `ntldd`，解析输出里所有 DLL 名；
3. 若 `/ucrt64/bin/<name>` 存在且目标里没有，就复制进来；
4. 重复直到一轮没有新增（系统 DLL 不在 `/ucrt64/bin`，由 Windows 提供，自动跳过）。

## 安装器（沿用上游 Veyon 组件式安装）

`classkeep-installer.nsi` 基于上游 `nsis/veyon.nsi.in` 的组件页结构：

| ROLE | 组件页 |
|---|---|
| `teacher` | 「服务端（必选，只读）」+「教师控制台（可选，`ClassKeep-master.exe`）」 |
| `student` | **只有「服务端（必选，只读）」**；编译时就不包含 Master 段 |

> 安装/卸载**沿用上游原有的 `ClassKeep-wcli.exe`**
> （`service register/unregister`、`config set`、`service start`）。
> 该 CLI 启动时会构造完整 `VeyonCore`，任何 `*-platform.dll` 或 QCA `rsa`
> 插件缺失都会 `qFatal` → UCRT `__fastfail`（「快速异常检测失败」对话框）。
> 修复方式是**把启动必需件补齐并硬校验**（见下节），而不是绕开 CLI。

## 启动必需件（缺一即崩）

`collect-windows-runtime.sh` 会对以下文件做**硬校验**，缺一个就让 CI 失败，
不再允许产出「装得上但一运行就崩」的安装包：

| 文件 | 缺失后果 |
|---|---|
| `plugins/windows-platform.dll` | `PlatformPluginManager.cpp:46` → `qFatal("no platform plugin available!")`，**所有** Veyon exe 启动即 `__fastfail` |
| `crypto/libqca-ossl.dll`（**必须是 Qt6 版**） | `CryptoCore.cpp:39` → `qFatal("RSA not supported!")`，同样 `__fastfail` |
| `libqca-qt6.dll` | `veyon-core.dll` 无法加载 |
| `platforms/qwindows.dll` | Qt 自身无法初始化 GUI |

`windows-platform` 依赖上游私有镜像里才有的 `-linterception`，MSYS2 不提供。
仓库提供了自制的最小兼容头
[`../3rdparty/interception/include/interception.h`](../3rdparty/interception/include/interception.h)，
让这个**必需**插件可以编译链接；Interception 驱动本身是可选的
（`useInterceptionDriver()` 默认关闭），stub 不改变默认行为，也不含任何第三方代码。
CI 已把 `windows-platform` 设为必需的独立构建步骤，不再静默跳过。

编译（CI 自动执行，需在仓库根、脚本文件带 UTF-8 BOM）：

```bash
{ printf '\xEF\xBB\xBF'; cat veyon/scripts/classkeep-installer.nsi; } > installer.nsi
makensis -DVERSION=2.2.0 -DROLE=teacher installer.nsi
makensis -DVERSION=2.2.0 -DROLE=student installer.nsi
```

输出：`dist/ClassKeep-Teacher-<ver>-win64-setup.exe`、
`dist/ClassKeep-Student-<ver>-win64-setup.exe`。

## 图标

`ClassKeep_logo.png`（仓库根，由仓库所有者提供）是唯一图标来源：

```bash
python veyon/scripts/generate_windows_icons.py   # 生成 .ico，提交进仓库
python mobile/tools/generate_icons.py            # 生成 Android mipmap PNG
```

CI 的 Windows job 没有 Pillow，直接消费已提交的 `.ico` / `.png`。
归属说明见仓库根 [`../../NOTICE.md`](../../NOTICE.md)。

## 发布

由仓库根 `.github/workflows/classkeep-release.yml` 完成：
Android job 出 APK → Windows job 出两个 setup EXE → package job 组装并生成
`SHA256SUMS.txt` / `RELEASE-NOTES.md` → release job 校验后上传。
详见 [`../docs/CLASSKEEP-RELEASE.md`](../docs/CLASSKEEP-RELEASE.md)。
