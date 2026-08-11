; 소리누리 플레이어 Inno Setup 설치 스크립트
; Inno Setup 6.x 이상 필요

#define MyAppName "소리누리"
#define MyAppNameEn "Sorinuri"
#define MyAppVersion "6.18.3"
#define MyAppPublisher "Gaon Communication"
#define MyAppURL "https://sorinuri.com"
#define MyAppExeName "Sorinuri.exe"
#define MyAppDescription "소리누리 - 전문 동영상/오디오 플레이어"

; Dolby Access Microsoft Store 링크
#define DolbyAccessWebURL "https://apps.microsoft.com/detail/9N0866FS04W8"

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
WizardStyle=modern
WizardResizable=no
RestartIfNeededByRun=no
; 설치 마법사 이미지 (소리누리 브랜드)
WizardImageFile=wizard-banner.bmp
WizardSmallImageFile=wizard-small.bmp
; 기존 버전 자동 제거 후 재설치 (업데이트 설치 지원)
CloseApplications=yes
CloseApplicationsFilter=*.exe
RestartApplications=no
ChangesAssociations=yes

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"


[CustomMessages]
korean.DolbyPageTitle=Dolby Atmos / 5.1 서라운드 오디오 설정
korean.DolbyPageDesc=소리누리는 Edge WebView2로 넷플릭스, 디즈니+ 등 OTT 서비스를 재생합니다. Dolby Atmos 및 5.1 서라운드 오디오를 AV 앰프나 사운드바로 전달하려면 Dolby Access가 필요합니다.
korean.DolbyTaskDesc=Dolby Access 설치 (넷플릭스 Dolby Atmos / 5.1 서라운드 지원)
korean.DolbyLinkText=>> Microsoft Store에서 Dolby Access 열기 (무료)
korean.DolbyLine1=  [O]  Dolby Access는 Microsoft Store에서 무료로 설치할 수 있습니다.
korean.DolbyLine2=  [O]  설치 후 별도 설정 없이 소리누리에서 자동으로 인식됩니다.
korean.DolbyLine3=  [O]  AV 앰프 / 사운드바 / HDMI 연결 시 Dolby Atmos 패스스루 지원.
korean.DolbyLine4=  [O]  헤드폰 사용자도 Dolby Atmos for Headphones 기능을 이용할 수 있습니다.
korean.DolbyLine5=  [!]  Dolby Access 없이도 소리누리 기본 기능은 정상 동작합니다.
korean.DolbyLine6=       OTT 멀티채널 오디오에만 필요합니다.
english.DolbyPageTitle=Dolby Atmos / 5.1 Surround Audio Setup
english.DolbyPageDesc=Sorinuri uses Edge WebView2 to play Netflix, Disney+ and other OTT services. Dolby Access is required to enable Dolby Atmos and 5.1 surround audio output.
english.DolbyTaskDesc=Install Dolby Access (Netflix Dolby Atmos / 5.1 Surround Support)
english.DolbyLinkText=>> Open Dolby Access on Microsoft Store (Free)
english.DolbyLine1=  [O]  Dolby Access is FREE on Microsoft Store.
english.DolbyLine2=  [O]  No extra configuration needed after install.
english.DolbyLine3=  [O]  Supports Dolby Atmos passthrough via HDMI / optical.
english.DolbyLine4=  [O]  Dolby Atmos for Headphones also supported.
english.DolbyLine5=  [!]  Dolby Access is OPTIONAL.
english.DolbyLine6=       Local file playback and passthrough work without it.

[Tasks]
Name: "desktopicon"; Description: "바탕화면에 아이콘 만들기(&D)"; GroupDescription: "추가 아이콘:"
Name: "quicklaunchicon"; Description: "빠른 실행에 아이콘 만들기(&Q)"; GroupDescription: "추가 아이콘:"; Flags: unchecked; OnlyBelowVersion: 6.1; Check: not IsAdminInstallMode
Name: "installdolby"; Description: "{cm:DolbyTaskDesc}"; GroupDescription: "Dolby 오디오 지원:"; Flags: unchecked
; ── 파일 형식 연결 ──────────────────────────────────────────────────────────
Name: "fileassoc"; Description: "호환 파일 형식을 소리누리로 연결(&F)"; GroupDescription: "파일 형식 연결:"; Flags: unchecked
Name: "fileassoc\video"; Description: "동영상 파일 (.mkv, .mp4, .avi, .mov, .wmv, .m2ts, .ts, .m4v, .webm, .flv, .3gp, .ogv, .rmvb, .rm)"; GroupDescription: "파일 형식 연결:"; Flags: unchecked; Check: WizardIsTaskSelected('fileassoc')
Name: "fileassoc\audio"; Description: "오디오 파일 (.flac, .mp3, .aac, .ogg, .opus, .wav, .m4a, .wma, .ape, .dsf, .dff, .mka)"; GroupDescription: "파일 형식 연결:"; Flags: unchecked; Check: WizardIsTaskSelected('fileassoc')

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
Filename: "{sys}\cmd.exe"; Parameters: "/c start """" ""{#DolbyAccessWebURL}"""; Description: "Dolby Access 설치 (Microsoft Store)"; Flags: nowait postinstall skipifsilent; Tasks: installdolby

