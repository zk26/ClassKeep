; ClassKeep - Windows installer (based on upstream Veyon nsis/veyon.nsi.in)
;
; Compile twice from the repository root (the release workflow supplies VERSION;
; 2.0.0 is only the fallback defined below):
;   makensis -DVERSION=2.0.0 -DROLE=teacher installer.nsi
;   makensis -DVERSION=2.0.0 -DROLE=student installer.nsi
;
; ROLE=teacher : component page with "Service" (read-only, registers/starts the
;                ClassKeepService service) and "Master" (optional console).
; ROLE=student : component page with ONLY the read-only Service section.
;
; Since 2.0.0 every shipped process is ClassKeep-*.exe and the Windows service
; is ClassKeepService. The installer also stops/removes the legacy <=1.5.1
; `veyon-*.exe` processes and `VeyonService`, which is what used to trigger
; "无法打开要写入的文件 ... veyon-worker.exe" during an upgrade.
;
; CI copies this file to the repository root with a UTF-8 BOM (Unicode true).

Unicode true

!include "LogicLib.nsh"
!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef VERSION
  !define VERSION "2.0.0"
!endif
!ifndef ROLE
  !define ROLE "teacher"
!endif

!if "${ROLE}" == "teacher"
  !define ROLE_EN "Master"
  !define ROLE_CN "教师端"
  !define OUT_FILE "dist\ClassKeep-Teacher-${VERSION}-win64-setup.exe"
  !define INST_DIR "$PROGRAMFILES64\ClassKeep\Master"
  !define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\ClassKeepMaster"
!else
  !define ROLE_EN "Student"
  !define ROLE_CN "学生端"
  !define OUT_FILE "dist\ClassKeep-Student-${VERSION}-win64-setup.exe"
  !define INST_DIR "$PROGRAMFILES64\ClassKeep\Student"
  !define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\ClassKeepStudent"
!endif

!define APP_NAME "ClassKeep"
!define WEB_SITE "https://github.com/zk26/ClassKeep"
!define INSTALL_TYPE "SetShellVarContext all"
!define REG_ROOT "HKLM"
!define LEGACY_TEACHER_DIR "$PROGRAMFILES64\ClassKeep\Teacher"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "ClassKeep"
VIAddVersionKey "CompanyName" "ClassKeep"
VIAddVersionKey "FileDescription" "ClassKeep ${ROLE_EN} Setup"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "GPL-2.0-or-later; based on Veyon (c) Veyon Solutions"

SetCompressor /SOLID lzma
SetCompressorDictSize 64
Name "${APP_NAME} ${ROLE_EN} ${VERSION}"
Caption "${APP_NAME} ${ROLE_EN} ${VERSION}"
OutFile "${OUT_FILE}"
BrandingText "${APP_NAME} ${VERSION}"
InstallDir "${INST_DIR}"
RequestExecutionLevel admin
ShowInstDetails show

!define MUI_ABORTWARNING
!define MUI_UNABORTWARNING
!define MUI_ICON "veyon\nsis\installer.ico"
!define MUI_UNICON "veyon\nsis\uninstaller.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

; ---------------------------------------------------------------------------
; Stop both the current and the legacy (<= 1.5.1) service + processes.
; `service unregister` does NOT stop a service (it only deletes its SCM entry),
; so this must happen before any File/Delete instruction.
; ---------------------------------------------------------------------------
Function StopClassKeepProcesses
  DetailPrint "正在停止 ClassKeep 服务与客户端进程…"
  nsExec::ExecToLog '"$SYSDIR\sc.exe" stop ClassKeepService'
  nsExec::ExecToLog '"$SYSDIR\sc.exe" stop VeyonService'
  nsExec::ExecToLog '"$SYSDIR\sc.exe" delete VeyonService'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM ClassKeep-worker.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM ClassKeep-server.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM ClassKeep-service.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM ClassKeep-master.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM ClassKeep-configurator.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM ClassKeep-cli.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM ClassKeep-wcli.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM veyon-worker.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM veyon-server.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM veyon-service.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM veyon-master.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM veyon-configurator.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM veyon-cli.exe'
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /T /IM veyon-wcli.exe'
  Sleep 1200
FunctionEnd

