#!/usr/bin/env python3
"""Static release gate for the Microsoft Store MSIX variant.

The Store package is intentionally built only through a manual workflow dispatch after
Partner Center supplies the package identity.  This check protects the most important
separation: the Store build must never invoke the self-hosted Inno updater, and its
manifest must retain every file association declared by the established installer.
"""
from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    path = ROOT / relative
    if not path.is_file():
        raise AssertionError(f"필수 파일이 없습니다: {relative}")
    return path.read_text(encoding="utf-8")


def require(text: str, needle: str, file_name: str) -> None:
    if needle not in text:
        raise AssertionError(f"{file_name}에 필수 정책이 없습니다: {needle}")


def main() -> int:
    cmake = read("CMakeLists.txt")
    window = read("src/MainWindow.cpp")
    manifest = read("packaging/msix/AppxManifest.xml.in")
    packer = read("scripts/build_store_msix.ps1")
    local_packer = read("scripts/build_local_test_msix.ps1")
    local_installer = read("scripts/Install-Sorinuri-LocalTest.ps1")
    local_remover = read("scripts/Remove-Sorinuri-LocalTest.ps1")
    workflow = read(".github/workflows/build-windows.yml")
    installer = read("installer/sorinuri-setup.iss")

    require(cmake, 'option(SORINURI_STORE_BUILD', "CMakeLists.txt")
    require(cmake, 'target_compile_definitions(Sorinuri PRIVATE SORINURI_STORE_BUILD=1)', "CMakeLists.txt")
    require(window, '#if !defined(SORINURI_STORE_BUILD)', "src/MainWindow.cpp")
    require(window, 'updater->checkForUpdates();', "src/MainWindow.cpp")

    for required in (
        'uap10:RuntimeBehavior="packagedClassicApp"',
        'uap10:TrustLevel="mediumIL"',
        '<rescap:Capability Name="runFullTrust" />',
        'Category="windows.fileTypeAssociation"',
        '__PACKAGE_IDENTITY_NAME__',
        '__PACKAGE_PUBLISHER__',
        '__SUPPORTED_FILE_TYPES__',
        'Microsoft.VCLibs.140.00.UWPDesktop',
    ):
        require(manifest, required, "packaging/msix/AppxManifest.xml.in")

    for required in (
        'PackageIdentityName',
        'PackagePublisher',
        'PackageVersion',
        "'vc_redist.x64.exe'",
        'OpenWithProgids',
        'makeappx.exe',
        'Compare-Object',
        'unsigned artifact',
    ):
        require(packer, required, "scripts/build_store_msix.ps1")

    for required in (
        'build_store_msix:',
        "inputs.build_store_msix == true",
        'Verify MSIX version derivation',
        "$projectText = Get-Content 'CMakeLists.txt' -Raw",
        "project\\(Sorinuri\\s+VERSION\\s+([0-9]+\\.[0-9]+\\.[0-9]+)",
        '$msixVersion = "$($Matches[1]).0"',
        '"MSIX_VERSION=$msixVersion" | Add-Content -Path $env:GITHUB_ENV -Encoding utf8',
        '$msixVersion = $env:MSIX_VERSION',
        'MSIX_PACKAGE_IDENTITY_NAME',
        'MSIX_PACKAGE_PUBLISHER',
        'MSIX_PUBLISHER_DISPLAY_NAME',
        '-DSORINURI_STORE_BUILD=ON',
        'build_store_msix.ps1',
        'Upload Microsoft Store submission package',
        'Sorinuri-Microsoft-Store-MSIX-Submission',
        'dist/Sorinuri-Store-*-x64.msix',
    ):
        require(workflow, required, ".github/workflows/build-windows.yml")

    if '$env:APP_VERSION.0' in workflow:
        raise AssertionError('MSIX 버전은 누락될 수 있는 APP_VERSION 환경변수가 아니라 CMakeLists.txt의 실제 제품 버전에서 생성해야 합니다.')

    for required in (
        'build_local_test_msix:',
        "inputs.build_local_test_msix == true",
        '$msixVersion = $env:MSIX_VERSION',
        'build_local_test_msix.ps1',
        'Upload local self-signed test package',
        'Sorinuri-LOCAL-TEST-NOT-FOR-DISTRIBUTION',
        'retention-days: 3',
    ):
        require(workflow, required, ".github/workflows/build-windows.yml")

    for required in (
        'New-SelfSignedCertificate',
        "Cert:\\CurrentUser\\My",
        'DigitalSignature',
        '1.3.6.1.5.5.7.3.3',
        'CertificateValidityDays',
        'Export-Certificate',
        'Remove-Item -LiteralPath ("Cert:\\CurrentUser\\My',
        'NOT-FOR-DISTRIBUTION',
    ):
        require(local_packer, required, "scripts/build_local_test_msix.ps1")
    if 'Export-PfxCertificate' in local_packer or '.pfx' in local_packer.lower():
        raise AssertionError('로컬 테스트 빌드에 PFX/private key 내보내기 경로가 있어서는 안 됩니다.')

    for required in (
        "Cert:\\LocalMachine\\TrustedPeople",
        'Get-AuthenticodeSignature',
        'Add-AppxPackage',
        'CN=Gaon Communication Sorinuri Local Test',
    ):
        require(local_installer, required, "scripts/Install-Sorinuri-LocalTest.ps1")
    if 'Cert:\\LocalMachine\\Root' in local_installer or 'TrustedRootCertificationAuthorities' in local_installer:
        raise AssertionError('로컬 테스트 인증서를 Trusted Root에 넣어서는 안 됩니다.')
    for required in (
        'Remove-AppxPackage',
        "Cert:\\LocalMachine\\TrustedPeople",
        'CN=Gaon Communication Sorinuri Local Test',
    ):
        require(local_remover, required, "scripts/Remove-Sorinuri-LocalTest.ps1")

    # Store package must list every legacy file association candidate, not a reduced subset.
    associations = set(re.findall(r'Software\\Classes\\(\.[A-Za-z0-9-]+)\\OpenWithProgids', installer))
    if len(associations) < 64:
        raise AssertionError(f"installer/sorinuri-setup.iss의 파일 연결 후보가 불완전합니다: {len(associations)}개")

    if 'Sorinuri-Setup-pending.exe' in manifest or 'vc_redist.x64.exe' in manifest:
        raise AssertionError('Store manifest에 Inno Setup 또는 VC++ 재배포 설치 EXE가 포함돼서는 안 됩니다.')

    print(f"MSIX Store package policy verification passed ({len(associations)} file associations).")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"MSIX Store package policy verification failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
