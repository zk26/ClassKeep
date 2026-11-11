; 课守 ClassKeep - Windows installer (based on upstream Veyon nsis/veyon.nsi.in)
;
; Compile twice from the repository root:
;   makensis -DVERSION=1.3.0 -DROLE=teacher installer.nsi
;   makensis -DVERSION=1.3.0 -DROLE=student installer.nsi
;
; ROLE=teacher : component page with "Service" (read-only, registers/starts the
;                Veyon service) and "Teacher console" (optional Master).
; ROLE=student : component page with ONLY the read-only Service section.
;                veyon-master.exe is never shipped, so the Master option does
;                not exist at all - this is the upstream /NoMaster behaviour.
;
; CI copies this file to the repository root with a UTF-8 BOM (Unicode true).

Unicode true

!include "LogicLib.nsh"
!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef VERSION
  !define VERSION "1.3.0"
!endif
!ifndef ROLE
  !define ROLE "teacher"
!endif

!if "${ROLE}" == "teacher"
  !define ROLE_CN "教师端"
  !define OUT_FILE "dist\ClassKeep-Teacher-${VERSION}-win64-setup.exe"
  !define INST_DIR "$PROGRAMFILES64\ClassKeep\Teacher"
  !define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\ClassKeepTeacher"
!else
  !define ROLE_CN "学生端"
  !define OUT_FILE "dist\ClassKeep-Student-${VERSION}-win64-setup.exe"
  !define INST_DIR "$PROGRAMFILES64\ClassKeep\Student"
  !define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\ClassKeepStudent"
!endif

!define APP_NAME "课守 ClassKeep"
!define WEB_SITE "https://github.com/zk26/ClassKeep"
!define INSTALL_TYPE "SetShellVarContext all"
!define REG_ROOT "HKLM"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "ClassKeep"
VIAddVersionKey "CompanyName" "ClassKeep"
VIAddVersionKey "FileDescription" "ClassKeep ${ROLE_CN} Setup"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "GPL-2.0-or-later; based on Veyon (c) Veyon Solutions"

SetCompressor /SOLID lzma
SetCompressorDictSize 64
Name "${APP_NAME} ${ROLE_CN} ${VERSION}"
Caption "${APP_NAME} ${ROLE_CN} ${VERSION}"
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

; ---------------------------------------------------------------- Service (RO)
Section "课守 ClassKeep 服务端（必需）" SEC_SERVICE
  SectionIn RO
  ${INSTALL_TYPE}
  SetOverwrite on
  SetOutPath "$INSTDIR"

  ; Keep the ORIGINAL upstream Veyon installation flow via veyon-wcli.exe.
  ; It requires a complete `plugins/windows-platform.dll` + Qt6 `libqca-ossl.dll`
  ; at startup; collect-windows-runtime.sh hard-verifies both so the CLI can no
  ; longer qFatal()/__fastfail while installing or uninstalling.
  ; On a FIRST install wcli does not exist yet, so skip the unregister call.
  IfFileExists "$INSTDIR\veyon-wcli.exe" 0 +2
    nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" service unregister'
  Delete "$INSTDIR\*.dll"
  RmDir /r "$INSTDIR\plugins"

  File "payload\veyon-server.exe"
  File "payload\veyon-service.exe"
  File "payload\veyon-configurator.exe"
  File "payload\veyon-cli.exe"
  File "payload\veyon-wcli.exe"
  File "payload\veyon-worker.exe"
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
  ; Chinese UI by default; switchable in Veyon Configurator -> General -> Language.
  SetOutPath "$INSTDIR\translations"
  File "payload\translations\*.qm"

  ; ------------------------------------------------------------------
  ; ORIGINAL upstream sequence, but with return codes + a real self-check.
  ; Previously ExecWait ignored every failure, so a crashed wcli silently
  ; produced an "installed" app whose service never ran (127.0.0.1 refused).
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR"
  FileOpen $9 "$INSTDIR\install.log" w
  FileWrite $9 "课守 ClassKeep ${VERSION} ${ROLE_CN} 安装日志$\r$\n"

  nsExec::ExecToStack '"$INSTDIR\veyon-wcli.exe" service register'
  Pop $R0
  Pop $1
  DetailPrint "注册服务 (service register): exit=$R0"
  FileWrite $9 "service register exit=$R0 output=$1$\r$\n"

  nsExec::ExecToStack '"$INSTDIR\veyon-wcli.exe" config set Network/FirewallExceptionEnabled 1'
  Pop $0
  Pop $1
  FileWrite $9 "config FirewallExceptionEnabled exit=$0 output=$1$\r$\n"

  nsExec::ExecToStack '"$INSTDIR\veyon-wcli.exe" config set Windows/SoftwareSASEnabled 1'
  Pop $0
  Pop $1
  FileWrite $9 "config SoftwareSASEnabled exit=$0 output=$1$\r$\n"

  WriteRegDWORD ${REG_ROOT} "SYSTEM\CurrentControlSet\Control\Lsa" "ForceGuest" "0"

  nsExec::ExecToStack '"$INSTDIR\veyon-wcli.exe" service start'
  Pop $R1
  Pop $1
  DetailPrint "启动服务 (service start): exit=$R1"
  FileWrite $9 "service start exit=$R1 output=$1$\r$\n"

  ; Ask the Service Control Manager directly. findstr exits 0 only when the
  ; service STATE line contains RUNNING, so $2 is our ground truth.
  nsExec::ExecToStack '"$SYSDIR\cmd.exe" /C "sc query VeyonService | findstr RUNNING"'
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
    nsExec::ExecToLog '"$SYSDIR\sc.exe" create VeyonService binPath= "$INSTDIR\veyon-service.exe" start= auto DisplayName= "ClassKeep Service"'
    nsExec::ExecToLog '"$SYSDIR\sc.exe" start VeyonService'
    nsExec::ExecToStack '"$SYSDIR\cmd.exe" /C "sc query VeyonService | findstr RUNNING"'
    Pop $2
    Pop $3
    FileWrite $9 "after fallback RUNNING exit=$2 output=$3$\r$\n"
  ${EndIf}

  ${If} $2 != "0"
    FileWrite $9 "RESULT: service NOT running$\r$\n"
    MessageBox MB_OK|MB_ICONEXCLAMATION "服务未能启动，安装未完成。$\r$\n$\r$\n请把 $INSTDIR\install.log 发给管理员排查。$\r$\n常见原因：未以管理员运行、被安全软件拦截、或缺少运行库。"
  ${Else}
    FileWrite $9 "RESULT: service RUNNING$\r$\n"
    DetailPrint "服务已正常运行 (VeyonService)"
  ${EndIf}
  FileClose $9
