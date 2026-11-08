; ==============================================================================
; Git Flux — Inno Setup Script
; ==============================================================================

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

#ifndef StageDir
  #define StageDir "..\..\build\packages\stage-windows-x64"
#endif

#ifndef OutputDir
  #define OutputDir "..\..\build\packages"
#endif

#define AppName "Git Flux"
#define AppPublisher "Dokjolly"
#define AppURL "https://github.com/dokjolly/GitFlux"
#define AppExeName "git_flux.exe"
#define AppId "{{E8B2A992-7E1A-4D78-9B43-0985223A4C71}}"

[Setup]
AppId={#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
DefaultDirName={autopf}\GitFlux
UninstallDisplayIcon={app}\{#AppExeName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
LicenseFile=..\..\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=git-flux-{#AppVersion}-windows-x64-setup
SetupIconFile=..\..\assets\git-flux.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog commandline
UsedUserAreasWarning=no
ChangesEnvironment=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "addtopath"; Description: "Add Git Flux to PATH"; GroupDescription: "System Integration:"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\assets\git-flux.ico"; DestDir: "{app}\assets"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\assets\git-flux.ico"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon; IconFilename: "{app}\assets\git-flux.ico"

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Registry]
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Tasks: addtopath; Check: NeedsAddPathHKCU
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Tasks: addtopath; Check: NeedsAddPathHKLM

[Code]
function NeedsAddPath(RootKey: Integer; SubKey: string): Boolean;
var
  CurrentPath: string;
  AppDir: string;
begin
  Result := True;
  if RegQueryStringValue(RootKey, SubKey, 'Path', CurrentPath) then
  begin
    AppDir := ExpandConstant('{app}');
    if Pos(';' + Uppercase(AppDir) + ';', ';' + Uppercase(CurrentPath) + ';') > 0 then
      Result := False
    else if (Copy(Uppercase(CurrentPath), Length(CurrentPath) - Length(AppDir) + 1, Length(AppDir)) = Uppercase(AppDir)) then
      Result := False;
  end;
end;

function NeedsAddPathHKCU(): Boolean;
begin
  Result := (not IsAdminInstallMode) and NeedsAddPath(HKEY_CURRENT_USER, 'Environment');
end;

function NeedsAddPathHKLM(): Boolean;
begin
  Result := IsAdminInstallMode and NeedsAddPath(HKEY_LOCAL_MACHINE, 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment');
end;

procedure RemovePath(RootKey: Integer; SubKey: string);
var
  CurrentPath, AppDir, CleanPath: string;
  I: Integer;
begin
  if RegQueryStringValue(RootKey, SubKey, 'Path', CurrentPath) then
  begin
    AppDir := Uppercase(ExpandConstant('{app}'));
    CleanPath := '';
    while Length(CurrentPath) > 0 do
    begin
      I := Pos(';', CurrentPath);
      if I = 0 then
      begin
        if Uppercase(Trim(CurrentPath)) <> AppDir then
        begin
          if CleanPath <> '' then CleanPath := CleanPath + ';';
          CleanPath := CleanPath + Trim(CurrentPath);
        end;
        CurrentPath := '';
      end
      else
      begin
        if Uppercase(Trim(Copy(CurrentPath, 1, I - 1))) <> AppDir then
        begin
          if CleanPath <> '' then CleanPath := CleanPath + ';';
          CleanPath := CleanPath + Trim(Copy(CurrentPath, 1, I - 1));
        end;
        Delete(CurrentPath, 1, I);
      end;
    end;
    RegWriteStringValue(RootKey, SubKey, 'Path', CleanPath);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    RemovePath(HKEY_CURRENT_USER, 'Environment');
    RemovePath(HKEY_LOCAL_MACHINE, 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment');
  end;
end;
