#!/usr/bin/env python3
"""
소리누리 랜딩페이지 자동 업데이트 스크립트
버전 범프 시 sorinuri.com/index.html을 자동으로 갱신합니다.

사용법:
  python3 scripts/update_landing.py <버전> <릴리즈_제목> <릴리즈_설명> [--installer-mb N] [--portable-mb N]

예시:
  python3 scripts/update_landing.py 6.12.0 \
    "오디오 핫플러그 · 외부 모니터 복원 · Intel HEVC 안전 폴백" \
    "헤드폰 연결/해제 시 오디오 자동 재초기화, 외부 모니터 연결/해제 시 검은 화면 수정, Intel 구형 GPU HEVC 프리즈 방지" \
    --installer-mb 92 --portable-mb 116

갱신 항목:
  1. <title> 및 OG 메타 태그
  2. 네비게이션 버전 배지
  3. 히어로 섹션 (버전 배지, 다운로드 버튼 링크·용량)
  4. 플레이어 미리보기 버전 배지
  5. 다운로드 섹션 (링크, 용량, 버전 표시, 날짜)
  6. 릴리즈 노트 (최신 항목 추가, 기존 항목 순서 조정)
  7. 푸터 다운로드 링크
"""
import sys, re, os, datetime, argparse

# ── 릴리즈 노트 최대 표시 개수 ─────────────────────────────────────
MAX_CHANGELOG_ITEMS = 5

def get_delay_class(idx):
    """릴리즈 노트 항목 인덱스에 따른 fade-in delay 클래스 반환"""
    if idx == 0:
        return "fade-in"
    elif idx <= 3:
        return f"fade-in fade-in-delay-{idx}"
    else:
        return "fade-in"