SectionEnd

; ------------------------------------------------------- Teacher console only
!if "${ROLE}" == "teacher"
Section "课守 ClassKeep 教师控制台" SEC_MASTER
  ${INSTALL_TYPE}
  SetOutPath "$INSTDIR"
  File "payload\veyon-master.exe"
  CreateDirectory "$SMPROGRAMS\课守 ClassKeep"
  CreateShortCut "$SMPROGRAMS\课守 ClassKeep\课守 ClassKeep 控制台.lnk" \
    "$INSTDIR\veyon-master.exe" "" "$INSTDIR\ClassKeep.ico"
  CreateShortCut "$DESKTOP\课守 ClassKeep 控制台.lnk" \
    "$INSTDIR\veyon-master.exe" "" "$INSTDIR\ClassKeep.ico"
SectionEnd
!endif

; --------------------------------------------------------------- Common / reg
Section -Post
  ${INSTALL_TYPE}
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateDirectory "$SMPROGRAMS\课守 ClassKeep"
  CreateShortCut "$SMPROGRAMS\课守 ClassKeep\课守 ClassKeep 配置.lnk" \
    "$INSTDIR\veyon-configurator.exe" "" "$INSTDIR\ClassKeep.ico"
  CreateShortCut "$SMPROGRAMS\课守 ClassKeep\卸载 ${ROLE_CN}.lnk" "$INSTDIR\uninstall.exe"

  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "DisplayName" "${APP_NAME} ${ROLE_CN}"
  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\ClassKeep.ico"
  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "Publisher" "ClassKeep"
  WriteRegStr ${REG_ROOT} "${UNINST_KEY}" "URLInfoAbout" "${WEB_SITE}"
SectionEnd

; ------------------------------------------------------------------- Uninstall
Section Uninstall
  ${INSTALL_TYPE}
  ; Student uninstall protection: the teacher console writes this HKLM flag.
  ; Only a teacher-side "unlock" command can clear it (see ClassroomPlugin).
  SetRegView 64
  ReadRegDWORD $0 ${REG_ROOT} "SOFTWARE\ClassKeep\Student" "UninstallProtection"
  ${If} $0 == 1
    MessageBox MB_OK|MB_ICONSTOP "教师端已开启卸载保护，请联系教师端解除保护后再卸载。"
    Abort
  ${EndIf}

  ; Mirror the upstream uninstall sequence.
  nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" config set Service/SoftwareSASEnabled 0'
  nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" config set Network/FirewallExceptionEnabled 0'
  nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" service unregister'
  Sleep 2000

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

  Delete "$SMPROGRAMS\课守 ClassKeep\课守 ClassKeep 控制台.lnk"
  Delete "$SMPROGRAMS\课守 ClassKeep\课守 ClassKeep 配置.lnk"
  Delete "$SMPROGRAMS\课守 ClassKeep\卸载 ${ROLE_CN}.lnk"
  RmDir "$SMPROGRAMS\课守 ClassKeep"
  Delete "$DESKTOP\课守 ClassKeep 控制台.lnk"
  DeleteRegKey ${REG_ROOT} "${UNINST_KEY}"
SectionEnd
