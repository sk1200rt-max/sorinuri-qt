param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedVersion
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) {
    throw "설치 smoke test 중단: 설치 파일을 찾을 수 없습니다: $InstallerPath"
}

$root = Join-Path $env:RUNNER_TEMP 'sorinuri-installer-smoke'
$installDir = Join-Path $root 'Sorinuri'
Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $root -Force | Out-Null

try {
    Write-Host '=== 1/5 Inno Setup 무인 설치 ==='
    $install = Start-Process -FilePath $InstallerPath `
        -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', "/DIR=$installDir", '/TASKS="fileassoc"') `
        -Wait -PassThru
    if ($install.ExitCode -ne 0) {
        throw "설치 프로그램이 실패했습니다. exit code=$($install.ExitCode)"
    }

    Write-Host '=== 2/5 설치 payload와 제거 프로그램 확인 ==='
    $requiredFiles = @(
        (Join-Path $installDir 'Sorinuri.exe'),
        (Join-Path $installDir 'libmpv-2.dll'),
        (Join-Path $installDir 'unins000.exe')
    )
    foreach ($path in $requiredFiles) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "설치 payload 누락: $path"
        }
    }
    $productVersion = (Get-Item -LiteralPath (Join-Path $installDir 'Sorinuri.exe')).VersionInfo.ProductVersion
    if ([string]::IsNullOrWhiteSpace($productVersion) -or -not $productVersion.StartsWith($ExpectedVersion)) {
        throw "설치된 실행 파일 버전 불일치: expected=$ExpectedVersion, actual=$productVersion"
    }

    Write-Host '=== 3/5 선택된 파일 연결 후보 등록 확인 ==='
    $openWith = 'Registry::HKEY_LOCAL_MACHINE\Software\Classes\.mkv\OpenWithProgids'
    $command = 'Registry::HKEY_LOCAL_MACHINE\Software\Classes\Sorinuri.mkv\shell\open\command'
    if (-not (Test-Path -LiteralPath $openWith) -or -not (Test-Path -LiteralPath $command)) {
        throw '파일 연결 선택 후 Open With/ProgID 등록이 확인되지 않습니다.'
    }
    $openCommand = (Get-ItemProperty -LiteralPath $command -Name '(default)' -ErrorAction Stop).'(default)'
    if ($openCommand -notlike "*$installDir*Sorinuri.exe*") {
        throw "파일 연결 실행 경로가 설치 위치와 다릅니다: $openCommand"
    }

    Write-Host '=== 4/5 실행 파일 로더·첫 실행 확인 ==='
    # --help는 QApplication과 배포된 Qt DLL을 실제로 로드하지만 MainWindow를 열지 않아
    # 비대화형 CI runner에서도 첫 실행 의존성 실패를 안정적으로 잡는다.
    $help = Start-Process -FilePath (Join-Path $installDir 'Sorinuri.exe') -ArgumentList '--help' `
        -RedirectStandardOutput (Join-Path $root 'help.stdout.txt') `
        -RedirectStandardError (Join-Path $root 'help.stderr.txt') `
        -Wait -PassThru
    if ($help.ExitCode -ne 0) {
        $stderr = Get-Content (Join-Path $root 'help.stderr.txt') -Raw -ErrorAction SilentlyContinue
        throw "실행 파일 로더 확인 실패. exit code=$($help.ExitCode) $stderr"
    }

    # 실제 창 생성 경로도 짧게 시작한다. 정상적으로 살아 있음을 확인한 뒤 강제 종료가 아닌
    # window close 메시지를 보내며, CI 세션 제약으로 즉시 끝나면 별도 실패로 표시한다.
    $app = Start-Process -FilePath (Join-Path $installDir 'Sorinuri.exe') -PassThru
    Start-Sleep -Seconds 5
    $app.Refresh()
    if ($app.HasExited) {
        throw "첫 실행 창이 5초 안에 종료됐습니다. exit code=$($app.ExitCode)"
    }
    $closed = $false
    try {
        $closed = $app.CloseMainWindow()
    } catch {
        $closed = $false
    }
    if (-not $closed) {
        Stop-Process -Id $app.Id -Force
    }
    $app.WaitForExit(10000) | Out-Null

    Write-Host '=== 5/5 제거 프로그램 확인 ==='
    $uninstaller = Join-Path $installDir 'unins000.exe'
    $uninstall = Start-Process -FilePath $uninstaller `
        -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-') `
        -Wait -PassThru
    if ($uninstall.ExitCode -ne 0) {
        throw "제거 프로그램이 실패했습니다. exit code=$($uninstall.ExitCode)"
    }
    if (Test-Path -LiteralPath (Join-Path $installDir 'Sorinuri.exe')) {
        throw '제거 뒤 Sorinuri.exe가 남아 있습니다.'
    }

    Write-Host "INSTALLER_SMOKE_TEST_PASS version=$ExpectedVersion"
} finally {
    Get-Process -Name 'Sorinuri' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
