#!/usr/bin/env python3
"""사전 테스트 배포와 production 분리 회귀 검사."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
workflow = (ROOT / ".github/workflows/release.yml").read_text(encoding="utf-8")
script = (ROOT / "scripts/publish_self_hosted_test_release.sh").read_text(encoding="utf-8")

checks: list[tuple[bool, str]] = [
    ("publish-test-to-sorinuri:" in workflow,
     "릴리즈 뒤 테스트 전용 게시 작업이 있어야 합니다."),
    ("needs: [release, publish-test-to-sorinuri]" in workflow,
     "정식 production 게시 전 테스트 패키지 게시 성공을 요구해야 합니다."),
    ("draft: true" in workflow and "prerelease: true" in workflow,
     "실장비 테스트 전 GitHub 릴리즈가 공개되면 안 됩니다."),
    ("scripts/publish_self_hosted_test_release.sh" in workflow,
     "테스트 작업은 분리된 테스트 게시 스크립트를 사용해야 합니다."),
    ("Upload pre-production package" in workflow and "actions/upload-artifact@v4" in workflow,
     "서명 완료된 테스트 패키지를 워크플로우 산출물로 보관해야 합니다."),
    ("Download packaged test assets" in workflow and "actions/download-artifact@v4" in workflow,
     "테스트 게시 작업은 draft 릴리즈가 아닌 워크플로우 산출물을 내려받아야 합니다."),
    ("BASE=\"https://sorinuri.com/downloads/testing/${VERSION}\"" in workflow,
     "테스트 검증은 정식 다운로드 파일과 분리된 /downloads/testing 경로를 확인해야 합니다."),
    ("TEST_ROOT=\"/var/www/sorinuri/downloads/testing\"" in script,
     "테스트 패키지는 /downloads/testing 전용 경로에 게시해야 합니다."),
    ("TESTING-ONLY.txt" in script,
     "테스트 패키지에는 production 전용이 아님을 알리는 표시 파일이 있어야 합니다."),
    ("sha256sum --check" in script,
     "테스트 게시 전 SHA-256 검증이 있어야 합니다."),
    ("test ! -e \"$DESTINATION\"" in script,
     "같은 버전의 테스트 패키지를 덮어쓰면 안 됩니다."),
    ("SITE_ROOT=" not in script and "version.json.new" not in script,
     "테스트 게시 스크립트는 자동 업데이트 매니페스트를 변경하면 안 됩니다."),
    (not re.search(r'\bmv\b[^\n]*(?:/downloads|version\.json|index\.html|changelog\.html)', script),
     "테스트 게시 스크립트는 정식 다운로드·매니페스트·웹 페이지를 이동하면 안 됩니다."),
]

failed = [message for ok, message in checks if not ok]
if failed:
    print("[FAIL] 사전 테스트 배포 격리 검사 실패")
    for message in failed:
        print(" -", message)
    sys.exit(1)

print("[PASS] 사전 테스트 배포 격리 검사 통과")
