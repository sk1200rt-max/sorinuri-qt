[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$PayloadSource,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')]
    [string]$PackageVersion,

    [string]$OutputDirectory = "dist-local-test",
    [ValidateRange(1, 30)]
    [int]$CertificateValidityDays = 14
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $repoRoot
$absoluteOutput = Join-Path $repoRoot $OutputDirectory
New-Item -ItemType Directory -Path $absoluteOutput -Force | Out-Null

# 이 고정 identity는 Partner Center Store identity와 일부러 다르며, 로컬 테스트에만 사용한다.
# PFX/private key는 CI artifact에 포함하지 않고 runner의 CurrentUser\My에만 일시 생성한 뒤 삭제한다.
$testIdentity = 'GaonCommunication.Sorinuri.LocalTest'
$testPublisher = 'CN=Gaon Communication Sorinuri Local Test'
$testDisplayName = '소리누리 (로컬 테스트)'
$testDescription = '가온 Communication 소리누리의 Microsoft Store 전 사전 테스트 전용 빌드'
$testPrefix = 'Sorinuri-LOCAL-TEST-NOT-FOR-DISTRIBUTION'
$certFriendlyName = 'Sorinuri Local MSIX Test Only'

function Get-SignToolPath {
    $candidates = @(
        (Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
            Sort-Object FullName -Descending |
            Select-Object -ExpandProperty FullName -First 1),
        (Get-Command signtool.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1)
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }
    if (-not $candidates) { throw '로컬 테스트 MSIX 생성 실패: Windows SDK의 signtool.exe를 찾지 못했습니다.' }
    return $candidates[0]
}

$certificate = $null
try {
    $certificate = New-SelfSignedCertificate `
        -Type Custom `
        -KeyUsage DigitalSignature `
        -CertStoreLocation 'Cert:\CurrentUser\My' `
        -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}') `
        -Subject $testPublisher `
        -FriendlyName $certFriendlyName `
        -NotAfter (Get-Date).AddDays($CertificateValidityDays)

    # build_store_msix.ps1는 repo root를 기준으로 OutputDirectory를 결합하므로,
    # 여기서는 절대 경로를 다시 전달하지 않고 상대 경로만 전달한다.
    $unsignedDirectoryRelative = Join-Path $OutputDirectory 'unsigned'
    $unsignedDirectory = Join-Path $absoluteOutput 'unsigned'
    New-Item -ItemType Directory -Path $unsignedDirectory -Force | Out-Null
    & (Join-Path $PSScriptRoot 'build_store_msix.ps1') `
        -PackageIdentityName $testIdentity `
        -PackagePublisher $testPublisher `
        -PublisherDisplayName '가온 Communication' `
        -PackageVersion $PackageVersion `
        -PackageDisplayName $testDisplayName `
        -PackageDescription $testDescription `
        -PayloadSource $PayloadSource `
        -OutputDirectory $unsignedDirectoryRelative `
        -PackageFilePrefix $testPrefix
    if ($LASTEXITCODE -ne 0) { throw '로컬 테스트 MSIX archive 생성 실패' }

    $unsignedMsix = Join-Path $unsignedDirectory ("{0}-{1}-x64.msix" -f $testPrefix, $PackageVersion)
    if (-not (Test-Path -LiteralPath $unsignedMsix)) { throw "unsigned MSIX가 없습니다: $unsignedMsix" }

    $signedMsix = Join-Path $absoluteOutput ("Sorinuri-LOCAL-TEST-NOT-FOR-DISTRIBUTION-{0}-x64.msix" -f $testPrefix, $PackageVersion)
    Move-Item -LiteralPath $unsignedMsix -Destination $signedMsix -Force
    Remove-Item -LiteralPath $unsignedDirectory -Recurse -Force -ErrorAction SilentlyContinue

    $signTool = Get-SignToolPath
    & $signTool sign /fd SHA256 /sha1 $certificate.Thumbprint /s My $signedMsix
    if ($LASTEXITCODE -ne 0) { throw '로컬 테스트 MSIX 서명 실패' }

    # .cer에는 공개 키만 들어 있으며, PFX/private key는 만든 PC/CI 밖으로 절대 내보내지 않는다.
    $certificatePath = Join-Path $absoluteOutput 'Sorinuri-LOCAL-TEST-CERTIFICATE-ONLY.cer'
    Export-Certificate -Cert $certificate -FilePath $certificatePath -Force | Out-Null

    # self-signed 테스트 인증서는 공인 루트 체인이 아니다. CI에서 공인 체인을 요구하지 않고,
    # 생성 직후 MSIX가 서명됐으며 내보낸 공개 인증서와 서명자가 정확히 같은지만 확인한다.
    # 실제 PC에서는 Install-Sorinuri-LocalTest.ps1가 Trusted People 등록 후 Add-AppxPackage로
    # Windows의 MSIX 서명·신뢰·무결성 검증을 수행한다.
    $signature = Get-AuthenticodeSignature -FilePath $signedMsix
    if (-not $signature.SignerCertificate) { throw '로컬 테스트 MSIX 서명자를 찾을 수 없습니다.' }
    if ($signature.SignerCertificate.Subject -ne $testPublisher) { throw '로컬 테스트 MSIX 서명 주체가 예상값과 다릅니다.' }
    if ($signature.SignerCertificate.Thumbprint -ne $certificate.Thumbprint) { throw '로컬 테스트 MSIX 서명 인증서가 내보낸 공개 인증서와 다릅니다.' }

    $installScript = Join-Path $repoRoot 'scripts/Install-Sorinuri-LocalTest.ps1'
    $removeScript = Join-Path $repoRoot 'scripts/Remove-Sorinuri-LocalTest.ps1'
    foreach ($file in @($installScript, $removeScript)) {
        if (-not (Test-Path -LiteralPath $file)) { throw "로컬 테스트 지원 스크립트 누락: $file" }
        Copy-Item -LiteralPath $file -Destination $absoluteOutput -Force
    }

    # Microsoft-signed VCLibs framework is supplied only for local sideload test.
    $vclibs = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\App Certification Kit" -Recurse -Filter 'Microsoft.VCLibs.x64.14.00.Desktop.appx' -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($vclibs) {
        Copy-Item -LiteralPath $vclibs.FullName -Destination $absoluteOutput -Force
    } else {
        Write-Warning 'VCLibs offline dependency를 runner에서 찾지 못했습니다. Install script는 Store 설치 여부를 확인한 뒤 정확한 오류를 표시합니다.'
    }

    $readme = @"
# 소리누리 로컬 MSIX 테스트 패키지 — 외부 배포 금지

이 파일은 가온 Communication 담당자의 현재 테스트 PC에서만 사용하는 self-signed MSIX입니다.
Microsoft Store 제출용 또는 고객 다운로드 파일이 아닙니다.

1. 관리자 권한 PowerShell에서 `Install-Sorinuri-LocalTest.ps1`를 실행합니다.
2. 앱 목록의 `소리누리 (로컬 테스트)`를 실행합니다.
3. 테스트 종료 후 `Remove-Sorinuri-LocalTest.ps1`를 관리자 PowerShell에서 실행하면 package와 Trusted People test certificate가 함께 제거됩니다.

포함된 `Sorinuri-LOCAL-TEST-CERTIFICATE-ONLY.cer`는 이 테스트 MSIX에만 신뢰를 부여하는 공개 인증서입니다. PFX/private key는 포함되지 않습니다. 인증서 만료일: $($certificate.NotAfter.ToString('yyyy-MM-dd HH:mm:ss K')).
"@
    Set-Content -LiteralPath (Join-Path $absoluteOutput 'README-LOCAL-TEST-ONLY.md') -Value $readme -Encoding UTF8

    Write-Host "로컬 전용 테스트 MSIX 생성 완료: $signedMsix"
    Write-Host "공개 인증서: $certificatePath"
    Write-Host '이 artifact는 고객·GitHub Release·sorinuri.com에 게시하지 마세요. Store 검증 후 Store 서명 package로 교체해야 합니다.'
}
finally {
    if ($certificate) {
        Remove-Item -LiteralPath ("Cert:\CurrentUser\My\{0}" -f $certificate.Thumbprint) -Force -ErrorAction SilentlyContinue
    }
}
