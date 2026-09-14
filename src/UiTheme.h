#pragma once

#include <QString>

// 소리누리 공통 UI 토큰과 핵심 컨트롤 스타일.
// 모든 주요 화면은 차분한 차콜·민트 체계를 공유한다. 강한 네온·그라데이션 대신
// 명확한 계층, 충분한 대비, 일관된 모서리와 여백으로 고급스러운 밀도를 유지한다.
namespace SorinuriUi {

inline const QString Surface       = QStringLiteral("#0A0F10");
inline const QString SurfaceAlt    = QStringLiteral("#101718");
inline const QString SurfaceRaised = QStringLiteral("#151F20");
inline const QString SurfaceHover  = QStringLiteral("#1C292A");
inline const QString SurfacePress  = QStringLiteral("#233536");
inline const QString Border        = QStringLiteral("#2B3B3C");
inline const QString BorderSoft    = QStringLiteral("#1C292A");
inline const QString Text          = QStringLiteral("#F2F7F6");
inline const QString TextMuted     = QStringLiteral("#A3B1B0");
inline const QString TextDim       = QStringLiteral("#71807F");
inline const QString Mint          = QStringLiteral("#00D4B4");
inline const QString MintHover     = QStringLiteral("#2AE2C5");
inline const QString MintDark      = QStringLiteral("#0A4940");
inline const QString Warning       = QStringLiteral("#E8B45A");
inline const QString Danger        = QStringLiteral("#E15B52");

inline QString iconButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background: transparent; border: 1px solid transparent; border-radius: 8px; }"
        "QPushButton:hover { background: %1; border-color: %2; }"
        "QPushButton:pressed { background: %3; border-color: %4; }"
        "QPushButton:disabled { color: %5; }")
        .arg(SurfaceHover, Border, SurfacePress, Mint, TextDim);
}

inline QString modeButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background: transparent; color: %1; border: 1px solid transparent;"
        " border-radius: 8px; padding: 0 10px; font-size: 11px; font-weight: 700; }"
        "QPushButton:hover { color: %2; background: %3; }"
        "QPushButton[active=true] { background: %4; color: %2; border-color: %2; }")
        .arg(TextMuted, Mint, SurfaceHover, MintDark);
}

inline QString comboBoxStyle()
{
    return QStringLiteral(
        "QComboBox { background: %1; border: 1px solid %2; border-radius: 8px;"
        " padding: 5px 9px; color: %3; font-size: 11px; min-height: 20px; }"
        "QComboBox:hover, QComboBox:focus { border-color: %4; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView { background: %1; color: %3; border: 1px solid %2;"
        " selection-background-color: %5; selection-color: %3; padding: 5px; }")
        .arg(SurfaceRaised, Border, Text, Mint, MintDark);
}

inline QString menuStyle()
{
    return QStringLiteral(
        "QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: 10px;"
        " font-size: 12px; padding: 6px 0; }"
        "QMenu::item { padding: 9px 32px 9px 14px; border-radius: 6px; margin: 1px 5px; }"
        "QMenu::item:selected { background: %4; color: %2; }"
        "QMenu::item:disabled { color: %5; }"
        "QMenu::separator { height: 1px; background: %3; margin: 5px 10px; }")
        .arg(SurfaceRaised, Text, Border, MintDark, TextDim);
}

inline QString toolTipStyle()
{
    return QStringLiteral(
        "QToolTip { background:#111A1B; color:#F8FCFB; border:1px solid #66807B;"
        " border-radius:5px; padding:6px 9px; font-family:'Segoe UI','Malgun Gothic',sans-serif;"
        " font-size:12px; font-weight:600; }");
}

inline QString statusBadgeStyle(const QString& color)
{
    return QStringLiteral(
        "background: %1; color: %2; border: 1px solid %3; border-radius: 8px;"
        "font-size: 10px; font-weight: 800; padding: 2px 8px;")
        .arg(MintDark, color, color);
}