; Delete both generations of executables and retry while a handle is still
; open (up to 10s). If a file is genuinely locked, show an actionable message
; instead of letting NSIS raise its raw "cannot open file to write" dialog.
Function RemoveLockedBinaries
  StrCpy $8 0
  ck_retry:
    Delete "$INSTDIR\ClassKeep-worker.exe"
    Delete "$INSTDIR\ClassKeep-server.exe"
    Delete "$INSTDIR\ClassKeep-service.exe"
    Delete "$INSTDIR\ClassKeep-configurator.exe"
    Delete "$INSTDIR\ClassKeep-cli.exe"
    Delete "$INSTDIR\ClassKeep-wcli.exe"
    Delete "$INSTDIR\ClassKeep-master.exe"
    Delete "$INSTDIR\veyon-worker.exe"
    Delete "$INSTDIR\veyon-server.exe"
    Delete "$INSTDIR\veyon-service.exe"
    Delete "$INSTDIR\veyon-configurator.exe"
    Delete "$INSTDIR\veyon-cli.exe"
    Delete "$INSTDIR\veyon-wcli.exe"
    Delete "$INSTDIR\veyon-master.exe"
    StrCpy $7 0
    IfFileExists "$INSTDIR\ClassKeep-worker.exe" 0 +2
      StrCpy $7 1
    IfFileExists "$INSTDIR\ClassKeep-server.exe" 0 +2
      StrCpy $7 1
    IfFileExists "$INSTDIR\ClassKeep-service.exe" 0 +2
      StrCpy $7 1
    IfFileExists "$INSTDIR\ClassKeep-master.exe" 0 +2
      StrCpy $7 1
    IfFileExists "$INSTDIR\veyon-worker.exe" 0 +2
      StrCpy $7 1
    IfFileExists "$INSTDIR\veyon-server.exe" 0 +2
      StrCpy $7 1
    IfFileExists "$INSTDIR\veyon-service.exe" 0 +2
      StrCpy $7 1
    IfFileExists "$INSTDIR\veyon-master.exe" 0 +2
      StrCpy $7 1
    IntCmp $7 0 ck_done
    IntOp $8 $8 + 1
    IntCmp $8 20 ck_giveup ck_retry ck_giveup
  ck_giveup:
    MessageBox MB_OK|MB_ICONSTOP "仍有 ClassKeep 进程占用安装目录，无法继续。$\r$\n$\r$\n请退出已登录的学生端（或重启电脑后重新安装），然后再次运行本安装程序。"
    Abort
  ck_done:
FunctionEnd

