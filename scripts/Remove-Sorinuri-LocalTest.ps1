#requires -RunAsAdministrator
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$expectedSubject = 'CN=Gaon Communication Sorinuri Local Test'
$expectedName = 'GaonCommunication.Sorinuri.LocalTest'

# 현재 사용자에게 설치된 테스트 package만 제거한다. Store 정식 package identity와는 다르다.
$packages = @(Get-AppxPackage -Name $expectedName -ErrorAction SilentlyContinue)
foreach ($package in $packages) {
    Remove-AppxPackage -Package $package.PackageFullName -ErrorAction Stop
    Write-Host "로컬 테스트 package 제거: $($package.PackageFullName)"
}

# .cer 공개 인증서에는 FriendlyName이 보존되지 않을 수 있으므로, 이 전용 subject와
# 정확히 일치하는 로컬 테스트 인증서만 제거한다. Store 정식 Publisher DN은 다르다.
$certificates = @(Get-ChildItem 'Cert:\\LocalMachine\\TrustedPeople' |
    Where-Object { $_.Subject -eq $expectedSubject })
foreach ($certificate in $certificates) {
    Remove-Item -LiteralPath ("Cert:\LocalMachine\TrustedPeople\{0}" -f $certificate.Thumbprint) -Force
    Write-Host "로컬 테스트 인증서 제거: $($certificate.Thumbprint)"
}

if ($packages.Count -eq 0 -and $certificates.Count -eq 0) {
    Write-Host '제거할 소리누리 로컬 테스트 package 또는 인증서가 없습니다.'
} else {
    Write-Host '로컬 테스트 정리 완료. Microsoft Store 정식 package·다른 인증서는 변경하지 않았습니다.'
}
