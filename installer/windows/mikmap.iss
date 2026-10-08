; Windows installer for MikMap, built with Inno Setup 6 (https://jrsoftware.org/isinfo.php).
;
;   ISCC /DAppVersion=0.1.0 /DSourceDir=<dir with mikmap.exe and assets\> /DOutDir=<output dir> mikmap.iss
;
; Produces MikMap-Setup.exe: the file name the store's Download tab expects. Installs per user by default (no
; administrator prompt, into %LOCALAPPDATA%\Programs\MikMap); the wizard offers "all users" via the privileges
; dialog. The installer is NOT code-signed, so SmartScreen warns on first run.

#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\src\build"
#endif
#ifndef OutDir
  #define OutDir "..\..\dist"
#endif

[Setup]
; Keep this GUID forever: it is how Windows recognises an upgrade of the same app.
AppId={{6B1F3C52-8A47-4D0E-9C73-2E5A7B41D908}
AppName=MikMap
AppVersion={#AppVersion}
VersionInfoVersion=0.0.0.0
DefaultDirName={autopf}\MikMap
DefaultGroupName=MikMap
DisableProgramGroupPage=yes
OutputDir={#OutDir}
OutputBaseFilename=MikMap-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\mikmap.exe
; The program writes only to Documents\MikMap and %APPDATA%\MikMap, never to its own folder.

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; Flags: unchecked

[Files]
Source: "{#SourceDir}\mikmap.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\MikMap"; Filename: "{app}\mikmap.exe"
Name: "{autodesktop}\MikMap"; Filename: "{app}\mikmap.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\mikmap.exe"; Description: "Launch MikMap"; Flags: nowait postinstall skipifsilent
