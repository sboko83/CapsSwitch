; CapsSwitch setup. Built by `build.ps1 package`, which passes the version:
;   ISCC.exe /DAppVersion=x.y.z installer\CapsSwitch.iss
; Installs for the current user without UAC by default; the first page offers
; an all-users install into Program Files. One setup carries both EXEs and
; picks the native one.
; The elevated sign-in option replaces the Run entry with a Task Scheduler job
; ("run with highest privileges"): a Run entry cannot elevate, the job starts
; silently at sign-in. Creating or deleting the job elevates through UAC once
; when the setup itself is not elevated.

#ifndef AppVersion
  #error Pass /DAppVersion=x.y.z (build.ps1 package does this).
#endif

[Setup]
AppId={{D6D12B3B-EAC5-4293-A516-29F73BC50F26}
AppName=CapsSwitch
AppVersion={#AppVersion}
AppVerName=CapsSwitch {#AppVersion}
AppPublisher=Sergey Bokonyaev
AppContact=sergey.bokonyaev@gmail.com
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
en.AutostartAdmin=Start it elevated, for switching in administrator windows (Task Scheduler)
ru.AutostartAdmin=Запускать от имени администратора — для окон администратора (через планировщик)
en.TaskCreateFailed=The scheduled task could not be created (UAC declined?). CapsSwitch will start with normal rights.
ru.TaskCreateFailed=Не удалось создать задачу планировщика (UAC отклонён?). CapsSwitch будет запускаться с обычными правами.

[Tasks]
Name: "autostart"; Description: "{cm:Autostart}"
; A child of autostart: clearing autostart clears it too. Up to 1.1.1 it was
; the top-level "autostart_admin"; CurPageChanged carries a cleared one over.
Name: "autostart\admin"; Description: "{cm:AutostartAdmin}"

[Files]
Source: "..\bin\x64\CapsSwitch.exe"; DestDir: "{app}"; Check: not IsArm64; Flags: ignoreversion
Source: "..\bin\arm64\CapsSwitch.exe"; DestDir: "{app}"; Check: IsArm64; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\CapsSwitch"; Filename: "{app}\CapsSwitch.exe"

; HKA is HKCU for a per-user install and HKLM for an all-users one.
; Rerunning setup with a task cleared removes an earlier entry. The elevated
; option replaces the Run entry with a scheduler job created in [Code].
[Registry]
Root: HKA; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "CapsSwitch"; ValueData: """{app}\CapsSwitch.exe"""; Tasks: autostart; Check: not IsAdminAutostart; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "CapsSwitch"; Tasks: not autostart; Flags: deletevalue
Root: HKA; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "CapsSwitch"; Tasks: autostart; Check: IsAdminAutostart; Flags: deletevalue

[Run]
; Stops any running copy, such as a portable one, so the installed EXE can start.
Filename: "{app}\CapsSwitch.exe"; Parameters: "--stop"; Flags: runhidden runasoriginaluser waituntilterminated
Filename: "{app}\CapsSwitch.exe"; Description: "{cm:LaunchProgram,CapsSwitch}"; Flags: nowait postinstall skipifsilent runasoriginaluser

[UninstallRun]
Filename: "{app}\CapsSwitch.exe"; Parameters: "--stop"; Flags: runhidden waituntilterminated; RunOnceId: "StopCapsSwitch"

[Code]
const
  TaskName = 'CapsSwitch';
  UninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{D6D12B3B-EAC5-4293-A516-29F73BC50F26}_is1';

var
  PreviousTasksApplied: Boolean;

function IsAdminAutostart: Boolean;
begin
  Result := WizardIsTaskSelected('autostart') and WizardIsTaskSelected('autostart\admin');
end;

{ The renamed child is new to the remembered selection and would come back
  checked, so an "autostart_admin" cleared before is cleared again, once. }
procedure CurPageChanged(CurPageID: Integer);
var
  Deselected: String;
begin
  if (CurPageID <> wpSelectTasks) or PreviousTasksApplied then Exit;
  PreviousTasksApplied := True;
  if RegQueryStringValue(HKA, UninstallKey, 'Inno Setup: Deselected Tasks', Deselected) and
     (Pos(',autostart_admin,', ',' + Deselected + ',') > 0) then
    WizardSelectTasks('!autostart\admin');
end;

{ Runs schtasks with the setup's own rights; a per-user setup is not elevated,
  so a denied command is retried once through a UAC prompt. }
function RunTaskCommand(const Params: String): Boolean;
var
  Code: Integer;
begin
  if Exec('schtasks.exe', Params, '', SW_HIDE, ewWaitUntilTerminated, Code) and (Code = 0) then
    Result := True
  else
    Result := ShellExec('runas', 'schtasks.exe', Params, '', SW_HIDE, ewWaitUntilTerminated, Code) and (Code = 0);
end;

function TaskExists: Boolean;
var
  Code: Integer;
begin
  Result := Exec('schtasks.exe', '/Query /TN ' + TaskName, '', SW_HIDE, ewWaitUntilTerminated, Code) and (Code = 0);
end;

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

{ schtasks /XML rejects UTF-8 with a BOM or an encoding declaration and reads
  a plain file as ANSI, so the file stays ASCII: every other character goes
  out as a numeric reference, a surrogate pair as one code point. }
function XmlText(const S: String): String;
var
  I, Code: Integer;
begin
  Result := '';
  I := 1;
  while I <= Length(S) do begin
    Code := Ord(S[I]);
    if (Code >= $D800) and (Code <= $DBFF) and (I < Length(S)) then begin
      Code := $10000 + ((Code - $D800) shl 10) + (Ord(S[I + 1]) - $DC00);
      I := I + 1;
    end;
    if Code = Ord('&') then Result := Result + '&amp;'
    else if Code = Ord('<') then Result := Result + '&lt;'
    else if Code = Ord('>') then Result := Result + '&gt;'
    else if Code > 127 then Result := Result + '&#' + IntToStr(Code) + ';'
    else Result := Result + S[I];
    I := I + 1;
  end;
end;

{ A job from XML, not from /TR: /TR splits an unquoted path at its first space
  ("C:\Program Files"), and its defaults skip the start on battery power, stop
  the app after 72 hours and run it below normal priority. }
function CreateTask: Boolean;
var
  User, Xml, XmlFile: String;
begin
  User := XmlText(GetEnv('USERDOMAIN') + '\' + GetUserNameString);
  Xml :=
    '<?xml version="1.0"?>' + #13#10 +
    '<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">' + #13#10 +
    '  <Triggers><LogonTrigger><UserId>' + User + '</UserId></LogonTrigger></Triggers>' + #13#10 +
    '  <Principals><Principal id="Author"><UserId>' + User + '</UserId>' +
      '<LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal></Principals>' + #13#10 +
    '  <Settings>' + #13#10 +
    '    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>' + #13#10 +
    '    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>' + #13#10 +
    '    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>' + #13#10 +
    '    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>' + #13#10 +
    '    <Priority>5</Priority>' + #13#10 +
    '  </Settings>' + #13#10 +
    '  <Actions Context="Author"><Exec><Command>' +
      XmlText(ExpandConstant('{app}\CapsSwitch.exe')) + '</Command></Exec></Actions>' + #13#10 +
    '</Task>' + #13#10;
  XmlFile := ExpandConstant('{tmp}\CapsSwitchTask.xml');
  Result := SaveStringToFile(XmlFile, Xml, False) and
    RunTaskCommand('/Create /F /TN ' + TaskName + ' /XML ' + AddQuotes(XmlFile));
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep <> ssPostInstall then Exit;
  if IsAdminAutostart then begin
    if not CreateTask then
      MsgBox(CustomMessage('TaskCreateFailed'), mbError, MB_OK);
  end
  { A re-run with the elevated option cleared removes the earlier job. }
  else if TaskExists then
    RunTaskCommand('/Delete /TN ' + TaskName + ' /F');
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    if TaskExists then
      RunTaskCommand('/Delete /TN ' + TaskName + ' /F');
end;
