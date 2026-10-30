// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chattheme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QPalette>

namespace {

QColor alpha(const QColor &color, int percent)
{
    return QColor(color.red(), color.green(), color.blue(), qRound(255 * percent / 100.0));
}

} // namespace

namespace ChatTheme {

ChatPalette buildPalette(bool dark)
{
    ChatPalette p;
    p.dark = dark;

    if (dark) {
        p.window = QColor(0x1e, 0x1e, 0x22);
        p.sidebar = QColor(0x17, 0x17, 0x1b);
        p.separator = alpha(Qt::white, 6);

        p.textPrimary = QColor(0xf2, 0xf2, 0xf2);
        p.textSecondary = alpha(Qt::white, 68);
        p.textMuted = alpha(Qt::white, 46);
        p.textFaint = alpha(Qt::white, 36);
        p.listTitle = alpha(Qt::white, 84);

        p.accent = QColor(0x4a, 0x90, 0xff);
        p.accentStrong = alpha(p.accent, 88);
        p.accentHover = alpha(p.accent.lighter(112), 96);
        p.accentText = QColor(Qt::white);

        p.highlight = alpha(p.accent.lighter(112), 24);
        p.hover = alpha(Qt::white, 6);
        p.hoverStrong = alpha(Qt::white, 12);
        p.headerBg = alpha(Qt::white, 2);
        p.disabledBg = alpha(Qt::white, 10);
        p.actionText = alpha(QColor(0xa0, 0xc8, 0xf0), 75);
        p.actionTextHover = QColor(0xcf, 0xe5, 0xff);
        p.badgeText = p.actionText;

        p.bubbleUser = alpha(p.accent, 20);
        p.bubbleUserText = QColor(0xf2, 0xf2, 0xf2);
        p.bubbleAssistant = alpha(Qt::white, 7);
        p.bubbleAssistantText = QColor(0xec, 0xec, 0xec);
        p.codeBg = QColor(0x2b, 0x2b, 0x31);
        p.link = QColor(0x7a, 0xa7, 0xd9);

        p.inputBg = alpha(Qt::white, 6);
        p.inputBorder = alpha(Qt::white, 8);
        p.status = alpha(QColor(0xff, 0xaa, 0x78), 92);

        p.avatarUserBg = alpha(p.accent, 88);
        p.avatarUserText = QColor(Qt::white);
        p.avatarAssistantBg = alpha(Qt::white, 14);
        p.avatarAssistantText = QColor(0xea, 0xea, 0xea);
        p.welcomeAvatarBg = alpha(p.accent, 22);
        p.welcomeAvatarText = QColor(0xcf, 0xe5, 0xff);
        p.noteBg = alpha(Qt::white, 4);
    } else {
        p.window = QColor(0xf7, 0xf7, 0xfa);
        p.sidebar = QColor(0xee, 0xee, 0xf3);
        p.separator = alpha(Qt::black, 9);

        p.textPrimary = QColor(0x1c, 0x1c, 0x1e);
        p.textSecondary = alpha(Qt::black, 64);
        p.textMuted = alpha(Qt::black, 60);
        p.textFaint = alpha(Qt::black, 52);
        p.listTitle = alpha(Qt::black, 72);

        p.accent = QColor(0x2f, 0x7e, 0xe8);
        p.accentStrong = alpha(p.accent, 92);
        p.accentHover = p.accent.darker(112);
        p.accentText = QColor(Qt::white);

        p.highlight = alpha(p.accent, 18);
        p.hover = alpha(Qt::black, 5);
        p.hoverStrong = alpha(Qt::black, 7);
        p.headerBg = alpha(Qt::black, 2);
        p.disabledBg = alpha(Qt::black, 8);
        p.actionText = QColor(0x1f, 0x5f, 0xae);
        p.actionTextHover = QColor(0x0f, 0x4d, 0x93);
        p.badgeText = QColor(0x1f, 0x5f, 0xae);

        p.bubbleUser = alpha(p.accent, 12);
        p.bubbleUserText = QColor(0x1c, 0x1c, 0x1e);
        p.bubbleAssistant = alpha(Qt::black, 5);
        p.bubbleAssistantText = QColor(0x1c, 0x1c, 0x1e);
        p.codeBg = alpha(Qt::black, 6);
        p.link = QColor(0x0f, 0x5c, 0xb0);

        p.inputBg = alpha(Qt::black, 4);
        p.inputBorder = alpha(Qt::black, 10);
        p.status = QColor(0x9a, 0x4a, 0x12);

        p.avatarUserBg = alpha(p.accent, 92);
        p.avatarUserText = QColor(Qt::white);
        p.avatarAssistantBg = alpha(Qt::black, 8);
        p.avatarAssistantText = QColor(0x1c, 0x1c, 0x1e);
        p.welcomeAvatarBg = alpha(p.accent, 14);
        p.welcomeAvatarText = p.accent;
        p.noteBg = alpha(Qt::black, 4);
    }

    return p;
}

QString assistantDocumentStyle(const ChatPalette &palette)
{
    /* Qt rich text does not inherit the <body> colour into block elements and
     * markdownToHtml() emits <div>/<h2>/<pre>, so set it on the blocks. The
     * link rule comes last so links keep their own colour. */
    return QStringLiteral("body, div, p, h1, h2, h3, h4, h5, h6, ul, ol, li,"
                          " pre, code, blockquote, table { color: %1; }"
                          "pre { background: %2; }"
                          "code { background: %2; }"
                          "a { color: %3; }"
                          "h1, h2, h3 { margin: 4px 0; }"
                          ".li { margin-left: 10px; }"
                          ".gap { height: 6px; }")
        .arg(css(palette.bubbleAssistantText),
             css(palette.codeBg),
             css(palette.link));
}

QString css(const QColor &color)
{
    if (color.alpha() >= 255) {
        return color.name(QColor::HexRgb);
    }
    return QStringLiteral("rgba(%1,%2,%3,%4%)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(qRound(color.alphaF() * 100));
}

namespace {
bool g_forced = false;
bool g_dark = true;
ChatPalette g_cache;
bool g_valid = false;
} // namespace

bool isDark()
{
    if (g_forced) {
        return g_dark;
    }
    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app) {
        return true;
    }
    return app->palette().color(QPalette::Window).lightness() < 128;
}

void setDark(bool dark)
{
    g_forced = true;
    g_dark = dark;
    g_valid = false;
}

void followSystem()
{
    g_forced = false;
    g_valid = false;
}

void refresh()
{
    g_valid = false;
}

const ChatPalette &palette()
{
    if (!g_valid) {
        g_cache = buildPalette(isDark());
        g_valid = true;
    }
    return g_cache;
}

} // namespace ChatTheme
