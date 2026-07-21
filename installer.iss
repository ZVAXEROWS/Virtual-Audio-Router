[Setup]
AppName=Virtual Audio Router
AppVersion=0.1.0
AppPublisher=VAR
AppPublisherURL=https://github.com/yourname/VirtualAudioRouter
DefaultDirName={autopf}\Virtual Audio Router
DefaultGroupName=Virtual Audio Router
OutputBaseFilename=VirtualAudioRouter_Setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
SetupIconFile=icon.ico
UninstallDisplayIcon={app}\VirtualAudioRouter.exe

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "gui\dist\VirtualAudioRouter\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "icon.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Virtual Audio Router"; Filename: "{app}\VirtualAudioRouter.exe"; IconFilename: "{app}\icon.ico"
Name: "{autodesktop}\Virtual Audio Router"; Filename: "{app}\VirtualAudioRouter.exe"; Tasks: desktopicon; IconFilename: "{app}\icon.ico"

[Run]
Filename: "{app}\VirtualAudioRouter.exe"; Description: "{cm:LaunchProgram,Virtual Audio Router}"; Flags: nowait postinstall skipifsilent
