#!/usr/bin/env python3
"""Windows App Control 오류 4551을 피하는 Inno Setup 패키징 경계를 검증한다."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
INSTALLER = ROOT / "installer" / "sorinuri-setup.iss"
WORKFLOW = ROOT / ".github" / "workflows" / "build-windows.yml"
RELEASE_WORKFLOW = ROOT / ".github" / "workflows" / "release.yml"
TEST_PUBLISH = ROOT / "scripts" / "publish_self_hosted_test_release.sh"
PRODUCTION_PUBLISH = ROOT / "scripts" / "publish_self_hosted_release.sh"


def require(text: str, needle: str, description: str) -> None:
    if needle not in text:
        raise AssertionError(f"누락: {description}\n기대 문자열: {needle}")


def main() -> int:
    installer = INSTALLER.read_text(encoding="utf-8")
    workflow = WORKFLOW.read_text(encoding="utf-8")
    release_workflow = RELEASE_WORKFLOW.read_text(encoding="utf-8")
    test_publish = TEST_PUBLISH.read_text(encoding="utf-8")
    production_publish = PRODUCTION_PUBLISH.read_text(encoding="utf-8")

    require(installer, "UseSetupLdr=no", "Inno Setup loaderless 패키징")
    require(
        installer,
        'Source: "..\\dist\\vc_redist.x64.exe"; DestDir: "{app}"',
        "VC++ 재배포 패키지의 설치 폴더 배치",
    )
    require(
        installer,
        'Filename: "{app}\\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"',
        "VC++ 재배포 패키지의 설치 폴더 실행",
    )
    if 'Source: "..\\dist\\vc_redist.x64.exe"; DestDir: "{tmp}"' in installer:
        raise AssertionError("VC++ 재배포 패키지가 다시 임시 폴더에서 실행되도록 설정됐습니다.")

    require(
        workflow,
        'Get-ChildItem -Path $installer.DirectoryName -Filter "$($installer.BaseName)-*.bin"',
        "loaderless BIN 파일 생성 검사",
    )
    require(workflow, "dist/Sorinuri-Setup-*.bin", "installer artifact의 BIN 파일 포함")
    require(release_workflow, "Sorinuri-Setup-${{ steps.version.outputs.version }}-*.bin", "GitHub 릴리즈 BIN 파일 포함")
    for publish_script, description in (
        (test_publish, "사전 테스트 게시"),
        (production_publish, "정식 게시"),
    ):
        require(publish_script, 'INSTALLER_BINS=("Sorinuri-Setup-${VERSION}-0.bin" "Sorinuri-Setup-${VERSION}-1.bin")', f"{description} BIN 파일 입력")
        require(publish_script, 'sha256sum "$INSTALLER" "${INSTALLER_BINS[0]}" "${INSTALLER_BINS[1]}" "$PORTABLE"', f"{description} BIN 파일 checksum")

    print("INSTALLER_APP_CONTROL_POLICY_VERIFIED")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"INSTALLER_APP_CONTROL_POLICY_FAILED: {exc}", file=sys.stderr)
        raise SystemExit(1)
