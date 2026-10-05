; CapsSwitch setup. Built by `build.ps1 package`, which passes the version:
;   ISCC.exe /DAppVersion=x.y.z installer\CapsSwitch.iss
; Installs for the current user without UAC by default; the first page offers
; an all-users install into Program Files. One setup carries both EXEs and
; picks the native one.

#ifndef AppVersion
  #error Pass /DAppVersion=x.y.z (build.ps1 package does this).
#endif

[Setup]
AppId={{D6D12B3B-EAC5-4293-A516-29F73BC50F26}
AppName=CapsSwitch
AppVersion={#AppVersion}
AppVerName=CapsSwitch {#AppVersion}
AppPublisher=Sergey Bokonyaev
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\CapsSwitch
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
SetupIconFile=..\assets\CapsSwitch.ico
UninstallDisplayIcon={app}\CapsSwitch.exe
OutputDir=..\dist
OutputBaseFilename=CapsSwitch-v{#AppVersion}-windows-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; The running instance is stopped with --stop, not through Restart Manager.
CloseApplications=no

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[CustomMessages]
en.Autostart=Start CapsSwitch when I sign in to Windows
ru.Autostart=Запускать CapsSwitch при входе в Windows

[Tasks]
Name: "autostart"; Description: "{cm:Autostart}"

[Files]
Source: "..\bin\x64\CapsSwitch.exe"; DestDir: "{app}"; Check: not IsArm64; Flags: ignoreversion
Source: "..\bin\arm64\CapsSwitch.exe"; DestDir: "{app}"; Check: IsArm64; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\CapsSwitch"; Filename: "{app}\CapsSwitch.exe"

; HKA is HKCU for a per-user install and HKLM for an all-users one.
; Rerunning setup with the task cleared removes an earlier entry.
[Registry]
Root: HKA; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "CapsSwitch"; ValueData: """{app}\CapsSwitch.exe"""; Tasks: autostart; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "CapsSwitch"; Tasks: not autostart; Flags: deletevalue

[Run]
; Stops any running copy, such as a portable one, so the installed EXE can start.
Filename: "{app}\CapsSwitch.exe"; Parameters: "--stop"; Flags: runhidden runasoriginaluser waituntilterminated
Filename: "{app}\CapsSwitch.exe"; Description: "{cm:LaunchProgram,CapsSwitch}"; Flags: nowait postinstall skipifsilent runasoriginaluser

[UninstallRun]
Filename: "{app}\CapsSwitch.exe"; Parameters: "--stop"; Flags: runhidden waituntilterminated; RunOnceId: "StopCapsSwitch"

[Code]
{ An update must stop the installed copy before its EXE is replaced. }
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Exe: String;
  ResultCode: Integer;
begin
  Exe := ExpandConstant('{app}\CapsSwitch.exe');
  if FileExists(Exe) then
    ExecAsOriginalUser(Exe, '--stop', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Result := '';
end;
