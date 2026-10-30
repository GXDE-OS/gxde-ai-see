// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "linktools.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QUrlQuery>

namespace {

const QStringList kImageExtensions = {
    QStringLiteral("jpg"),  QStringLiteral("jpeg"), QStringLiteral("png"),
    QStringLiteral("gif"),  QStringLiteral("webp"), QStringLiteral("svg"),
    QStringLiteral("bmp"),  QStringLiteral("avif"), QStringLiteral("heic"),
    QStringLiteral("ico"),
};

const QStringList kAudioExtensions = {
    QStringLiteral("mp3"),  QStringLiteral("flac"), QStringLiteral("wav"),
    QStringLiteral("ogg"),  QStringLiteral("oga"),  QStringLiteral("m4a"),
    QStringLiteral("aac"),  QStringLiteral("opus"), QStringLiteral("wma"),
};

const QStringList kVideoExtensions = {
    QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("webm"),
    QStringLiteral("avi"), QStringLiteral("mov"), QStringLiteral("flv"),
    QStringLiteral("m4v"), QStringLiteral("ts"),
};

const QStringList kDocumentExtensions = {
    QStringLiteral("pdf"),  QStringLiteral("doc"),  QStringLiteral("docx"),
    QStringLiteral("xls"),  QStringLiteral("xlsx"), QStringLiteral("ppt"),
    QStringLiteral("pptx"), QStringLiteral("odt"),  QStringLiteral("ods"),
    QStringLiteral("txt"),  QStringLiteral("md"),   QStringLiteral("rtf"),
    QStringLiteral("epub"),
};

const QStringList kArchiveExtensions = {
    QStringLiteral("zip"), QStringLiteral("7z"),  QStringLiteral("rar"),
    QStringLiteral("tar"), QStringLiteral("gz"),  QStringLiteral("bz2"),
    QStringLiteral("xz"),  QStringLiteral("zst"), QStringLiteral("deb"),
    QStringLiteral("rpm"), QStringLiteral("apk"), QStringLiteral("iso"),
};

const QStringList kCodeExtensions = {
    QStringLiteral("json"), QStringLiteral("xml"), QStringLiteral("html"),
    QStringLiteral("htm"),  QStringLiteral("js"),  QStringLiteral("css"),
    QStringLiteral("py"),   QStringLiteral("c"),   QStringLiteral("cpp"),
    QStringLiteral("h"),    QStringLiteral("sh"),  QStringLiteral("go"),
    QStringLiteral("rs"),   QStringLiteral("java"),
};

QString extensionOfPath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix;
}

} // namespace

QString LinkInfo::kindLabel() const
{
    switch (kind) {
    case Http:
        return fileType == QStringLiteral("page") ? QStringLiteral("网页链接")
                                                  : QStringLiteral("文件链接");
    case Standard:
        return QStringLiteral("链接");
    case Custom:
        return QStringLiteral("应用链接");
    default:
        return QString();
    }
}

