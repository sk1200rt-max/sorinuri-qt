; 소리누리 플레이어 Inno Setup 설치 스크립트
; Inno Setup 6.x 이상 필요

#define MyAppName "소리누리"
#define MyAppNameEn "Sorinuri"
#define MyAppVersion "6.18.4"
#define MyAppPublisher "Gaon Communication"
#define MyAppURL "https://sorinuri.com"
#define MyAppExeName "Sorinuri.exe"
#define MyAppDescription "소리누리 - 전문 동영상/오디오 플레이어"

[Setup]
AppId={{8A3F2C1D-9E4B-4F7A-B2D6-1C5E8F3A9B2E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppNameEn}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=..\dist
OutputBaseFilename=Sorinuri-Setup-{#MyAppVersion}
SetupIconFile=..\resources\sorinuri.ico
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
MinVersion=10.0.17763
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
; 체크박스·버튼은 Windows의 DPI 인식 네이티브 컨트롤로 렌더링한다.
; 250% 배율에서 modern 스타일 체크박스가 잘리던 문제를 방지한다.
WizardStyle=modern excludelightcontrols
WizardResizable=no
RestartIfNeededByRun=no
; 설치 마법사 이미지 (소리누리 브랜드)
WizardImageFile=wizard-banner.bmp
WizardSmallImageFile=wizard-small.bmp
; 소리누리 실행 파일만 정상 종료를 요청한다. 강제 종료나 자동 무인 제거는 하지 않는다.
CloseApplications=yes
CloseApplicationsFilter=Sorinuri.exe
RestartApplications=no
ChangesAssociations=yes

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"


[Tasks]
Name: "desktopicon"; Description: "바탕화면에 아이콘 만들기(&D)"; GroupDescription: "추가 아이콘:"
Name: "quicklaunchicon"; Description: "빠른 실행에 아이콘 만들기(&Q)"; GroupDescription: "추가 아이콘:"; Flags: unchecked; OnlyBelowVersion: 6.1; Check: not IsAdminInstallMode
; ── 파일 형식 연결 ──────────────────────────────────────────────────────────
Name: "fileassoc"; Description: "호환 파일 형식을 소리누리로 연결(&F)"; GroupDescription: "파일 형식 연결:"; Flags: unchecked

[Files]
Source: "..\dist\Sorinuri-Portable\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\Sorinuri-Portable\libmpv-2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\Sorinuri-Portable\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.zip,*.bat"
; ffmpeg 번들 (화면 녹화 기능용) - 선택적 설치
; 빌드 시 dist/ffmpeg.exe가 있으면 자동 포함
Source: "..\dist\ffmpeg.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
; Visual C++ 런타임 재배포 패키지 (설치 시 자동 실행)
Source: "..\dist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: ignoreversion skipifsourcedoesntexist deleteafterinstall

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "{#MyAppDescription}"
Name: "{group}\{#MyAppName} 제거"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "{#MyAppDescription}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon

[Run]
; Visual C++ 런타임 자동 설치 (이미 설치되어 있으면 건너땀)
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Visual C++ 런타임 설치 중..."; Flags: skipifdoesntexist waituntilterminated
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Registry]
; ── 동영상 형식 연결 (Tasks: fileassoc) ────────────────────────────────
Root: HKCU; Subkey: "Software\Classes\.mkv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mkv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mkv"; ValueType: string; ValueName: ""; ValueData: "MKV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mkv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mkv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.mp4\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mp4"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp4"; ValueType: string; ValueName: ""; ValueData: "MP4 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp4\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp4\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.avi\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.avi"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.avi"; ValueType: string; ValueName: ""; ValueData: "AVI 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.avi\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.avi\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.mov\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mov"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mov"; ValueType: string; ValueName: ""; ValueData: "MOV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mov\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mov\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.wmv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.wmv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wmv"; ValueType: string; ValueName: ""; ValueData: "WMV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wmv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wmv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.m2ts\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.m2ts"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m2ts"; ValueType: string; ValueName: ""; ValueData: "M2TS 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m2ts\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m2ts\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.ts\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ts"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ts"; ValueType: string; ValueName: ""; ValueData: "TS 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ts\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ts\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.m4v\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.m4v"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m4v"; ValueType: string; ValueName: ""; ValueData: "M4V 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m4v\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m4v\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.webm\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.webm"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.webm"; ValueType: string; ValueName: ""; ValueData: "WebM 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.webm\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.webm\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.flv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.flv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flv"; ValueType: string; ValueName: ""; ValueData: "FLV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.3gp\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.3gp"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.3gp"; ValueType: string; ValueName: ""; ValueData: "3GP 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.3gp\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.3gp\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.ogv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ogv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ogv"; ValueType: string; ValueName: ""; ValueData: "OGV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ogv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ogv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.rmvb\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.rmvb"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.rmvb"; ValueType: string; ValueName: ""; ValueData: "RMVB 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.rmvb\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.rmvb\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.rm\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.rm"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.rm"; ValueType: string; ValueName: ""; ValueData: "RM 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.rm\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.rm\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

; ── 오디오 형식 연결 (Tasks: fileassoc) ────────────────────────────────
Root: HKCU; Subkey: "Software\Classes\.flac\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.flac"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flac"; ValueType: string; ValueName: ""; ValueData: "FLAC 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flac\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flac\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.mp3\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mp3"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp3"; ValueType: string; ValueName: ""; ValueData: "MP3 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp3\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp3\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.aac\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.aac"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.aac"; ValueType: string; ValueName: ""; ValueData: "AAC 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.aac\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.aac\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.ogg\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ogg"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ogg"; ValueType: string; ValueName: ""; ValueData: "OGG 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ogg\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ogg\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.opus\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.opus"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.opus"; ValueType: string; ValueName: ""; ValueData: "Opus 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.opus\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.opus\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.wav\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.wav"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wav"; ValueType: string; ValueName: ""; ValueData: "WAV 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wav\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wav\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.m4a\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.m4a"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m4a"; ValueType: string; ValueName: ""; ValueData: "M4A 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m4a\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m4a\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.wma\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.wma"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wma"; ValueType: string; ValueName: ""; ValueData: "WMA 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wma\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wma\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.ape\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ape"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ape"; ValueType: string; ValueName: ""; ValueData: "APE 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ape\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ape\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.dsf\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.dsf"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.dsf"; ValueType: string; ValueName: ""; ValueData: "DSF 오디오 파일 (DSD)"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.dsf\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.dsf\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.dff\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.dff"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.dff"; ValueType: string; ValueName: ""; ValueData: "DFF 오디오 파일 (DSD)"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.dff\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.dff\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCU; Subkey: "Software\Classes\.mka\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mka"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mka"; ValueType: string; ValueName: ""; ValueData: "MKA 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mka\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mka\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc


; ── Windows Capabilities 등록 (탐색기 우클릭 "연결 프로그램" 목록 표시 필수) ──────────
; RegisteredApplications 없이는 Windows가 소리누리를 연결 프로그램 목록에 표시하지 않음
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "소리누리"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "소리누리 - Windows 하이엔드 미디어 플레이어"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mkv"; ValueData: "Sorinuri.mkv"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp4"; ValueData: "Sorinuri.mp4"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".avi"; ValueData: "Sorinuri.avi"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mov"; ValueData: "Sorinuri.mov"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wmv"; ValueData: "Sorinuri.wmv"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m2ts"; ValueData: "Sorinuri.m2ts"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ts"; ValueData: "Sorinuri.ts"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4v"; ValueData: "Sorinuri.m4v"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".webm"; ValueData: "Sorinuri.webm"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flv"; ValueData: "Sorinuri.flv"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".3gp"; ValueData: "Sorinuri.3gp"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogv"; ValueData: "Sorinuri.ogv"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".rmvb"; ValueData: "Sorinuri.rmvb"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".rm"; ValueData: "Sorinuri.rm"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flac"; ValueData: "Sorinuri.flac"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp3"; ValueData: "Sorinuri.mp3"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".aac"; ValueData: "Sorinuri.aac"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogg"; ValueData: "Sorinuri.ogg"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".opus"; ValueData: "Sorinuri.opus"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wav"; ValueData: "Sorinuri.wav"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4a"; ValueData: "Sorinuri.m4a"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wma"; ValueData: "Sorinuri.wma"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ape"; ValueData: "Sorinuri.ape"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dsf"; ValueData: "Sorinuri.dsf"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dff"; ValueData: "Sorinuri.dff"
Root: HKCU; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mka"; ValueData: "Sorinuri.mka"
; RegisteredApplications: Windows에 소리누리를 공식 등록 (연결 프로그램 목록 표시 핵심 키)
Root: HKCU; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "소리누리"; ValueData: "Software\Sorinuri\Capabilities"; Flags: uninsdeletevalue
[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
// ============================================================================
// Shell32 DLL 직접 호출 (파일 연결 즉시 반영)
// ============================================================================
procedure SHChangeNotifyDirect(wEventId: Integer; uFlags: Cardinal; dwItem1: Integer; dwItem2: Integer);
  external 'SHChangeNotify@shell32.dll stdcall';

// 설치 완료 후 파일 연결 변경 알림
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then begin
    // 파일 연결 선택 시에만 Windows 셸에 연결 변경을 알린다.
    // Explorer 종료·재시작이나 아이콘/썸네일 캐시 삭제는 수행하지 않는다.
    if WizardIsTaskSelected('fileassoc') then
      SHChangeNotifyDirect($08000000, $0000, 0, 0);
  end;
end;
