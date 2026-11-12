; ClassKeep - Windows installer (based on upstream Veyon nsis/veyon.nsi.in)
;
; Compile twice from the repository root (the release workflow supplies VERSION;
; 2.2.0 is only the fallback defined below):
;   makensis -DVERSION=2.2.0 -DROLE=teacher installer.nsi
;   makensis -DVERSION=2.2.0 -DROLE=student installer.nsi
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
  !define VERSION "2.2.0"
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
!define PUBLISHER "zk26"
!define WEB_SITE "https://github.com/zk26/ClassKeep"
!define INSTALL_TYPE "SetShellVarContext all"
!define REG_ROOT "HKLM"
!define LEGACY_TEACHER_DIR "$PROGRAMFILES64\ClassKeep\Teacher"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "ClassKeep"
VIAddVersionKey "CompanyName" "${PUBLISHER}"
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
  ; "service not installed" / "process not found" are EXPECTED on a fresh
  ; install. SetDetailsPrint alone does NOT suppress them: ExecToLog writes the
  ; child's stdout/stderr straight into the details pane (that is what produced
  ; the wall of "[SC] OpenService 失败 1060" / "没有找到进程" messages).
  ; ExecToStack captures the output instead; both stack values are popped into
  ; throw-away registers so the later service checks still see their own values.
  ; One taskkill call with multiple /IM filters is enough for every binary.
  SetDetailsPrint none
  nsExec::ExecToStack '"$SYSDIR\sc.exe" stop ClassKeepService'
  Pop $R8
  Pop $R9
  nsExec::ExecToStack '"$SYSDIR\sc.exe" stop VeyonService'
  Pop $R8
  Pop $R9
  nsExec::ExecToStack '"$SYSDIR\sc.exe" delete VeyonService'
  Pop $R8
  Pop $R9
  nsExec::ExecToStack '"$SYSDIR\taskkill.exe" /F /T /IM ClassKeep-worker.exe /IM ClassKeep-server.exe /IM ClassKeep-service.exe /IM ClassKeep-master.exe /IM ClassKeep-configurator.exe /IM ClassKeep-cli.exe /IM ClassKeep-wcli.exe /IM veyon-worker.exe /IM veyon-server.exe /IM veyon-service.exe /IM veyon-master.exe /IM veyon-configurator.exe /IM veyon-cli.exe /IM veyon-wcli.exe'
  Pop $R8
  Pop $R9
  Sleep 1200
  SetDetailsPrint both
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
  ; Its Chinese console output is GBK and renders as mojibake in a Unicode
  ; installer, so keep it out of the details log too.
  SetDetailsPrint none
  IfFileExists "$INSTDIR\ClassKeep-wcli.exe" 0 ck_legacy_wcli
    nsExec::ExecToStack '"$INSTDIR\ClassKeep-wcli.exe" service unregister'
    Pop $R8
    Pop $R9
    Goto ck_unregister_done
  ck_legacy_wcli:
    IfFileExists "$INSTDIR\veyon-wcli.exe" 0 ck_unregister_done
      nsExec::ExecToStack '"$INSTDIR\veyon-wcli.exe" service unregister'
      Pop $R8
      Pop $R9
  ck_unregister_done:
  SetDetailsPrint both

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
  ; Give the SCM time to actually report the state. Querying immediately after
  ; "service start" can catch START_PENDING and report a false success.
  Sleep 3000

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
    nsExec::ExecToStack '"$SYSDIR\sc.exe" create ClassKeepService binPath= "$INSTDIR\ClassKeep-service.exe" start= auto DisplayName= "ClassKeep Service"'
    Pop $R8
    Pop $R9
    FileWrite $9 "fallback create exit=$R8 output=$R9$\r$\n"
    nsExec::ExecToStack '"$SYSDIR\sc.exe" start ClassKeepService'
    Pop $R8
    Pop $R9
    FileWrite $9 "fallback start exit=$R8 output=$R9$\r$\n"
    nsExec::ExecToStack '"$SYSDIR\cmd.exe" /C "sc query ClassKeepService | findstr RUNNING"'
    Pop $2
    Pop $3
    FileWrite $9 "after fallback RUNNING exit=$2 output=$3$\r$\n"
  ${EndIf}

  ; A service can start and then exit a second later (missing DLL, wrong
  ; install path, blocked by security software). Wait and verify again so the
  ; installer never claims "service running" for a service that is already gone.
  Sleep 3000
  nsExec::ExecToStack '"$SYSDIR\cmd.exe" /C "sc query ClassKeepService | findstr RUNNING"'
  Pop $2
  Pop $3
  FileWrite $9 "final RUNNING check exit=$2 output=$3$\r$\n"

  ; The service must also spawn ClassKeep-server.exe in the console session:
  ; no server process means no tray icon and no 127.0.0.1 VNC connection, even
  ; though the Configurator may show the service itself as running.
  nsExec::ExecToStack '"$SYSDIR\tasklist.exe" /FI "IMAGENAME eq ClassKeep-server.exe" /NH'
  Pop $R8
  Pop $R9
  FileWrite $9 "server processes exit=$R8 output=$R9$\r$\n"

  ${If} $2 != "0"
    FileWrite $9 "RESULT: service NOT running$\r$\n"
    MessageBox MB_OK|MB_ICONEXCLAMATION "服务未能持续运行，安装未完成。$\r$\n$\r$\n请把 $INSTDIR\install.log 发给管理员排查。$\r$\n常见原因：未以管理员运行、被安全软件拦截、或缺少运行库。"
  ${Else}
    FileWrite $9 "RESULT: service RUNNING$\r$\n"
    DetailPrint "服务已正常运行 (ClassKeepService)"
    ; The raw tasklist output is in install.log - logiclib has no "contains"
    ; operator, so leave the interpretation to that log instead of guessing.
    DetailPrint "server 进程清单已写入 install.log，连不上时请查看该段"
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
  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "Publisher" "${PUBLISHER}"
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

  SetDetailsPrint none
  IfFileExists "$INSTDIR\ClassKeep-wcli.exe" 0 ck_un_legacy_wcli
    nsExec::ExecToStack '"$INSTDIR\ClassKeep-wcli.exe" config set Service/SoftwareSASEnabled 0'
    Pop $R8
    Pop $R9
    nsExec::ExecToStack '"$INSTDIR\ClassKeep-wcli.exe" config set Network/FirewallExceptionEnabled 0'
    Pop $R8
    Pop $R9
    nsExec::ExecToStack '"$INSTDIR\ClassKeep-wcli.exe" service unregister'
    Pop $R8
    Pop $R9
    Goto ck_un_done_wcli
  ck_un_legacy_wcli:
    IfFileExists "$INSTDIR\veyon-wcli.exe" 0 ck_un_done_wcli
      nsExec::ExecToStack '"$INSTDIR\veyon-wcli.exe" config set Service/SoftwareSASEnabled 0'
      Pop $R8
      Pop $R9
      nsExec::ExecToStack '"$INSTDIR\veyon-wcli.exe" config set Network/FirewallExceptionEnabled 0'
      Pop $R8
      Pop $R9
      nsExec::ExecToStack '"$INSTDIR\veyon-wcli.exe" service unregister'
      Pop $R8
      Pop $R9
  ck_un_done_wcli:

  ; NSIS does NOT allow an uninstall section to Call a plain function (it
  ; requires an "un." prefixed function). Inline the same stop/cleanup logic
  ; here so uninstalling 1.5.1 also releases the locked worker executables.
  ; ExecToStack keeps the expected "not installed / not found" noise out of the
  ; details pane, on uninstall as well as on install.
  nsExec::ExecToStack '"$SYSDIR\sc.exe" stop ClassKeepService'
  Pop $R8
  Pop $R9
  nsExec::ExecToStack '"$SYSDIR\sc.exe" stop VeyonService'
  Pop $R8
  Pop $R9
  nsExec::ExecToStack '"$SYSDIR\taskkill.exe" /F /T /IM ClassKeep-worker.exe /IM ClassKeep-server.exe /IM ClassKeep-service.exe /IM ClassKeep-master.exe /IM ClassKeep-configurator.exe /IM ClassKeep-cli.exe /IM ClassKeep-wcli.exe /IM veyon-worker.exe /IM veyon-server.exe /IM veyon-service.exe /IM veyon-master.exe /IM veyon-configurator.exe /IM veyon-cli.exe /IM veyon-wcli.exe'
  Pop $R8
  Pop $R9
  Sleep 1200
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
  Sleep 1000
  SetDetailsPrint both

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
