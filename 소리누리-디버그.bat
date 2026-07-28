@echo off
chcp 65001 > nul
title 소리누리 디버그 콘솔
color 0A

echo ============================================================
echo   소리누리 Qt 디버그 모드
echo   모든 로그가 이 창에 출력됩니다.
echo ============================================================
echo.

set SCRIPT_DIR=%~dp0
set EXE=%SCRIPT_DIR%Sorinuri.exe

if not exist "%EXE%" (
    echo [오류] Sorinuri.exe 를 찾을 수 없습니다.
    echo 이 배치 파일은 Sorinuri.exe 와 같은 폴더에 있어야 합니다.
    pause
    exit /b 1
)

echo [시작] %EXE%
echo ---- 로그 시작 ----
echo.

"%EXE%" 2>&1

echo.
echo ---- 프로그램 종료 ----
pause
