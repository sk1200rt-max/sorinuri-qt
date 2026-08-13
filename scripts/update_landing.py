#!/usr/bin/env python3
"""
소리누리 랜딩페이지 + 릴리즈 아카이브 자동 업데이트 스크립트
버전 범프 시 sorinuri.com/index.html 및 sorinuri.com/changelog.html을 자동으로 갱신합니다.

사용법:
  python3 scripts/update_landing.py <버전> <릴리즈_제목> <릴리즈_설명> [--installer-mb N] [--portable-mb N]

예시:
  python3 scripts/update_landing.py 6.12.0 \
    "오디오 핫플러그 · 외부 모니터 복원 · Intel HEVC 안전 폴백" \
    "헤드폰 연결/해제 시 오디오 자동 재초기화, 외부 모니터 연결/해제 시 검은 화면 수정" \
    --installer-mb 92 --portable-mb 116

갱신 항목:
  index.html  - 버전, 다운로드 링크, 릴리즈 노트 (최신 5개)
  changelog.html - 전체 릴리즈 이력 아카이브 페이지 (자동 생성/갱신)
  releases_data.json - 릴리즈 이력 데이터 파일 (서버 동기화)
"""
import sys, re, os, datetime, argparse, json, shutil, subprocess

# ── 설정 ──────────────────────────────────────────────────────────────
MAX_CHANGELOG_ITEMS = 5  # 랜딩페이지에 표시할 최대 릴리즈 노트 수
SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))
RELEASES_JSON = os.path.join(SCRIPTS_DIR, "releases_data.json")
DOWNLOAD_BASE_URL = "https://sorinuri.com/downloads"


def get_delay_class(idx):
    if idx == 0:
        return "fade-in"
    elif idx <= 3:
        return f"fade-in fade-in-delay-{idx}"
    return "fade-in"


def load_releases():
    """releases_data.json 로드 (없으면 빈 리스트)"""
    if os.path.exists(RELEASES_JSON):
        with open(RELEASES_JSON, encoding="utf-8") as f:
            return json.load(f)
    return []


def save_releases(releases):
    """releases_data.json 저장"""
    with open(RELEASES_JSON, "w", encoding="utf-8") as f:
        json.dump(releases, f, ensure_ascii=False, indent=2)


def add_release(releases, new_ver, title, desc, installer_mb, portable_mb):
    """릴리즈 목록 맨 앞에 새 항목 추가 (중복 버전은 업데이트)"""
    today = datetime.date.today().strftime("%Y-%m-%d")
    installer_url = f"{DOWNLOAD_BASE_URL}/Sorinuri-Setup-{new_ver}.exe"
    portable_url  = f"{DOWNLOAD_BASE_URL}/Sorinuri-Qt-{new_ver}-Portable.zip"

    new_entry = {
        "version":   new_ver,
        "date":      today,
        "title":     title,
        "desc":      desc,
        "installer": installer_url,
        "portable":  portable_url,
        "tags":      []
    }

    # 같은 버전이 이미 있으면 교체, 없으면 맨 앞에 삽입
    releases = [r for r in releases if r["version"] != new_ver]
    releases.insert(0, new_entry)
    return releases


# ── changelog.html 생성 ───────────────────────────────────────────────

def build_tag_html(tags):
    if not tags:
        return ""
    items = "".join(f'<span class="cl-tag">{t}</span>' for t in tags)
    return f'<div class="cl-tags">{items}</div>'


