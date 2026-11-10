<#
.SYNOPSIS
    Packages the three ClassKeep release artifacts (teacher, student, Android app).

.DESCRIPTION
    Takes compiled binaries from a machine with the full toolchain and produces:
      ClassKeep-Teacher-<ver>-win64.zip
      ClassKeep-Student-<ver>-win64.zip
      ClassKeep-Mobile-<ver>.apk
      SHA256SUMS.txt
      RELEASE-NOTES.md (draft)

    It never creates a partial/fake artifact: all three inputs must exist.

.EXAMPLE
    .\scripts\package-classkeep.ps1 -Version 1.0.0 `
        -TeacherDir D:\build\teacher\bin -StudentDir D:\build\student\bin `
        -ApkPath D:\build\mobile\app-release-unsigned.apk -OutputDir dist
#>
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$TeacherDir,
    [Parameter(Mandatory = $true)][string]$StudentDir,
    [Parameter(Mandatory = $true)][string]$ApkPath,
    [string]$OutputDir = 'dist',
    [string]$RepoRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Write-TextFile([string]$Path, [string]$Content) {
    [System.IO.File]::WriteAllText($Path, $Content, $utf8NoBom)
}

foreach ($item in @(
    @{ Name = 'TeacherDir'; Path = $TeacherDir },
    @{ Name = 'StudentDir'; Path = $StudentDir },
    @{ Name = 'ApkPath';    Path = $ApkPath }
)) {
    if (-not (Test-Path -LiteralPath $item.Path)) {
        throw "$($item.Name) not found: $($item.Path)`nA release needs all three real artifacts. See docs/CLASSKEEP-RELEASE.md."
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $TeacherDir 'classroom.dll'))) {
    throw "classroom.dll missing in TeacherDir - plugin was not built."
}
if (-not (Test-Path -LiteralPath (Join-Path $StudentDir 'classroom.dll'))) {
    throw "classroom.dll missing in StudentDir - plugin was not built."
}

$out = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDir))
$stage = Join-Path $out '_stage'
New-Item -ItemType Directory -Force -Path $out | Out-Null
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
$teacherStage = Join-Path $stage 'teacher'
$studentStage = Join-Path $stage 'student'
New-Item -ItemType Directory -Force -Path $teacherStage, $studentStage | Out-Null

$teacherZip = "ClassKeep-Teacher-$Version-win64.zip"
$studentZip = "ClassKeep-Student-$Version-win64.zip"
$mobileApk  = "ClassKeep-Mobile-$Version.apk"

# ---------- shared assets ----------
$docsSource = Join-Path $RepoRoot 'docs'
$copying    = Join-Path $RepoRoot 'COPYING'
foreach ($target in @($teacherStage, $studentStage)) {
    $docsTarget = Join-Path $target 'docs'
    New-Item -ItemType Directory -Force -Path (Join-Path $target 'config'), $docsTarget | Out-Null
    if (Test-Path -LiteralPath $docsSource) {
        Copy-Item -Path (Join-Path $docsSource '*.md') -Destination $docsTarget -Force
    }
    if (Test-Path -LiteralPath $copying) { Copy-Item -LiteralPath $copying -Destination $target -Force }
}
$license = Join-Path $docsSource 'CLASSKEEP-LICENSE.md'
if (Test-Path -LiteralPath $license) { Copy-Item -LiteralPath $license -Destination (Join-Path $studentStage 'docs') -Force }

# ---------- config templates ----------
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
Write-TextFile (Join-Path $teacherStage 'config/veyon-config.json') $teacherConfig
Write-TextFile (Join-Path $studentStage 'config/veyon-config.json') $studentConfig

Write-TextFile (Join-Path $teacherStage 'config/classes.csv') @'
seat,name,ip
1,张三,192.168.1.11
2,李四,192.168.1.12
'@
Write-TextFile (Join-Path $teacherStage 'config/network-security-note.txt') @'
ClassKeep bridge uses TCP+UDP <BridgePort> (default 11880) on the LAN.
Allow inbound TCP/UDP on that port for the teacher machine only.
Do NOT expose this port to the public internet.
The mobile app authenticates with the per-lesson key (SHA-256 challenge/response).
'@
Write-TextFile (Join-Path $studentStage 'config/uwf-exclusions.txt') @'
# Add these with: uwfmgr file add-exclusion <path>   (reboot required)
# Directories that must survive UWF restore, if your school keeps local work:
# D:\作业
# C:\Users\Public\Documents\ClassKeep
# Note: behaviour logs are NOT stored on student machines by default.
'@

