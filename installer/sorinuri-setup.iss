; 소리누리 플레이어 Inno Setup 설치 스크립트
; Inno Setup 6.x 이상 필요

#define MyAppName "소리누리"
#define MyAppNameEn "Sorinuri"
#define MyAppVersion "6.20.4"
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
; 설치 실행 파일 자체의 크기를 줄여 Windows Defender/SmartScreen의 초기 파일 검사와
; 디스크 I/O 부담을 낮춘다. lzma2/normal은 해제 시 약 2MB만 필요해 저사양 환경에서도
; 안정적이며, non-solid 구성은 마법사 표시 뒤 필요한 파일을 바로 추출할 수 있다.
Compression=lzma2/normal
SolidCompression=no
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
MinVersion=10.0.17763
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
; modern dynamic은 모든 체크박스·버튼·라디오 컨트롤을 같은 DPI 스케일 규칙으로 렌더링한다.
; excludelightcontrols는 고배율에서 기본 체크박스만 작게 남아 잘리는 원인이므로 사용하지 않는다.
WizardStyle=modern dynamic
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
; Visual C++ 런타임 자동 설치 (이미 설치되어 있으면 건너뜀)
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Visual C++ 런타임 설치 중..."; Flags: skipifdoesntexist waituntilterminated
; Windows 10/11의 UserChoice 기본값은 설치 프로그램이 강제할 수 없다.
; 다만 사용자가 '호환 파일 형식을 소리누리로 연결'을 선택한 경우에는 레지스트리
; 등록·Explorer 갱신이 끝난 뒤 해당 기본 앱 화면을 열어 한 번의 '기본값으로 설정'으로
; 전체 지원 형식을 소리누리로 확정할 수 있게 한다. 원래 사용자 컨텍스트로 실행해
; 관리자 권한 Settings 프로세스와 ShellExecute 오류를 피한다.
Filename: "{app}\{#MyAppExeName}"; Parameters: "--register-file-associations"; Flags: nowait skipifsilent runasoriginaluser; Tasks: fileassoc
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Registry]
; ── 동영상 형식 연결 (Tasks: fileassoc) ────────────────────────────────
Root: HKLM64; Subkey: "Software\Classes\.mkv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mkv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mkv"; ValueType: string; ValueName: ""; ValueData: "MKV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mkv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mkv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mp4\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mp4"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mp4"; ValueType: string; ValueName: ""; ValueData: "MP4 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mp4\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mp4\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.avi\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.avi"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.avi"; ValueType: string; ValueName: ""; ValueData: "AVI 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.avi\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.avi\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mov\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mov"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mov"; ValueType: string; ValueName: ""; ValueData: "MOV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mov\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mov\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.wmv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.wmv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wmv"; ValueType: string; ValueName: ""; ValueData: "WMV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wmv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wmv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.m2ts\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.m2ts"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.m2ts"; ValueType: string; ValueName: ""; ValueData: "M2TS 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.m2ts\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.m2ts\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.ts\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ts"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ts"; ValueType: string; ValueName: ""; ValueData: "TS 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ts\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ts\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.m4v\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.m4v"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.m4v"; ValueType: string; ValueName: ""; ValueData: "M4V 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.m4v\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.m4v\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.webm\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.webm"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.webm"; ValueType: string; ValueName: ""; ValueData: "WebM 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.webm\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.webm\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.flv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.flv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.flv"; ValueType: string; ValueName: ""; ValueData: "FLV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.flv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.flv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.3gp\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.3gp"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.3gp"; ValueType: string; ValueName: ""; ValueData: "3GP 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.3gp\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.3gp\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.ogv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ogv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ogv"; ValueType: string; ValueName: ""; ValueData: "OGV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ogv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ogv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.rmvb\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.rmvb"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.rmvb"; ValueType: string; ValueName: ""; ValueData: "RMVB 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.rmvb\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.rmvb\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.rm\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.rm"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.rm"; ValueType: string; ValueName: ""; ValueData: "RM 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.rm\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.rm\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

