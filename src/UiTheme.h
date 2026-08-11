#pragma once

#include <QString>

// 소리누리 공통 UI 토큰과 핵심 컨트롤 스타일.
// 화면마다 다른 청색·회색·모서리 값이 생기지 않도록 플레이어, 메뉴, 트랙 선택기에 함께 사용한다.
namespace SorinuriUi {

inline const QString Surface      = QStringLiteral("#0B0F10");
inline const QString SurfaceAlt   = QStringLiteral("#12191A");
inline const QString SurfaceHover = QStringLiteral("#1A2526");
inline const QString Border       = QStringLiteral("#263233");
inline const QString BorderSoft   = QStringLiteral("#1A2324");
inline const QString Text         = QStringLiteral("#EEF6F5");
inline const QString TextMuted    = QStringLiteral("#8C9A99");
inline const QString TextDim      = QStringLiteral("#62706F");
inline const QString Mint         = QStringLiteral("#00D4B4");
inline const QString MintHover    = QStringLiteral("#20E0C3");
inline const QString MintDark     = QStringLiteral("#063B35");
inline const QString Danger       = QStringLiteral("#D94D45");

inline QString iconButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background: transparent; border: 1px solid transparent; border-radius: 6px; }"
        "QPushButton:hover { background: %1; border-color: %2; }"
        "QPushButton:pressed { background: %3; border-color: %4; }")
        .arg(SurfaceHover, Border, MintDark, Mint);
}

inline QString modeButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2;"
        " border-radius: 6px; padding: 0 10px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { color: %3; border-color: %3; background: %4; }"
        "QPushButton[active=true] { background: %4; color: %3; border-color: %3; }")
        .arg(TextMuted, Border, Mint, MintDark);
}

inline QString comboBoxStyle()
{
    return QStringLiteral(
        "QComboBox { background: %1; border: 1px solid %2; border-radius: 6px;"
        " padding: 4px 8px; color: %3; font-size: 11px; }"
        "QComboBox:hover { border-color: %4; }"
        "QComboBox:focus { border-color: %4; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: %1; color: %3; border: 1px solid %2;"
        " selection-background-color: %5; selection-color: %3; padding: 4px; }")
        .arg(SurfaceAlt, Border, Text, Mint, MintDark);
}

inline QString menuStyle()
{
    return QStringLiteral(
        "QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: 8px;"
        " font-size: 12px; padding: 6px 0; }"
        "QMenu::item { padding: 8px 30px 8px 14px; border-radius: 4px; margin: 1px 5px; }"
        "QMenu::item:selected { background: %4; color: %2; }"
        "QMenu::item:disabled { color: %5; }"
        "QMenu::separator { height: 1px; background: %3; margin: 5px 10px; }")
        .arg(SurfaceAlt, Text, Border, MintDark, TextDim);
}

inline QString statusBadgeStyle(const QString& color)
{
    return QStringLiteral(
        "background: %1; color: %2; border: 1px solid %3; border-radius: 6px;"
        " font-size: 10px; font-weight: 700; padding: 2px 7px;")
        .arg(MintDark, color, color);
}

} // namespace SorinuriUi
