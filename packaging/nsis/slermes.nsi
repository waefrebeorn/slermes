; NSIS installer script for Slermes
; Build: makensis packaging/nsis/slermes.nsi
; Produces: Slermes-Windows-x64-installer.exe

!include "MUI2.nsh"
!include "LogicLib.nsh"

; --- Configuration ---
!define APP_NAME "Slermes"
!define APP_VERSION "502.0.0"
!define PUBLISHER "Slermes Fork"
!define WEBSITE "https://github.com/waefrebeorn/slermes"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "Slermes-Windows-x64-installer.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
RequestExecutionLevel admin

; --- Modern UI ---
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; --- Sections ---
Section "Install"
  SetOutPath "$INSTDIR"

  ; Main binary
  File "slermes.exe"

  ; Config template
  File /oname=config.yaml.example "config.example.yaml"

  ; Documentation
  File "README.md"

  ; Create start menu shortcuts
  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\slermes.exe"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe"

  ; Add to PATH
  EnVar::SetHKCU "PATH" "$INSTDIR"

  ; Write uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Write registry for Add/Remove Programs
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
    "DisplayName" "${APP_NAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
    "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
    "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
    "Publisher" "${PUBLISHER}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
    "URLInfoAbout" "${WEBSITE}"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\slermes.exe"
  Delete "$INSTDIR\config.yaml.example"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\uninstall.exe"

  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"
  RMDir "$SMPROGRAMS\${APP_NAME}"
  RMDir "$INSTDIR"

  ; Remove from PATH
  EnVar::DeleteValue HKCU "PATH" "$INSTDIR"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
SectionEnd
