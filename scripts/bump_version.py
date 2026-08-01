#!/usr/bin/env python3
"""
소리누리 버전 일괄 업데이트 스크립트
사용법: python3 scripts/bump_version.py 6.1.0

수정 대상:
  1. CMakeLists.txt  - MyAppVersion, project VERSION
  2. src/UpdateChecker.h - currentVersion()
  3. .github/workflows/build-windows.yml - APP_VERSION (2곳)
  4. installer/sorinuri-setup.iss - AppVersion (있는 경우)
"""
import sys, re, os

def bump(new_ver):
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
        # #define MyAppVersion "x.x.x" 패턴 (주요 패턴)
        c2 = re.sub(r'(#define MyAppVersion\s+")[\d\.]+"', lambda m: m.group(1) + new_ver + '"', c)
        # AppVersion=x.x.x 패턴 (혹시 있을 경우)
        c2 = re.sub(r'(AppVersion=)[\d\.]+', lambda m: m.group(1) + new_ver, c2)
        # OutputBaseFilename=Sorinuri-Setup-x.x.x 패턴
        c2 = re.sub(r'(OutputBaseFilename=Sorinuri-Setup-)[\d\.]+', lambda m: m.group(1) + new_ver, c2)
        if c2 != c:
            open(path, 'w').write(c2)
            changed.append('installer/sorinuri-setup.iss')

    if changed:
        print(f'✅ 버전 {new_ver}으로 업데이트 완료:')
        for f in changed:
            print(f'   - {f}')
    else:
        print('변경된 파일 없음 (이미 해당 버전이거나 패턴 불일치)')

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('사용법: python3 scripts/bump_version.py <버전번호>')
        print('예시:   python3 scripts/bump_version.py 6.1.0')
        sys.exit(1)
    bump(sys.argv[1])
