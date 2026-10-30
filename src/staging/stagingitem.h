// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFileInfo>
#include <QJsonObject>
#include <QString>

/**
 * One entry in the staging area (暂存区).
 *
 * File  - a local file, kept as a URI.  Watched for deletion.
 * Image - an image either dropped from a file or as raw image data.  Raw
 *         images are written to the cache directory so the entry survives
 *         the original source being removed.
 * Text  - a plain text snippet, stored inline.
 */
struct StagingItem
{
    enum Type {
        File = 0,
        Text = 1,
        Image = 2,
    };

    Type type = File;
    QString id;
    QString uri;   // file path (File), cache path (Image)
    QString text;  // full text (Text)
    QString name;  // display name
    bool checked = false;
    qint64 createdAt = 0;

    bool exists() const;
    QString dedupeKey() const;
    QJsonObject toJson() const;
    static StagingItem fromJson(const QJsonObject &obj);
};
