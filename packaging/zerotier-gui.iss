#ifndef PackageVersion
  #define PackageVersion "0.1.0"
#endif
#ifndef PayloadDir
  #define PayloadDir "..\dist"
#endif
#ifndef PackageOutput
  #define PackageOutput "..\packages"
#endif

[Setup]
AppId={{83C4E33A-AC50-43C7-9E69-7A84D1837D95}
AppName=ZeroTier GUI
AppVersion={#PackageVersion}
AppPublisher=vusiif
AppPublisherURL=https://github.com/vusiif/zerotier-gui
AppSupportURL=https://gitee.com/vusiif/zerotier-gui
DefaultDirName={autopf}\ZeroTier GUI
DefaultGroupName=ZeroTier GUI
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#PackageOutput}
OutputBaseFilename=ZeroTier-GUI-{#PackageVersion}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\zerotier_gui.exe
CloseApplications=yes

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\ZeroTier GUI"; Filename: "{app}\zerotier_gui.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\ZeroTier GUI"; Filename: "{app}\zerotier_gui.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing Microsoft Visual C++ runtime..."; Flags: waituntilterminated; Check: NeedsRuntime
Filename: "{app}\zerotier_gui.exe"; Description: "Launch ZeroTier GUI"; Flags: postinstall nowait skipifsilent

[Code]
function NeedsRuntime: Boolean;
var
  Installed: Cardinal;
begin
  Result := not RegQueryDWordValue(HKLM64,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed);
  if not Result then
    Result := Installed <> 1;
end;
