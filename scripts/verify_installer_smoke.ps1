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
    # 실행 파일 resource의 ProductVersion 문자열은 빌드 도구 조합에 따라 비어 있을 수 있다.
    # 설치 프로그램이 실제로 Windows에 등록한 DisplayVersion을 기준으로 패키지 버전을 판정한다.
    $uninstallRoot = 'Registry::HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Uninstall'
    $uninstallRecord = Get-ChildItem -LiteralPath $uninstallRoot | ForEach-Object {
        Get-ItemProperty -LiteralPath $_.PSPath
    } | Where-Object {
        $_.DisplayName -eq '소리누리' -and $_.DisplayVersion -like "$ExpectedVersion*"
    } | Select-Object -First 1
    if ($null -eq $uninstallRecord) {
        throw "설치 버전 등록을 확인할 수 없습니다. expected=$ExpectedVersion"
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

    # GitHub hosted runner는 비대화형 Session 0이어서 Qt/OpenGL 앱의 실제 창 생성은
    # Windows desktop shell 조건을 재현하지 못한다. 이 환경에서는 위 --help 로더 검증까지를
    # 통과 기준으로 하고, 대화형 Windows 세션에서만 실제 MainWindow 생존 검사를 수행한다.
    $isInteractiveDesktop = [Environment]::UserInteractive -and ((Get-Process -Id $PID).SessionId -ne 0)
    if ($isInteractiveDesktop) {
        $launchStartedAt = Get-Date
        $launchStdout = Join-Path $root 'launch.stdout.txt'
        $launchStderr = Join-Path $root 'launch.stderr.txt'
        $app = Start-Process -FilePath (Join-Path $installDir 'Sorinuri.exe') `
            -RedirectStandardOutput $launchStdout `
            -RedirectStandardError $launchStderr `
            -PassThru
        Start-Sleep -Seconds 5
        $app.Refresh()
        if ($app.HasExited) {
            $stdout = Get-Content -LiteralPath $launchStdout -Raw -ErrorAction SilentlyContinue
            $stderr = Get-Content -LiteralPath $launchStderr -Raw -ErrorAction SilentlyContinue
            $appEvents = Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $launchStartedAt.AddSeconds(-2) } `
                -ErrorAction SilentlyContinue | Where-Object {
                    $_.ProviderName -eq 'Application Error' -or $_.Message -match 'Sorinuri\.exe'
                } | Select-Object -First 3 | Format-List TimeCreated, ProviderName, Id, Message | Out-String
            throw "첫 실행 창이 5초 안에 종료됐습니다. exit code=$($app.ExitCode)`nstdout=$stdout`nstderr=$stderr`napplication_events=$appEvents"
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
        Write-Host '대화형 Windows 세션의 첫 창 생성 확인 완료'
    } else {
        Write-Host '비대화형 CI Session 0: GUI 창 생성은 검사하지 않음; 실행 파일 로더 검증 완료'
    }

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
