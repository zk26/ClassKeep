# `scripts/` — 发布打包脚本

- 仓库：<https://github.com/zk26/ClassKeep> · 本目录位于 `veyon/scripts/`
- 许可：**GPL-2.0-or-later**（见仓库根 [`../../LICENSE`](../../LICENSE) 与 [`../../NOTICE.md`](../../NOTICE.md)）
- 产物规范：[`../docs/CLASSKEEP-RELEASE.md`](../docs/CLASSKEEP-RELEASE.md)
- 分发合规：[`../docs/CLASSKEEP-LICENSE.md`](../docs/CLASSKEEP-LICENSE.md)、[`../docs/CLASSKEEP-THIRD-PARTY.md`](../docs/CLASSKEEP-THIRD-PARTY.md)

## `package-classkeep.ps1`

把**已经编译好**的三类产物组装成一个可发布的目录。

```powershell
.\scripts\package-classkeep.ps1 `
    -Version 1.0.0 `
    -TeacherDir D:\build\teacher\bin `
    -StudentDir D:\build\student\bin `
    -ApkPath D:\build\mobile\app-debug.apk `
    -OutputDir dist
```

| 参数 | 必填 | 说明 |
|---|---|---|
| `-Version` | 是 | 版本号（不含 `v`），与 `mobile/app/build.gradle.kts` 的 `versionName` 一致 |
| `-TeacherDir` | 是 | 教师端编译输出目录，须含 `classroom.dll` |
| `-StudentDir` | 是 | 学生端编译输出目录，须含 `classroom.dll` |
| `-ApkPath` | 是 | Android APK 路径 |
| `-OutputDir` | 否 | 输出目录，默认 `dist` |
| `-RepoRoot` | 否 | 仓库内 `veyon/` 根，默认脚本上级目录（用于取 `docs/` 与 `COPYING`） |

脚本行为：

1. 校验三类输入**全部存在**且两端目录含 `classroom.dll`；缺一即报错退出，**不产出半成品**；
2. 分别组装教师端 / 学生端 ZIP，注入 `config/` 预设模板、`docs/`、`COPYING`、
   `install-*.ps1` 与 `README.txt`；
3. 复制 APK 并按版本重命名；
4. 生成 `SHA256SUMS.txt` 与 `RELEASE-NOTES.md` 草稿。

输出（`dist/` 已被根 `.gitignore` 排除，**不进仓库**）：

```
ClassKeep-Teacher-<ver>-win64.zip
ClassKeep-Student-<ver>-win64.zip
ClassKeep-Mobile-<ver>.apk
SHA256SUMS.txt
RELEASE-NOTES.md
```

## 注意

- 脚本**不编译任何东西**，只打包已有产物。桌面端需 Qt 6 / QCA / LibVNC / CMake；
  Android 需 JDK 21+ 与 Android SDK，构建方式见 [`../docs/CLASSKEEP.md`](../docs/CLASSKEEP.md)。
- 生成的 `install-*.ps1` 是部署模板，需按学校实际路径与账户审查后再运行。
- 分发 ZIP 时请同时附上 `LICENSE` / `NOTICE.md` / `COPYING` 与完整源码获取方式（GPL 要求）。
- 自动构建与发布由 [`../.github/workflows/classkeep-release.yml`](../.github/workflows/classkeep-release.yml) 完成，
  本脚本用于本地或离线打包。

## `package-presets.ps1`（部署预设包）

在没有 Qt / QCA / LibVNC / CMake 的机器上，**无法编译桌面端二进制**。
该脚本不改变这一事实，而是产出**真实可用的部署预设包**：配置模板、安装辅助脚本、
全套文档、`LICENSE` / `NOTICE.md` / `COPYING`。

```powershell
.\veyon\scripts\package-presets.ps1 -Version 1.0.0 -OutputDir dist
```

输出：

```
ClassKeep-Teacher-1.0.0-win64-preset.zip
ClassKeep-Student-1.0.0-win64-preset.zip
SHA256SUMS.txt        （自动纳入 dist/ 下所有 ClassKeep-* 产物）
```

> **两个 ZIP 都不含任何 Veyon / 课守可执行文件或 DLL**，包内 `README.txt` 首行明确标注了这一点，
> 文件名也带 `-preset` 后缀，避免与完整二进制包混淆。它们不是占位包：配置、脚本、文档与许可文件
> 均可直接用于部署。完整二进制包仍由 `package-classkeep.ps1` 或 CI 生成。

| 脚本 | 需要编译产物 | 产出 |
|---|---|---|
| `package-classkeep.ps1` | 是（教师端 / 学生端目录 + APK） | 两个完整二进制 ZIP + APK + 校验 + 说明 |
| `package-presets.ps1` | 否 | 两个部署预设 ZIP + 校验 |
