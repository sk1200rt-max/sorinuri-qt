# libmpv 종료·오디오 해제 조사 기록

- 출처: https://github.com/mpv-player/mpv/blob/master/include/mpv/client.h
- 확인일: 2026-08-30

`mpv_terminate_destroy(mpv_handle *ctx)`는 플레이어와 모든 client를 종료하고, 모두 파괴될 때까지 대기하는 blocking API로 문서화되어 있다. 마지막 mpv handle을 종료할 때 코어의 최종 종료까지 기다리는 것이 장점이다. 따라서 메인 창의 `closeEvent()`에서 OpenGL render context를 먼저 해제하고 `mpv_terminate_destroy()`를 명시적으로 완료하면, Qt 객체 소멸과 이벤트 루프 종료까지 지연되는 WASAPI 독점 출력 해제를 앞당길 수 있다.

문서상 비동기 명령은 다른 호출과 임의 순서로 섞일 수 있으므로, 종료 직전 `ao-reload`나 `audio-exclusive` 같은 비동기 재협상 명령을 추가하지 않는다. 종료는 `mpv_terminate_destroy()` 단일 동기 경로로 보장한다.

- 보조 출처: https://mpv.io/manual/stable/
- libmpv는 Windows에서 타이머 해상도를 변경할 수 있고, 임베더는 종료 시 클라이언트 API handle의 정상 파괴를 보장해야 한다.
