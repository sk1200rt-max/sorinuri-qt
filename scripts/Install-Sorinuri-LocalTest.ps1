#requires -RunAsAdministrator
[CmdletBinding()]
param(
    [string]$PackagePath,
    [string]$CertificatePath,
    [string]$VclibsPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptRoot = Split-Path -Parent $PSCommandPath
$expectedSubject = 'CN=Gaon Communication Sorinuri Local Test'
$expectedName = 'GaonCommunication.Sorinuri.LocalTest'

if ([string]::IsNullOrWhiteSpace($PackagePath)) {
    $packages = @(Get-ChildItem -LiteralPath $scriptRoot -Filter 'Sorinuri-LOCAL-TEST-NOT-FOR-DISTRIBUTION-*-x64.msix' | Sort-Object LastWriteTime -Descending)
    if ($packages.Count -ne 1) { throw "로컬 테스트 MSIX를 하나만 찾을 수 있어야 합니다. -PackagePath로 지정하세요. (찾음: $($packages.Count))" }
    $PackagePath = $packages[0].FullName
}
if ([string]::IsNullOrWhiteSpace($CertificatePath)) { $CertificatePath = Join-Path $scriptRoot 'Sorinuri-LOCAL-TEST-CERTIFICATE-ONLY.cer' }
if ([string]::IsNullOrWhiteSpace($VclibsPath)) {
    $candidate = Get-ChildItem -LiteralPath $scriptRoot -Filter 'Microsoft.VCLibs.x64.14.00.Desktop.appx' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($candidate) { $VclibsPath = $candidate.FullName }
}

foreach ($path in @($PackagePath, $CertificatePath)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "필수 로컬 테스트 파일이 없습니다: $path" }
}

$certificate = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($CertificatePath)
if ($certificate.Subject -ne $expectedSubject) {
    throw "인증서 주체가 로컬 테스트 패키지와 일치하지 않습니다."
}
if ($certificate.NotAfter -le (Get-Date)) { throw "테스트 인증서가 만료됐습니다: $($certificate.NotAfter)" }

# Trusted Root에는 넣지 않는다. 이 특정 self-signed package 인증서만 Trusted People에 신뢰한다.
$existing = Get-ChildItem 'Cert:\LocalMachine\TrustedPeople' |
    Where-Object { $_.Thumbprint -eq $certificate.Thumbprint }
if (-not $existing) {
    Import-Certificate -FilePath $CertificatePath -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' | Out-Null
}

# Trusted People에 넣은 공개 인증서와 정확히 일치하는 self-signed MSIX만 허용한다.
$signature = Get-AuthenticodeSignature -FilePath $PackagePath
if (-not $signature.SignerCertificate) { throw 'MSIX 서명자를 찾을 수 없습니다.' }
if ($signature.SignerCertificate.Subject -ne $expectedSubject) {
    throw "잘못된 테스트 패키지 서명 주체입니다. 예상: $expectedSubject / 실제: $($signature.SignerCertificate.Subject)"
}
if ($signature.SignerCertificate.Thumbprint -ne $certificate.Thumbprint) {
    throw 'MSIX 서명 인증서가 함께 제공된 로컬 테스트 공개 인증서와 일치하지 않습니다.'
}
# Get-AuthenticodeSignature의 공인 루트 체인 상태는 self-signed 테스트 인증서의
# Trusted People 배포 모델과 다를 수 있다. 아래 Add-AppxPackage가 Windows의 실제
# MSIX 서명·신뢰·무결성 검증을 수행하므로 그 결과를 설치 승인 기준으로 사용한다.

if ($VclibsPath -and (Test-Path -LiteralPath $VclibsPath)) {
    try {
        Add-AppxPackage -Path $VclibsPath -ErrorAction Stop
    } catch {
        # 최신 Windows/Store에 VCLibs가 이미 있어도 설치 시도는 충돌할 수 있으므로 package 존재 여부를 확인한다.
        $vclibsInstalled = Get-AppxPackage -AllUsers -Name 'Microsoft.VCLibs.140.00.UWPDesktop' -ErrorAction SilentlyContinue
        if (-not $vclibsInstalled) { throw }
    }
}

# 같은 local-test identity의 이전 package를 교체하는 것은 현재 사용자 범위에서만 수행한다.
Get-AppxPackage -Name $expectedName -ErrorAction SilentlyContinue | Remove-AppxPackage -ErrorAction SilentlyContinue
Add-AppxPackage -Path $PackagePath

$installed = Get-AppxPackage -Name $expectedName -ErrorAction SilentlyContinue
if (-not $installed) { throw '로컬 테스트 MSIX 설치 후 package identity를 확인하지 못했습니다.' }

Write-Host "설치 완료: $($installed.Name) $($installed.Version)"
Write-Host '시작 메뉴에서 “소리누리 (로컬 테스트)”를 실행하세요. 이 패키지는 Microsoft Store 제출·외부 배포용이 아닙니다.'
