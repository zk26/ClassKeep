<#
.SYNOPSIS
    Builds the teacher/student DEPLOYMENT PRESET packages for a ClassKeep release.

.DESCRIPTION
    These packages contain configuration templates, installation helpers,
    documentation and license files. They deliberately contain NO Veyon or
    classroom binaries: on a machine without the Qt/QCA/LibVNC/CMake toolchain
    we cannot produce those, and shipping an empty or placeholder binary would
    be misleading. Use package-classkeep.ps1 for the full binary packages.

    Every package carries a prominent README.txt stating this explicitly.

.EXAMPLE
    .\veyon\scripts\package-presets.ps1 -Version 1.0.0 -OutputDir dist
#>
param(
    [string]$Version = '1.0.0',
    [string]$OutputDir = 'dist',
    [string]$RepoRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$workspaceRoot = Split-Path -Parent $RepoRoot

function Write-TextFile([string]$Path, [string]$Content) {
    $dir = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    [System.IO.File]::WriteAllText($Path, $Content, $utf8NoBom)
}

$out = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDir))
$stage = Join-Path $out '_presets'
New-Item -ItemType Directory -Force -Path $out | Out-Null
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
$teacher = Join-Path $stage 'teacher'
$student = Join-Path $stage 'student'
New-Item -ItemType Directory -Force -Path (Join-Path $teacher 'config'),
    (Join-Path $teacher 'docs'), (Join-Path $student 'config'),
    (Join-Path $student 'docs') | Out-Null

$teacherZip = "ClassKeep-Teacher-$Version-win64-preset.zip"
$studentZip = "ClassKeep-Student-$Version-win64-preset.zip"

# ---------------------------------------------------------------- shared legal files
$licensePath = Join-Path $workspaceRoot 'LICENSE'
$noticePath = Join-Path $workspaceRoot 'NOTICE.md'
$copyingPath = Join-Path $RepoRoot 'COPYING'
$docsSource = Join-Path $RepoRoot 'docs'
foreach ($target in @($teacher, $student)) {
    if (Test-Path -LiteralPath $licensePath) { Copy-Item -LiteralPath $licensePath -Destination $target -Force }
    if (Test-Path -LiteralPath $noticePath) { Copy-Item -LiteralPath $noticePath -Destination $target -Force }
    if (Test-Path -LiteralPath $copyingPath) { Copy-Item -LiteralPath $copyingPath -Destination $target -Force }
    if (Test-Path -LiteralPath $docsSource) {
        Copy-Item -Path (Join-Path $docsSource '*.md') -Destination (Join-Path $target 'docs') -Force
    }
}

# ---------------------------------------------------------------- config templates
$teacherConfig = @'
{
  "Classroom": {
    "Enabled": true,
    "DataDirectory": "%APPDATA%/ClassKeep",
    "ExitProtection": true,
    "ExitRequiresAdmin": true,
    "AllowSelfLogin": true,
    "MonitoringEnabled": true,
    "CaptureInterval": 5,
    "BridgeEnabled": true,
    "BridgePort": 11880,
    "SyncEnabled": true
  }
}
'@
$studentConfig = @'
{
  "Classroom": {
    "Enabled": true,
    "DataDirectory": "%APPDATA%/ClassKeep",
    "ExitProtection": true,
    "ExitRequiresAdmin": true,
    "ExitAdminUsername": "Administrator",
    "AllowSelfLogin": true,
    "MonitoringEnabled": true,
    "CaptureInterval": 5,
    "BridgeEnabled": false,
    "SyncEnabled": true
  }
}
'@
Write-TextFile (Join-Path $teacher 'config/veyon-config.json') $teacherConfig
Write-TextFile (Join-Path $student 'config/veyon-config.json') $studentConfig

Write-TextFile (Join-Path $teacher 'config/classes.csv') @'
seat,name,ip
1,张三,192.168.1.11
2,李四,192.168.1.12
'@
Write-TextFile (Join-Path $teacher 'config/network-security-note.txt') @'
ClassKeep bridge uses TCP+UDP <BridgePort> (default 11880) on the LAN.
Allow inbound on that port for the teacher machine only.
Do NOT expose this port to the public internet.
The mobile app authenticates with the per-lesson key (SHA-256 challenge/response).
'@
Write-TextFile (Join-Path $student 'config/uwf-exclusions.txt') @'
# Apply with:  uwfmgr file add-exclusion <path>   (reboot required)
# Only add directories that must survive UWF restore, e.g. local coursework:
# D:\作业
# C:\Users\Public\Documents\ClassKeep
# Behaviour logs are NOT stored on student machines by default.
'@

