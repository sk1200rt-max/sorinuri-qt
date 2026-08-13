#!/usr/bin/env python3
"""
소리누리 버전 일괄 업데이트 스크립트
사용법: python3 scripts/bump_version.py <버전> --scope <audio|renderer|ui|installer> --purpose "변경 목적" [--title "제목"] [--desc "설명"] [--installer-mb N] [--portable-mb N]

예시:
  python3 scripts/bump_version.py 6.12.0 \
    --title "오디오 핫플러그 · 외부 모니터 복원 · Intel HEVC 안전 폴백" \
    --desc "헤드폰 연결/해제 시 오디오 자동 재초기화, 외부 모니터 검은 화면 수정, Intel 구형 GPU HEVC 프리즈 방지" \
    --installer-mb 92 --portable-mb 116

수정 대상:
  1. CMakeLists.txt  - MyAppVersion, project VERSION
  2. src/UpdateChecker.h - currentVersion()
  3. .github/workflows/build-windows.yml - APP_VERSION (2곳)
  4. installer/sorinuri-setup.iss - AppVersion (있는 경우)
  5. resources/sorinuri.rc - FILEVERSION, PRODUCTVERSION, FileVersion, ProductVersion
  6. src/main.cpp - app.setApplicationVersion()
  7. sorinuri.com/index.html - 버전, 다운로드 링크, 릴리즈 노트 (--title/--desc 제공 시)
"""
import sys, re, os, argparse, json


