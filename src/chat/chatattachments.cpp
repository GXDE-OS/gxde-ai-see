// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatattachments.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QUrl>

namespace {

constexpr qint64 kMaxInlineImage = 20 * 1024 * 1024;
constexpr qint64 kMaxInlineText = 512 * 1024;

QJsonObject textBlock(const QString &text)
{
    return QJsonObject{ { QStringLiteral("type"), QStringLiteral("text") },
                        { QStringLiteral("text"), text } };
}

bool isAttachmentLine(const QString &line, QString *path)
{
    for (const QString &prefix :
         { QStringLiteral("[文件] "), QStringLiteral("[图片] "), QStringLiteral("[附件] ") }) {
        if (line.startsWith(prefix)) {
            *path = line.mid(prefix.size()).trimmed();
            return !path->isEmpty();
        }
    }
    return false;
}

bool isInlineTextMime(const QString &name)
{
    return name.startsWith(QLatin1String("text/")) || name == QLatin1String("application/json")
           || name == QLatin1String("application/xml") || name == QLatin1String("application/x-yaml")
           || name == QLatin1String("application/javascript");
}

/* ACP: 图片用 resource 块带 base64。只要同一条 prompt 里还有普通文件，
 * 单独的 image 块会被服务端静默丢掉，resource 块则不会。 */
QJsonObject imageBlock(const QString &path, const QString &mimeType)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxInlineImage) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) {
        return {};
    }
    return QJsonObject{
        { QStringLiteral("type"), QStringLiteral("resource") },
        { QStringLiteral("resource"),
          QJsonObject{
              { QStringLiteral("uri"), QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded) },
              { QStringLiteral("mimeType"), mimeType },
              { QStringLiteral("blob"), QString::fromLatin1(bytes.toBase64()) },
          } },
    };
}

} // namespace

namespace ChatAttachments {

bool isImageFile(const QString &path)
{
    const QMimeDatabase mimeDatabase;
    return mimeDatabase.mimeTypeForFile(QFileInfo(path)).name().startsWith(QLatin1String("image/"));
}

QString takeLines(const QString &text, QStringList *paths)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    QStringList kept;
    for (const QString &line : lines) {
        QString path;
        if (isAttachmentLine(line.trimmed(), &path)) {
            paths->append(path);
        } else {
            kept.append(line);
        }
    }
    return kept.join(QLatin1Char('\n')).trimmed();
}

QJsonArray promptBlocks(const QString &context, const QString &text, const QStringList &paths)
{
    QJsonArray blocks;
    if (!context.isEmpty()) {
        blocks.append(textBlock(context));
    }
    if (!text.isEmpty()) {
        blocks.append(textBlock(text));
    }

    const QMimeDatabase mimeDatabase;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (!info.exists()) {
            blocks.append(
                textBlock(QObject::tr("【附件】%1 不存在，可能已被移动或删除").arg(info.fileName())));
            continue;
        }
        const QMimeType mime = mimeDatabase.mimeTypeForFile(info);
        const QString name = info.fileName();
        if (mime.name().startsWith(QLatin1String("image/"))) {
            const QJsonObject image = imageBlock(info.absoluteFilePath(), mime.name());
            if (!image.isEmpty()) {
                blocks.append(image);
                continue;
            }
        }
        if (isInlineTextMime(mime.name())) {
            QFile file(info.absoluteFilePath());
            if (file.open(QIODevice::ReadOnly) && file.size() <= kMaxInlineText) {
                blocks.append(textBlock(QObject::tr("【附件】%1\n%2")
                                            .arg(name, QString::fromUtf8(file.readAll()))));
                continue;
            }
        }
        blocks.append(textBlock(QObject::tr("【附件】%1（路径：%2，类型：%3）无法直接内嵌，"
                                            "需要时请用读取文件的工具查看。")
                                    .arg(name, info.absoluteFilePath(), mime.name())));
    }

    if (blocks.isEmpty()) {
        blocks.append(textBlock(QString()));
    }
    return blocks;
}

} // namespace ChatAttachments
