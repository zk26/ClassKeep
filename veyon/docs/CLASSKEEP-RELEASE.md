# 课守 ClassKeep 发布产物规范

仓库：<https://github.com/zk26/ClassKeep> · 许可 **GPL-2.0-or-later**。

每个 Release（`v<版本>` tag）必须同时提供 **教师端、学生端、APP** 三类产物，另加校验文件。
本文件定义命名、内容、构建来源、校验与安装方式。

> **合规前置**：分发二进制前必须阅读 [`CLASSKEEP-LICENSE.md`](CLASSKEEP-LICENSE.md)、
> [`../../NOTICE.md`](../../NOTICE.md) 与 [`CLASSKEEP-THIRD-PARTY.md`](CLASSKEEP-THIRD-PARTY.md)，
> 随产物附 `LICENSE` / `NOTICE.md` / `COPYING` 并提供完整源码获取方式。
> 安装后的数据合规义务见 [`CLASSKEEP-PRIVACY.md`](CLASSKEEP-PRIVACY.md)。

---

## 1. 版本号与命名

版本格式 `vMAJOR.MINOR.PATCH`（示例 `v1.0.0`），与 `mobile/app/build.gradle.kts` 的 `versionName` 保持一致。

| 产物 | 文件名 | 格式 |
|---|---|---|
| 教师端 | `ClassKeep-Teacher-<ver>-win64.zip` | ZIP |
| 学生端 | `ClassKeep-Student-<ver>-win64.zip` | ZIP |
| APP | `ClassKeep-Mobile-<ver>.apk` | Android APK |
| 校验 | `SHA256SUMS.txt` | 纯文本 |
| 发布说明 | `RELEASE-NOTES.md` | Markdown（同时写入 Release 正文） |

GitHub 还会自动附带 `Source code (zip)` / `(tar.gz)`，无需手工上传。

---

## 2. 产物内容清单

### 2.1 `ClassKeep-Teacher-<ver>-win64.zip`

运行位置：**教师机**（Windows 10/11）。

```
veyon-master.exe                Veyon Master
veyon-configurator.exe          Veyon Configurator
veyon-service.exe               Veyon Service（本机如需被管理）
veyon-cli.exe                   命令行工具
classroom.dll                   课守插件
*.dll / Qt*.dll / platforms\    运行依赖
config\
  veyon-config.json             预设配置模板（含 Classroom 段）
  classes.csv                   班级名单模板（seat,name,ip）
  network-security-note.txt     Bridge 端口与网段说明
docs\
  CLASSKEEP.md
  CLASSKEEP-DEPLOY.md
  CLASSKEEP-PROTOCOL.md
  CLASSKEEP-RELEASE.md
  CLASSKEEP-LICENSE.md
COPYING                         GPL v2 许可证全文
install-teacher.ps1             静默安装脚本（注册服务、写配置、放行防火墙）
README.txt                      三步快速上手
```

### 2.2 `ClassKeep-Student-<ver>-win64.zip`

运行位置：**学生机**（Windows 10/11 企业版/教育版/IoT 企业版，超级管理员登录，UWF 还原）。

```
veyon-service.exe               Veyon Service（LocalSystem）
veyon-server.exe                Veyon Server
veyon-worker.exe                Veyon Feature Worker
classroom.dll                   课守插件
*.dll / Qt*.dll / platforms\    运行依赖
config\
  veyon-config.json             预设：退出保护开、自助登录开、密钥留空（由教师端下发）
  uwf-exclusions.txt            建议加入 UWF 排除的目录清单
docs\
  CLASSKEEP-DEPLOY.md
  CLASSKEEP-LICENSE.md
COPYING
install-student.ps1             静默安装 + UWF 排除项 + 服务自启
README.txt
```

> 学生端与教师端共用同一套 Veyon 二进制，差别只在**安装组件与预设配置**。
> 若使用 Veyon 官方安装器，可在安装时勾选组件，再导入对应 `veyon-config.json`。

### 2.3 `ClassKeep-Mobile-<ver>.apk`

```
app-release.apk（或 unsigned debug APK，见下）
```

- `applicationId = top.ikee002.classkeep`，`minSdk 30`。
- Release 构建需签名；CI 里可用上传的 keystore（secrets）或产出未签名 APK 并在 Release 说明中标注**需自行签名**。
- debug 构建产物仅用于内测，不得当作正式版发布。

### 2.4 `SHA256SUMS.txt`

```
<sha256小写>  ClassKeep-Teacher-1.0.0-win64.zip
<sha256小写>  ClassKeep-Student-1.0.0-win64.zip
<sha256小写>  ClassKeep-Mobile-1.0.0.apk
```

由 `scripts/package-classkeep.ps1` 自动生成，发布前必须校验。

---