; ---------------------------------------------------------------- Service (RO)
Section "ClassKeep Service (required)" SEC_SERVICE
  SectionIn RO
  ${INSTALL_TYPE}
  SetOverwrite on
  SetOutPath "$INSTDIR"

  Call StopClassKeepProcesses

  ; Unregister the service with whichever CLI the previous version shipped.
  ; `plugins/windows-platform.dll` + Qt6 `libqca-ossl.dll` must be present for
  ; the CLI to start; collect-windows-runtime.sh hard-verifies both.
  IfFileExists "$INSTDIR\ClassKeep-wcli.exe" 0 ck_legacy_wcli
    nsExec::ExecToLog '"$INSTDIR\ClassKeep-wcli.exe" service unregister'
    Goto ck_unregister_done
  ck_legacy_wcli:
    IfFileExists "$INSTDIR\veyon-wcli.exe" 0 ck_unregister_done
      nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" service unregister'
  ck_unregister_done:

  Call RemoveLockedBinaries
  Delete "$INSTDIR\*.dll"
  RmDir /r "$INSTDIR\plugins"

  File "payload\ClassKeep-server.exe"
  File "payload\ClassKeep-service.exe"
  File "payload\ClassKeep-configurator.exe"
  File "payload\ClassKeep-cli.exe"
  File "payload\ClassKeep-wcli.exe"
  File "payload\ClassKeep-worker.exe"
  File "payload\*.dll"
  File "payload\LICENSE.TXT"
  File "payload\README.TXT"
  File "payload\COPYING.TXT"
  File "payload\ClassKeep.ico"

  SetOutPath "$INSTDIR\plugins"
  File "payload\plugins\*.dll"
  SetOutPath "$INSTDIR\crypto"
  File "payload\crypto\*.dll"
  SetOutPath "$INSTDIR\imageformats"
  File /nonfatal "payload\imageformats\*.dll"
  SetOutPath "$INSTDIR\platforms"
  File "payload\platforms\qwindows.dll"
  SetOutPath "$INSTDIR\styles"
  File /nonfatal "payload\styles\*.dll"
  SetOutPath "$INSTDIR\tls"
  File /nonfatal "payload\tls\*.dll"
  ; Chinese UI by default; switchable in ClassKeep Configurator -> General -> Language.
  SetOutPath "$INSTDIR\translations"
  File "payload\translations\*.qm"

  ; ------------------------------------------------------------------
  ; Upstream sequence, but with return codes + a real service self-check.
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR"
  FileOpen $9 "$INSTDIR\install.log" w
  FileWrite $9 "ClassKeep ${VERSION} ${ROLE_EN} install log$\r$\n"

  nsExec::ExecToStack '"$INSTDIR\ClassKeep-wcli.exe" service register'
  Pop $R0
  Pop $1
  DetailPrint "注册服务 (service register): exit=$R0"
  FileWrite $9 "service register exit=$R0 output=$1$\r$\n"

  nsExec::ExecToStack '"$INSTDIR\ClassKeep-wcli.exe" config set Network/FirewallExceptionEnabled 1'
  Pop $0
  Pop $1
  FileWrite $9 "config FirewallExceptionEnabled exit=$0 output=$1$\r$\n"

  nsExec::ExecToStack '"$INSTDIR\ClassKeep-wcli.exe" config set Windows/SoftwareSASEnabled 1'
  Pop $0
  Pop $1
  FileWrite $9 "config SoftwareSASEnabled exit=$0 output=$1$\r$\n"

  WriteRegDWORD ${REG_ROOT} "SYSTEM\CurrentControlSet\Control\Lsa" "ForceGuest" "0"

  nsExec::ExecToStack '"$INSTDIR\ClassKeep-wcli.exe" service start'
  Pop $R1
  Pop $1
  DetailPrint "启动服务 (service start): exit=$R1"
  FileWrite $9 "service start exit=$R1 output=$1$\r$\n"

  ; Ask the Service Control Manager directly. findstr exits 0 only when the
  ; service STATE line contains RUNNING, so $2 is our ground truth.
  nsExec::ExecToStack '"$SYSDIR\cmd.exe" /C "sc query ClassKeepService | findstr RUNNING"'
  Pop $2
  Pop $3
  FileWrite $9 "sc query RUNNING exit=$2 output=$3$\r$\n"

  ; Fallback ONLY when the primary wcli path failed. Service name / binary /
  ; autostart are identical to WindowsServiceFunctions.cpp.
  ${If} $R0 != "0"
  ${OrIf} $R1 != "0"
  ${OrIf} $2 != "0"
    DetailPrint "wcli 未能启动服务，改用系统服务控制兜底…"
    FileWrite $9 "fallback: sc create + sc start$\r$\n"
    nsExec::ExecToLog '"$SYSDIR\sc.exe" create ClassKeepService binPath= "$INSTDIR\ClassKeep-service.exe" start= auto DisplayName= "ClassKeep Service"'
    nsExec::ExecToLog '"$SYSDIR\sc.exe" start ClassKeepService'
    nsExec::ExecToStack '"$SYSDIR\cmd.exe" /C "sc query ClassKeepService | findstr RUNNING"'
    Pop $2
    Pop $3
    FileWrite $9 "after fallback RUNNING exit=$2 output=$3$\r$\n"
  ${EndIf}

  ${If} $2 != "0"
    FileWrite $9 "RESULT: service NOT running$\r$\n"
    MessageBox MB_OK|MB_ICONEXCLAMATION "服务未能启动，安装未完成。$\r$\n$\r$\n请把 $INSTDIR\install.log 发给管理员排查。$\r$\n常见原因：未以管理员运行、被安全软件拦截、或缺少运行库。"
  ${Else}
    FileWrite $9 "RESULT: service RUNNING$\r$\n"
    DetailPrint "服务已正常运行 (ClassKeepService)"
  ${EndIf}
  FileClose $9

  ; One-time cleanup of the <=1.5.1 layout (Teacher dir + Add/Remove entry).
  RmDir /r "${LEGACY_TEACHER_DIR}"
  DeleteRegKey ${REG_ROOT} "Software\Microsoft\Windows\CurrentVersion\Uninstall\ClassKeepTeacher"