def bump(new_ver, scope, purpose):
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    changed = []

    # 1. CMakeLists.txt
    path = os.path.join(root, 'CMakeLists.txt')
    c = open(path).read()
    c2 = re.sub(r'(MyAppVersion\s+)"[\d\.]+"', lambda m: m.group(1) + f'"{new_ver}"', c)
    c2 = re.sub(r'(project\(Sorinuri VERSION\s+)[\d\.]+', lambda m: m.group(1) + new_ver, c2)
    if c2 != c:
        open(path, 'w').write(c2)
        changed.append('CMakeLists.txt')

    # 2. UpdateChecker.h
    path = os.path.join(root, 'src', 'UpdateChecker.h')
    c = open(path).read()
    c2 = re.sub(r'(currentVersion\(\) \{ return )"[\d\.]+"', lambda m: m.group(1) + f'"{new_ver}"', c)
    if c2 != c:
        open(path, 'w').write(c2)
        changed.append('src/UpdateChecker.h')

    # 3. build-windows.yml
    path = os.path.join(root, '.github', 'workflows', 'build-windows.yml')
    c = open(path).read()
    c2 = re.sub(r'(APP_VERSION:\s*)"[\d\.]+"', lambda m: m.group(1) + f'"{new_ver}"', c)
    if c2 != c:
        open(path, 'w').write(c2)
        changed.append('.github/workflows/build-windows.yml')

    # 4. installer/sorinuri-setup.iss (있는 경우)
    path = os.path.join(root, 'installer', 'sorinuri-setup.iss')
    if os.path.exists(path):
        c = open(path).read()
        c2 = re.sub(r'(#define MyAppVersion\s+")[\d\.]+"', lambda m: m.group(1) + new_ver + '"', c)
        c2 = re.sub(r'(AppVersion=)[\d\.]+', lambda m: m.group(1) + new_ver, c2)
        c2 = re.sub(r'(OutputBaseFilename=Sorinuri-Setup-)[\d\.]+', lambda m: m.group(1) + new_ver, c2)
        if c2 != c:
            open(path, 'w').write(c2)
            changed.append('installer/sorinuri-setup.iss')

    # 5. resources/sorinuri.rc
    path = os.path.join(root, 'resources', 'sorinuri.rc')
    if os.path.exists(path):
        c = open(path).read()
        rc_ver = new_ver.replace('.', ',') + ',0'
        c2 = re.sub(r'(FILEVERSION\s+)[\d,]+', lambda m: m.group(1) + rc_ver, c)
        c2 = re.sub(r'(PRODUCTVERSION\s+)[\d,]+', lambda m: m.group(1) + rc_ver, c2)
        str_ver = new_ver + '.0'
        c2 = re.sub(r'(VALUE "FileVersion",\s+")[\d\.]+', lambda m: m.group(1) + str_ver, c2)
        c2 = re.sub(r'(VALUE "ProductVersion",\s+")[\d\.]+', lambda m: m.group(1) + str_ver, c2)
        if c2 != c:
            open(path, 'w').write(c2)
            changed.append('resources/sorinuri.rc')

    # 변경 범위 선언: CI 릴리즈 게이트가 이전 태그 이후의 제품 코드 변경을
    # 이 영역으로 한정해, 지시 밖 기능이 섞인 배포를 차단한다.
    scope_dir = os.path.join(root, '.release')
    os.makedirs(scope_dir, exist_ok=True)
    scope_path = os.path.join(scope_dir, 'scope.json')
    with open(scope_path, 'w', encoding='utf-8') as f:
        json.dump({
            'version': new_ver,
            'domain': scope,
            'purpose': purpose,
        }, f, ensure_ascii=False, indent=2)
        f.write('\n')
    changed.append('.release/scope.json')

    # 6. src/main.cpp
    path = os.path.join(root, 'src', 'main.cpp')
    if os.path.exists(path):
        c = open(path).read()
        c2 = re.sub(r'(app\.setApplicationVersion\(")[\d\.]+"', lambda m: m.group(1) + new_ver + '"', c)
        if c2 != c:
            open(path, 'w').write(c2)
            changed.append('src/main.cpp')

    if changed:
        print(f'✅ 버전 {new_ver}으로 업데이트 완료:')
        for f in changed:
            print(f'   - {f}')
    else:
        print('변경된 파일 없음 (이미 해당 버전이거나 패턴 불일치)')

    return changed


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='소리누리 버전 일괄 업데이트')
    parser.add_argument('version',          help='새 버전 번호 (예: 6.12.0)')
    parser.add_argument('--title',          default=None, help='릴리즈 제목 (랜딩페이지 업데이트 시 필요)')
    parser.add_argument('--desc',           default=None, help='릴리즈 상세 설명 (랜딩페이지 업데이트 시 필요)')
    parser.add_argument('--installer-mb',   type=int, default=None, help='인스톨러 파일 크기(MB)')
    parser.add_argument('--portable-mb',    type=int, default=None, help='포터블 파일 크기(MB)')
    parser.add_argument('--no-landing',     action='store_true', help='랜딩페이지 업데이트 건너뜀')
    parser.add_argument('--scope', required=True,
                        help='이번 릴리즈에서 변경할 제품 영역. 관련 영역을 함께 수정할 때는 쉼표로 구분 (예: audio,installer)')
    parser.add_argument('--purpose', required=True,
                        help='사용자 지시와 연결되는 이번 릴리즈의 단일 변경 목적')
    args = parser.parse_args()

    # 소스 코드 버전 범프 및 CI용 변경 범위 선언
    bump(args.version, args.scope, args.purpose)

    # 랜딩페이지 자동 업데이트
    if not args.no_landing:
        if args.title and args.desc:
            print(f'\n🌐 랜딩페이지 자동 업데이트 시작...')
            try:
                # update_landing.py를 같은 scripts/ 디렉토리에서 import
                scripts_dir = os.path.dirname(os.path.abspath(__file__))
                sys.path.insert(0, scripts_dir)
                from update_landing import update_landing
                update_landing(
                    new_ver=args.version,
                    release_title=args.title,
                    release_desc=args.desc,
                    installer_mb=args.installer_mb,
                    portable_mb=args.portable_mb,
                )
                print('✅ 랜딩페이지 업데이트 완료!')
            except Exception as e:
                print(f'⚠️  랜딩페이지 업데이트 실패: {e}')
                print('   수동으로 python3 scripts/update_landing.py 를 실행하세요.')
        else:
            print('\n💡 랜딩페이지 업데이트를 건너뜁니다.')
            print('   --title 과 --desc 옵션을 추가하면 자동으로 업데이트됩니다.')
            print(f'   예시: python3 scripts/bump_version.py {args.version} \\')
            print(f'           --title "릴리즈 제목" \\')
            print(f'           --desc "릴리즈 상세 설명" \\')
            print(f'           --installer-mb 92 --portable-mb 116')
