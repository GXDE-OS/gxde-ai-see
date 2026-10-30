// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "texttools.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>

#include <unicode/ubrk.h>
#include <unicode/utypes.h>

namespace TextTools {

QStringList segment(const QString &text)
{
    QStringList words;
    if (text.isEmpty()) {
        return words;
    }

    UErrorCode status = U_ZERO_ERROR;
    UBreakIterator *iterator =
        ubrk_open(UBRK_WORD, "zh", reinterpret_cast<const UChar *>(text.utf16()), text.size(),
                  &status);
    if (U_FAILURE(status) || !iterator) {
        return words;
    }

    int32_t start = ubrk_first(iterator);
    for (int32_t end = ubrk_next(iterator); end != UBRK_DONE; start = end, end = ubrk_next(iterator)) {
        if (ubrk_getRuleStatus(iterator) == UBRK_WORD_NONE) {
            continue;
        }
        const QString word = text.mid(start, end - start).trimmed();
        if (!word.isEmpty()) {
            words.append(word);
        }
    }

    ubrk_close(iterator);
    return words;
}

QStringList extractLinks(const QString &text)
{
    static const QRegularExpression pattern(
        QStringLiteral("(https?://[^\\s<>\"'\\x{3000}\\x{FF08}\\x{FF09}\\x{3010}\\x{3011}]+"
                       "|www\\.[^\\s<>\"'\\x{3000}\\x{FF08}\\x{FF09}\\x{3010}\\x{3011}]+"
                       "|[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,})"),
        QRegularExpression::CaseInsensitiveOption);

    QStringList links;
    QSet<QString> seen;
    auto it = pattern.globalMatch(text);
    while (it.hasNext()) {
        QString link = it.next().captured(1);
        while (link.endsWith(QLatin1Char('.')) || link.endsWith(QLatin1Char(',')) ||
               link.endsWith(QLatin1Char(')'))) {
            link.chop(1);
        }
        if (!link.isEmpty() && !seen.contains(link)) {
            seen.insert(link);
            links.append(link);
        }
    }
    return links;
}

QString readTextFile(const QString &path, qint64 maxBytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    const QByteArray data = file.read(maxBytes);
    return QString::fromUtf8(data);
}

} // namespace TextTools