SectionEnd

; --------------------------------------------------------------- Master only
!if "${ROLE}" == "teacher"
Section "ClassKeep Master" SEC_MASTER
  ${INSTALL_TYPE}
  SetOutPath "$INSTDIR"
  File "payload\ClassKeep-master.exe"
  CreateDirectory "$SMPROGRAMS\ClassKeep"
  CreateShortCut "$SMPROGRAMS\ClassKeep\ClassKeep Master.lnk" \
    "$INSTDIR\ClassKeep-master.exe" "" "$INSTDIR\ClassKeep.ico"
  CreateShortCut "$DESKTOP\ClassKeep Master.lnk" \
    "$INSTDIR\ClassKeep-master.exe" "" "$INSTDIR\ClassKeep.ico"
SectionEnd
!endif

; --------------------------------------------------------------- Common / reg
Section -Post
  ${INSTALL_TYPE}
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateDirectory "$SMPROGRAMS\ClassKeep"
  CreateShortCut "$SMPROGRAMS\ClassKeep\ClassKeep Configurator.lnk" \
    "$INSTDIR\ClassKeep-configurator.exe" "" "$INSTDIR\ClassKeep.ico"
  CreateShortCut "$SMPROGRAMS\ClassKeep\Uninstall ${ROLE_EN}.lnk" "$INSTDIR\uninstall.exe"

  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "DisplayName" "ClassKeep ${ROLE_EN}"
  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\ClassKeep.ico"
  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "Publisher" "ClassKeep"
  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "URLInfoAbout" "${WEB_SITE}"
SectionEnd

; ------------------------------------------------------------------- Uninstall
Section Uninstall
  ${INSTALL_TYPE}
  ; Student uninstall protection: the Master writes this HKLM flag.
  ; Only a teacher-side "unlock" command can clear it (see ClassroomPlugin).
  SetRegView 64
  ReadRegDWORD $0 ${REG_ROOT} "SOFTWARE\ClassKeep\Student" "UninstallProtection"
  ${If} $0 == 1
    MessageBox MB_OK|MB_ICONSTOP "教师端已开启卸载保护，请联系教师端解除保护后再卸载。"
    Abort
  ${EndIf}

  IfFileExists "$INSTDIR\ClassKeep-wcli.exe" 0 ck_un_legacy_wcli
    nsExec::ExecToLog '"$INSTDIR\ClassKeep-wcli.exe" config set Service/SoftwareSASEnabled 0'
    nsExec::ExecToLog '"$INSTDIR\ClassKeep-wcli.exe" config set Network/FirewallExceptionEnabled 0'
    nsExec::ExecToLog '"$INSTDIR\ClassKeep-wcli.exe" service unregister'
    Goto ck_un_done_wcli
  ck_un_legacy_wcli:
    IfFileExists "$INSTDIR\veyon-wcli.exe" 0 ck_un_done_wcli
      nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" config set Service/SoftwareSASEnabled 0'
      nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" config set Network/FirewallExceptionEnabled 0'
      nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" service unregister'
  ck_un_done_wcli:
  Call StopClassKeepProcesses
  Call RemoveLockedBinaries
  Sleep 1000

  Delete "$INSTDIR\*.exe"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\*.TXT"
  Delete "$INSTDIR\*.ico"
  RmDir /r "$INSTDIR\plugins"
  RmDir /r "$INSTDIR\crypto"
  RmDir /r "$INSTDIR\imageformats"
  RmDir /r "$INSTDIR\platforms"
  RmDir /r "$INSTDIR\styles"
  RmDir /r "$INSTDIR\tls"
  RmDir /r "$INSTDIR\translations"
  Delete "$INSTDIR\install.log"
  RmDir "$INSTDIR"

  Delete "$SMPROGRAMS\ClassKeep\ClassKeep Master.lnk"
  Delete "$SMPROGRAMS\ClassKeep\ClassKeep Configurator.lnk"
  Delete "$SMPROGRAMS\ClassKeep\Uninstall ${ROLE_EN}.lnk"
  RmDir "$SMPROGRAMS\ClassKeep"
  Delete "$DESKTOP\ClassKeep Master.lnk"
  DeleteRegKey ${REG_ROOT} "${UNINST_KEY}"
SectionEnd