; ── 오디오 형식 연결 (Tasks: fileassoc) ────────────────────────────────
Root: HKLM64; Subkey: "Software\Classes\.flac\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.flac"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.flac"; ValueType: string; ValueName: ""; ValueData: "FLAC 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.flac\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.flac\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mp3\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mp3"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mp3"; ValueType: string; ValueName: ""; ValueData: "MP3 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mp3\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mp3\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.aac\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.aac"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.aac"; ValueType: string; ValueName: ""; ValueData: "AAC 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.aac\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.aac\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.ogg\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ogg"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ogg"; ValueType: string; ValueName: ""; ValueData: "OGG 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ogg\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ogg\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.opus\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.opus"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.opus"; ValueType: string; ValueName: ""; ValueData: "Opus 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.opus\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.opus\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.wav\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.wav"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wav"; ValueType: string; ValueName: ""; ValueData: "WAV 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wav\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wav\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.m4a\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.m4a"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.m4a"; ValueType: string; ValueName: ""; ValueData: "M4A 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.m4a\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.m4a\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.wma\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.wma"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wma"; ValueType: string; ValueName: ""; ValueData: "WMA 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wma\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wma\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.ape\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ape"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ape"; ValueType: string; ValueName: ""; ValueData: "APE 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ape\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ape\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.dsf\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.dsf"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dsf"; ValueType: string; ValueName: ""; ValueData: "DSF 오디오 파일 (DSD)"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dsf\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dsf\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.dff\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.dff"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dff"; ValueType: string; ValueName: ""; ValueData: "DFF 오디오 파일 (DSD)"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dff\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dff\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mka\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mka"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mka"; ValueType: string; ValueName: ""; ValueData: "MKA 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mka\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mka\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc


; ── v6.20.1: 공개 지원 형식 전체 등록 ───────────────────────────────
Root: HKLM64; Subkey: "Software\Classes\.asf\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.asf"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.asf"; ValueType: string; ValueName: ""; ValueData: "ASF 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.asf\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.asf\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.f4v\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.f4v"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.f4v"; ValueType: string; ValueName: ""; ValueData: "F4V 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.f4v\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.f4v\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mts\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mts"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mts"; ValueType: string; ValueName: ""; ValueData: "MTS 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mts\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mts\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.m2t\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.m2t"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.m2t"; ValueType: string; ValueName: ""; ValueData: "M2T 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.m2t\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.m2t\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.ogm\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ogm"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ogm"; ValueType: string; ValueName: ""; ValueData: "OGM 미디어 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ogm\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ogm\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.3g2\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.3g2"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.3g2"; ValueType: string; ValueName: ""; ValueData: "3GPP2 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.3g2\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.3g2\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mpg\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mpg"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpg"; ValueType: string; ValueName: ""; ValueData: "MPEG 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpg\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpg\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mpeg\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mpeg"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpeg"; ValueType: string; ValueName: ""; ValueData: "MPEG 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpeg\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpeg\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mpe\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mpe"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpe"; ValueType: string; ValueName: ""; ValueData: "MPEG 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpe\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.vob\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.vob"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.vob"; ValueType: string; ValueName: ""; ValueData: "DVD 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.vob\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.vob\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.divx\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.divx"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.divx"; ValueType: string; ValueName: ""; ValueData: "DivX 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.divx\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.divx\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.xvid\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.xvid"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.xvid"; ValueType: string; ValueName: ""; ValueData: "Xvid 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.xvid\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.xvid\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mxf\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mxf"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mxf"; ValueType: string; ValueName: ""; ValueData: "MXF 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mxf\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mxf\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.dvr-ms\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.dvr-ms"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dvr-ms"; ValueType: string; ValueName: ""; ValueData: "DVR-MS 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dvr-ms\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dvr-ms\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.tp\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.tp"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.tp"; ValueType: string; ValueName: ""; ValueData: "MPEG 전송 스트림"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.tp\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.tp\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.trp\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.trp"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.trp"; ValueType: string; ValueName: ""; ValueData: "MPEG 전송 스트림"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.trp\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.trp\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.tod\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.tod"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.tod"; ValueType: string; ValueName: ""; ValueData: "TOD 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.tod\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.tod\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mod\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mod"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mod"; ValueType: string; ValueName: ""; ValueData: "MOD 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mod\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mod\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mp2\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mp2"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mp2"; ValueType: string; ValueName: ""; ValueData: "MPEG 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mp2\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mp2\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mpa\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mpa"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpa"; ValueType: string; ValueName: ""; ValueData: "MPEG 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpa\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpa\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.alac\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.alac"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.alac"; ValueType: string; ValueName: ""; ValueData: "ALAC 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.alac\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.alac\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.wave\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.wave"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wave"; ValueType: string; ValueName: ""; ValueData: "WAVE 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wave\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wave\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.oga\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.oga"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.oga"; ValueType: string; ValueName: ""; ValueData: "Ogg 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.oga\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.oga\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.wv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.wv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wv"; ValueType: string; ValueName: ""; ValueData: "WavPack 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.wv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.dsd\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.dsd"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dsd"; ValueType: string; ValueName: ""; ValueData: "DSD 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dsd\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dsd\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.dts\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.dts"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dts"; ValueType: string; ValueName: ""; ValueData: "DTS 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dts\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.dts\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.ac3\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ac3"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ac3"; ValueType: string; ValueName: ""; ValueData: "Dolby Digital 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ac3\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.ac3\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.eac3\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.eac3"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.eac3"; ValueType: string; ValueName: ""; ValueData: "Dolby Digital Plus 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.eac3\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.eac3\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.truehd\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.truehd"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.truehd"; ValueType: string; ValueName: ""; ValueData: "Dolby TrueHD 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.truehd\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.truehd\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.thd\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.thd"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.thd"; ValueType: string; ValueName: ""; ValueData: "Dolby TrueHD 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.thd\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.thd\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.aiff\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.aiff"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.aiff"; ValueType: string; ValueName: ""; ValueData: "AIFF 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.aiff\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.aiff\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.aif\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.aif"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.aif"; ValueType: string; ValueName: ""; ValueData: "AIFF 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.aif\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.aif\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.au\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.au"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.au"; ValueType: string; ValueName: ""; ValueData: "AU 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.au\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.au\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.amr\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.amr"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.amr"; ValueType: string; ValueName: ""; ValueData: "AMR 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.amr\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.amr\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.tak\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.tak"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.tak"; ValueType: string; ValueName: ""; ValueData: "TAK 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.tak\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.tak\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.tta\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.tta"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.tta"; ValueType: string; ValueName: ""; ValueData: "TTA 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.tta\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.tta\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.mpc\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mpc"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpc"; ValueType: string; ValueName: ""; ValueData: "Musepack 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpc\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.mpc\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKLM64; Subkey: "Software\Classes\.spx\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.spx"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.spx"; ValueType: string; ValueName: ""; ValueData: "Speex 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.spx\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Sorinuri.spx\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

