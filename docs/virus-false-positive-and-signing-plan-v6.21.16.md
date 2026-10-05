# 소리누리 `sorinuri.exe` 바이러스 오인식 대응 및 코드서명 계획

작성 기준: 2026-10-06, v6.21.16 준비 단계

## 1. 현재 확인된 문제

- `sorinuri.exe` 또는 설치 파일이 일부 Windows 보안 제품에서 바이러스·PUA·신뢰할 수 없는 게시자로 분류될 수 있음.
- 현재 릴리스 workflow에는 Windows runner에서 `New-SelfSignedCertificate`로 단기 자체 서명을 생성하는 구형 단계가 남아 있음.
- 자체 서명 인증서는 파일 변조 여부 확인에는 사용할 수 있지만, Windows Defender SmartScreen이나 일반 백신 제품이 신뢰하는 **공인 게시자 신원**을 제공하지 않음.
- Microsoft Store용 MSIX는 Partner Center 제출 과정에서 Microsoft Store 서명으로 재서명되므로, 일반 설치용 Inno Setup EXE의 서명 문제와는 별도임.

## 2. 권장 인증서 선택

### 권장 1: Microsoft Artifact Signing

GitHub Actions에서 빌드하는 현재 구조에는 Microsoft Artifact Signing(구 Trusted Signing)이 가장 적합합니다.

- Azure 기반의 관리형 코드서명 서비스
- GitHub Actions OIDC 인증 사용 가능
- 개인키를 GitHub Secrets나 저장소에 저장하지 않음
- 공개적으로 검증 가능한 서명 체인과 RFC 3161 타임스탬프 사용
- CI에서 설치 EXE, portable 실행 파일 및 배포 대상 실행 파일을 일관되게 서명 가능

필요한 설정:

- Azure Artifact Signing 계정
- 인증서 프로필
- 서명 리소스가 위치한 엔드포인트(예: Korea Central 또는 실제 생성 리전)
- GitHub Actions OIDC용 Azure 앱 등록
- 해당 앱에 Artifact Signing 서명 권한 부여
- GitHub Repository Variables/Secrets에 다음 값 등록
  - `ARTIFACT_SIGNING_ENDPOINT`
  - `ARTIFACT_SIGNING_ACCOUNT`
  - `ARTIFACT_SIGNING_PROFILE`
  - `AZURE_CLIENT_ID`
  - `AZURE_TENANT_ID`
  - `AZURE_SUBSCRIPTION_ID`

### 대안 2: 공인 OV/EV Authenticode 인증서

공인 코드서명 사업자에서 발급받은 OV 또는 EV 인증서를 사용할 수 있습니다.

- 개인키는 반드시 하드웨어 토큰 또는 클라우드 HSM에 보관
- `.pfx`와 개인키를 GitHub 저장소·artifact·배포 서버에 저장하지 않음
- CI에는 서명 서비스 또는 HSM 연동 방식만 사용
- Authenticode 서명에는 SHA-256 파일 해시와 SHA-256 RFC 3161 타임스탬프 사용

EV 인증서도 SmartScreen 경고가 즉시 사라진다는 보장은 없습니다. 새 파일·새 인증서·새 배포 URL은 초기 평판이 낮을 수 있습니다.

### 사용 금지: 자체 서명 인증서

다음 방식은 정식 고객 배포에 사용하지 않습니다.

- `New-SelfSignedCertificate`로 CI에서 매번 생성하는 인증서
- 자체 서명 인증서가 포함된 설치 패키지
- 사용자가 Trusted Root/Trusted People에 인증서를 수동 설치해야 하는 배포
- 자체 서명 MSIX를 Microsoft Store 또는 GitHub Release 고객 파일로 재사용

자체 서명은 담당자 PC의 격리된 로컬 테스트에만 사용합니다.

## 3. 서명 대상과 순서

### 일반 설치본 및 portable 배포

1. Windows Release 빌드
2. portable 폴더의 실행 코드 서명
3. 서명된 portable 파일로 Inno Setup 설치 EXE 생성
4. 설치 EXE 서명
5. 서명 검증
   - `signtool verify /pa /all /v`
   - 서명자, 인증서 체인, 타임스탬프, SHA-256 확인