namespace LinkTools {

QString fileTypeForExtension(const QString &extension)
{
    if (extension.isEmpty()) {
        return QStringLiteral("page");
    }
    if (kImageExtensions.contains(extension)) {
        return QStringLiteral("image");
    }
    if (kAudioExtensions.contains(extension)) {
        return QStringLiteral("audio");
    }
    if (kVideoExtensions.contains(extension)) {
        return QStringLiteral("video");
    }
    if (kDocumentExtensions.contains(extension)) {
        return QStringLiteral("document");
    }
    if (kArchiveExtensions.contains(extension)) {
        return QStringLiteral("archive");
    }
    if (kCodeExtensions.contains(extension)) {
        return QStringLiteral("code");
    }
    return QStringLiteral("unknown");
}

QString decodeThunder(const QString &text)
{
    static const QString prefix = QStringLiteral("thunder://");
    if (!text.startsWith(prefix, Qt::CaseInsensitive)) {
        return QString();
    }

    QString payload = text.mid(prefix.size()).trimmed();
    payload.replace(QLatin1Char('-'), QLatin1Char('+'));
    payload.replace(QLatin1Char('_'), QLatin1Char('/'));

    QByteArray encoded = payload.toLatin1();
    while (encoded.size() % 4 != 0) {
        encoded.append('=');
    }

    const QByteArray decoded = QByteArray::fromBase64(encoded);
    QString value = QString::fromUtf8(decoded);
    if (value.startsWith(QStringLiteral("AA"))) {
        value.remove(0, 2);
    }
    if (value.endsWith(QStringLiteral("ZZ"))) {
        value.chop(2);
    }
    return value.trimmed();
}

QString displayNameFor(const QUrl &url)
{
    if (url.scheme().compare(QStringLiteral("magnet"), Qt::CaseInsensitive) == 0) {
        const QUrlQuery query(url);
        const QString name = query.queryItemValue(QStringLiteral("dn"));
        if (!name.isEmpty()) {
            return name;
        }
    }

    QString path = url.path();
    const QString name = path.section(QLatin1Char('/'), -1, -1, QString::SectionSkipEmpty);
    if (!name.isEmpty()) {
        return name;
    }
    return url.host();
}

LinkInfo parseUrl(const QUrl &url)
{
    LinkInfo info;
    if (!url.isValid() || url.scheme().isEmpty()) {
        return info;
    }

    info.url = url;
    info.scheme = url.scheme().toLower();
    info.displayName = displayNameFor(url);

    if (info.scheme == QStringLiteral("http") || info.scheme == QStringLiteral("https")) {
        info.kind = LinkInfo::Http;
        info.extension = extensionOfPath(url.path());
        info.fileType = fileTypeForExtension(info.extension);
        info.looksLikeFile = info.fileType != QStringLiteral("page") &&
                             info.fileType != QStringLiteral("unknown");
    } else if (info.scheme == QStringLiteral("thunder")) {
        info.kind = LinkInfo::Custom;
        info.decodedUrl = decodeThunder(url.toString());
        if (!info.decodedUrl.isEmpty()) {
            const QUrl real(info.decodedUrl);
            info.extension = extensionOfPath(real.path());
            info.fileType = fileTypeForExtension(info.extension);
            info.displayName = displayNameFor(real);
        }
    } else if (info.scheme == QStringLiteral("magnet") || info.scheme == QStringLiteral("ed2k") ||
               info.scheme == QStringLiteral("spk") ||
               info.scheme == QStringLiteral("x-scheme-handler")) {
        info.kind = LinkInfo::Custom;
    } else if (info.scheme == QStringLiteral("ftp") || info.scheme == QStringLiteral("mailto") ||
               info.scheme == QStringLiteral("tel") || info.scheme == QStringLiteral("file")) {
        info.kind = LinkInfo::Standard;
    } else {
        info.kind = LinkInfo::Custom;
    }

    return info;
}

LinkInfo parse(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.contains(QLatin1Char('\n'))) {
        return LinkInfo();
    }

    if (trimmed.startsWith(QStringLiteral("www."), Qt::CaseInsensitive)) {
        return parseUrl(QUrl(QStringLiteral("https://") + trimmed));
    }

    /* QUrl lowercases the host part, which destroys base64 payloads such as
     * thunder://.  Handle these schemes on the raw string. */
    if (trimmed.startsWith(QStringLiteral("thunder://"), Qt::CaseInsensitive)) {
        LinkInfo info;
        info.kind = LinkInfo::Custom;
        info.raw = trimmed;
        info.scheme = QStringLiteral("thunder");
        info.url = QUrl(trimmed);
        info.decodedUrl = decodeThunder(trimmed);
        if (!info.decodedUrl.isEmpty()) {
            const QUrl real(info.decodedUrl);
            info.extension = extensionOfPath(real.path());
            info.fileType = fileTypeForExtension(info.extension);
            info.displayName = displayNameFor(real);
        }
        return info;
    }

    static const QRegularExpression schemePattern(
        QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*://?"), QRegularExpression::CaseInsensitiveOption);
    if (!schemePattern.match(trimmed).hasMatch() &&
        !trimmed.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive) &&
        !trimmed.startsWith(QStringLiteral("mailto:"), Qt::CaseInsensitive) &&
        !trimmed.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive)) {
        return LinkInfo();
    }

    LinkInfo info = parseUrl(QUrl(trimmed));
    if (info.isValid() && info.raw.isEmpty()) {
        info.raw = trimmed;
    }
    return info;
}

bool isHttpLink(const QString &text)
{
    const LinkInfo info = parse(text);
    return info.kind == LinkInfo::Http;
}

} // namespace LinkTools