[Registry]
; ── 동영상 형식 연결 (Tasks: fileassoc\video) ────────────────────────────────
Root: HKCU; Subkey: "Software\Classes\.mkv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mkv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mkv"; ValueType: string; ValueName: ""; ValueData: "MKV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mkv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mkv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.mp4\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mp4"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp4"; ValueType: string; ValueName: ""; ValueData: "MP4 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp4\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp4\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.avi\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.avi"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.avi"; ValueType: string; ValueName: ""; ValueData: "AVI 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.avi\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.avi\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.mov\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mov"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mov"; ValueType: string; ValueName: ""; ValueData: "MOV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mov\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mov\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.wmv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.wmv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wmv"; ValueType: string; ValueName: ""; ValueData: "WMV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wmv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wmv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.m2ts\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.m2ts"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m2ts"; ValueType: string; ValueName: ""; ValueData: "M2TS 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m2ts\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m2ts\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.ts\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ts"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ts"; ValueType: string; ValueName: ""; ValueData: "TS 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ts\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ts\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.m4v\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.m4v"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m4v"; ValueType: string; ValueName: ""; ValueData: "M4V 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m4v\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m4v\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.webm\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.webm"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.webm"; ValueType: string; ValueName: ""; ValueData: "WebM 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.webm\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.webm\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.flv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.flv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flv"; ValueType: string; ValueName: ""; ValueData: "FLV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.3gp\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.3gp"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.3gp"; ValueType: string; ValueName: ""; ValueData: "3GP 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.3gp\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.3gp\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.ogv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ogv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ogv"; ValueType: string; ValueName: ""; ValueData: "OGV 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ogv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ogv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.rmvb\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.rmvb"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.rmvb"; ValueType: string; ValueName: ""; ValueData: "RMVB 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.rmvb\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.rmvb\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

Root: HKCU; Subkey: "Software\Classes\.rm\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.rm"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.rm"; ValueType: string; ValueName: ""; ValueData: "RM 비디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.rm\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\video
Root: HKCU; Subkey: "Software\Classes\Sorinuri.rm\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\video

; ── 오디오 형식 연결 (Tasks: fileassoc\audio) ────────────────────────────────
Root: HKCU; Subkey: "Software\Classes\.flac\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.flac"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flac"; ValueType: string; ValueName: ""; ValueData: "FLAC 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flac\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flac\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\audio

Root: HKCU; Subkey: "Software\Classes\.mp3\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mp3"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp3"; ValueType: string; ValueName: ""; ValueData: "MP3 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp3\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp3\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\audio

Root: HKCU; Subkey: "Software\Classes\.aac\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.aac"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.aac"; ValueType: string; ValueName: ""; ValueData: "AAC 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.aac\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.aac\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\audio

Root: HKCU; Subkey: "Software\Classes\.ogg\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ogg"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ogg"; ValueType: string; ValueName: ""; ValueData: "OGG 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ogg\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ogg\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\audio

Root: HKCU; Subkey: "Software\Classes\.opus\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.opus"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.opus"; ValueType: string; ValueName: ""; ValueData: "Opus 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.opus\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.opus\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\audio

Root: HKCU; Subkey: "Software\Classes\.wav\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.wav"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wav"; ValueType: string; ValueName: ""; ValueData: "WAV 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wav\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wav\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\audio

Root: HKCU; Subkey: "Software\Classes\.m4a\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.m4a"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m4a"; ValueType: string; ValueName: ""; ValueData: "M4A 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m4a\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m4a\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\audio

Root: HKCU; Subkey: "Software\Classes\.wma\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.wma"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wma"; ValueType: string; ValueName: ""; ValueData: "WMA 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wma\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.wma\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\audio

Root: HKCU; Subkey: "Software\Classes\.ape\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.ape"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ape"; ValueType: string; ValueName: ""; ValueData: "APE 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ape\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.ape\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\audio

