# v4.4.0 진행 상황 (컨텍스트 보존용)

## 현재 미커밋 변경사항 (git status로 확인 가능)
### 완료된 코드 수정
1. **MpvWidget.cpp**: paintGL의 mpv_opengl_fbo에 internal_format=0x8058(GL_RGBA8) 지정 → INVALID_ENUM 반복 오류 해결
2. **MpvCore.h/.cpp**:
   - `audioDeviceList()` 추가: audio-device-list를 QVariantList {name, description}로 반환
   - `deviceLikelySupportsPassthrough()` 추가: 장치 description에 hdmi/digital/spdif/receiver/nvidia high definition 등 키워드가 있으면 패스스루 유지, 없으면(USB DAC 등) 미지원 판단
   - `loadFile()`에서 !append 시 사전 감지: 지원이면 spdif 복원(passthroughEnabled_&&spdifCodecs_), 미지원이면 spdif=""
   - `passthroughEnabled_`, `spdifCodecs_` 멤버 추가. setAudioPassthrough/setSpdifCodecs가 이 멤버 갱신
   - (기존 유지) AO 실패 로그 감지 → spdif 해제+ao-reload 자동복구, FILE_LOADED 2초 워치독
3. **SettingsDialog.cpp**:
   - refreshAudioDevices(): mpv_->audioDeviceList() 실제 파싱해 WASAPI 장치 콤보 구성, audio/device 설정 복원
   - applyToMpv(): setAudioDevice(currentData) 적용, setAudioPassthrough(true/false) 호출
   - onApply(): audio/device 저장
4. **AlbumArtExtractor.h/.cpp 신규**: ID3v2 APIC / FLAC PICTURE / MP4 covr 파싱해 내장 앨범아트 추출
5. **MainWindow.cpp**:
   - include AlbumArtExtractor.h
   - loadMusicMeta: 내장 앨범아트 추출 우선, 폴더 커버 폴백. bitDepth를 audio-params/format에서 실측
   - switchToMusicMode: gapless-audio=yes 적용, 중복 controlBar_->hide() 제거
   - switchToVideoMode: 중복 controlBar_->show() 제거

### 남은 작업 (v4.4.0)
- [ ] B5: 비트퍼펙트 실시간 표시 - MusicWidget에 setOutputInfo(QString) 같은 메서드 추가, MainWindow의 audioFormatChanged에서 audio-out-params(samplerate/format) + audio-exclusive 여부 statusBar_에 반영. statusBar_는 MusicWidget.h:168 QLabel. loadMeta의 status 문자열은 "BIT-PERFECT" 하드코딩 → 실제 출력과 비교해 표시하도록
- [ ] B2: LyricsWidget loadForTrack에 filePath 전달 (MusicWidget.cpp:349에서 ""로 전달 중 → currentMeta_에 filePath 추가 필요하거나 MusicMeta에 filePath 필드 추가)
- [ ] C1: 이어보기 - MainWindow에 위치 저장/복원. QSettings "resume/<hash>" = position. onFileLoaded에서 5s<pos<95% 시 seek. 종료/파일전환 시 저장. settings general/remember_pos 체크 반영
- [ ] C2: 최근 파일 - QSettings "recent/files" QStringList, 컨트롤바 파일 버튼에 QMenu 또는 타이틀바. 간단히: onFileLoaded에서 목록 갱신, openFile 버튼 우클릭 메뉴 or 별도 버튼
- [ ] C3: ChapterWidget 이미 존재/연결됨 (chapterWidget_ 지연 초기화) → 확인만
- [ ] 빌드 버전 4.4.0 업데이트: installer/sorinuri-setup.iss, .github/workflows/build-windows.yml, src/UpdateChecker.h, CMakeLists.txt
- [ ] CMakeLists.txt에 AlbumArtExtractor.cpp/h 추가 필요!
- [ ] version.json 릴리스 노트는 워크플로가 자동 생성하는지 확인 필요

## 주요 참고사항
- MusicMeta 구조체는 MusicWidget.h에 정의 (title, artist, album, year, genre, trackNum, sampleRate, bitDepth, channels, codec, replayGain, hasReplayGain, albumArt)
- MusicWidget::loadMeta의 statusBar_ 문자열 형식: "CODEC · DECODE · 2.0 · 192kHz · 24bit · BIT-PERFECT"
- MainWindow: settings_ = QSettings("Sorinuri","SorinuriPlayer"), MEDIA_EXTS 목록 존재
- MainWindow::onFileLoaded(path)에서 isMusicMode_ 분기
- 빌드/배포: git push → GitHub Actions (TOKEN은 로컬 환경 참조, repo sk1200rt-max/sorinuri-qt) → 서버 자동배포
- 버전 sed 교체 패턴: MyAppVersion "X" / APP_VERSION: "X" / "X" in UpdateChecker.h / VERSION X in CMakeLists.txt
