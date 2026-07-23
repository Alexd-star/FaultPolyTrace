#ifndef PackageSource
  #error PackageSource must point to a verified FaultPolyTrace package directory.
#endif
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef InstallerOutput
  #define InstallerOutput "."
#endif

[Setup]
AppId={{A6D4F8E2-82AE-4D06-8A1C-86E869D0DF65}
AppName=FaultPolyTrace
AppVersion={#AppVersion}
AppPublisher=FaultPolyTrace Project Team
DefaultDirName={localappdata}\Programs\FaultPolyTrace
DefaultGroupName=FaultPolyTrace
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
OutputDir={#InstallerOutput}
OutputBaseFilename=FaultPolyTrace_Setup_v{#AppVersion}
SetupIconFile={#PackageSource}\faultpolytrace.ico
UninstallDisplayIcon={app}\FaultPolyTrace.exe
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
Uninstallable=yes

[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#PackageSource}\*"; DestDir: "{app}"; \
  Excludes: "src\*,standalone_app\*,scripts\*,答辩评审材料包\*,figure\*,examples\outputs\acceptance_*\*,examples\outputs\standalone_acceptance_*\*,examples\outputs\official\perf_*,examples\outputs\official\test_*,FaultPolyTrace.iss,ChineseSimplified.isl,geoeast_plugin\qt_launcher\*,geoeast_plugin\bo_runner\*.cpp,geoeast_plugin\bo_runner\*.h,geoeast_plugin\bo_runner\*.pro,geoeast_plugin\bo_runner\*.vcxproj,geoeast_plugin\bo_runner\*.filters,geoeast_plugin\mainframe_plugin\*.cpp,geoeast_plugin\mainframe_plugin\*.pro"; \
  Flags: ignoreversion recursesubdirs

[Icons]
Name: "{autoprograms}\FaultPolyTrace"; Filename: "{app}\FaultPolyTrace.exe"; WorkingDir: "{app}"
Name: "{autoprograms}\FaultPolyTrace User Manual"; Filename: "{app}\docs\用户操作手册.md"
Name: "{autodesktop}\FaultPolyTrace"; Filename: "{app}\FaultPolyTrace.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\FaultPolyTrace.exe"; Description: "Launch FaultPolyTrace"; Flags: nowait postinstall skipifsilent
