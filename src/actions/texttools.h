// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

namespace TextTools {

/** Chinese word segmentation (ICU word break, zh locale). */
QStringList segment(const QString &text);

/** Extract http(s) links, www links and e-mail addresses. */
QStringList extractLinks(const QString &text);

/** Read a text file with a size cap; returns an empty string on failure. */
QString readTextFile(const QString &path, qint64 maxBytes = 4 * 1024 * 1024);

} // namespace TextTools
