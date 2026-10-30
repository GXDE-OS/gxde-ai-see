// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "contentclassifier.h"

#include <QFileInfo>
#include <QImageReader>
#include <QMimeData>
#include <QUrl>

namespace {

QString extensionOfFile(const QString &path)
{
    return QFileInfo(path).suffix().toLower();
}

} // namespace

QString DropContent::summary() const
{
    switch (kind) {
    case Files: {
        if (filePaths.size() == 1) {
            const QFileInfo info(filePaths.first());
            return QStringLiteral("文件 · %1").arg(info.fileName());
        }
        return QStringLiteral("%1 个文件").arg(filePaths.size());
    }
    case Image:
        return QStringLiteral("图片");
    case Text:
        return QStringLiteral("文字 · %1 字").arg(text.size());
    case Link:
    case CustomLink: {
        const QString label = link.fileType != QStringLiteral("page") && !link.fileType.isEmpty()
                                  ? link.fileType
                                  : link.kindLabel();
        return link.displayName.isEmpty() ? label
                                          : QStringLiteral("%1 · %2").arg(label, link.displayName);
    }
    default:
        return QString();
    }
}

namespace ContentClassifier {

DropContent classifyText(const QString &text)
{
    DropContent content;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return content;
    }

    const LinkInfo info = LinkTools::parse(trimmed);
    if (info.isValid()) {
        content.kind = info.kind == LinkInfo::Http ? DropContent::Link : DropContent::CustomLink;
        content.link = info;
        content.text = trimmed;
        return content;
    }

    content.kind = DropContent::Text;
    content.text = text;
    return content;
}

DropContent classify(const QMimeData *mime)
{
    DropContent content;
    if (!mime) {
        return content;
    }

    content.mimes = mime->formats();

    if (mime->hasUrls()) {
        QStringList localFiles;
        LinkInfo link;
        for (const QUrl &url : mime->urls()) {
            if (url.isLocalFile()) {
                localFiles.append(url.toLocalFile());
            } else if (!link.isValid()) {
                link = LinkTools::parseUrl(url);
            }
        }

        if (!localFiles.isEmpty()) {
            content.kind = DropContent::Files;
            content.filePaths = localFiles;
            if (localFiles.size() == 1) {
                const QString suffix = extensionOfFile(localFiles.first());
                if (LinkTools::fileTypeForExtension(suffix) == QStringLiteral("image") &&
                    QImageReader(localFiles.first()).canRead()) {
                    content.kind = DropContent::Image;
                    content.image = QImage(localFiles.first());
                }
            }
            return content;
        }

        if (link.isValid()) {
            content.kind =
                link.kind == LinkInfo::Http ? DropContent::Link : DropContent::CustomLink;
            content.link = link;
            content.text = link.url.toString();
            return content;
        }
    }

    if (mime->hasImage()) {
        content.image = qvariant_cast<QImage>(mime->imageData());
        if (!content.image.isNull()) {
            content.kind = DropContent::Image;
            return content;
        }
    }

    if (mime->hasText()) {
        return classifyText(mime->text());
    }

    return content;
}

} // namespace ContentClassifier