# ---------- installers ----------
$installTeacher = @'
# ClassKeep teacher machine installer - run as Administrator from this folder.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script as Administrator.'
}
Copy-Item -Path (Join-Path $here 'config/veyon-config.json') -Destination "$env:APPDATA\ClassKeep\veyon-config.json" -Force
New-NetFirewallRule -DisplayName 'ClassKeep Bridge TCP' -Direction Inbound -Protocol TCP -LocalPort 11880 -Action Allow -ErrorAction SilentlyContinue | Out-Null
New-NetFirewallRule -DisplayName 'ClassKeep Discovery UDP' -Direction Inbound -Protocol UDP -LocalPort 11880 -Action Allow -ErrorAction SilentlyContinue | Out-Null
Write-Host 'Teacher preset installed. Start Veyon Master, open ClassKeep, then click "Start new lesson".'
'@
$installStudent = @'
# ClassKeep student machine installer - run as Administrator from this folder.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script as Administrator.'
}
$config = Get-Content -Raw (Join-Path $here 'config/veyon-config.json')
$dest = "$env:APPDATA\ClassKeep\veyon-config.json"
New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
[System.IO.File]::WriteAllText($dest, $config, (New-Object System.Text.UTF8Encoding($false)))
Write-Host 'Student preset installed.'
Write-Host 'UWF: apply exclusions listed in config/uwf-exclusions.txt, then REBOOT (not shutdown).'
Write-Host 'The lesson key is pushed by the teacher machine; do not set it manually.'
'@
Write-TextFile (Join-Path $teacherStage 'install-teacher.ps1') $installTeacher
Write-TextFile (Join-Path $studentStage 'install-student.ps1') $installStudent

Write-TextFile (Join-Path $teacherStage 'README.txt') @'
课守 ClassKeep - Teacher machine
1. Run install-teacher.ps1 as Administrator.
2. Open Veyon Configurator -> ClassKeep to review settings and the bridge port.
3. Open Veyon Master -> ClassKeep -> import classes.csv -> "Start new lesson".
4. Sign students in by IP, or let them type seat + name + lesson key.

See docs/CLASSKEEP-DEPLOY.md and docs/CLASSKEEP-PROTOCOL.md.
License: GPL-2.0-or-later (COPYING).
'@
Write-TextFile (Join-Path $studentStage 'README.txt') @'
课守 ClassKeep - Student machine
Requires Windows 10/11 Enterprise, Education or IoT Enterprise (UWF).
1. Run install-student.ps1 as Administrator.
2. Apply UWF exclusions from config/uwf-exclusions.txt, then REBOOT.
3. The floating ClassKeep bar appears after logon. No local data is stored.

See docs/CLASSKEEP-DEPLOY.md. License: GPL-2.0-or-later (COPYING).
'@

# ---------- binaries ----------
Copy-Item -Path (Join-Path $TeacherDir '*') -Destination $teacherStage -Recurse -Force
Copy-Item -Path (Join-Path $StudentDir '*') -Destination $studentStage -Recurse -Force

$teacherZipPath = Join-Path $out $teacherZip
$studentZipPath = Join-Path $out $studentZip
$apkDest        = Join-Path $out $mobileApk
foreach ($zip in @($teacherZipPath, $studentZipPath)) {
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
}
Compress-Archive -Path (Join-Path $teacherStage '*') -DestinationPath $teacherZipPath -CompressionLevel Optimal
Compress-Archive -Path (Join-Path $studentStage '*') -DestinationPath $studentZipPath -CompressionLevel Optimal
Copy-Item -LiteralPath $ApkPath -Destination $apkDest -Force

# ---------- checksums & notes ----------
$lines = foreach ($name in @($teacherZip, $studentZip, $mobileApk)) {
    $hash = (Get-FileHash -LiteralPath (Join-Path $out $name) -Algorithm SHA256).Hash.ToLower()
    "$hash  $name"
}
Write-TextFile (Join-Path $out 'SHA256SUMS.txt') (($lines -join "`n") + "`n")

$notesTemplate = @'
# 课守 ClassKeep {VER}

## 产物
- {TEACHER} - 教师机（Master + Configurator + 课守插件 + Bridge）
- {STUDENT} - 学生机（Service/Server/Worker + 课守插件，UWF 环境）
- {APK} - Android 教室端

## 校验
```powershell
Get-Content SHA256SUMS.txt | ForEach-Object {
    $hash, $file = $_ -split '\s+', 2
    if ((Get-FileHash $file -Algorithm SHA256).Hash.ToLower() -ne $hash) { throw "校验失败: $file" }
}
```

## 安装顺序
学生机 -> 教师机 -> 手机端 -> 教师端「开始新课堂」。
详见 docs/CLASSKEEP-DEPLOY.md。

## 许可证
GPL-2.0-or-later，见 COPYING 与 docs/CLASSKEEP-LICENSE.md。
源码：https://github.com/zk26/ClassKeep
'@
$notes = $notesTemplate.Replace('{VER}', $Version).Replace('{TEACHER}', $teacherZip).
    Replace('{STUDENT}', $studentZip).Replace('{APK}', $mobileApk)
Write-TextFile (Join-Path $out 'RELEASE-NOTES.md') $notes

Remove-Item -LiteralPath $stage -Recurse -Force
Write-Host "Done. Artifacts in $out"
Get-ChildItem -LiteralPath $out -File | Select-Object Name, @{N='KB';E={[math]::Round($_.Length/1KB,1)}} | Format-Table -AutoSize