## 3. 构建来源

产物由 [`.github/workflows/classkeep-release.yml`](../.github/workflows/classkeep-release.yml) 在推送 `v*` tag 时构建：

| Job | Runner | 产出 |
|---|---|---|
| `android` | `ubuntu-latest` + JDK 21 + Android SDK | `ClassKeep-Mobile-<ver>.apk` |
| `windows` | `windows-latest` + Qt 6 + QCA/LibVNC + CMake | 教师端 / 学生端两份 ZIP |
| `release` | `ubuntu-latest` | 汇总 artifacts、生成 `SHA256SUMS.txt`、创建 Release 并上传全部 assets |

Windows job 需要先备好 Veyon 的第三方依赖（QCA、LibVNC、libjpeg、LZO、zlib、OpenSSL 等）。
workflow 中已写明依赖准备方式与可替换的缓存路径；若 CI 依赖不全，`windows` job 会明确失败而不会产出残缺包。

> **本仓库当前不包含预编译二进制**（`dist/`、`*.apk`、发布 ZIP 均已由根 `.gitignore` 排除）。
>
> **v1.0.0 Release 现状**：已附带本机实测编译的 `ClassKeep-Mobile-1.0.0.apk`（debug 内测包）、
> `SHA256SUMS.txt` 与 `RELEASE-NOTES.md`；教师端与学生端 ZIP **尚未附带**，因为开发机缺
> Qt 6 / QCA / LibVNC / CMake，无法本地编译。这两类由 CI 产出，或按上一节在有工具链的机器上
> 用 `scripts/package-classkeep.ps1` 生成后手动上传到该 Release。
> **不提供任何占位包、空包或伪造产物**；缺失的产物在 Release 说明中明确标注状态。

---

## 4. 本地打包

在已有编译产物的机器上：

```powershell
cd veyon
.\scripts\package-classkeep.ps1 `
    -Version 1.0.0 `
    -TeacherDir D:\build\teacher\bin `
    -StudentDir D:\build\student\bin `
    -ApkPath D:\build\mobile\app-release-unsigned.apk `
    -OutputDir dist
```

脚本会：
1. 校验三类输入目录/文件存在（缺失即报错，不产出半成品）；
2. 分别组装教师端、学生端 ZIP，注入 `config/` 模板、`docs/`、`COPYING` 与安装脚本；
3. 复制 APK 并按版本重命名；
4. 生成 `dist/SHA256SUMS.txt` 与 `dist/RELEASE-NOTES.md` 草稿。

`dist/` 已加入 `.gitignore`，产物不进仓库。

---

## 5. 校验与安装

### 校验（PowerShell）

```powershell
cd dist
Get-Content SHA256SUMS.txt | ForEach-Object {
    $hash, $file = $_ -split '\s+', 2
    $actual = (Get-FileHash $file -Algorithm SHA256).Hash.ToLower()
    if ($actual -ne $hash) { throw "校验失败: $file" }
}
Write-Output '全部校验通过'
```

### 安装顺序

1. **学生机**：以管理员运行 `install-student.ps1` → 确认 UWF 已开启且排除项已加 → 重启。
2. **教师机**：以管理员运行 `install-teacher.ps1` → Configurator 里打开「课守 ClassKeep」页设置 Bridge 端口 → 打开 Master。
3. **手机**：安装 APK → 输入本堂课密钥与教师机 IP/端口（自动发现会填）→ 立即同步。
4. 首次上课在教师端点「开始新课堂」生成密钥，再按 IP 登录学生。

详细操作步骤与排障见 [`CLASSKEEP-DEPLOY.md`](CLASSKEEP-DEPLOY.md)。

---

## 6. 发布流程

```powershell
# 1. 确认版本号一致
#    mobile/app/build.gradle.kts -> versionName / versionCode

# 2. 打 tag 并推送（CI 会自动构建并创建 Release）
cd veyon
git tag v1.0.0
git push git@github.com:zk26/ClassKeep.git v1.0.0

# 3. 在 GitHub Actions 页面确认三个 job 全部成功
# 4. 打开 Releases 页面核对三类产物 + SHA256SUMS.txt
```

若 CI 暂不可用，可用 `scripts/package-classkeep.ps1` 产出后，在 GitHub Releases 页面
「Create a new release → 选择 tag → 拖入三个产物 + SHA256SUMS.txt」手动发布。

---

## 7. 分发合规

按 GPL-2.0-or-later，每次分发二进制必须同时提供对应完整源码。
GitHub 自动附带的源码归档 + 本仓库的 commit 记录满足该要求；
离线分发（如 U 盘拷给其他学校）必须额外附上源码 ZIP、`COPYING` 与
[`CLASSKEEP-LICENSE.md`](CLASSKEEP-LICENSE.md)。
