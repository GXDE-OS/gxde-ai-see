// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

/**
 * Classification of a link dropped onto the content factory.
 */
struct LinkInfo
{
    enum Kind {
        None,
        Http,      // http(s) URL
        Standard,  // other standard URL (ftp, mailto, tel, file)
        Custom,    // app specific scheme (thunder, spk, magnet, ed2k, ...)
        Other,
    };

    Kind kind = None;
    QUrl url;
    QString scheme;
    QString extension;   // lower case, without dot
    QString fileType;    // image / audio / video / document / archive / code / page / unknown
    QString raw;         // original text, preserved for custom schemes
    QString decodedUrl;  // thunder:// and friends
    QString displayName; // magnet dn / last path segment
    bool looksLikeFile = false;

    bool isValid() const { return kind != None; }
    QString kindLabel() const;
};

namespace LinkTools {

LinkInfo parse(const QString &text);
LinkInfo parseUrl(const QUrl &url);

/** Map a file extension to a coarse content type. */
QString fileTypeForExtension(const QString &extension);

/** Decode thunder:// (base64 of "AA" + url + "ZZ") and magnet display name. */
QString decodeThunder(const QString &text);

QString displayNameFor(const QUrl &url);

bool isHttpLink(const QString &text);

} // namespace LinkTools
