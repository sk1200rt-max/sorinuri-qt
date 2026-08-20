#!/usr/bin/env python3
"""소리누리 릴리즈 변경 범위와 핵심 회귀 정책을 CI에서 강제한다.

`.release/scope.json`은 현재 릴리즈가 허용받은 제품 변경 영역을 선언한다.
이 검증은 이전 태그 이후의 변경 파일을 분류해, 선언하지 않은 영역이 섞이면
Windows 빌드와 서버 게시 이전에 실패한다.
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCOPE_FILE = ROOT / ".release" / "scope.json"

# 릴리즈별로 1개 제품 영역만 허용한다. 배포·버전·문서는 모든 영역에서 허용된다.
DOMAINS: dict[str, tuple[str, ...]] = {
    "audio": (
        "src/MpvCore.cpp", "src/MpvCore.h", "src/AudioAdvancedWidget.cpp",
        "src/AudioAdvancedWidget.h", "src/HiFiEngine.cpp", "src/HiFiEngine.h",
        "src/AudioInfoBar.cpp", "src/AudioInfoBar.h",
    ),
    "renderer": (
        "src/MpvWidget.cpp", "src/MpvWidget.h", "src/RenderEnvironment.h",
        "src/VideoAdvancedWidget.cpp", "src/VideoAdvancedWidget.h", "src/UpscaleWidget.cpp",
    ),
    "ui": (
        "src/MainWindow.cpp", "src/MainWindow.h", "src/ControlBar.cpp", "src/ControlBar.h",
        "src/SettingsDialog.cpp", "src/SettingsDialog.h", "src/TrackSelector.cpp",
        "src/TrackSelector.h", "src/TitleBar.cpp", "src/TitleBar.h", "src/UiTheme.h",
    ),
    "playback": (
        "src/PlaybackQueue.cpp", "src/PlaybackQueue.h", "src/PlaylistWidget.cpp",
        "src/PlaylistWidget.h", "src/OriginalsWidget.cpp", "src/OriginalsWidget.h",
        "src/YtdlpManager.cpp", "src/YtdlpManager.h",
    ),
    "installer": (
        "installer/", "resources/sorinuri.rc", "src/UpdateDialog.cpp", "src/UpdateDialog.h",
    ),
}

ALWAYS_ALLOWED_PREFIXES = (
    ".github/workflows/", "scripts/", ".release/", "docs/", ".verification/",
)
ALWAYS_ALLOWED_FILES = {
    "CMakeLists.txt", "src/UpdateChecker.h", "src/main.cpp",
    "scripts/releases_data.json", "CHANGELOG.md",
}
VERSION_RE = re.compile(r'^\d+\.\d+\.\d+$')


def version_metadata_only(path: str, base_tag: str) -> bool:
    """자동 버전 범프가 만드는 메타데이터 변경만 허용한다.

    설치 스크립트와 리소스 파일은 제품 파일이지만, 버전 숫자 이외의 변경은
    installer 영역으로 선언되지 않으면 반드시 차단한다.
    """
    if path not in {"installer/sorinuri-setup.iss", "resources/sorinuri.rc"}:
        return False
    # CI의 커밋 diff와 로컬의 staged/unstaged diff를 모두 합친다.
    diff = "\n".join(filter(None, [
        run("git", "diff", "--no-color", "--unified=0", f"{base_tag}..HEAD", "--", path),
        run("git", "diff", "--no-color", "--cached", "--unified=0", "--", path),
        run("git", "diff", "--no-color", "--unified=0", "--", path),
    ]))
    changed = [line[1:] for line in diff.splitlines()
               if line.startswith(("+", "-")) and not line.startswith(("+++", "---"))]
    if not changed:
        return False
    allowed_prefixes = {
        "installer/sorinuri-setup.iss": (
            "#define MyAppVersion ", "AppVersion=", "OutputBaseFilename=Sorinuri-Setup-",
        ),
        "resources/sorinuri.rc": (
            "FILEVERSION ", "PRODUCTVERSION ", 'VALUE "FileVersion", ', 'VALUE "ProductVersion", ',
        ),
    }
    return all(line.lstrip().startswith(allowed_prefixes[path]) for line in changed)


def run(*args: str) -> str:
    return subprocess.check_output(args, cwd=ROOT, text=True, stderr=subprocess.STDOUT).strip()


def current_version() -> str:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(Sorinuri VERSION (\d+\.\d+\.\d+)", cmake)
    if not match:
        raise ValueError("CMakeLists.txt에서 현재 버전을 찾을 수 없습니다.")
    return match.group(1)


def previous_tag(version: str) -> str:
    """현재 선언 버전이 태그된 경우에만 부모에서 직전 태그를 선택한다."""
    try:
        exact_tag = run("git", "describe", "--tags", "--exact-match", "HEAD")
    except subprocess.CalledProcessError:
        exact_tag = ""
    try:
        # 새 버전 태그에서만 부모 기준으로 직전 태그를 찾는다. 작업 트리가
        # 직전 태그(vX.Y.Z) 위에 있으나 scope는 다음 버전인 경우에는 HEAD를
        # 기준으로 해야 vX.Y.Z를 올바른 비교 기준으로 사용한다.
        start = "HEAD^" if exact_tag == f"v{version}" else "HEAD"
        return run("git", "describe", "--tags", "--abbrev=0", start)
    except subprocess.CalledProcessError as exc:
        raise ValueError(f"이전 릴리즈 태그를 찾지 못했습니다: {exc.output}") from exc


def changed_files(base_tag: str) -> list[str]:
    # CI 커밋, 로컬 staged/unstaged 파일, 새 파일을 모두 검사한다.
    groups = [
        run("git", "diff", "--no-color", "--name-only", f"{base_tag}..HEAD"),
        run("git", "diff", "--no-color", "--name-only", "--cached"),
        run("git", "diff", "--no-color", "--name-only"),
        run("git", "ls-files", "--others", "--exclude-standard"),
    ]
    return sorted({line for output in groups for line in output.splitlines() if line})


def domain_for(path: str) -> str | None:
    for domain, entries in DOMAINS.items():
        for entry in entries:
            if entry.endswith("/") and path.startswith(entry):
                return domain
            if path == entry:
                return domain
    return None


def always_allowed(path: str) -> bool:
    return path in ALWAYS_ALLOWED_FILES or path.startswith(ALWAYS_ALLOWED_PREFIXES)


def main() -> int:
    if not SCOPE_FILE.exists():
        print("릴리즈 게이트 실패: .release/scope.json이 없습니다.", file=sys.stderr)
        return 1

    try:
        scope = json.loads(SCOPE_FILE.read_text(encoding="utf-8"))
        version = str(scope["version"])
        # 연관된 여러 영역은 쉼표 구분 선언을 허용하되, 선언 밖 제품 변경은 계속 차단한다.
        domains = tuple(part.strip() for part in str(scope["domain"]).split(",") if part.strip())
        purpose = str(scope["purpose"]).strip()
    except (json.JSONDecodeError, KeyError, TypeError) as exc:
        print(f"릴리즈 게이트 실패: scope.json 형식 오류: {exc}", file=sys.stderr)
        return 1

    if not VERSION_RE.fullmatch(version) or version != current_version():
        print("릴리즈 게이트 실패: scope.json 버전과 앱 버전이 일치하지 않습니다.", file=sys.stderr)
        return 1
    if not domains or any(domain not in DOMAINS for domain in domains) or not purpose:
        print("릴리즈 게이트 실패: 허용되지 않은 변경 영역 또는 빈 변경 목적입니다.", file=sys.stderr)
        return 1

    base = previous_tag(version)
    unexpected: list[tuple[str, str]] = []
    for path in changed_files(base):
        if version_metadata_only(path, base):
            continue
        found = domain_for(path)
        if found is None:
            if not always_allowed(path):
                unexpected.append((path, "분류되지 않은 제품 파일"))
        elif found not in domains:
            unexpected.append((path, f"선언 영역={','.join(domains)}, 실제 영역={found}"))

    if unexpected:
        print(f"릴리즈 게이트 실패: {base} 이후 선언 범위({','.join(domains)}) 밖 변경이 있습니다.", file=sys.stderr)
        for path, reason in unexpected:
            print(f" - {path}: {reason}", file=sys.stderr)
        return 1

    print(f"릴리즈 게이트 통과: v{version} | 영역={','.join(domains)} | 기준={base} | 목적={purpose}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
