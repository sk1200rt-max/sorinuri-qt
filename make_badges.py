"""
오디오 포맷 배지 SVG 생성
- 실제 플레이어에서 사용하는 스타일의 깔끔한 배지
- 각 포맷별 고유 색상과 스타일
"""
import os

BADGE_DIR = "resources/badges"
os.makedirs(BADGE_DIR, exist_ok=True)

def badge(text, sub, fg, bg, width=120, height=36):
    """텍스트 기반 배지 SVG"""
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">
  <rect width="{width}" height="{height}" rx="3" fill="{bg}"/>
  <text x="{width//2}" y="{14 if sub else 22}" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="13" font-weight="700" 
        letter-spacing="0.5" fill="{fg}">{text}</text>
  {"" if not sub else f'<text x="{width//2}" y="28" text-anchor="middle" font-family="Arial, Helvetica, sans-serif" font-size="8" font-weight="400" letter-spacing="1.5" fill="{fg}" opacity="0.7">{sub}</text>'}
</svg>'''

def badge_dolby_atmos(width=140, height=36):
    """Dolby Atmos 스타일 배지"""
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">
  <rect width="{width}" height="{height}" rx="3" fill="#000033"/>
  <rect x="1" y="1" width="{width-2}" height="{height-2}" rx="2" fill="none" stroke="#3355aa" stroke-width="0.5"/>
  <text x="{width//2}" y="14" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="11" font-weight="700" 
        letter-spacing="0.5" fill="#ffffff">DOLBY</text>
  <text x="{width//2}" y="28" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="10" font-weight="400" 
        letter-spacing="2" fill="#aabbff">ATMOS</text>
</svg>'''

def badge_truehd(width=130, height=36):
    """Dolby TrueHD 스타일 배지"""
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">
  <rect width="{width}" height="{height}" rx="3" fill="#001133"/>
  <rect x="1" y="1" width="{width-2}" height="{height-2}" rx="2" fill="none" stroke="#2244aa" stroke-width="0.5"/>
  <text x="{width//2}" y="14" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="11" font-weight="700" 
        letter-spacing="0.5" fill="#ffffff">DOLBY</text>
  <text x="{width//2}" y="28" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="9" font-weight="400" 
        letter-spacing="1.5" fill="#88aaff">TrueHD</text>
</svg>'''

def badge_dts_hd(width=130, height=36):
    """DTS-HD Master Audio 스타일 배지"""
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">
  <rect width="{width}" height="{height}" rx="3" fill="#001a00"/>
  <rect x="1" y="1" width="{width-2}" height="{height-2}" rx="2" fill="none" stroke="#004400" stroke-width="0.5"/>
  <text x="{width//2}" y="14" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="13" font-weight="900" 
        letter-spacing="1" fill="#ffffff">DTS-HD</text>
  <text x="{width//2}" y="28" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="8" font-weight="400" 
        letter-spacing="1" fill="#44cc44">MASTER AUDIO</text>
</svg>'''

def badge_dts_x(width=100, height=36):
    """DTS:X 스타일 배지"""
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">
  <rect width="{width}" height="{height}" rx="3" fill="#001a00"/>
  <rect x="1" y="1" width="{width-2}" height="{height-2}" rx="2" fill="none" stroke="#004400" stroke-width="0.5"/>
  <text x="{width//2}" y="14" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="13" font-weight="900" 
        letter-spacing="1" fill="#ffffff">DTS</text>
  <text x="{width//2}" y="28" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="11" font-weight="700" 
        letter-spacing="2" fill="#44cc44">:X</text>
</svg>'''

def badge_dts(width=80, height=36):
    """DTS 스타일 배지"""
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">
  <rect width="{width}" height="{height}" rx="3" fill="#001a00"/>
  <rect x="1" y="1" width="{width-2}" height="{height-2}" rx="2" fill="none" stroke="#004400" stroke-width="0.5"/>
  <text x="{width//2}" y="23" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="15" font-weight="900" 
        letter-spacing="2" fill="#ffffff">DTS</text>
</svg>'''

def badge_dd_plus(width=90, height=36):
    """Dolby Digital+ 스타일 배지"""
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">
  <rect width="{width}" height="{height}" rx="3" fill="#000033"/>
  <rect x="1" y="1" width="{width-2}" height="{height-2}" rx="2" fill="none" stroke="#3355aa" stroke-width="0.5"/>
  <text x="{width//2}" y="14" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="11" font-weight="700" 
        letter-spacing="0.5" fill="#ffffff">DOLBY</text>
  <text x="{width//2}" y="28" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="9" font-weight="400" 
        letter-spacing="1" fill="#aabbff">DIGITAL+</text>
</svg>'''

def badge_dd(width=110, height=36):
    """Dolby Digital 스타일 배지"""
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">
  <rect width="{width}" height="{height}" rx="3" fill="#000033"/>
  <rect x="1" y="1" width="{width-2}" height="{height-2}" rx="2" fill="none" stroke="#3355aa" stroke-width="0.5"/>
  <text x="{width//2}" y="14" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="11" font-weight="700" 
        letter-spacing="0.5" fill="#ffffff">DOLBY</text>
  <text x="{width//2}" y="28" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="9" font-weight="400" 
        letter-spacing="1" fill="#aabbff">DIGITAL</text>
</svg>'''

def badge_pcm(width=80, height=36):
    """PCM 배지"""
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">
  <rect width="{width}" height="{height}" rx="3" fill="#1a1a1a"/>
  <rect x="1" y="1" width="{width-2}" height="{height-2}" rx="2" fill="none" stroke="#333" stroke-width="0.5"/>
  <text x="{width//2}" y="14" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="11" font-weight="700" 
        letter-spacing="1" fill="#cccccc">PCM</text>
  <text x="{width//2}" y="28" text-anchor="middle" 
        font-family="Arial, Helvetica, sans-serif" font-size="8" font-weight="400" 
        letter-spacing="1" fill="#888888">LOSSLESS</text>
</svg>'''

badges = {
    "dolby-atmos": badge_dolby_atmos(),
    "truehd":      badge_truehd(),
    "dts-hd":      badge_dts_hd(),
    "dts-x":       badge_dts_x(),
    "dts":         badge_dts(),
    "dd-plus":     badge_dd_plus(),
    "dd":          badge_dd(),
    "pcm":         badge_pcm(),
}

for name, content in badges.items():
    path = os.path.join(BADGE_DIR, f"{name}.svg")
    with open(path, "w") as f:
        f.write(content)
    print(f"생성: {path}")

print(f"\n총 {len(badges)}개 배지 생성 완료")