def update_landing(new_ver, release_title, release_desc,
                   installer_mb=None, portable_mb=None,
                   server_host="sorinuri.com",
                   server_user="amoross",
                   server_pass="mallgoer2023",
                   web_root="/var/www/sorinuri"):
    """
    서버의 index.html을 다운로드하여 수정 후 재업로드합니다.
    sshpass가 없으면 로컬 /tmp/current_index.html을 사용합니다.
    """
    import subprocess, tempfile, shutil

    today = datetime.date.today().strftime("%Y-%m-%d")
    installer_url = f"https://sorinuri.com/downloads/Sorinuri-Setup-{new_ver}.exe"
    portable_url  = f"https://sorinuri.com/downloads/Sorinuri-Qt-{new_ver}-Portable.zip"

    # ── 서버에서 index.html 다운로드 ─────────────────────────────────
    local_html = f"/tmp/landing_{new_ver}.html"
    sshpass_ok = shutil.which("sshpass") is not None

    if sshpass_ok:
        ret = subprocess.run([
            "sshpass", "-p", server_pass,
            "scp", "-o", "StrictHostKeyChecking=no",
            f"{server_user}@{server_host}:{web_root}/index.html",
            local_html
        ], capture_output=True)
        if ret.returncode != 0:
            print(f"⚠️  서버 다운로드 실패: {ret.stderr.decode()}")
            print("   /tmp/current_index.html 사용 시도...")
            shutil.copy("/tmp/current_index.html", local_html)
    else:
        print("⚠️  sshpass 없음 → /tmp/current_index.html 사용")
        shutil.copy("/tmp/current_index.html", local_html)

    with open(local_html, encoding="utf-8") as f:
        html = f.read()

    # ── 현재 버전 추출 (교체 전) ──────────────────────────────────────
    m = re.search(r'<span class="nav-ver">v([\d\.]+)</span>', html)
    old_ver = m.group(1) if m else "unknown"
    print(f"   현재 버전: v{old_ver} → v{new_ver}")

    # ── 1. 타이틀 및 메타 업데이트 ───────────────────────────────────
    html = re.sub(
        r'<title>소리누리 by 가온 Communication[^<]*</title>',
        f'<title>소리누리 by 가온 Communication — Windows 하이엔드 미디어 플레이어</title>',
        html
    )
    html = re.sub(
        r'(content="가온 Communication이 제공하는 소리누리는[^"]*")',
        'content="가온 Communication이 제공하는 소리누리는 Windows용 Qt6 기반 하이엔드 미디어 플레이어입니다. 4K HDR, DSD 네이티브, VST3, 실시간 스펙트럼, 스마트폰 리모컨까지."',
        html
    )

    # ── 2. 네비게이션 버전 배지 ───────────────────────────────────────
    html = re.sub(
        r'<span class="nav-ver">v[\d\.]+</span>',
        f'<span class="nav-ver">v{new_ver}</span>',
        html
    )

    # ── 3. 히어로 섹션 ───────────────────────────────────────────────
    # 버전 배지
    html = re.sub(
        r'v[\d\.]+ 최신 업데이트 출시',
        f'v{new_ver} 최신 업데이트 출시',
        html
    )
    # 인스톨러 다운로드 버튼
    html = re.sub(
        r'href="https://sorinuri\.com/downloads/Sorinuri-Setup-[\d\.]+\.exe" class="btn-primary"',
        f'href="{installer_url}" class="btn-primary"',
        html
    )
    # 포터블 다운로드 버튼
    html = re.sub(
        r'href="https://sorinuri\.com/downloads/Sorinuri-Qt-[\d\.]+-Portable\.zip" class="btn-secondary"',
        f'href="{portable_url}" class="btn-secondary"',
        html
    )
    # 인스톨러 용량
    if installer_mb:
        html = re.sub(
            r'(⬇ 인스톨러 다운로드 <span[^>]*>)\d+ MB(</span>)',
            rf'\g<1>{installer_mb} MB\g<2>',
            html
        )
    # 포터블 용량
    if portable_mb:
        html = re.sub(
            r'(📦 포터블 버전 <span[^>]*>)\d+ MB(</span>)',
            rf'\g<1>{portable_mb} MB\g<2>',
            html
        )
    # 히어로 메타 버전
    html = re.sub(
        r'(Windows 10/11 64-bit · 무료 · <span>)v?[\d\.]+(</span>)',
        rf'\g<1>v{new_ver}\g<2>',
        html
    )

    # ── 4. 플레이어 미리보기 버전 배지 ───────────────────────────────
    html = re.sub(
        r'<span class="preview-badge-sm">v[\d\.]+</span>',
        f'<span class="preview-badge-sm">v{new_ver}</span>',
        html
    )

    # ── 5. 다운로드 섹션 ─────────────────────────────────────────────
    # 인스톨러 링크
    html = re.sub(
        r'href="https://sorinuri\.com/downloads/Sorinuri-Setup-[\d\.]+\.exe" class="dl-btn primary"',
        f'href="{installer_url}" class="dl-btn primary"',
        html
    )
    # 포터블 링크
    html = re.sub(
        r'href="https://sorinuri\.com/downloads/Sorinuri-Qt-[\d\.]+-Portable\.zip" class="dl-btn secondary"',
        f'href="{portable_url}" class="dl-btn secondary"',
        html
    )
    # 인스톨러 용량 (dl-card-size)
    if installer_mb:
        html = re.sub(
            r'(<div class="dl-card-size">)\d+ MB( · Windows 10/11 64-bit</div>)',
            rf'\g<1>{installer_mb} MB\g<2>',
            html
        )
    # 포터블 용량 (dl-card-size)
    if portable_mb:
        html = re.sub(
            r'(<div class="dl-card-size">)\d+ MB( · ZIP 압축</div>)',
            rf'\g<1>{portable_mb} MB\g<2>',
            html
        )
    # 버전 표시 및 날짜
    html = re.sub(
        r'(<div class="dl-ver fade-in">현재 버전: <strong>)v?[\d\.]+(<\/strong> \()[\d\-]+(\)<\/div>)',
        rf'\g<1>v{new_ver}\g<2>{today}\g<3>',
        html
    )

    # ── 6. 릴리즈 노트 (최신 항목 추가) ─────────────────────────────
    # 새 항목 HTML 생성
    new_cl_item = f'''      <div class="cl-item fade-in">
        <div class="cl-ver latest">v{new_ver}</div>
        <div class="cl-content">
          <div class="cl-title">{release_title}</div>
          <div class="cl-desc">{release_desc}</div>
        </div>
      </div>'''

    # 기존 최신(latest) 항목을 prev로 변경
    html = html.replace(
        '<div class="cl-ver latest">',
        '<div class="cl-ver prev">'
    )

    # changelog-list 시작 직후에 새 항목 삽입
    html = html.replace(
        '    <div class="changelog-list">',
        f'    <div class="changelog-list">\n{new_cl_item}'
    )

    # delay 클래스 재정렬: 기존 항목들의 delay를 순서에 맞게 재조정
    # cl-item들을 모두 찾아서 순서대로 delay 클래스 재할당
    def reorder_delays(html_text):
        # changelog-list 내부 추출
        list_match = re.search(
            r'(<div class="changelog-list">)(.*?)(</div>\s*</div>\s*</section>)',
            html_text, re.DOTALL
        )
        if not list_match:
            return html_text

        list_content = list_match.group(2)
        # 각 cl-item 분리
        items = re.split(r'(?=\s*<div class="cl-item)', list_content)
        items = [i for i in items if i.strip()]

        new_items = []
        for idx, item in enumerate(items[:MAX_CHANGELOG_ITEMS]):
            # 기존 delay 클래스 제거 후 새로 할당
            item = re.sub(r'<div class="cl-item [^"]*">', '<div class="cl-item PLACEHOLDER">', item)
            delay_cls = get_delay_class(idx)
            item = item.replace('cl-item PLACEHOLDER', f'cl-item {delay_cls}')
            new_items.append(item)

        new_list = list_match.group(1) + ''.join(new_items) + '\n    ' + list_match.group(3)
        return html_text[:list_match.start()] + new_list + html_text[list_match.end():]

    html = reorder_delays(html)

    # ── 7. 푸터 다운로드 링크 ────────────────────────────────────────
    html = re.sub(
        r'href="https://sorinuri\.com/downloads/Sorinuri-Setup-[\d\.]+\.exe">인스톨러 v[\d\.]+</a>',
        f'href="{installer_url}">인스톨러 v{new_ver}</a>',
        html
    )
    html = re.sub(
        r'href="https://sorinuri\.com/downloads/Sorinuri-Qt-[\d\.]+-Portable\.zip">포터블 v[\d\.]+</a>',
        f'href="{portable_url}">포터블 v{new_ver}</a>',
        html
    )

    # ── 파일 저장 ─────────────────────────────────────────────────────
    with open(local_html, "w", encoding="utf-8") as f:
        f.write(html)
    print(f"   ✅ 로컬 수정 완료: {local_html}")

    # ── 서버에 업로드 ─────────────────────────────────────────────────
    if sshpass_ok:
        ret = subprocess.run([
            "sshpass", "-p", server_pass,
            "scp", "-o", "StrictHostKeyChecking=no",
            local_html,
            f"{server_user}@{server_host}:{web_root}/index.html"
        ], capture_output=True)
        if ret.returncode == 0:
            print(f"   ✅ 서버 업로드 완료: {server_host}{web_root}/index.html")
        else:
            print(f"   ❌ 서버 업로드 실패: {ret.stderr.decode()}")
            print(f"   수동 업로드 필요: scp {local_html} {server_user}@{server_host}:{web_root}/index.html")
    else:
        print(f"   ⚠️  sshpass 없음 → 수동 업로드 필요:")
        print(f"   scp {local_html} {server_user}@{server_host}:{web_root}/index.html")

    return local_html


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="소리누리 랜딩페이지 자동 업데이트")
    parser.add_argument("version",       help="새 버전 번호 (예: 6.12.0)")
    parser.add_argument("title",         help="릴리즈 제목 (예: '오디오 핫플러그 · 외부 모니터 복원')")
    parser.add_argument("description",   help="릴리즈 상세 설명")
    parser.add_argument("--installer-mb", type=int, default=None, help="인스톨러 파일 크기(MB)")
    parser.add_argument("--portable-mb",  type=int, default=None, help="포터블 파일 크기(MB)")
    args = parser.parse_args()

    print(f"\n🚀 소리누리 랜딩페이지 업데이트: v{args.version}")
    update_landing(
        new_ver=args.version,
        release_title=args.title,
        release_desc=args.description,
        installer_mb=args.installer_mb,
        portable_mb=args.portable_mb,
    )
    print("\n✅ 랜딩페이지 업데이트 완료!")
