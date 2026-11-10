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

| 产物 | 文件名 | 构建来源 |
|---|---|---|
| 教师端 | `ClassKeep-Teacher-<ver>-win64.zip` | `ClassKeep Release` → `windows` job（MSYS2 UCRT64 + Qt6/MinGW） |
| 学生端 | `ClassKeep-Student-<ver>-win64.zip` | 同上：同一套二进制，注入不同的预设配置与安装脚本 |
| APP | `ClassKeep-Mobile-<ver>.apk` | `ClassKeep Release` → `android` job（Ubuntu + JDK 21） |
| 校验 | `SHA256SUMS.txt` | 覆盖上述三类产物 |
| 发布说明 | `RELEASE-NOTES.md` | 由 `scripts/release-notes-template.md` 生成 |

**三类产物都是真实编译的，不含占位包、空包或伪造二进制。**

> 历史说明：`v1.0.0` 曾附带 `-preset` 部署配置包（当时 CI 尚未支持 Windows 编译），
> 由 `scripts/package-presets.ps1` 生成，内含配置模板、安装脚本、文档与许可文件，但**明确标注不含可执行文件**。
> 自 `v1.1.0` 起改由上面的完整二进制包取代；`package-presets.ps1` 仍保留，供无工具链环境做离线配置部署。
> 命名上 `-win64.zip` 与 `-win64-preset.zip` 严格区分，不得混用。

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

产物由 [`../../.github/workflows/classkeep-release.yml`](../../.github/workflows/classkeep-release.yml)
在推送 `v*` tag 或手动 `Run workflow` 时构建：

| Job | Runner | 产出 |
|---|---|---|
| `android` | `ubuntu-24.04` + JDK 21 + Android SDK | `ClassKeep-Mobile-<ver>.apk` |
| `windows` | `windows-latest` + MSYS2 **UCRT64** + Qt6/MinGW | 真实编译的 `.exe` / `.dll`（含 `classroom.dll`）+ Qt6/CRT 运行时 |
| `package` | `windows-latest` | 用 `package-classkeep.ps1` 组装教师端/学生端 ZIP + `SHA256SUMS.txt` |
| `release` | `ubuntu-24.04` | 校验后创建 Release 并上传全部 assets |

Windows 依赖由 `msys2/setup-msys2@v2` 在 UCRT64 下安装（QCA 包名是 `qca-qt6`，
LDAP/SASL 是 `openldap` / `cyrus-sasl`）；3rdparty 按上游 v4.11.4 的 gitlink SHA 精确检出。
详见 [`CLASSKEEP-BUILD.md`](CLASSKEEP-BUILD.md) 路线 D。

另外 [`../../.github/workflows/classkeep-windows.yml`](../../.github/workflows/classkeep-windows.yml)
可单独手动触发，只做 Windows 编译验证（产出 `classkeep-windows-binaries` artifact）。

> 仓库内**不提交预编译二进制**（`dist/`、`*.apk`、发布 ZIP 均由根 `.gitignore` 排除）。
>
> 版本历史：`v1.0.0` 附带的是 `-preset` 部署配置包；自 `v1.1.0` 起，
> **教师端 / 学生端 / APP 三类产物全部由 GitHub Actions 真实编译**，
> `SHA256SUMS.txt` 覆盖三者。不含任何占位包、空包或伪造二进制。

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