Root: HKCU; Subkey: "Software\Classes\.dsf\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.dsf"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.dsf"; ValueType: string; ValueName: ""; ValueData: "DSF 오디오 파일 (DSD)"; Flags: uninsdeletekey; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.dsf\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.dsf\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\audio

Root: HKCU; Subkey: "Software\Classes\.dff\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.dff"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.dff"; ValueType: string; ValueName: ""; ValueData: "DFF 오디오 파일 (DSD)"; Flags: uninsdeletekey; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.dff\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.dff\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\audio

Root: HKCU; Subkey: "Software\Classes\.mka\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mka"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mka"; ValueType: string; ValueName: ""; ValueData: "MKA 오디오 파일"; Flags: uninsdeletekey; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mka\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc\audio
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mka\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc\audio


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

// ============================================================================
// Dolby Access Custom Page
// ============================================================================
var
  DolbyPage: TWizardPage;
  DolbyInfoMemo: TNewMemo;
  DolbyLinkLabel: TNewStaticText;

function GetDolbyCaption(): String;
begin
  // SubCaption을 빈 문자열로 변경했으므로
  // DolbyPageDesc를 메모 첫 줄에 포함하여 전체 내용을 메모 안에 표시
  Result := CustomMessage('DolbyPageDesc') + #13#10 + #13#10 +
    '------------------------------------------------------------' + #13#10 + #13#10 +
    CustomMessage('DolbyLine1') + #13#10 +
    CustomMessage('DolbyLine2') + #13#10 +
    CustomMessage('DolbyLine3') + #13#10 +
    CustomMessage('DolbyLine4') + #13#10 + #13#10 +
    '------------------------------------------------------------' + #13#10 + #13#10 +
    CustomMessage('DolbyLine5') + #13#10 +
    CustomMessage('DolbyLine6');
end;

procedure DolbyLinkClick(Sender: TObject);
var
  ResultCode: Integer;