# ---------------------------------------------------------------- install helpers
Write-TextFile (Join-Path $teacher 'install-teacher.ps1') @'
# ClassKeep teacher machine preset - run as Administrator from this folder.
# NOTE: this script only deploys configuration. It does NOT install Veyon binaries.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script as Administrator.'
}
$dest = Join-Path $env:APPDATA 'ClassKeep'
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item -Path (Join-Path $here 'config/veyon-config.json') -Destination (Join-Path $dest 'veyon-config.json') -Force
New-NetFirewallRule -DisplayName 'ClassKeep Bridge TCP' -Direction Inbound -Protocol TCP -LocalPort 11880 -Action Allow -ErrorAction SilentlyContinue | Out-Null
New-NetFirewallRule -DisplayName 'ClassKeep Discovery UDP' -Direction Inbound -Protocol UDP -LocalPort 11880 -Action Allow -ErrorAction SilentlyContinue | Out-Null
Write-Host 'Preset installed. Now install Veyon 4.11.4 (Master + Configurator) with the ClassKeep plugin,'
Write-Host 'then open the ClassKeep console and click "Start new lesson".'
'@
Write-TextFile (Join-Path $student 'install-student.ps1') @'
# ClassKeep student machine preset - run as Administrator from this folder.
# NOTE: this script only deploys configuration. It does NOT install Veyon binaries.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script as Administrator.'
}
$dest = Join-Path $env:APPDATA 'ClassKeep'
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item -Path (Join-Path $here 'config/veyon-config.json') -Destination (Join-Path $dest 'veyon-config.json') -Force
Write-Host 'Preset installed. Next steps:'
Write-Host ' 1. Install Veyon 4.11.4 (Service + Server + Worker) with the ClassKeep plugin.'
Write-Host ' 2. Apply UWF exclusions listed in config/uwf-exclusions.txt.'
Write-Host ' 3. REBOOT (not shutdown) so UWF changes take effect.'
Write-Host ' 4. The lesson key is pushed by the teacher machine; do not set it manually.'
'@

# ---------------------------------------------------------------- prominent disclaimers
Write-TextFile (Join-Path $teacher 'README.txt') @'
课守 ClassKeep - 教师机部署预设包 (Deployment preset, NOT a binary installer)
=====================================================================

*** 本包不包含任何 Veyon / 课守可执行文件或 DLL ***
It contains configuration templates, firewall helpers, documentation and
license files only. A full binary package (ClassKeep-Teacher-<ver>-win64.zip)
is produced by CI or by scripts/package-classkeep.ps1 on a machine with
Qt 6 / QCA / LibVNC / CMake.

Contents
  config/veyon-config.json       教师机预设（Bridge 开启）
  config/classes.csv             班级名单模板 (seat,name,ip)
  config/network-security-note.txt   Bridge 端口与网段说明
  install-teacher.ps1            配置部署 + 防火墙放行（需管理员）
  docs/                          全部功能/协议/部署/许可/隐私文档
  LICENSE / COPYING              GNU GPL v2 全文
  NOTICE.md                      版权与衍生作品声明

使用顺序
  1. 以管理员运行 install-teacher.ps1
  2. 安装 Veyon 4.11.4（Master + Configurator）并带上 classroom 插件
  3. 打开 ClassKeep 控制台，导入 classes.csv，点「开始新课堂」
  4. 按 IP 登录学生，或让学生输入 座位号 + 姓名 + 本堂课密钥

部署前必读 docs/CLASSKEEP-PRIVACY.md（本软件采集学生行为数据）。
许可：GPL-2.0-or-later，见 LICENSE 与 NOTICE.md。
'@
Write-TextFile (Join-Path $student 'README.txt') @'
课守 ClassKeep - 学生机部署预设包 (Deployment preset, NOT a binary installer)
=====================================================================

*** 本包不包含任何 Veyon / 课守可执行文件或 DLL ***
It contains configuration templates, UWF guidance, documentation and
license files only. A full binary package (ClassKeep-Student-<ver>-win64.zip)
is produced by CI or by scripts/package-classkeep.ps1 on a machine with
Qt 6 / QCA / LibVNC / CMake.

要求：Windows 10/11 企业版、教育版或 IoT 企业版（UWF），
      使用超级管理员账户登录，仅启用 Windows Defender。

Contents
  config/veyon-config.json       学生机预设（Bridge 关闭）
  config/uwf-exclusions.txt      UWF 排除目录建议
  install-student.ps1            配置部署（需管理员）
  docs/                          部署 / 隐私 / 许可 / 第三方文档
  LICENSE / COPYING              GNU GPL v2 全文
  NOTICE.md                      版权与衍生作品声明

使用顺序
  1. 以管理员运行 install-student.ps1
  2. 安装 Veyon 4.11.4（Service + Server + Worker）并带上 classroom 插件
  3. 按 config/uwf-exclusions.txt 添加排除项，然后【重启】（不是关机再开机）
  4. 密钥由教师机下发，学生端不自行设置；学生端默认不保存行为数据

部署前必读 docs/CLASSKEEP-PRIVACY.md。
许可：GPL-2.0-or-later，见 LICENSE 与 NOTICE.md。
'@

# ---------------------------------------------------------------- zip + checksums
$teacherZipPath = Join-Path $out $teacherZip
$studentZipPath = Join-Path $out $studentZip
foreach ($zip in @($teacherZipPath, $studentZipPath)) {
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
}
Compress-Archive -Path (Join-Path $teacher '*') -DestinationPath $teacherZipPath -CompressionLevel Optimal
Compress-Archive -Path (Join-Path $student '*') -DestinationPath $studentZipPath -CompressionLevel Optimal
Remove-Item -LiteralPath $stage -Recurse -Force

$artifacts = Get-ChildItem -LiteralPath $out -File |
    Where-Object { $_.Name -match '^ClassKeep-(Teacher|Student|Mobile)-.*\.(zip|apk)$' } |
    Sort-Object Name
$lines = foreach ($file in $artifacts) {
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLower()
    "$hash  $($file.Name)"
}
Write-TextFile (Join-Path $out 'SHA256SUMS.txt') (($lines -join "`n") + "`n")

Write-Host "Preset packages written to $out"
$artifacts | Select-Object Name, @{N='KB';E={[math]::Round($_.Length/1KB,1)}} | Format-Table -AutoSize
