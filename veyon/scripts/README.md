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
