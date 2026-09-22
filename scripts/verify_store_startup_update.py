#!/usr/bin/env python3
"""Static regression gate for Microsoft Store startup update handling.

The Store MSIX must never run the self-hosted EXE updater. Instead, a Store-installed
build checks Microsoft Store package availability after its main window appears and asks
Windows to download/install the verified Store package through its own consent UI.
"""
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    path = ROOT / relative
    if not path.is_file():
        raise AssertionError(f"필수 파일이 없습니다: {relative}")
    return path.read_text(encoding="utf-8")


def require(text: str, needle: str, relative: str) -> None:
    if needle not in text:
        raise AssertionError(f"{relative}에 필수 Store 시작 업데이트 경로가 없습니다: {needle}")


def main() -> int:
    cmake = read("CMakeLists.txt")
    window = read("src/MainWindow.cpp")
    main_cpp = read("src/main.cpp")
    manager_h = read("src/StoreUpdateManager.h")
    manager_cpp = read("src/StoreUpdateManager.cpp")

    for needle in (
        "src/StoreUpdateManager.cpp",
        "src/StoreUpdateManager.h",
        "target_compile_definitions(Sorinuri PRIVATE SORINURI_STORE_BUILD=1)",
        "target_compile_options(Sorinuri PRIVATE /await)",
        "target_link_libraries(Sorinuri PRIVATE windowsapp)",
    ):
        require(cmake, needle, "CMakeLists.txt")

    for needle in (
        '#if defined(SORINURI_STORE_BUILD) && defined(Q_OS_WIN)',
        '#include "StoreUpdateManager.h"',
        "auto* storeUpdater = new StoreUpdateManager(this);",
        "storeUpdater->checkForUpdates(this);",
        "QTimer::singleShot(5000, this",
        "#else",
        "updater->checkForUpdates();",
    ):
        require(window, needle, "src/MainWindow.cpp")

    for needle in (
        "winrt::init_apartment(winrt::apartment_type::single_threaded)",
        "winrt::uninit_apartment()",
        "StoreWinrtApartment",
    ):
        require(main_cpp, needle, "src/main.cpp")

    for needle in (
        "winrt/Windows.Services.Store.h",
        "StoreContext",
        "checkForUpdates(QWidget* ownerWindow)",
        "requestDownloadAndInstall",
    ):
        require(manager_h, needle, "src/StoreUpdateManager.h")

    for needle in (
        "StoreContext::GetDefault()",
        "IInitializeWithWindow",
        "GetAppAndOptionalStorePackageUpdatesAsync()",
        "RequestDownloadAndInstallStorePackageUpdatesAsync(updates)",
        "QMetaObject::invokeMethod",
        "Qt::QueuedConnection",
        "StorePackageUpdateState::Completed",
        "StorePackageUpdateState::Canceled",
        "QPointer<StoreUpdateManager>",
    ):
        require(manager_cpp, needle, "src/StoreUpdateManager.cpp")

    forbidden = (
        "UpdateDialog",
        "Sorinuri-Setup-pending.exe",
        "installer_url",
        "QProcess::startDetached",
    )
    for needle in forbidden:
        if needle in manager_cpp:
            raise AssertionError(
                f"Store update manager가 자체 EXE updater를 참조하면 안 됩니다: {needle}"
            )

    print("Microsoft Store startup update verification passed.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"Microsoft Store startup update verification failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
