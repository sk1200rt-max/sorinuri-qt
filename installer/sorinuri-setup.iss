; 소리누리 플레이어 Inno Setup 설치 스크립트
; Inno Setup 6.x 이상 필요

#define MyAppName "소리누리"
#define MyAppNameEn "Sorinuri"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Sorinuri"
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
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
MinVersion=10.0.17763
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
WizardStyle=modern
WizardResizable=no
RestartIfNeededByRun=no

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "바탕화면에 아이콘 만들기(&D)"; GroupDescription: "추가 아이콘:"; Flags: unchecked
Name: "quicklaunchicon"; Description: "빠른 실행에 아이콘 만들기(&Q)"; GroupDescription: "추가 아이콘:"; Flags: unchecked; OnlyBelowVersion: 6.1; Check: not IsAdminInstallMode
Name: "installdolby"; Description: "Dolby Access 설치 (넷플릭스 Dolby Atmos / 5.1 서라운드 지원)"; GroupDescription: "Dolby 오디오 지원:"; Flags: unchecked

[Files]
Source: "..\dist\Sorinuri-Portable\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\Sorinuri-Portable\libmpv-2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\Sorinuri-Portable\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.zip,*.bat"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "{#MyAppDescription}"
Name: "{group}\{#MyAppName} 제거"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "{#MyAppDescription}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
Filename: "{sys}\cmd.exe"; Parameters: "/c start """" ""{#DolbyAccessWebURL}"""; Description: "Dolby Access 설치 (Microsoft Store)"; Flags: nowait postinstall skipifsilent; Tasks: installdolby

[Registry]
Root: HKCU; Subkey: "Software\Classes\.mkv\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mkv"; ValueData: ""; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mkv"; ValueType: string; ValueName: ""; ValueData: "MKV 비디오 파일"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mkv\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mkv\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

Root: HKCU; Subkey: "Software\Classes\.mp4\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.mp4"; ValueData: ""; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp4"; ValueType: string; ValueName: ""; ValueData: "MP4 비디오 파일"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp4\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"
Root: HKCU; Subkey: "Software\Classes\Sorinuri.mp4\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

Root: HKCU; Subkey: "Software\Classes\.m2ts\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.m2ts"; ValueData: ""; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m2ts"; ValueType: string; ValueName: ""; ValueData: "M2TS 비디오 파일"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m2ts\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"
Root: HKCU; Subkey: "Software\Classes\Sorinuri.m2ts\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

Root: HKCU; Subkey: "Software\Classes\.flac\OpenWithProgids"; ValueType: string; ValueName: "Sorinuri.flac"; ValueData: ""; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flac"; ValueType: string; ValueName: ""; ValueData: "FLAC 오디오 파일"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flac\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"
Root: HKCU; Subkey: "Software\Classes\Sorinuri.flac\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
// ─────────────────────────────────────────────────────────────────────────────
// Dolby Access 안내 커스텀 페이지
// ─────────────────────────────────────────────────────────────────────────────
var
  DolbyPage: TWizardPage;
  DolbyInfoLabel: TLabel;
  DolbyLinkLabel: TNewStaticText;

procedure DolbyLinkClick(Sender: TObject);
var
  ResultCode: Integer;
begin
  ShellExec('open', '{#DolbyAccessWebURL}', '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
end;

procedure InitializeWizard;
begin
  DolbyPage := CreateCustomPage(
    wpSelectTasks,
    'Dolby Atmos / 5.1 서라운드 오디오 설정',
    '넷플릭스, 디즈니+ 등 OTT 서비스에서 Dolby 고음질 오디오를 즐기려면 Dolby Access 앱이 필요합니다.');

  DolbyInfoLabel := TLabel.Create(DolbyPage);
  DolbyInfoLabel.Parent := DolbyPage.Surface;
  DolbyInfoLabel.Left := 0;
  DolbyInfoLabel.Top := 0;
  DolbyInfoLabel.Width := DolbyPage.SurfaceWidth;
  DolbyInfoLabel.AutoSize := False;
  DolbyInfoLabel.Height := 260;
  DolbyInfoLabel.WordWrap := True;
  DolbyInfoLabel.Caption :=
    '소리누리는 Edge WebView2를 통해 넷플릭스, 디즈니+ 등 OTT 서비스를 내장 브라우저로 ' +
    '재생할 수 있습니다. 이때 Dolby Atmos 및 5.1 채널 서라운드 오디오를 AV 앰프나 사운드바로 ' +
    '전달하려면 Windows에 Dolby Access 앱이 설치되어 있어야 합니다.' + #13#10 + #13#10 +
    '------------------------------------------------------------' + #13#10 +
    #13#10 +
    '  [O]  Dolby Access는 Microsoft Store에서 무료로 설치할 수 있습니다.' + #13#10 +
    '  [O]  설치 후 별도 설정 없이 소리누리에서 자동으로 인식됩니다.' + #13#10 +
    '  [O]  AV 앰프 / 사운드바 / HDMI 연결 시 Dolby Atmos 패스스루 지원.' + #13#10 +
    '  [O]  헤드폰 사용자도 Dolby Atmos for Headphones 기능을 이용할 수 있습니다.' + #13#10 +
    #13#10 +
    '------------------------------------------------------------' + #13#10 +
    #13#10 +
    '  [!]  Dolby Access 없이도 소리누리 기본 기능(로컬 파일 재생, 패스스루)은' + #13#10 +
    '       정상 동작합니다. OTT 멀티채널 오디오에만 필요합니다.' + #13#10 +
    #13#10 +
    '다음 페이지에서 설치 완료 후 Dolby Access를 자동으로 열지 선택할 수 있습니다.';

  DolbyLinkLabel := TNewStaticText.Create(DolbyPage);
  DolbyLinkLabel.Parent := DolbyPage.Surface;
  DolbyLinkLabel.Left := 0;
  DolbyLinkLabel.Top := DolbyInfoLabel.Top + DolbyInfoLabel.Height + 8;
  DolbyLinkLabel.Width := DolbyPage.SurfaceWidth;
  DolbyLinkLabel.AutoSize := True;
  DolbyLinkLabel.Caption := '>> 지금 바로 Microsoft Store에서 Dolby Access 페이지 열기';
  DolbyLinkLabel.Font.Color := $00CC6600;
  DolbyLinkLabel.Font.Style := [fsUnderline];
  DolbyLinkLabel.Cursor := crHand;
  DolbyLinkLabel.OnClick := @DolbyLinkClick;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssInstall then begin
    Exec('taskkill.exe', '/F /IM Sorinuri.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;