def build_changelog_html(releases, current_ver):
    """전체 릴리즈 이력 아카이브 페이지 HTML 생성"""

    def release_item(r, is_latest):
        badge_cls = "cl-ver latest" if is_latest else "cl-ver prev"
        installer_btn = (
            f'<a href="{r["installer"]}" class="cl-dl-btn primary">⬇ 인스톨러</a>'
            if r.get("installer") else ""
        )
        portable_btn = (
            f'<a href="{r["portable"]}" class="cl-dl-btn secondary">📦 포터블</a>'
            if r.get("portable") else ""
        )
        tags_html = build_tag_html(r.get("tags", []))
        return f"""      <div class="cl-item{'  cl-latest' if is_latest else ''}">
        <div class="cl-left">
          <div class="{badge_cls}">v{r['version']}</div>
          <div class="cl-date">{r.get('date', '')}</div>
        </div>
        <div class="cl-content">
          <div class="cl-title">{r['title']}</div>
          <div class="cl-desc">{r['desc']}</div>
          {tags_html}
          <div class="cl-dl-btns">{installer_btn}{portable_btn}</div>
        </div>
      </div>"""

    items_html = "\n".join(
        release_item(r, i == 0) for i, r in enumerate(releases)
    )

    return f"""<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>소리누리 릴리즈 노트 — 가온 Communication</title>
<meta name="description" content="소리누리 전체 버전 업데이트 이력. 가온 Communication이 제공하는 Windows 하이엔드 미디어 플레이어.">
<meta property="og:title" content="소리누리 릴리즈 노트 — 가온 Communication">
<meta property="og:type" content="website">
<style>
  :root {{
    --bg: #080810;
    --bg2: #0d0d1a;
    --card: #13131f;
    --card2: #181828;
    --border: rgba(255,255,255,0.07);
    --border2: rgba(255,255,255,0.12);
    --text: #f0f0f8;
    --text2: #8888aa;
    --text3: #555570;
    --accent: #4f8ef7;
    --accent2: #00c8b4;
    --accent3: #a78bfa;
  }}
  *, *::before, *::after {{ box-sizing: border-box; margin: 0; padding: 0; }}
  html {{ scroll-behavior: smooth; }}
  body {{
    background: var(--bg);
    color: var(--text);
    font-family: 'Segoe UI', 'Malgun Gothic', -apple-system, sans-serif;
    line-height: 1.6;
    min-height: 100vh;
  }}
  a {{ color: inherit; text-decoration: none; }}

  /* ── 네비게이션 ── */
  nav {{
    position: sticky; top: 0; z-index: 100;
    display: flex; align-items: center; justify-content: space-between;
    padding: 0 40px; height: 60px;
    background: rgba(8,8,16,0.92);
    backdrop-filter: blur(20px);
    border-bottom: 1px solid var(--border);
  }}
  .nav-logo {{
    display: flex; align-items: center; gap: 10px;
    font-size: 17px; font-weight: 800;
  }}
  .nav-logo-icon {{
    width: 28px; height: 28px;
    background: linear-gradient(135deg, var(--accent), var(--accent2));
    border-radius: 7px;
    display: flex; align-items: center; justify-content: center;
    font-size: 13px;
  }}
  .nav-back {{
    font-size: 13px; color: var(--text2);
    display: flex; align-items: center; gap: 6px;
    transition: color 0.2s;
  }}
  .nav-back:hover {{ color: var(--accent2); }}

  /* ── 헤더 ── */
  .page-header {{
    max-width: 860px; margin: 0 auto;
    padding: 64px 24px 40px;
    border-bottom: 1px solid var(--border);
  }}
  .page-header-label {{
    font-size: 11px; font-weight: 700; letter-spacing: 3px;
    text-transform: uppercase; color: var(--accent2);
    margin-bottom: 14px;
  }}
  .page-header h1 {{
    font-size: 36px; font-weight: 900; letter-spacing: -1px;
    margin-bottom: 12px;
  }}
  .page-header p {{
    font-size: 15px; color: var(--text2);
  }}
  .page-header-meta {{
    margin-top: 20px;
    display: flex; gap: 20px; flex-wrap: wrap;
  }}
  .meta-badge {{
    background: var(--card); border: 1px solid var(--border2);
    border-radius: 8px; padding: 8px 16px;
    font-size: 13px; color: var(--text2);
  }}
  .meta-badge strong {{ color: var(--accent2); }}

  /* ── 릴리즈 목록 ── */
  .changelog-wrap {{
    max-width: 860px; margin: 0 auto;
    padding: 40px 24px 80px;
  }}
  .cl-item {{
    display: flex; gap: 28px;
    padding: 28px 0;
    border-bottom: 1px solid var(--border);
    transition: background 0.2s;
  }}
  .cl-item.cl-latest {{
    background: linear-gradient(90deg, rgba(0,200,180,0.04) 0%, transparent 100%);
    border-radius: 12px;
    padding: 28px 20px;
    margin: 0 -20px;
    border-bottom: none;
    border: 1px solid rgba(0,200,180,0.15);
    margin-bottom: 16px;
  }}
  .cl-left {{
    flex-shrink: 0; width: 100px; text-align: center;
    padding-top: 4px;
  }}
  .cl-ver {{
    display: inline-block;
    padding: 5px 12px; border-radius: 6px;
    font-size: 13px; font-weight: 800; letter-spacing: 0.5px;
    margin-bottom: 8px;
  }}
  .cl-ver.latest {{
    background: rgba(0,200,180,0.15); color: var(--accent2);
    border: 1px solid rgba(0,200,180,0.3);
  }}
  .cl-ver.prev {{
    background: rgba(255,255,255,0.06); color: var(--text2);
    border: 1px solid var(--border);
  }}
  .cl-date {{
    font-size: 11px; color: var(--text3);
  }}
  .cl-content {{ flex: 1; min-width: 0; }}
  .cl-title {{
    font-size: 16px; font-weight: 700;
    margin-bottom: 8px; color: var(--text);
    line-height: 1.4;
  }}
  .cl-item.cl-latest .cl-title {{ color: #fff; font-size: 17px; }}
  .cl-desc {{
    font-size: 13px; color: var(--text2);
    line-height: 1.7; margin-bottom: 12px;
  }}
  .cl-tags {{
    display: flex; flex-wrap: wrap; gap: 6px;
    margin-bottom: 14px;
  }}
  .cl-tag {{
    background: rgba(79,142,247,0.1); color: var(--accent);
    border: 1px solid rgba(79,142,247,0.2);
    border-radius: 4px; padding: 2px 8px;
    font-size: 11px; font-weight: 600;
  }}
  .cl-dl-btns {{
    display: flex; gap: 8px; flex-wrap: wrap;
  }}
  .cl-dl-btn {{
    padding: 6px 14px; border-radius: 6px;
    font-size: 12px; font-weight: 700;
    transition: all 0.2s;
  }}
  .cl-dl-btn.primary {{
    background: var(--accent); color: #fff;
  }}
  .cl-dl-btn.primary:hover {{ background: #3a7de8; }}
  .cl-dl-btn.secondary {{
    background: var(--card2); color: var(--text2);
    border: 1px solid var(--border2);
  }}
  .cl-dl-btn.secondary:hover {{ color: var(--text); border-color: var(--text2); }}

  /* ── 푸터 ── */
  footer {{
    border-top: 1px solid var(--border);
    padding: 32px 40px;
    text-align: center;
    font-size: 12px; color: var(--text3);
  }}
  footer a {{ color: var(--accent2); }}

  @media (max-width: 600px) {{
    nav {{ padding: 0 16px; }}
    .page-header {{ padding: 40px 16px 28px; }}
    .page-header h1 {{ font-size: 26px; }}
    .changelog-wrap {{ padding: 24px 16px 60px; }}
    .cl-item {{ flex-direction: column; gap: 12px; }}
    .cl-left {{ width: auto; text-align: left; display: flex; align-items: center; gap: 12px; }}
    .cl-item.cl-latest {{ margin: 0; padding: 20px 16px; }}
  }}
</style>
</head>
<body>

<nav>
  <div class="nav-logo">
    <div class="nav-logo-icon">🎵</div>
    소리누리
  </div>
  <a href="/" class="nav-back">← 홈으로</a>
</nav>

<div class="page-header">
  <div class="page-header-label">Release Notes</div>
  <h1>릴리즈 노트</h1>
  <p>소리누리의 모든 업데이트 이력을 확인하세요.</p>
  <div class="page-header-meta">
    <div class="meta-badge">최신 버전 <strong>v{current_ver}</strong></div>
    <div class="meta-badge">총 <strong>{len(releases)}개</strong> 릴리즈</div>
    <div class="meta-badge">by <strong>가온 Communication</strong></div>
  </div>
</div>

<div class="changelog-wrap">
{items_html}
</div>

<footer>
  <p>© 2026 가온 Communication. All rights reserved. &nbsp;|&nbsp;
  <a href="/">소리누리 홈</a> &nbsp;|&nbsp;
  <a href="https://github.com/sk1200rt-max/sorinuri-qt">GitHub</a></p>
</footer>

</body>
</html>"""


