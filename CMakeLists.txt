cmake_minimum_required(VERSION 3.20)
project(Sorinuri VERSION 6.11.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 패키지
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    OpenGL
    OpenGLWidgets
    Concurrent
    Network
    Sql
)

# libmpv 헤더/라이브러리 경로 (GitHub Actions에서 설정)
if(WIN32)
    set(MPV_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/deps/libmpv/include" CACHE PATH "libmpv include dir")
    set(MPV_LIB_DIR     "${CMAKE_SOURCE_DIR}/deps/libmpv/lib"     CACHE PATH "libmpv lib dir")
    find_library(MPV_LIBRARY NAMES mpv libmpv PATHS "${MPV_LIB_DIR}" NO_DEFAULT_PATH)
else()
    find_path(MPV_INCLUDE_DIR mpv/client.h)
    find_library(MPV_LIBRARY NAMES mpv)
endif()

message(STATUS "MPV include: ${MPV_INCLUDE_DIR}")
message(STATUS "MPV library: ${MPV_LIBRARY}")

# 소스 파일
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/MpvWidget.cpp
    src/MpvWidget.h
    src/MpvCore.cpp
    src/MpvCore.h
    src/PlaylistWidget.cpp
    src/PlaylistWidget.h
    src/ControlBar.cpp
    src/ControlBar.h
    src/AudioSettings.cpp
    src/AudioSettings.h
    src/SubtitleManager.cpp
    src/SubtitleManager.h
    src/Settings.cpp
    src/Settings.h
    src/TitleBar.cpp
    src/TitleBar.h
    src/AudioInfoBar.cpp
    src/AudioInfoBar.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/TrackSelector.cpp
    src/TrackSelector.h
    src/UrlDialog.cpp
    src/UrlDialog.h
    src/YtdlpManager.cpp
    src/YtdlpManager.h
    src/OttWidget.cpp
    src/OttWidget.h
    src/ProFeaturesWidget.cpp
    src/ProFeaturesWidget.h
    src/ShortcutOverlay.cpp
    src/MediaInfoOverlay.cpp
    src/ShortcutOverlay.h
    src/MusicWidget.cpp
    src/MiniPlayerWidget.cpp
    src/CompactPlayerWidget.cpp
    src/WhisperWidget.cpp
    src/UpscaleWidget.cpp
    src/ChapterWidget.cpp
    src/MultiViewWidget.cpp
    src/MusicWidget.h
    src/MiniPlayerWidget.h
    src/CompactPlayerWidget.h
    src/WhisperWidget.h
    src/UpscaleWidget.h
    src/ChapterWidget.h
    src/MultiViewWidget.h
    src/LyricsWidget.cpp
    src/LyricsWidget.h
    src/HiFiEngine.cpp
    src/HiFiEngine.h
    src/UpdateChecker.cpp
    src/UpdateChecker.h
    src/UpdateDialog.cpp
    src/UpdateDialog.h
    src/AlbumArtExtractor.cpp
    src/AlbumArtExtractor.h
    src/SubtitleDownloader.cpp
    src/SubtitleDownloader.h
    src/SubtitleSearchDialog.cpp
    src/SubtitleSearchDialog.h
    src/AudioAdvancedWidget.cpp
    src/AudioAdvancedWidget.h
    src/VideoAdvancedWidget.cpp
    src/VideoAdvancedWidget.h
    src/NetworkBrowserWidget.cpp
    src/NetworkBrowserWidget.h
    src/OriginalsWidget.cpp
    src/OriginalsWidget.h
    src/OsdWidget.cpp
    src/OsdWidget.h
    src/MediaLibraryWidget.cpp
    src/MediaLibraryWidget.h
    src/SubtitleEditorWidget.cpp
    src/SubtitleEditorWidget.h
    src/AdManager.cpp
    src/AdManager.h
    src/SplashAdWidget.cpp
    src/SplashAdWidget.h
    src/RenderEnvironment.h
    resources/resources.qrc
)

# Windows 실행 파일 설정
if(WIN32)
    set(SOURCES ${SOURCES} resources/sorinuri.rc)
    add_executable(Sorinuri WIN32 ${SOURCES})
    # RC 컴파일러가 아이콘/manifest 파일을 찾을 수 있도록 include 경로 추가
    # - resources/ : manifest(sorinuri.manifest) 위치
    # - resources/icons/ : 아이콘(sorinuri.ico) 위치
    # - ${CMAKE_SOURCE_DIR} : 소스 루트 (절대 경로 기준점)
    set_source_files_properties(resources/sorinuri.rc PROPERTIES
        COMPILE_FLAGS "/I\"${CMAKE_SOURCE_DIR}/resources\" /I\"${CMAKE_SOURCE_DIR}/resources/icons\" /I\"${CMAKE_SOURCE_DIR}\""
    )
    # MSVC 링커의 자동 manifest 삽입 비활성화
    # (RC 파일에 이미 Per-Monitor V2 manifest가 포함되어 있으므로 중복 방지)
    if(MSVC)
        set_target_properties(Sorinuri PROPERTIES
            LINK_FLAGS "/MANIFEST:NO"
        )
    endif()
else()
    add_executable(Sorinuri ${SOURCES})
endif()

target_include_directories(Sorinuri PRIVATE
    src
    ${MPV_INCLUDE_DIR}
)

target_link_libraries(Sorinuri PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Concurrent
    Qt6::Network
    Qt6::Sql
    ${MPV_LIBRARY}
)

# Windows: OpenGL32 링크 (glClear, glClearColor 등)
if(WIN32)
    target_link_libraries(Sorinuri PRIVATE opengl32)
endif()

# Windows: WebView2 SDK 연동 (OTT 스트리밍 - Netflix Dolby Atmos 지원)
if(WIN32)
    # WebView2 SDK NuGet 패키지에서 헤더/라이브러리 경로
    set(WEBVIEW2_SDK_DIR "${CMAKE_SOURCE_DIR}/deps/webview2" CACHE PATH "WebView2 SDK dir")
    if(EXISTS "${WEBVIEW2_SDK_DIR}/include/WebView2.h")
        target_include_directories(Sorinuri PRIVATE "${WEBVIEW2_SDK_DIR}/include")
        target_link_libraries(Sorinuri PRIVATE
            "${WEBVIEW2_SDK_DIR}/lib/x64/WebView2Loader.dll.lib")
        message(STATUS "WebView2 SDK: ${WEBVIEW2_SDK_DIR}")
    else()
        message(STATUS "WebView2 SDK not found - OTT 기능 비활성화")
        target_compile_definitions(Sorinuri PRIVATE WEBVIEW2_NOT_AVAILABLE)
    endif()
    # WRL (Windows Runtime C++ Template Library) - WebView2 COM 인터페이스용
    target_link_libraries(Sorinuri PRIVATE ole32 oleaut32 shlwapi)
endif()

# Windows: DLL 복사
if(WIN32)
    add_custom_command(TARGET Sorinuri POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MPV_LIB_DIR}/libmpv-2.dll"
            $<TARGET_FILE_DIR:Sorinuri>
        COMMENT "libmpv-2.dll 복사"
    )

    # Qt DLL 배포
    add_custom_command(TARGET Sorinuri POST_BUILD
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe"
            --no-translations
            --no-system-d3d-compiler
            $<TARGET_FILE:Sorinuri>
        COMMENT "Qt DLL 배포"
    )
endif()

# 설치
install(TARGETS Sorinuri DESTINATION .)
if(WIN32)
    install(FILES "${MPV_LIB_DIR}/libmpv-2.dll" DESTINATION .)
endif()