begin
  ShellExec('open', '{#DolbyAccessWebURL}', '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
end;

procedure InitializeWizard;
var
  ScaleFactor: Integer;
  BaseFontSize: Integer;
  LinkH: Integer;
begin
  // HiDPI 스케일 팩터 적용 (96 DPI = 100%, 192 DPI = 200% 등)
  ScaleFactor := WizardForm.Font.PixelsPerInch;
  // 기본 폰트 크기: 96 DPI에서 9pt, HiDPI에서 비례 확대
  // 250% 배율(240 DPI)에서 글씨 잘림 방지: 최대 11pt로 제한
  BaseFontSize := MulDiv(9, ScaleFactor, 96);
  if BaseFontSize > 10 then BaseFontSize := 10;  // 250% 배율에서 잘림 방지

  // SubCaption을 빈 문자열로 설정
  // 이유: SubCaption이 있으면 Inno Setup이 페이지 상단에 별도 헤더 영역을 만들어
  // SurfaceHeight가 줄어들고 메모 내용이 잘림.
  // DolbyPageDesc는 GetDolbyCaption()에서 메모 첫 줄로 표시함.
  DolbyPage := CreateCustomPage(
    wpSelectTasks,
    CustomMessage('DolbyPageTitle'),
    '');

  // 링크 레이블 높이를 DPI에 맞게 계산 (100% = 22px, 250% = 약 28px)
  LinkH := MulDiv(22, ScaleFactor, 96);

  // TNewMemo: 세로 스크롤바 자동 표시 → HiDPI에서도 내용 잘림 없음
  // SurfaceHeight에서 링크 레이블 + 여백(8px)을 뺀 나머지 전체 사용
  DolbyInfoMemo := TNewMemo.Create(DolbyPage);
  DolbyInfoMemo.Parent := DolbyPage.Surface;
  DolbyInfoMemo.Left := 0;
  DolbyInfoMemo.Top := 0;
  DolbyInfoMemo.Width := DolbyPage.SurfaceWidth;
  DolbyInfoMemo.Height := DolbyPage.SurfaceHeight - LinkH - 8;
  DolbyInfoMemo.ScrollBars := ssVertical;  // 세로 스크롤바 (ssAutoBoth는 Inno Setup 미지원)
  DolbyInfoMemo.ReadOnly := True;
  DolbyInfoMemo.WantReturns := False;
  DolbyInfoMemo.Font.Size := BaseFontSize;
  DolbyInfoMemo.Lines.Text := GetDolbyCaption();

  // 링크 레이블: 메모 바로 아래 고정 배치
  // Top을 SurfaceHeight - LinkH로 설정하여 항상 하단에 표시
  DolbyLinkLabel := TNewStaticText.Create(DolbyPage);
  DolbyLinkLabel.Parent := DolbyPage.Surface;
  DolbyLinkLabel.Left := 0;
  DolbyLinkLabel.Top := DolbyPage.SurfaceHeight - LinkH;
  DolbyLinkLabel.Width := DolbyPage.SurfaceWidth;
  DolbyLinkLabel.AutoSize := False;
  DolbyLinkLabel.Height := LinkH;
  DolbyLinkLabel.Caption := CustomMessage('DolbyLinkText');
  DolbyLinkLabel.Font.Size := BaseFontSize;
  DolbyLinkLabel.Font.Color := $00CC6600;
  DolbyLinkLabel.Font.Style := [fsUnderline];
  DolbyLinkLabel.Cursor := crHand;
  DolbyLinkLabel.OnClick := @DolbyLinkClick;
end;

// 기존 버전 자동 제거 함수
function InitializeSetup(): Boolean;
var
  UninstallString: String;
  ResultCode: Integer;
  Found: Boolean;
begin
  Result := True;

  // 1단계: 실행 중인 소리누리 프로세스 강제 종료
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM Sorinuri.exe /T', '',
       SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Sleep(2000);  // 프로세스 완전 종료 대기

  // 2단계: HKCU 또는 HKLM에서 기존 버전 제거 문자열 확인
  Found := RegQueryStringValue(HKCU,
    'Software\Microsoft\Windows\CurrentVersion\Uninstall\{8A3F2C1D-9E4B-4F7A-B2D6-1C5E8F3A9B2E}_is1',
    'UninstallString', UninstallString);
  if not Found then
    Found := RegQueryStringValue(HKLM,
      'Software\Microsoft\Windows\CurrentVersion\Uninstall\{8A3F2C1D-9E4B-4F7A-B2D6-1C5E8F3A9B2E}_is1',
      'UninstallString', UninstallString);
  if not Found then
    Found := RegQueryStringValue(HKLM,
      'Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\{8A3F2C1D-9E4B-4F7A-B2D6-1C5E8F3A9B2E}_is1',
      'UninstallString', UninstallString);

  if Found then begin
    Exec(RemoveQuotes(UninstallString), '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '',
         SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(2000);  // 제거 완료 대기
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssInstall then begin
    Exec('taskkill.exe', '/F /IM Sorinuri.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
  // 설치 완료 후 아이콘 캐시 강제 삭제 + 갱신
  // Windows 10/11에서 ie4uinit만으로는 부족 → IconCache.db 직접 삭제 필요
  if CurStep = ssPostInstall then begin
    // 1단계: Explorer 종료 (아이콘 캐시 파일 잠금 해제)
    Exec('taskkill.exe', '/F /IM explorer.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);

    // 2단계: IconCache.db 직접 삭제 (Windows 10/11 아이콘 캐시 DB)
    Exec(ExpandConstant('{sys}\cmd.exe'),
         '/C del /F /Q "%LocalAppData%\IconCache.db" 2>nul',
         '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    // Windows 10/11: Explorer 아이콘 캐시 DB (모든 크기)
    Exec(ExpandConstant('{sys}\cmd.exe'),
         '/C del /F /Q "%LocalAppData%\Microsoft\Windows\Explorer\iconcache*.db" 2>nul',
         '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    // 바탕화면 썸네일 캐시도 함께 삭제
    Exec(ExpandConstant('{sys}\cmd.exe'),
         '/C del /F /Q "%LocalAppData%\Microsoft\Windows\Explorer\thumbcache*.db" 2>nul',
         '', SW_HIDE, ewWaitUntilTerminated, ResultCode);

    // 3단계: ie4uinit으로 쉘 아이콘 캐시 재초기화
    Exec(ExpandConstant('{sys}\ie4uinit.exe'), '-show', '',
         SW_HIDE, ewWaitUntilTerminated, ResultCode);

    // 4단계: Explorer 재시작 (탐색기 + 바탕화면 새로고침)
    Exec(ExpandConstant('{sys}\cmd.exe'),
         '/C start explorer.exe', '',
         SW_HIDE, ewNoWait, ResultCode);

    // 5단계: SHChangeNotify 직접 호출 (파일 연결 즉시 적용)
    // SHCNE_ASSOCCHANGED = 0x08000000, SHCNF_IDLIST = 0x0000
    SHChangeNotifyDirect($08000000, $0000, 0, 0);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  AppDir: String;
  ResultCode: Integer;
begin
  if CurUninstallStep = usPostUninstall then begin
    AppDir := ExpandConstant('{app}');
    // Force delete remaining files and folder
    if DirExists(AppDir) then begin
      Exec('cmd.exe', '/C rmdir /S /Q "' + AppDir + '"',
           '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    end;
  end;
end;