# ── 랜딩페이지(index.html) 업데이트 ─────────────────────────────────

def update_index_html(html, new_ver, releases, installer_mb, portable_mb):
    """index.html 내 버전·링크·릴리즈 노트 갱신"""
    today = datetime.date.today().strftime("%Y-%m-%d")
    installer_url = f"{DOWNLOAD_BASE_URL}/Sorinuri-Setup-{new_ver}.exe"
    portable_url  = f"{DOWNLOAD_BASE_URL}/Sorinuri-Qt-{new_ver}-Portable.zip"

    # 1. 타이틀·메타
    html = re.sub(
        r'<title>소리누리 by 가온 Communication[^<]*</title>',
        '<title>소리누리 by 가온 Communication — Windows 하이엔드 미디어 플레이어</title>',
        html
    )

    # 2. 네비게이션 버전 배지
    html = re.sub(
        r'<span class="nav-ver">v[\d\.]+</span>',
        f'<span class="nav-ver">v{new_ver}</span>',
        html
    )

    # 3. 히어로 섹션
    html = re.sub(r'v[\d\.]+ 최신 업데이트 출시', f'v{new_ver} 최신 업데이트 출시', html)
    html = re.sub(
        r'href="https://(?:sorinuri\.com/downloads|github\.com/[^"]+/releases/download/[^"]+)/Sorinuri-Setup-[\d\.]+\.exe" class="btn-primary"',
        f'href="{installer_url}" class="btn-primary"', html
    )
    html = re.sub(
        r'href="https://(?:sorinuri\.com/downloads|github\.com/[^"]+/releases/download/[^"]+)/Sorinuri-Qt-[\d\.]+-Portable\.zip" class="btn-secondary"',
        f'href="{portable_url}" class="btn-secondary"', html
    )
    if installer_mb:
        html = re.sub(
            r'(⬇ 인스톨러 다운로드 <span[^>]*>)\d+ MB(</span>)',
            rf'\g<1>{installer_mb} MB\g<2>', html
        )
    if portable_mb:
        html = re.sub(
            r'(📦 포터블 버전 <span[^>]*>)\d+ MB(</span>)',
            rf'\g<1>{portable_mb} MB\g<2>', html
        )
    html = re.sub(
        r'(Windows 10/11 64-bit · 무료 · <span>)v?[\d\.]+(</span>)',
        rf'\g<1>v{new_ver}\g<2>', html
    )

    # 4. 플레이어 미리보기 버전
    html = re.sub(
        r'<span class="preview-badge-sm">v[\d\.]+</span>',
        f'<span class="preview-badge-sm">v{new_ver}</span>', html
    )

    # 5. 다운로드 섹션
    html = re.sub(
        r'href="https://(?:sorinuri\.com/downloads|github\.com/[^"]+/releases/download/[^"]+)/Sorinuri-Setup-[\d\.]+\.exe" class="dl-btn primary"',
        f'href="{installer_url}" class="dl-btn primary"', html
    )
    html = re.sub(
        r'href="https://(?:sorinuri\.com/downloads|github\.com/[^"]+/releases/download/[^"]+)/Sorinuri-Qt-[\d\.]+-Portable\.zip" class="dl-btn secondary"',
        f'href="{portable_url}" class="dl-btn secondary"', html
    )
    if installer_mb:
        html = re.sub(
            r'(<div class="dl-card-size">)\d+ MB( · Windows 10/11 64-bit</div>)',
            rf'\g<1>{installer_mb} MB\g<2>', html
        )
    if portable_mb:
        html = re.sub(
            r'(<div class="dl-card-size">)\d+ MB( · ZIP 압축</div>)',
            rf'\g<1>{portable_mb} MB\g<2>', html
        )
    html = re.sub(
        r'(<div class="dl-ver fade-in">현재 버전: <strong>)v?[\d\.]+(<\/strong> \()[\d\-]+(\)<\/div>)',
        rf'\g<1>v{new_ver}\g<2>{today}\g<3>', html
    )

    # 6. 릴리즈 노트 섹션 — releases 데이터로 완전 재생성
    recent = releases[:MAX_CHANGELOG_ITEMS]
    items_html = ""
    for idx, r in enumerate(recent):
        badge_cls = "latest" if idx == 0 else "prev"
        delay_cls = get_delay_class(idx)
        items_html += f"""      <div class="cl-item {delay_cls}">
        <div class="cl-ver {badge_cls}">v{r['version']}</div>
        <div class="cl-content">
          <div class="cl-title">{r['title']}</div>
          <div class="cl-desc">{r['desc']}</div>
        </div>
      </div>\n"""

    # 아카이브 링크 버튼 추가
    archive_link = (
        '\n    <div style="text-align:center;margin-top:32px;">'
        '<a href="/changelog.html" style="display:inline-flex;align-items:center;gap:8px;'
        'padding:10px 24px;border-radius:8px;border:1px solid rgba(255,255,255,0.12);'
        'color:#8888aa;font-size:13px;transition:all 0.2s;" '
        'onmouseover="this.style.color=\'#f0f0f8\';this.style.borderColor=\'rgba(255,255,255,0.25)\'" '
        'onmouseout="this.style.color=\'#8888aa\';this.style.borderColor=\'rgba(255,255,255,0.12)\'">'
        '📋 전체 릴리즈 노트 보기</a></div>'
    )

    new_changelog_section = (
        f'    <div class="changelog-list">\n{items_html}    </div>'
        f'{archive_link}'
    )

    html = re.sub(
        r'    <div class="changelog-list">.*?</div>\s*(?=\s*</div>\s*</section>)',
        new_changelog_section,
        html,
        flags=re.DOTALL
    )

    # 7. 푸터 다운로드 링크
    html = re.sub(
        r'href="https://sorinuri\.com/downloads/Sorinuri-Setup-[\d\.]+\.exe">인스톨러 v[\d\.]+</a>',
        f'href="{installer_url}">인스톨러 v{new_ver}</a>', html
    )
    html = re.sub(
        r'href="https://sorinuri\.com/downloads/Sorinuri-Qt-[\d\.]+-Portable\.zip">포터블 v[\d\.]+</a>',
        f'href="{portable_url}">포터블 v{new_ver}</a>', html
    )

    return html