; ── Windows Capabilities 등록 (탐색기 우클릭 "연결 프로그램" 목록 표시 필수) ──────────
; RegisteredApplications 없이는 Windows가 소리누리를 연결 프로그램 목록에 표시하지 않음
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "소리누리"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "소리누리 - Windows 하이엔드 미디어 플레이어"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mkv"; ValueData: "Sorinuri.mkv"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp4"; ValueData: "Sorinuri.mp4"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".avi"; ValueData: "Sorinuri.avi"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mov"; ValueData: "Sorinuri.mov"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wmv"; ValueData: "Sorinuri.wmv"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m2ts"; ValueData: "Sorinuri.m2ts"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ts"; ValueData: "Sorinuri.ts"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4v"; ValueData: "Sorinuri.m4v"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".webm"; ValueData: "Sorinuri.webm"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flv"; ValueData: "Sorinuri.flv"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".3gp"; ValueData: "Sorinuri.3gp"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogv"; ValueData: "Sorinuri.ogv"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".rmvb"; ValueData: "Sorinuri.rmvb"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".rm"; ValueData: "Sorinuri.rm"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flac"; ValueData: "Sorinuri.flac"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp3"; ValueData: "Sorinuri.mp3"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".aac"; ValueData: "Sorinuri.aac"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogg"; ValueData: "Sorinuri.ogg"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".opus"; ValueData: "Sorinuri.opus"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wav"; ValueData: "Sorinuri.wav"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4a"; ValueData: "Sorinuri.m4a"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wma"; ValueData: "Sorinuri.wma"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ape"; ValueData: "Sorinuri.ape"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dsf"; ValueData: "Sorinuri.dsf"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dff"; ValueData: "Sorinuri.dff"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mka"; ValueData: "Sorinuri.mka"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".asf"; ValueData: "Sorinuri.asf"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".f4v"; ValueData: "Sorinuri.f4v"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mts"; ValueData: "Sorinuri.mts"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m2t"; ValueData: "Sorinuri.m2t"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogm"; ValueData: "Sorinuri.ogm"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".3g2"; ValueData: "Sorinuri.3g2"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpg"; ValueData: "Sorinuri.mpg"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpeg"; ValueData: "Sorinuri.mpeg"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpe"; ValueData: "Sorinuri.mpe"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".vob"; ValueData: "Sorinuri.vob"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".divx"; ValueData: "Sorinuri.divx"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".xvid"; ValueData: "Sorinuri.xvid"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mxf"; ValueData: "Sorinuri.mxf"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dvr-ms"; ValueData: "Sorinuri.dvr-ms"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tp"; ValueData: "Sorinuri.tp"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".trp"; ValueData: "Sorinuri.trp"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tod"; ValueData: "Sorinuri.tod"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mod"; ValueData: "Sorinuri.mod"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp2"; ValueData: "Sorinuri.mp2"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpa"; ValueData: "Sorinuri.mpa"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".alac"; ValueData: "Sorinuri.alac"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wave"; ValueData: "Sorinuri.wave"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".oga"; ValueData: "Sorinuri.oga"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wv"; ValueData: "Sorinuri.wv"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dsd"; ValueData: "Sorinuri.dsd"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dts"; ValueData: "Sorinuri.dts"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ac3"; ValueData: "Sorinuri.ac3"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".eac3"; ValueData: "Sorinuri.eac3"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".truehd"; ValueData: "Sorinuri.truehd"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".thd"; ValueData: "Sorinuri.thd"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".aiff"; ValueData: "Sorinuri.aiff"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".aif"; ValueData: "Sorinuri.aif"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".au"; ValueData: "Sorinuri.au"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".amr"; ValueData: "Sorinuri.amr"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tak"; ValueData: "Sorinuri.tak"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tta"; ValueData: "Sorinuri.tta"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpc"; ValueData: "Sorinuri.mpc"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Sorinuri\Capabilities\FileAssociations"; ValueType: string; ValueName: ".spx"; ValueData: "Sorinuri.spx"; Tasks: fileassoc
; RegisteredApplications: Windows에 소리누리를 공식 등록 (연결 프로그램 목록 표시 핵심 키)
Root: HKLM64; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "소리누리"; ValueData: "Software\Sorinuri\Capabilities"; Flags: uninsdeletevalue; Tasks: fileassoc

