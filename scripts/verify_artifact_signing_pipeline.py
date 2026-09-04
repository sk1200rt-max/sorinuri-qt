#!/usr/bin/env python3
"""Artifact Signing release pipeline이 공인 서명·검증 순서를 지키는지 정적으로 검사한다."""
from __future__ import annotations

from pathlib import Path
import sys

workflow = (Path(__file__).resolve().parents[1] / ".github" / "workflows" / "release.yml").read_text(
    encoding="utf-8"
)

required = {
    "OIDC 토큰 권한": "id-token: write",
    "설정 누락 fail-fast": "Verify Artifact Signing configuration",
    "OIDC Azure 로그인": "azure/login@v3",
    "공식 Artifact Signing Action": "azure/artifact-signing-action@v2",
    "Korea Central/다른 리전 endpoint 변수": "ARTIFACT_SIGNING_ENDPOINT",
    "서명 계정 변수": "ARTIFACT_SIGNING_ACCOUNT",
    "certificate profile 변수": "ARTIFACT_SIGNING_PROFILE",
    "portable 실행 코드 사전 서명": "Sign portable executable code with Artifact Signing",
    "설치 EXE 사후 서명": "Sign installer with Artifact Signing",
    "RFC3161 Microsoft timestamp": "http://timestamp.acs.microsoft.com",
    "SHA-256 파일 digest": "file-digest: SHA256",
    "SHA-256 timestamp digest": "timestamp-digest: SHA256",
    "upstream 서명 보존": "append-signature: true",
    "Authenticode 신뢰 검증": "signtool verify /pa /all /v",
    "서명 manifest": "signing-manifest",
}
for label, needle in required.items():
    if needle not in workflow:
        print(f"Artifact Signing 검증 실패: {label}을 찾을 수 없습니다.", file=sys.stderr)
        sys.exit(1)

for forbidden in ("New-SelfSignedCertificate", "Code Sign (Self-Signed)", "timestamp.digicert.com"):
    if forbidden in workflow:
        print(f"Artifact Signing 검증 실패: 자체 서명 잔재가 남아 있습니다: {forbidden}", file=sys.stderr)
        sys.exit(1)

# 공개 가능한 portable 코드에 먼저 서명한 뒤 installer를 만들고, installer를 사후 서명한 뒤 ZIP을 만든다.
portable_sign = workflow.find("Sign portable executable code with Artifact Signing")
installer_build = workflow.find("Build Installer")
installer_sign = workflow.find("Sign installer with Artifact Signing")
portable_zip = workflow.find("Create signed portable archive")
if min(portable_sign, installer_build, installer_sign, portable_zip) < 0 or not (
    portable_sign < installer_build < installer_sign < portable_zip
):
    print("Artifact Signing 검증 실패: portable 서명 → installer 생성 → installer 서명 → ZIP 순서가 아닙니다.", file=sys.stderr)
    sys.exit(1)

# secrets 부재를 무시하고 자체 서명/무서명 릴리즈가 진행되는 조건은 허용하지 않는다.
preflight = workflow[workflow.find("Verify Artifact Signing configuration"):portable_sign]
for name in ("AS_ENDPOINT", "AS_ACCOUNT", "AS_PROFILE", "AZURE_CLIENT_ID", "AZURE_TENANT_ID", "AZURE_SUBSCRIPTION_ID"):
    if name not in preflight:
        print(f"Artifact Signing 검증 실패: fail-fast 단계에 {name} 확인이 없습니다.", file=sys.stderr)
        sys.exit(1)

print("Artifact Signing pipeline 검증 통과: OIDC, fail-fast, portable/installer 순차 서명, timestamp, Authenticode 검증 확인")
