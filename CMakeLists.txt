cmake_minimum_required(VERSION 3.20)
project(Sorinuri VERSION 1.0.0 LANGUAGES CXX)

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
    resources/resources.qrc
)

# Windows 실행 파일 설정
if(WIN32)
    set(SOURCES ${SOURCES} resources/sorinuri.rc)
    add_executable(Sorinuri WIN32 ${SOURCES})
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
    ${MPV_LIBRARY}
)

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
