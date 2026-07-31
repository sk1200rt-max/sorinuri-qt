; 소리누리 플레이어 Inno Setup 설치 스크립트
; Inno Setup 6.x 이상 필요

#define MyAppName "소리누리"
#define MyAppNameEn "Sorinuri"
#define MyAppVersion "4.3.7"
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
Name: "desktopicon"; Description: "바탕화면에 아이콘 만들기(&D)"; GroupDescription: "추가 아이콘:"; Flags: unchecked
Name: "quicklaunchicon"; Description: "빠른 실행에 아이콘 만들기(&Q)"; GroupDescription: "추가 아이콘:"; Flags: unchecked; OnlyBelowVersion: 6.1; Check: not IsAdminInstallMode
Name: "installdolby"; Description: "{cm:DolbyTaskDesc}"; GroupDescription: "Dolby 오디오 지원:"; Flags: unchecked

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
// ============================================================================
// Dolby Access Custom Page
// ============================================================================
var
  DolbyPage: TWizardPage;
  DolbyInfoLabel: TLabel;
  DolbyLinkLabel: TNewStaticText;

function GetDolbyCaption(): String;
begin
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
begin
  DolbyPage := CreateCustomPage(
    wpSelectTasks,
    CustomMessage('DolbyPageTitle'),
    CustomMessage('DolbyPageDesc'));

  DolbyInfoLabel := TLabel.Create(DolbyPage);
  DolbyInfoLabel.Parent := DolbyPage.Surface;
  DolbyInfoLabel.Left := 0;
  DolbyInfoLabel.Top := 0;
  DolbyInfoLabel.Width := DolbyPage.SurfaceWidth;
  DolbyInfoLabel.AutoSize := False;
  DolbyInfoLabel.Height := 260;
  DolbyInfoLabel.WordWrap := True;
  DolbyInfoLabel.Caption := GetDolbyCaption();

  DolbyLinkLabel := TNewStaticText.Create(DolbyPage);
  DolbyLinkLabel.Parent := DolbyPage.Surface;
  DolbyLinkLabel.Left := 0;
  DolbyLinkLabel.Top := DolbyInfoLabel.Top + DolbyInfoLabel.Height + 8;
  DolbyLinkLabel.Width := DolbyPage.SurfaceWidth;
  DolbyLinkLabel.AutoSize := True;
  DolbyLinkLabel.Caption := CustomMessage('DolbyLinkText');
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
