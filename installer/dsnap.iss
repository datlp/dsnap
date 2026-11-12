; ==============================================================================
; dsnap - Inno Setup Installer Script
; Architecture: Pure Win32 x64 Native
; Supports: All Users (Admin / Program Files) vs Current User (Non-Admin / LocalAppData)
; ==============================================================================

#define MyAppName "dsnap"
#ifndef MyAppVersion
  #define MyAppVersion "1.1.0"
#endif
#define MyAppPublisher "datlp"
#define MyAppURL "https://github.com/datlp/dsnap"
#define MyAppExeName "dsnap.exe"

[Setup]
; Unique App ID (generated GUID)
AppId={{C8E284D7-0624-4B3B-8C91-6E9528C5D9B0}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases

; Dual Installation Modes (All Users vs Current User)
; "dialog" shows a prompt asking whether to install for All Users or Current User
PrivilegesRequiredOverridesAllowed=dialog
PrivilegesRequired=lowest

; Target Directories:
; All Users: C:\Program Files\dsnap
; Current User: %LOCALAPPDATA%\Programs\dsnap
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes

; Output Configuration
OutputDir=..\release
OutputBaseFilename=dsnap-v{#MyAppVersion}-setup
SetupIconFile=..\res\dsnap.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

; Visual & Modern UI Style
WizardStyle=modern
DisableWelcomePage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "autostart"; Description: "Start dsnap automatically when Windows starts"; GroupDescription: "Startup options:"

[Files]
Source: "..\dsnap.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
; Start Menu Shortcut
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"
; Optional Desktop Shortcut
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; IconFilename: "{app}\{#MyAppExeName}"

[Registry]
; Autostart task (Runs only for current user when selected)
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#MyAppName}"; ValueData: """{app}\{#MyAppExeName}"""; Tasks: autostart; Flags: uninsdeletevalue

[Run]
; Option to launch dsnap immediately after installation
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
// Helper to terminate running dsnap before updating or uninstalling
function InitializeSetup(): Boolean;
var
  ResultCode: Integer;
begin
  Result := True;
  Exec('taskkill.exe', '/F /IM dsnap.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

function InitializeUninstall(): Boolean;
var
  ResultCode: Integer;
begin
  Result := True;
  Exec('taskkill.exe', '/F /IM dsnap.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