6. 서명된 파일의 SHA-256 manifest 생성
7. ZIP과 설치 번들을 함께 배포

설치 EXE만 서명하고 내부의 `Sorinuri.exe`, `ffmpeg.exe`, `yt-dlp.exe`, DLL을 서명하지 않는 방식은 신뢰성 측면에서 불충분할 수 있습니다. 배포에 포함하는 외부 실행 파일은 공급자 공식 서명을 보존하고, 자체 산출물은 가능한 범위에서 동일한 정책으로 서명합니다.

### Microsoft Store MSIX

1. Partner Center identity를 사용해 unsigned MSIX 생성
2. Partner Center에 업로드
3. 패키지 분석 완료 확인
4. Partner Center에서 제출
5. Microsoft Store가 패키지를 재서명·배포

Store 패키지에는 자체 서명 인증서를 넣지 않습니다. Store용 MSIX를 일반 웹 다운로드용 설치 파일로 사용하지 않습니다.

## 4. 바이러스 오인식 대응 절차

1. 공인 서명된 새 빌드를 별도 파일명으로 생성합니다.
2. SHA-256 해시를 기록합니다.
3. Windows Defender에서 탐지명, 탐지 경로, 파일 해시를 확인합니다.
4. Microsoft Security Intelligence의 파일 제출 페이지에서 **Software developer**로 false positive 샘플을 제출합니다.
5. 사용 중인 다른 백신 제품에도 동일한 해시와 서명 정보로 오탐지 재검토를 요청합니다.
6. 결과를 릴리스 기록에 남깁니다.

제출 자료:

- 정확한 파일명과 버전
- SHA-256
- Authenticode 서명자와 인증서 체인
- 다운로드 URL
- 탐지 제품명과 탐지명
- 탐지 시각 및 Windows 보안 이벤트 정보
- 해당 파일이 수행하는 기능과 외부 프로세스 실행 여부
- 재현에 필요한 최소 실행 절차

VirusTotal 결과는 참고 자료로만 사용하고, 공인 서명·Microsoft 검토 결과를 대체하지 않습니다.

## 5. SmartScreen 경고에 대한 현실적인 설명

공인 인증서를 사용해도 새 프로그램은 초기 다운로드 평판이 낮아 SmartScreen 경고가 남을 수 있습니다. 다음 조치를 함께 유지해야 합니다.

- 동일한 게시자 인증서와 일관된 제품 이름 사용
- 릴리스마다 파일을 불필요하게 재패킹하지 않기
- HTTPS 다운로드만 제공
- GitHub Release와 공식 사이트의 해시 일치
- 서명된 설치 파일과 명확한 릴리스 노트 제공
- Microsoft false-positive 검토 요청
- 사용자에게 임의의 보안 기능 해제나 Defender 예외 등록을 안내하지 않기

## 6. v6.21.16 진행 차단 사항

현재 v6.21.16 로컬 커밋은 생성됐으나, GitHub push가 다음 오류로 거부되었습니다.

```text
remote: Permission to sk1200rt-max/sorinuri-qt.git denied to sk1200rt-max.
fatal: unable to access ... 403
```

따라서 GitHub Actions 빌드와 Partner Center 제출은 아직 시작되지 않았습니다. 인증서 설정이 없는 상태에서 자체 서명 파일을 정식 고객 배포본으로 만들거나, 빌드·스토어 제출이 완료됐다고 보고하지 않습니다.

## 7. 완료 조건

- [ ] Git push 권한 복구
- [ ] v6.21.16 build-windows workflow 성공
- [ ] Store MSIX `6.21.16.0` 생성 및 SHA-256 기록
- [ ] Partner Center 새 submission에 MSIX 업로드
- [ ] 패키지 분석 완료
- [ ] 인증 제출 완료
- [ ] 일반 설치본에 공인 코드서명 적용
- [ ] `signtool verify /pa /all /v` 통과
- [ ] Microsoft false-positive 제출
- [ ] 공개 Store 페이지 및 설치본 다운로드 검증
