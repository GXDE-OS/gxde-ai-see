// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless checks that the assistant markdown document really uses the chat
// palette. Qt rich text does not inherit colour from <body>, so this guards
// the block selectors in ChatTheme::assistantDocumentStyle().

#include <QColor>
#include <QGuiApplication>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextStream>

#include "chat/chattheme.h"

static int failures = 0;

static void check(bool condition, const QString &what)
{
    QTextStream out(stdout);
    out << (condition ? "PASS  " : "FAIL  ") << what << "\n";
    if (!condition) {
        ++failures;
    }
}

static QColor colorOf(QTextDocument &document, const QString &needle)
{
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (fragment.isValid() && fragment.text().contains(needle)) {
                return fragment.charFormat().foreground().color();
            }
        }
    }
    return QColor();
}

static void checkTheme(bool dark)
{
    const QString tag = dark ? QStringLiteral("dark") : QStringLiteral("light");
    const ChatPalette palette = ChatTheme::buildPalette(dark);

    QTextDocument document;
    document.setDefaultStyleSheet(ChatTheme::assistantDocumentStyle(palette));
    document.setHtml(QStringLiteral("<div>plain text</div>"
                                    "<div><a href=\"https://example.com\">link</a></div>"
                                    "<pre><code>code</code></pre>"));

    const QColor body = colorOf(document, QStringLiteral("plain text"));
    check(body.isValid() && body.rgb() == palette.bubbleAssistantText.rgb(),
          tag + QStringLiteral(" markdown block text uses the palette"));
    const QColor link = colorOf(document, QStringLiteral("link"));
    check(link.isValid() && link.rgb() == palette.link.rgb(),
          tag + QStringLiteral(" markdown link keeps the link colour"));
    const QColor code = colorOf(document, QStringLiteral("code"));
    check(code.isValid() && code.rgb() == palette.bubbleAssistantText.rgb(),
          tag + QStringLiteral(" code block text uses the palette"));
}

int main(int argc, char *argv[])
{
    /* No display needed: the offscreen platform ships with libqt6gui6. */
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication app(argc, argv);

    checkTheme(true);
    checkTheme(false);

    QTextStream(stdout) << (failures == 0 ? "ALL PASS\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
