// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "markdown.h"

#include <QRegularExpression>

QString escapedHtml(const QString &text)
{
    QString html = text.toHtmlEscaped();
    html.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    return html;
}

namespace {

QString renderInline(QString text)
{
    text = text.toHtmlEscaped();
    text.replace(QRegularExpression(QStringLiteral("`([^`]+)`")),
                 QStringLiteral("<code>\\1</code>"));
    text.replace(QRegularExpression(QStringLiteral("\\*\\*([^*]+)\\*\\*")),
                 QStringLiteral("<b>\\1</b>"));
    text.replace(
        QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]\\((https?://[^)\\s]+)\\)")),
        QStringLiteral("<a href=\"\\2\">\\1</a>"));
    text.replace(QRegularExpression(QStringLiteral("(^|[\\s(])(https?://[^\\s<]+)")),
                 QStringLiteral("\\1<a href=\"\\2\">\\2</a>"));
    return text;
}

} // namespace

QString markdownToHtml(const QString &markdown)
{
    QString html;
    QString code;
    bool inCode = false;

    const QStringList lines = markdown.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.trimmed().startsWith(QStringLiteral("```"))) {
            if (inCode) {
                html += QStringLiteral("<pre><code>%1</code></pre>").arg(code.toHtmlEscaped());
                code.clear();
            }
            inCode = !inCode;
            continue;
        }
        if (inCode) {
            code += line;
            code += QLatin1Char('\n');
            continue;
        }

        if (line.startsWith(QStringLiteral("### "))) {
            html += QStringLiteral("<h3>%1</h3>").arg(renderInline(line.mid(4)));
        } else if (line.startsWith(QStringLiteral("## "))) {
            html += QStringLiteral("<h2>%1</h2>").arg(renderInline(line.mid(3)));
        } else if (line.startsWith(QStringLiteral("# "))) {
            html += QStringLiteral("<h2>%1</h2>").arg(renderInline(line.mid(2)));
        } else if (line.startsWith(QStringLiteral("- ")) ||
                   line.startsWith(QStringLiteral("* "))) {
            html += QStringLiteral("<div class='li'>• %1</div>").arg(renderInline(line.mid(2)));
        } else if (line.trimmed().isEmpty()) {
            html += QStringLiteral("<div class='gap'></div>");
        } else {
            html += QStringLiteral("<div>%1</div>").arg(renderInline(line));
        }
    }

    if (inCode) {
        html += QStringLiteral("<pre><code>%1</code></pre>").arg(code.toHtmlEscaped());
    }
    return html;
}
