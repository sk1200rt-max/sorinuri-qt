[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9.-]{2,49}$')]
    [string]$PackageIdentityName,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^CN=.+')]
    [string]$PackagePublisher,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$PublisherDisplayName,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')]
    [string]$PackageVersion,

    [ValidateNotNullOrEmpty()]
    [string]$PackageDisplayName = "소리누리",

    [ValidateNotNullOrEmpty()]
    [string]$PackageDescription = "가온 Communication의 고음질 Windows 미디어 플레이어",

    [string]$PayloadSource = "dist/Sorinuri-Portable",
    [string]$OutputDirectory = "dist",

    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9.-]{2,79}$')]
    [string]$PackageFilePrefix = "Sorinuri-Store"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Require-Path([string]$Path, [string]$Description) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Store MSIX 패키지 실패: $Description 누락 ($Path)"
    }
}

function Get-MakeAppxPath {
    $candidates = @(
        (Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter makeappx.exe -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\x64\\makeappx\.exe$' } |
            Sort-Object FullName -Descending |
            Select-Object -ExpandProperty FullName -First 1),
        (Get-Command makeappx.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1)
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

    if (-not $candidates) {
        throw "Store MSIX 패키지 실패: Windows SDK의 makeappx.exe를 찾지 못했습니다. windows-2022 runner와 Windows SDK를 사용하세요."
    }
    return $candidates[0]
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $repoRoot
$payloadRoot = Join-Path $repoRoot $PayloadSource
$templatePath = Join-Path $repoRoot 'packaging/msix/AppxManifest.xml.in'
$outputRoot = Join-Path $repoRoot $OutputDirectory
$stagingRoot = Join-Path $outputRoot 'Sorinuri-MSIX-staging'

Require-Path $templatePath 'AppxManifest 템플릿'
Require-Path (Join-Path $payloadRoot 'Sorinuri.exe') '소리누리 실행 파일'
Require-Path (Join-Path $payloadRoot 'libmpv-2.dll') 'libmpv runtime'
Require-Path (Join-Path $payloadRoot 'platforms/qwindows.dll') 'Qt Windows platform plugin'

# MSIX는 별도의 설치 EXE를 실행할 수 없다. VC++ runtime은 manifest dependency로 Store가 공급한다.
$forbiddenFiles = @('vc_redist.x64.exe', 'Sorinuri-Setup-pending.exe', '소리누리-디버그.bat')
foreach ($name in $forbiddenFiles) {
    if (Test-Path (Join-Path $payloadRoot $name)) {
        Write-Host "Store payload에서 제외: $name"
    }
}

# version components must fit MSIX's UInt16 version fields.
foreach ($part in $PackageVersion.Split('.')) {
    $number = [UInt32]$part
    if ($number -gt 65535) {
        throw "Store MSIX 패키지 실패: 버전 구성요소는 0~65535여야 합니다 ($PackageVersion)"
    }
}

# 현재 Inno Setup registry registrations와 Store file association이 하나도 어긋나지 않게 한다.
$issPath = Join-Path $repoRoot 'installer/sorinuri-setup.iss'
Require-Path $issPath '기존 파일 연결 설치 스크립트'
$extensions = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
Get-Content -LiteralPath $issPath -Encoding UTF8 | ForEach-Object {
    if ($_ -match 'Software\\Classes\\(\.[A-Za-z0-9-]+)\\OpenWithProgids') {
        [void]$extensions.Add($matches[1].ToLowerInvariant())
    }
}
if ($extensions.Count -lt 64) {
    throw "Store MSIX 패키지 실패: 기존 파일 연결 후보를 모두 찾지 못했습니다 (현재 $($extensions.Count)개, 최소 64개 필요)."
}
$fileTypeEntries = (($extensions | Sort-Object) | ForEach-Object {
    "              <uap:FileType>$($_)</uap:FileType>"
}) -join [Environment]::NewLine

# Staging is rebuilt from scratch to prevent stale or installer-only files from leaking into the Store package.
Remove-Item -LiteralPath $stagingRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stagingRoot -Force | Out-Null
Copy-Item -Path (Join-Path $payloadRoot '*') -Destination $stagingRoot -Recurse -Force
foreach ($name in $forbiddenFiles) {
    Remove-Item -LiteralPath (Join-Path $stagingRoot $name) -Force -ErrorAction SilentlyContinue
}

$assetsPath = Join-Path $stagingRoot 'Assets'
New-Item -ItemType Directory -Path $assetsPath -Force | Out-Null
$sourceLogo = Join-Path $repoRoot 'resources/sorinuri-app.png'
Require-Path $sourceLogo '소리누리 브랜드 원본 로고'

# MSIX asset size variants are generated deterministically; no artwork/content is changed or cropped.
Add-Type -AssemblyName System.Drawing
function Save-Logo([int]$width, [int]$height, [string]$name) {
    $sourceImage = [System.Drawing.Image]::FromFile($sourceLogo)
    try {
        $bitmap = New-Object System.Drawing.Bitmap($width, $height)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $scale = [Math]::Min($width / $sourceImage.Width, $height / $sourceImage.Height)
                $drawWidth = [int][Math]::Round($sourceImage.Width * $scale)
                $drawHeight = [int][Math]::Round($sourceImage.Height * $scale)
                $x = [int][Math]::Floor(($width - $drawWidth) / 2)
                $y = [int][Math]::Floor(($height - $drawHeight) / 2)
                $graphics.DrawImage($sourceImage, $x, $y, $drawWidth, $drawHeight)
                $bitmap.Save((Join-Path $assetsPath $name), [System.Drawing.Imaging.ImageFormat]::Png)
            } finally { $graphics.Dispose() }
        } finally { $bitmap.Dispose() }
    } finally { $sourceImage.Dispose() }
}
Save-Logo 44 44 'Square44x44Logo.png'
Save-Logo 150 150 'Square150x150Logo.png'
Save-Logo 310 150 'Wide310x150Logo.png'
Save-Logo 50 50 'StoreLogo.png'

