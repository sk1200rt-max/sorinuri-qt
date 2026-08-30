#pragma once

#include <QString>

// 소리누리 공통 UI 토큰과 핵심 컨트롤 스타일.
// 모든 주요 화면은 이 차분한 차콜·민트 체계를 공유한다. 강한 네온·그라데이션 대신
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

inline QString statusBadgeStyle(const QString& color)
{
    return QStringLiteral(
        "background: %1; color: %2; border: 1px solid %3; border-radius: 8px;"
        "font-size: 10px; font-weight: 800; padding: 2px 8px;")
        .arg(MintDark, color, color);
}

inline QString dialogStyle()
{
    return QStringLiteral(
        "QDialog { background: %1; color: %2; font-family: 'Segoe UI', 'Malgun Gothic', sans-serif; font-size: 13px; }"
        "QTabWidget::pane { border: 1px solid %3; background: %1; top: -1px; }"
        "QTabBar::tab { background: transparent; color: %4; padding: 10px 18px; border: none;"
        " border-bottom: 2px solid transparent; font-weight: 600; }"
        "QTabBar::tab:selected { color: %5; border-bottom-color: %5; }"
        "QTabBar::tab:hover { color: %2; background: %6; }"
        "QGroupBox { border: 1px solid %3; border-radius: 10px; margin-top: 14px; padding: 13px 10px 10px;"
        " color: %4; font-size: 11px; font-weight: 700; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
        "QComboBox { background: %7; border: 1px solid %3; border-radius: 8px; padding: 6px 9px; color: %2; min-width: 200px; }"
        "QComboBox:hover, QComboBox:focus { border-color: %5; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: %7; color: %2; selection-background-color: %8; }"
        "QCheckBox { color: %2; spacing: 7px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid %3; border-radius: 4px; background: %7; }"
        "QCheckBox::indicator:checked { background: %5; border-color: %5; }"
        "QSlider::groove:horizontal { height: 4px; background: %3; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: %5; border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; background: %2; border: 3px solid %5; border-radius: 7px; }"
        "QPushButton { background: %7; border: 1px solid %3; border-radius: 8px; padding: 7px 16px; color: %2; min-height: 28px; }"
        "QPushButton:hover { background: %6; border-color: %4; }"
        "QPushButton:pressed { background: %8; border-color: %5; }"
        "QPushButton#btnApply, QPushButton#btnOk { background: %8; border-color: %5; color: %5; font-weight: 700; }"
        "QPushButton#btnApply:hover, QPushButton#btnOk:hover { background: %9; color: %2; }"
        "QLabel { color: %2; } QLabel.hint { color: %4; font-size: 11px; }")
        .arg(Surface, Text, Border, TextMuted, Mint, SurfaceHover, SurfaceRaised, MintDark, SurfacePress);
}

} // namespace SorinuriUi
