// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QString>

/**
 * Colour palette for the chat window (小埃同学).
 *
 * The chat UI used to hard-code a dark palette; the values below now come in
 * a light and a dark variant so the window can follow the DTK theme.
 */
struct ChatPalette
{
    bool dark = true;

    QColor window;      ///< main conversation pane background
    QColor sidebar;     ///< conversation list background
    QColor separator;   ///< hairline borders

    QColor textPrimary;
    QColor textSecondary;
    QColor textMuted;
    QColor textFaint;
    QColor listTitle;  ///< conversation title (unselected)

    QColor accent;        ///< brand blue for text / tints
    QColor accentStrong;  ///< filled buttons
    QColor accentHover;
    QColor accentText;    ///< text drawn on top of accentStrong

    QColor highlight;  ///< selected conversation row
    QColor hover;      ///< hovered conversation row
    QColor hoverStrong;  ///< hovered filled/outlined button
    QColor headerBg;     ///< conversation header strip
    QColor disabledBg;   ///< disabled filled button
    QColor actionText;   ///< inline message actions / model badge
    QColor actionTextHover;
    QColor badgeText;  ///< model badge label

    QColor bubbleUser;
    QColor bubbleUserText;
    QColor bubbleAssistant;
    QColor bubbleAssistantText;
    QColor codeBg;
    QColor link;

    QColor inputBg;
    QColor inputBorder;
    QColor status;  ///< warning / streaming status line

    QColor avatarUserBg;
    QColor avatarUserText;
    QColor avatarAssistantBg;
    QColor avatarAssistantText;
    QColor welcomeAvatarBg;
    QColor welcomeAvatarText;
    QColor noteBg;  ///< inline system note background
};

namespace ChatTheme {

/** Pure palette builder: needs no widget, so it can be unit-tested. */
ChatPalette buildPalette(bool dark);

/** Colour as a Qt style-sheet value; alpha becomes a percentage. */
QString css(const QColor &color);

/**
 * Default style sheet for assistant markdown documents.
 *
 * Qt rich text has no colour inheritance from <body>, and the markdown
 * renderer emits <div>/<h2>/<pre> blocks, so the colour has to be set on the
 * block selectors themselves.
 */
QString assistantDocumentStyle(const ChatPalette &palette);

/** Current theme, derived from the application palette unless forced. */
bool isDark();

/** Pin the theme (called on startup and whenever DTK reports a change). */
void setDark(bool dark);

/** Go back to deriving the theme from QApplication::palette(). */
void followSystem();

/** Drop the cached palette so the next palette() call rebuilds it. */
void refresh();

/** Cached palette for the current theme. */
const ChatPalette &palette();

} // namespace ChatTheme