# ── 메인 함수 ─────────────────────────────────────────────────────────

def update_landing(new_ver, release_title, release_desc,
                   installer_mb=None, portable_mb=None,
                   server_host="sorinuri.com",
                   server_user="amoross",
                   server_pass="mallgoer2023",
                   web_root="/var/www/sorinuri",
                   publish=False):

    sshpass_ok = shutil.which("sshpass") is not None

    # ── releases_data.json 업데이트 ──────────────────────────────────
    releases = load_releases()
    releases = add_release(releases, new_ver, release_title, release_desc,
                           installer_mb, portable_mb)
    save_releases(releases)
    print(f"   ✅ releases_data.json 업데이트 완료 (총 {len(releases)}개 릴리즈)")

    # ── index.html 다운로드 ──────────────────────────────────────────
    local_index = f"/tmp/landing_{new_ver}.html"
    if sshpass_ok:
        ret = subprocess.run([
            "sshpass", "-p", server_pass,
            "scp", "-o", "StrictHostKeyChecking=no",
            f"{server_user}@{server_host}:{web_root}/index.html",
            local_index
        ], capture_output=True)
        if ret.returncode != 0:
            print(f"   ⚠️  서버 다운로드 실패 → /tmp/current_index.html 사용")
            shutil.copy("/tmp/current_index.html", local_index)
    else:
        print("   ⚠️  sshpass 없음 → /tmp/current_index.html 사용")
        shutil.copy("/tmp/current_index.html", local_index)

    with open(local_index, encoding="utf-8") as f:
        html = f.read()

    m = re.search(r'<span class="nav-ver">v([\d\.]+)</span>', html)
    old_ver = m.group(1) if m else "unknown"
    print(f"   현재 버전: v{old_ver} → v{new_ver}")

    # ── index.html 수정 ───────────────────────────────────────────────
    html = update_index_html(html, new_ver, releases, installer_mb, portable_mb)
    with open(local_index, "w", encoding="utf-8") as f:
        f.write(html)
    print(f"   ✅ index.html 수정 완료")

    # ── changelog.html 생성 ───────────────────────────────────────────
    local_changelog = f"/tmp/changelog_{new_ver}.html"
    changelog_html = build_changelog_html(releases, new_ver)
    with open(local_changelog, "w", encoding="utf-8") as f:
        f.write(changelog_html)
    print(f"   ✅ changelog.html 생성 완료 ({len(releases)}개 릴리즈)")

    # ── 서버 업로드 ───────────────────────────────────────────────────
    # 버전 범프 단계에서는 운영 페이지를 바꾸지 않는다. 실제 업로드는
    # 태그 릴리즈의 sorinuri-production 승인 뒤 publish=True로만 허용된다.
    if publish and sshpass_ok:
        files_to_upload = [
            (local_index,     f"{web_root}/index.html"),
            (local_changelog, f"{web_root}/changelog.html"),
            (RELEASES_JSON,   f"{web_root}/releases.json"),
        ]
        all_ok = True
        for src, dst in files_to_upload:
            ret = subprocess.run([
                "sshpass", "-p", server_pass,
                "scp", "-o", "StrictHostKeyChecking=no",
                src, f"{server_user}@{server_host}:{dst}"
            ], capture_output=True)
            if ret.returncode == 0:
                print(f"   ✅ 업로드: {dst}")
            else:
                print(f"   ❌ 업로드 실패: {dst} — {ret.stderr.decode()}")
                all_ok = False
        if not all_ok:
            print(f"\n   수동 업로드:")
            for src, dst in files_to_upload:
                print(f"   scp {src} {server_user}@{server_host}:{dst}")
    elif publish:
        print("\n   ❌ 승인된 서버 게시에 필요한 sshpass가 없습니다.")
    else:
        print("   ⏸️  랜딩페이지 게시 보류: production 승인 뒤에만 업로드합니다.")

    return local_index, local_changelog


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="소리누리 랜딩페이지 + 아카이브 자동 업데이트")
    parser.add_argument("version",       help="새 버전 번호 (예: 6.12.0)")
    parser.add_argument("title",         help="릴리즈 제목")
    parser.add_argument("description",   help="릴리즈 상세 설명")
    parser.add_argument("--installer-mb", type=int, default=None)
    parser.add_argument("--portable-mb",  type=int, default=None)
    parser.add_argument("--publish", action="store_true",
                        help="production 승인 뒤에만 서버 업로드 수행")
    args = parser.parse_args()

    print(f"\n🚀 소리누리 랜딩페이지 + 아카이브 업데이트: v{args.version}")
    update_landing(
        new_ver=args.version,
        release_title=args.title,
        release_desc=args.description,
        installer_mb=args.installer_mb,
        portable_mb=args.portable_mb,
        publish=args.publish,
    )
    print("\n✅ 모든 업데이트 완료!")
    print(f"   🌐 https://sorinuri.com/changelog.html")
