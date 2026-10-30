// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QStringList>

#include "linktools.h"

class QMimeData;

/**
 * The payload dropped onto the content factory, already classified.
 */
struct DropContent
{
    enum Kind {
        None,
        Files,
        Image,
        Text,
        Link,       // http(s)
        CustomLink, // thunder://, spk://, magnet:, ...
    };

    Kind kind = None;
    QStringList filePaths;
    QString text;
    QStringList mimes;
    LinkInfo link;
    QImage image;

    bool valid() const { return kind != None; }
    QString summary() const;
};

namespace ContentClassifier {
DropContent classify(const QMimeData *mime);
DropContent classifyText(const QString &text);
}