; Windows ‘연결 프로그램’과 기본 앱 검색에 사용하는 실행 파일 등록.
; Capabilities만 등록하면 일부 Windows 11 빌드에서 앱 이름이 목록에 늦게 나타날 수 있어,
; Applications\Sorinuri.exe와 SupportedTypes도 같은 범위로 명시한다.
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "소리누리"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mkv"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mp4"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".avi"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mov"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".wmv"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".m2ts"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".ts"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".m4v"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".webm"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".flv"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".3gp"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".ogv"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".rmvb"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".rm"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".flac"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mp3"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".aac"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".ogg"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".opus"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".wav"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".m4a"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".wma"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".ape"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".dsf"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".dff"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mka"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".asf"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".f4v"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mts"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".m2t"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".ogm"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".3g2"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mpg"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mpeg"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mpe"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".vob"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".divx"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".xvid"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mxf"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".dvr-ms"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".tp"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".trp"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".tod"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mod"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mp2"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mpa"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".alac"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".wave"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".oga"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".wv"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".dsd"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".dts"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".ac3"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".eac3"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".truehd"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".thd"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".aiff"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".aif"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".au"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".amr"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".tak"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".tta"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".mpc"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Classes\Applications\Sorinuri.exe\SupportedTypes"; ValueType: string; ValueName: ".spx"; ValueData: ""; Tasks: fileassoc
Root: HKLM64; Subkey: "Software\Microsoft\Windows\CurrentVersion\App Paths\Sorinuri.exe"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletekey; Tasks: fileassoc

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
      // SHCNF_DWORD($0003) | SHCNF_FLUSH($1000): Settings/Explorer가
      // Capabilities와 RegisteredApplications를 즉시 다시 읽도록 보장한다.
      SHChangeNotifyDirect($08000000, $1003, 0, 0);
  end;
end;