// 환경 설정과 하이엔드 설정 패널이 공유하는 고대비 표면 규칙이다.
// 비활성 선택지 역시 작동 불가 이유를 읽을 수 있어야 하며, 모든 입력 컨트롤은
// 250% HiDPI 환경에서 충분한 클릭·터치 면적을 유지한다.
inline QString settingsPanelStyle()
{
    return QStringLiteral(
        "QWidget { background: %1; color: %2; font-family: 'Segoe UI', 'Malgun Gothic', sans-serif; font-size: 13px; }"
        "QScrollArea, QScrollArea > QWidget > QWidget { background: %1; }"
        "QTabWidget::pane { border: 1px solid %3; background: %1; top: -1px; }"
        "QTabBar::tab { background: transparent; color: %4; padding: 10px 18px; border: none;"
        " border-bottom: 2px solid transparent; font-weight: 600; }"
        "QTabBar::tab:selected { color: %5; border-bottom-color: %5; }"
        "QTabBar::tab:hover { color: %2; background: %6; }"
        "QGroupBox { background: %7; border: 1px solid %3; border-radius: 10px; margin-top: 14px;"
        " padding: 13px 10px 10px; color: %2; font-size: 11px; font-weight: 700; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
        "QLabel { color: %2; }"
        "QLabel#settingsDescription { color: %4; font-size: 11px; background: transparent; }"
        "QLabel#settingsPath { background: %8; color: %4; border: 1px solid %3; border-radius: 8px; padding: 6px 9px; }"
        "QLabel#settingsPath[ready=true] { background: %7; color: %2; border-color: %3; }"
        "QLabel#settingsStatusMuted { color: %4; font-size: 11px; background: transparent; }"
        "QLabel#settingsStatusActive { color: %5; font-size: 11px; font-weight: 700; background: transparent; }"
        "QLineEdit { background: %7; border: 1px solid %3; border-radius: 8px; padding: 6px 9px; color: %2; min-height: 20px; }"
        "QLineEdit:hover, QLineEdit:focus { border-color: %5; }"
        "QLineEdit:read-only { background: %8; color: %4; }"
        "QLineEdit:disabled { background: %8; color: %9; border-color: %10; }"
        "QComboBox { background: %7; border: 1px solid %3; border-radius: 8px; padding: 6px 9px; color: %2; min-width: 200px; }"
        "QComboBox:hover, QComboBox:focus { border-color: %5; }"
        "QComboBox:disabled { background: %8; color: %4; border-color: %10; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView { background: %7; color: %2; border: 1px solid %3; selection-background-color: %11; selection-color: %2; padding: 5px; }"
        "QCheckBox { color: %2; spacing: 8px; min-height: 22px; }"
        "QCheckBox:disabled { color: %4; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid %3; border-radius: 4px; background: %7; }"
        "QCheckBox::indicator:checked { background: %5; border-color: %5; }"
        "QCheckBox::indicator:disabled { background: %8; border-color: %4; }"
        "QSlider::groove:horizontal { height: 5px; background: %3; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: %5; border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 16px; height: 16px; margin: -6px 0; background: %2; border: 3px solid %5; border-radius: 8px; }"
        "QSlider:disabled::sub-page:horizontal { background: %10; }"
        "QSlider:disabled::handle:horizontal { border-color: %4; background: %8; }"
        "QPushButton { background: %7; border: 1px solid %3; border-radius: 8px; padding: 7px 16px; color: %2; min-height: 28px; }"
        "QPushButton:hover { background: %6; border-color: %5; }"
        "QPushButton:pressed { background: %11; border-color: %5; }"
        "QPushButton:disabled { background: %8; color: %4; border-color: %10; }"
        "QListWidget { background: %7; color: %2; border: 1px solid %3; border-radius: 8px; }"
        "QListWidget::item { padding: 6px 8px; border-bottom: 1px solid %10; }"
        "QListWidget::item:selected { background: %11; color: %2; }"
        "QScrollBar:vertical { background: %1; width: 12px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: %3; min-height: 32px; border-radius: 6px; }"
        "QScrollBar::handle:vertical:hover { background: %4; }")
        .arg(Surface)
        .arg(Text)
        .arg(Border)
        .arg(TextMuted)
        .arg(Mint)
        .arg(SurfaceHover)
        .arg(SurfaceRaised)
        .arg(SurfaceAlt)
        .arg(TextDim)
        .arg(BorderSoft)
        .arg(MintDark);
}

inline QString dialogStyle()
{
    return settingsPanelStyle() + QStringLiteral(
        "QDialog { background: %1; color: %2; }"
        "QPushButton#btnApply, QPushButton#btnOk { background: %3; border-color: %4; color: %4; font-weight: 700; }"
        "QPushButton#btnApply:hover, QPushButton#btnOk:hover { background: %5; color: %2; }")
        .arg(Surface, Text, MintDark, Mint, SurfacePress);
}

} // namespace SorinuriUi
