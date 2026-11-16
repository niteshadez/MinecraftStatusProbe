[Setup]
AppName=Minecraft Status Probe
AppVersion=1.0
DefaultDirName={autopf}\MinecraftStatusProbe
DefaultGroupName=Minecraft Status Probe
OutputDir=installer_output
OutputBaseFilename=MinecraftStatusProbe_Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
; Set the installer setup wizard icon (replace with the relative path to your .ico file)
SetupIconFile=logo.ico

[Files]
; Package the compiled executable
Source: "cmake-build-debug\MinecraftStatusProbeV1.exe"; DestDir: "{app}"; Flags: ignoreversion

; Package the plain text README document into the install folder
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion

; Package all necessary MinGW runtime DLLs directly into the install folder
Source: "cmake-build-debug\libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "cmake-build-debug\libgcc_s_seh-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "cmake-build-debug\libstdc++-6.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Minecraft Status Probe"; Filename: "{app}\MinecraftStatusProbeV1.exe"
Name: "{autodesktop}\Minecraft Status Probe"; Filename: "{app}\MinecraftStatusProbeV1.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Run]
; Both checkboxes will now be checked by default upon install completion
Filename: "{app}\MinecraftStatusProbeV1.exe"; Description: "Launch Minecraft Status Probe"; Flags: postinstall nowait skipifsilent
Filename: "{app}\README.txt"; Description: "View Readme document"; Flags: shellexec postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{userappdata}\MinecraftStatusProbe\targets.md"
Type: files; Name: "{userappdata}\MinecraftStatusProbe\players.md"
Type: files; Name: "{userappdata}\MinecraftStatusProbe\export_path.txt"
Type: dirifempty; Name: "{userappdata}\MinecraftStatusProbe"

Type: files; Name: "{userdocs}\matches.txt"
Type: files; Name: "{userdesktop}\matches.txt"

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var
    AppDataDir: String;
begin
  if CurStep = ssPostInstall then
  begin
    AppDataDir := ExpandConstant('{userappdata}\MinecraftStatusProbe');
    ForceDirectories(AppDataDir);
    SaveStringToFile(AppDataDir + '\targets.md', '', False);
    SaveStringToFile(AppDataDir + '\players.md', '', False);
  end;
end;