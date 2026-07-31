# v5.0.0 UI 버그 분석 메모

## 관찰 내용

### 1. 전문기능(P키) 닫기 동작
- 현재 `ProFeaturesWidget` 내부에는 패널 자체를 닫는 전용 버튼이나 시그널이 없음.
- `MainWindow`에서는 `P` 키가 `toggleProFeatures()`를 호출하여 패널 표시/숨김을 토글함.
- 사용자가 말한 "오른쪽 상단 닫기"는 패널 전용 닫기가 아니라 앱 타이틀바 우측 닫기 버튼으로 인식될 가능성이 높음.
- 따라서 **전문기능 패널 내부 우상단에 별도의 닫기 버튼을 제공**하고, 이 버튼은 앱 종료가 아니라 패널 숨김만 수행하도록 분리하는 것이 맞음.

### 2. 타임라인 위 얇은 라인
- `ControlBar` 전체에 `border-top: 1px solid #1a1a1a;`가 걸려 있음.
- 스크린샷에서 보이는 얇은 선은 재생 타임라인 자체 위 라인이라기보다, 컨트롤바 최상단 경계선이 영상과 맞닿는 위치에 그려지는 현상일 가능성이 높음.
- 또한 `mainLayout->setContentsMargins(8, 4, 8, 4)`와 `seekSlider` 핸들 margin이 섞이면서 시각적으로 선 정렬이 어긋나 보일 수 있음.
- 해결 방향: **컨트롤바 border-top 제거**, 대신 seek 영역 전용 상단 여백/정렬을 재조정.

### 3. 설정창 버튼 외곽선 잘림
- `SettingsDialog` 하단 버튼(`적용`, `확인`, `취소`)은 `btnWidget` 안에 들어가지만 생성 parent가 `this`임.
- 스타일시트에서 버튼 border와 hover outline이 있는데, 버튼 행 여백이 충분하지 않거나 parent/레이아웃 조합 때문에 위아래가 눌려 보일 수 있음.
- 해결 방향: 버튼 parent를 `btnWidget`으로 일치시키고, 버튼 최소 높이와 하단 바 padding/margin을 재조정.

## 수정 원칙
- 기존 재생/설정/자막/전문기능 로직은 건드리지 않음.
- 새 패널 닫기 버튼은 `ProFeaturesWidget` 독립 시그널로만 추가.
- 타이틀바 닫기 버튼(앱 종료)은 기존 동작 유지.
- ControlBar는 기능 변경 없이 레이아웃/스타일만 조정.
- SettingsDialog는 버튼 영역 parent/layout/style만 정밀 조정.