$manifest = Get-Content -LiteralPath $templatePath -Raw -Encoding UTF8
$replacements = @{
    '__PACKAGE_IDENTITY_NAME__' = [System.Security.SecurityElement]::Escape($PackageIdentityName)
    '__PACKAGE_PUBLISHER__' = [System.Security.SecurityElement]::Escape($PackagePublisher)
    '__PACKAGE_VERSION__' = [System.Security.SecurityElement]::Escape($PackageVersion)
    '__PACKAGE_DISPLAY_NAME__' = [System.Security.SecurityElement]::Escape($PackageDisplayName)
    '__PACKAGE_DESCRIPTION__' = [System.Security.SecurityElement]::Escape($PackageDescription)
    '__PUBLISHER_DISPLAY_NAME__' = [System.Security.SecurityElement]::Escape($PublisherDisplayName)
    '__SUPPORTED_FILE_TYPES__' = $fileTypeEntries
}
foreach ($key in $replacements.Keys) { $manifest = $manifest.Replace($key, $replacements[$key]) }
if ($manifest -match '__[A-Z_]+__') { throw 'Store MSIX 패키지 실패: 대체되지 않은 manifest placeholder가 있습니다.' }
$manifestPath = Join-Path $stagingRoot 'AppxManifest.xml'
[System.IO.File]::WriteAllText($manifestPath, $manifest, (New-Object System.Text.UTF8Encoding($false)))

# Structural checks before archive generation make failures actionable on CI.
[xml]$xml = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8
$namespace = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
$namespace.AddNamespace('uap', 'http://schemas.microsoft.com/appx/manifest/uap/windows10')
$manifestTypes = @($xml.SelectNodes('//uap:FileType', $namespace) | ForEach-Object { $_.'#text'.ToLowerInvariant() })
if ($manifestTypes.Count -ne $extensions.Count -or (Compare-Object ($extensions | Sort-Object) ($manifestTypes | Sort-Object))) {
    throw "Store MSIX 패키지 실패: AppxManifest 파일 연결 목록이 Inno Setup과 일치하지 않습니다."
}
foreach ($required in @('Sorinuri.exe', 'libmpv-2.dll', 'platforms/qwindows.dll', 'Assets/Square44x44Logo.png', 'Assets/Square150x150Logo.png', 'Assets/Wide310x150Logo.png', 'Assets/StoreLogo.png')) {
    Require-Path (Join-Path $stagingRoot $required) "Store payload 필수 파일"
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$packagePath = Join-Path $outputRoot ("{0}-{1}-x64.msix" -f $PackageFilePrefix, $PackageVersion)
Remove-Item -LiteralPath $packagePath -Force -ErrorAction SilentlyContinue
$makeAppx = Get-MakeAppxPath
& $makeAppx pack /d $stagingRoot /p $packagePath /o
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $packagePath)) {
    throw "Store MSIX 패키지 실패: makeappx archive 생성에 실패했습니다."
}

# Validate the actual archive, not only the staging folder.
$unpackCheck = Join-Path $outputRoot 'Sorinuri-MSIX-verify'
Remove-Item -LiteralPath $unpackCheck -Recurse -Force -ErrorAction SilentlyContinue
& $makeAppx unpack /p $packagePath /d $unpackCheck /o
if ($LASTEXITCODE -ne 0) { throw 'Store MSIX 패키지 실패: 생성된 archive를 다시 열 수 없습니다.' }
Require-Path (Join-Path $unpackCheck 'AppxManifest.xml') '패키지 manifest'
Require-Path (Join-Path $unpackCheck 'Sorinuri.exe') '패키지 실행 파일'
Remove-Item -LiteralPath $unpackCheck -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "Store 제출용 unsigned MSIX 생성 완료: $packagePath"
Write-Host "Microsoft Store는 제출·인증 후 package에 서명합니다. 이 unsigned artifact는 고객에게 직접 배포하지 마세요."
