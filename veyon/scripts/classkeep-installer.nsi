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

  ; Upstream uses `veyon-wcli.exe service register/unregister` here. That CLI
  ; constructs a full VeyonCore at startup and qFatal()s (Windows __fastfail
  ; "快速异常检测失败" dialog) whenever the platform/QCA plugin is missing,
  ; which blocked BOTH install and uninstall. Register the same Windows service
  ; with the built-in sc.exe instead - no Qt process, no crash dialog.
  nsExec::ExecToLog '"$SYSDIR\sc.exe" stop VeyonService'
  nsExec::ExecToLog '"$SYSDIR\sc.exe" delete VeyonService'
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

  ; Same service name / binary / auto start as WindowsServiceControl.cpp.
  nsExec::ExecToLog '"$SYSDIR\sc.exe" create VeyonService binPath= "$INSTDIR\veyon-service.exe" start= auto DisplayName= "Veyon Service"'
  nsExec::ExecToLog '"$SYSDIR\sc.exe" description VeyonService "Veyon / ClassKeep Service"'
  nsExec::ExecToLog '"$SYSDIR\netsh.exe" advfirewall firewall add rule name="ClassKeep Veyon Service" dir=in action=allow program="$INSTDIR\veyon-service.exe" enable=yes'
  WriteRegDWORD ${REG_ROOT} "SYSTEM\CurrentControlSet\Control\Lsa" "ForceGuest" "0"
  nsExec::ExecToLog '"$SYSDIR\sc.exe" start VeyonService'
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
  ; No veyon-wcli here either - see the install section for the __fastfail reason.
  nsExec::ExecToLog '"$SYSDIR\sc.exe" stop VeyonService'
  nsExec::ExecToLog '"$SYSDIR\sc.exe" delete VeyonService'
  nsExec::ExecToLog '"$SYSDIR\netsh.exe" advfirewall firewall delete rule name="ClassKeep Veyon Service"'
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
  RmDir "$INSTDIR"

  Delete "$SMPROGRAMS\课守 ClassKeep\课守 ClassKeep 控制台.lnk"
  Delete "$SMPROGRAMS\课守 ClassKeep\课守 ClassKeep 配置.lnk"
  Delete "$SMPROGRAMS\课守 ClassKeep\卸载 ${ROLE_CN}.lnk"
  RmDir "$SMPROGRAMS\课守 ClassKeep"
  Delete "$DESKTOP\课守 ClassKeep 控制台.lnk"
  DeleteRegKey ${REG_ROOT} "${UNINST_KEY}"
SectionEnd
