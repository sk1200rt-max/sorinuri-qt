"""
소리누리 플레이어 아이콘 세트 생성
- 24x24 SVG, 선 굵기 1.5px, 색상 #aaaaaa
- 통일된 스타일: 둥근 끝, 심플한 라인 아이콘
"""
import os

ICON_DIR = "resources/icons"
os.makedirs(ICON_DIR, exist_ok=True)

# SVG 템플릿 (24x24 뷰박스)
def svg(content, size=24):
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size} {size}" width="{size}" height="{size}">
  <g stroke="#aaaaaa" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" fill="none">
    {content}
  </g>
</svg>'''

icons = {
    # 재생
    "play": svg('<polygon points="6,4 20,12 6,20" fill="#aaaaaa" stroke="none"/>'),

    # 일시정지
    "pause": svg('<line x1="7" y1="4" x2="7" y2="20"/><line x1="17" y1="4" x2="17" y2="20"/>'),

    # 정지
    "stop": svg('<rect x="5" y="5" width="14" height="14" rx="1" fill="#aaaaaa" stroke="none"/>'),

    # 이전 트랙
    "prev": svg('<polygon points="7,12 16,5 16,19" fill="#aaaaaa" stroke="none"/><line x1="5" y1="5" x2="5" y2="19"/>'),

    # 다음 트랙
    "next": svg('<polygon points="17,12 8,5 8,19" fill="#aaaaaa" stroke="none"/><line x1="19" y1="5" x2="19" y2="19"/>'),

    # 파일 열기 (폴더)
    "open": svg('<path d="M3 7 Q3 5 5 5 L10 5 L12 7 L19 7 Q21 7 21 9 L21 18 Q21 20 19 20 L5 20 Q3 20 3 18 Z"/>'),

    # 볼륨
    "volume": svg('<polygon points="4,9 4,15 8,15 14,19 14,5 8,9" fill="#aaaaaa" stroke="none"/><path d="M17 9 Q20 12 17 15"/><path d="M19 6 Q24 12 19 18"/>'),

    # 볼륨 음소거
    "mute": svg('<polygon points="4,9 4,15 8,15 14,19 14,5 8,9" fill="#aaaaaa" stroke="none"/><line x1="18" y1="9" x2="23" y2="15"/><line x1="23" y1="9" x2="18" y2="15"/>'),

    # 전체화면
    "fullscreen": svg('<polyline points="3,9 3,3 9,3"/><polyline points="15,3 21,3 21,9"/><polyline points="21,15 21,21 15,21"/><polyline points="9,21 3,21 3,15"/>'),

    # 전체화면 종료
    "fullscreen_exit": svg('<polyline points="9,3 3,3 3,9"/><polyline points="15,3 21,3 21,9"/><polyline points="3,15 3,21 9,21"/><polyline points="21,15 21,21 15,21"/><line x1="9" y1="3" x2="9" y2="9"/><line x1="3" y1="9" x2="9" y2="9"/>'),

    # 설정 (기어)
    "settings": svg('''<circle cx="12" cy="12" r="3"/>
    <path d="M12 2 L13.5 5.5 L17 4 L18 7.5 L21.5 8 L20 11.5 L22 14 L19 15.5 L19.5 19 L16 19.5 L14.5 22 L12 20.5 L9.5 22 L8 19.5 L4.5 19 L5 15.5 L2 14 L4 11.5 L2.5 8 L6 7.5 L7 4 L10.5 5.5 Z"/>'''),

    # 자막
    "subtitle": svg('<rect x="2" y="5" width="20" height="14" rx="2"/><line x1="5" y1="11" x2="19" y2="11"/><line x1="5" y1="15" x2="13" y2="15"/>'),

    # 오디오 트랙
    "audio": svg('<circle cx="12" cy="12" r="4"/><path d="M12 2 A10 10 0 0 1 22 12"/><path d="M12 2 A10 10 0 0 0 2 12"/>'),

    # 재생목록
    "playlist": svg('<line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="18" x2="21" y2="18"/>'),

    # 최소화
    "minimize": svg('<line x1="5" y1="12" x2="19" y2="12"/>'),

    # 최대화
    "maximize": svg('<rect x="4" y="4" width="16" height="16" rx="1"/>'),

    # 창 복원
    "restore": svg('<rect x="7" y="4" width="13" height="13" rx="1"/><path d="M4 7 L4 20 L17 20"/>'),

    # 닫기
    "close": svg('<line x1="5" y1="5" x2="19" y2="19"/><line x1="19" y1="5" x2="5" y2="19"/>'),

    # 전체화면 토글 (타이틀바용)
    "expand": svg('<polyline points="3,9 3,3 9,3"/><polyline points="15,3 21,3 21,9"/><polyline points="21,15 21,21 15,21"/><polyline points="9,21 3,21 3,15"/>'),
}

for name, content in icons.items():
    path = os.path.join(ICON_DIR, f"{name}.svg")
    with open(path, "w") as f:
        f.write(content)
    print(f"생성: {path}")

print(f"\n총 {len(icons)}개 아이콘 생성 완료")
