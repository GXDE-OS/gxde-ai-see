// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "attachment.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QMimeType>
#include <QUrl>
#include <QUuid>

namespace {

constexpr qint64 kReadSlackBytes = 64;

QString tr(const char *text)
{
    return QCoreApplication::translate("AttachmentUtils", text);
}

QString formatMime(const QByteArray &format)
{
    const QByteArray lower = format.toLower();
    if (lower == "jpg" || lower == "jpeg") {
        return QStringLiteral("image/jpeg");
    }
    if (lower == "png" || lower == "gif" || lower == "webp") {
        return QStringLiteral("image/") + QString::fromLatin1(lower);
    }
    return QString();
}

bool supportedImageFormat(const QByteArray &format)
{
    return !formatMime(format).isEmpty();
}

QString storeImageBytes(const QImage &image, const QString &dir)
{
    if (image.isNull() || dir.isEmpty()) {
        return QString();
    }
    if (!QDir().mkpath(dir)) {
        return QString();
    }
    const QString target =
        dir + QLatin1Char('/') + AttachmentUtils::newId() + QStringLiteral(".png");
    return image.save(target, "PNG") ? target : QString();
}

QString copyOrConvertImage(const QFileInfo &info, const QByteArray &format,
                           const QString &dir)
{
    if (dir.isEmpty()) {
        return QString();
    }
    if (!QDir().mkpath(dir)) {
        return QString();
    }

    const QString base = dir + QLatin1Char('/') + AttachmentUtils::newId();
    if (supportedImageFormat(format)) {
        const QString extension = format.toLower() == QByteArray("jpeg")
            ? QStringLiteral(".jpg")
            : QLatin1Char('.') + QString::fromLatin1(format.toLower());
        const QString target = base + extension;
        if (QFile::copy(info.absoluteFilePath(), target)) {
            return target;
        }
    }

    /* Formats the vision API does not accept (BMP, TIFF, …) are re-encoded. */
    QImageReader reader(info.absoluteFilePath());
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    return storeImageBytes(image, dir);
}

QString decodeText(const QByteArray &bytes)
{
    if (bytes.isEmpty()) {
        return QString();
    }
    /* UTF-16 with a BOM is common in Windows exports. */
    if (bytes.startsWith("\xff\xfe") || bytes.startsWith("\xfe\xff")) {
        return QString::fromUtf16(reinterpret_cast<const char16_t *>(bytes.constData() + 2),
                                  (bytes.size() - 2) / 2);
    }
    if (bytes.contains('\0')) {
        return QString();
    }
    QString text = QString::fromUtf8(bytes);
    if (text.contains(QChar::ReplacementCharacter)) {
        text = QString::fromLocal8Bit(bytes);
    }
    return text;
}

QString readTextFile(const QString &path, qint64 maxChars, bool *truncated)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    /* UTF-8 never needs more than four bytes per character. */
    const qint64 maxBytes = maxChars * 4 + kReadSlackBytes;
    QByteArray bytes = file.read(maxBytes);
    const bool clipped = file.bytesAvailable() > 0;
    QString text = decodeText(bytes);
    if (clipped) {
        if (truncated) {
            *truncated = true;
        }
        if (text.size() > maxChars) {
            text.truncate(maxChars);
        }
    }
    /* A clipped multi byte sequence at the very end would decode to U+FFFD. */
    while (text.endsWith(QChar::ReplacementCharacter)) {
        text.chop(1);
    }
    return text;
}

QString textAttachmentName(const QString &text, bool withExtension)
{
    QString first = text.section(QLatin1Char('\n'), 0, 0).trimmed();
    first = first.simplified();
    if (first.isEmpty()) {
        first = QCoreApplication::translate("AttachmentUtils", "长文本");
    }
    const bool clipped = first.size() > 28;
    first = first.left(28);
    if (clipped) {
        first += QStringLiteral("…");
    }
    return withExtension ? first + QStringLiteral(".txt") : first;
}

const QStringList &textExtensions()
{
    static const QStringList extensions = {
        QStringLiteral("txt"),   QStringLiteral("md"),    QStringLiteral("markdown"),
        QStringLiteral("rst"),   QStringLiteral("log"),   QStringLiteral("csv"),
        QStringLiteral("tsv"),   QStringLiteral("json"),  QStringLiteral("jsonl"),
        QStringLiteral("yaml"),  QStringLiteral("yml"),   QStringLiteral("toml"),
        QStringLiteral("ini"),   QStringLiteral("conf"),  QStringLiteral("cfg"),
        QStringLiteral("xml"),   QStringLiteral("html"),  QStringLiteral("htm"),
        QStringLiteral("css"),   QStringLiteral("scss"),  QStringLiteral("less"),
        QStringLiteral("js"),    QStringLiteral("mjs"),   QStringLiteral("cjs"),
        QStringLiteral("ts"),    QStringLiteral("tsx"),   QStringLiteral("jsx"),
        QStringLiteral("vue"),   QStringLiteral("py"),    QStringLiteral("rb"),
        QStringLiteral("pl"),    QStringLiteral("php"),   QStringLiteral("lua"),
        QStringLiteral("sh"),    QStringLiteral("bash"),  QStringLiteral("zsh"),
        QStringLiteral("fish"),  QStringLiteral("bat"),   QStringLiteral("ps1"),
        QStringLiteral("c"),     QStringLiteral("h"),     QStringLiteral("cc"),
        QStringLiteral("cpp"),   QStringLiteral("cxx"),   QStringLiteral("hpp"),
        QStringLiteral("hxx"),   QStringLiteral("cs"),    QStringLiteral("java"),
        QStringLiteral("kt"),    QStringLiteral("kts"),   QStringLiteral("go"),
        QStringLiteral("rs"),    QStringLiteral("swift"), QStringLiteral("sql"),
        QStringLiteral("cmake"), QStringLiteral("gradle"), QStringLiteral("patch"),
        QStringLiteral("diff"),  QStringLiteral("desktop"), QStringLiteral("service"),
        QStringLiteral("qml"),   QStringLiteral("ui"),    QStringLiteral("pro"),
        QStringLiteral("pri"),   QStringLiteral("tex"),   QStringLiteral("srt"),
        QStringLiteral("vtt"),
    };
    return extensions;
}

} // namespace

namespace AttachmentUtils {

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString displaySize(qint64 bytes)
{
    if (bytes < 0) {
        return QString();
    }
    if (bytes < 1024) {
        return tr("%1 B").arg(bytes);
    }
    const qreal kb = bytes / 1024.0;
    if (kb < 1024.0) {
        return tr("%1 KB").arg(QString::number(kb, 'f', kb < 10.0 ? 1 : 0));
    }
    const qreal mb = kb / 1024.0;
    if (mb < 1024.0) {
        return tr("%1 MB").arg(QString::number(mb, 'f', mb < 10.0 ? 1 : 0));
    }
    return tr("%1 GB").arg(QString::number(mb / 1024.0, 'f', 2));
}

bool looksLikeText(const QString &mimeType, const QString &path)
{
    if (mimeType.startsWith(QLatin1String("text/"))) {
        return true;
    }
    static const QStringList textMimeTypes = {
        QStringLiteral("application/json"),
        QStringLiteral("application/ld+json"),
        QStringLiteral("application/xml"),
        QStringLiteral("application/javascript"),
        QStringLiteral("application/x-javascript"),
        QStringLiteral("application/x-yaml"),
        QStringLiteral("application/yaml"),
        QStringLiteral("application/x-sh"),
        QStringLiteral("application/x-shellscript"),
        QStringLiteral("application/x-python"),
        QStringLiteral("application/x-perl"),
        QStringLiteral("application/x-php"),
        QStringLiteral("application/x-httpd-php"),
        QStringLiteral("application/x-desktop"),
        QStringLiteral("application/x-qml"),
        QStringLiteral("application/cmake"),
    };
    if (textMimeTypes.contains(mimeType)) {
        return true;
    }
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (textExtensions().contains(suffix)) {
        return true;
    }
    const QString name = info.fileName().toLower();
    return name == QLatin1String("makefile") || name == QLatin1String("dockerfile") ||
        name == QLatin1String("cmakelists.txt") || name == QLatin1String("license") ||
        name == QLatin1String("readme") || name == QLatin1String("changelog");
}

bool isLongText(const QString &text)
{
    return text.size() > kLongTextThreshold;
}

ChatAttachment fromText(const QString &text, const QString &name)
{
    ChatAttachment attachment;
    if (text.trimmed().isEmpty()) {
        return attachment;
    }
    attachment.id = newId();
    attachment.kind = ChatAttachment::Text;
    attachment.name = name.trimmed().isEmpty() ? textAttachmentName(text, true)
                                               : name.trimmed();
    attachment.mimeType = QStringLiteral("text/plain");
    attachment.size = text.toUtf8().size();
    attachment.text = text;
    if (attachment.text.size() > kMaxTextChars) {
        attachment.text.truncate(kMaxTextChars);
        attachment.truncated = true;
    }
    return attachment;
}

ChatAttachment fromFile(const QString &path, const QString &imageStoreDir)
{
    ChatAttachment attachment;
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return attachment;
    }

    attachment.id = newId();
    attachment.name = info.fileName();
    attachment.path = info.absoluteFilePath();
    attachment.size = info.size();

    const QMimeType mime = QMimeDatabase().mimeTypeForFile(info);
    attachment.mimeType = mime.name();

    QImageReader reader(attachment.path);
    reader.setAutoTransform(true);
    if (mime.name().startsWith(QLatin1String("image/")) && reader.canRead()) {
        attachment.kind = ChatAttachment::Image;
        const QByteArray format = reader.format().toLower();
        const QString stored = copyOrConvertImage(info, format, imageStoreDir);
        if (!stored.isEmpty()) {
            attachment.path = stored;
        }
        const QFileInfo storedInfo(attachment.path);
        attachment.size = storedInfo.size();
        const QByteArray storedFormat = QImageReader(attachment.path).format().toLower();
        attachment.mimeType = formatMime(storedFormat);
        if (attachment.mimeType.isEmpty()) {
            attachment.mimeType = QStringLiteral("image/png");
        }
        return attachment;
    }

    if (looksLikeText(attachment.mimeType, attachment.path)) {
        bool truncated = false;
        attachment.text = readTextFile(attachment.path, kMaxTextChars, &truncated);
        attachment.truncated = truncated;
    }
    return attachment;
}

ChatAttachment fromImage(const QImage &image, const QString &suggestedName,
                         const QString &imageStoreDir)
{
    ChatAttachment attachment;
    if (image.isNull()) {
        return attachment;
    }
    const QString stored = storeImageBytes(image, imageStoreDir);
    if (stored.isEmpty()) {
        return attachment;
    }
    attachment.id = newId();
    attachment.kind = ChatAttachment::Image;
    attachment.name = suggestedName.trimmed().isEmpty()
            ? QCoreApplication::translate("AttachmentUtils", "图片.png")
                                                        : suggestedName.trimmed();
    attachment.path = stored;
    attachment.mimeType = QStringLiteral("image/png");
    attachment.size = QFileInfo(stored).size();
    return attachment;
}

QString promptText(const QString &userText, const QVector<ChatAttachment> &attachments)
{
    QString out = userText;
    qint64 budget = kMaxPromptChars;

    for (const ChatAttachment &attachment : attachments) {
        if (!attachment.isValid()) {
            continue;
        }

        const QString type = attachment.mimeType.isEmpty()
            ? tr("未知类型")
            : attachment.mimeType;
        QString header = tr("【附件：%1（%2，%3）】")
                             .arg(attachment.name, type, displaySize(attachment.size));
        if (attachment.truncated) {
            header += tr("（内容过长，已截断）");
        }

        if (attachment.kind == ChatAttachment::Image) {
            /* The image itself travels in a separate content block; the text
             * block carries its name so an image-only message is still
             * well-formed for the model. */
            out += QStringLiteral("\n\n") + header;
            continue;
        }

        QString body = attachment.text;
        if (body.isEmpty() && !attachment.path.isEmpty() &&
            looksLikeText(attachment.mimeType, attachment.path)) {
            body = readTextFile(attachment.path, kMaxTextChars, nullptr);
        }

        if (body.isEmpty()) {
            if (attachment.kind == ChatAttachment::File) {
                out += QStringLiteral("\n\n") + header + QLatin1Char('\n') +
                    tr("（二进制文件，无法读取文本内容，仅提供文件信息）");
            }
            continue;
        }

        if (budget <= 0) {
            out += QStringLiteral("\n\n") + tr("（其余附件因长度限制未发送）");
            break;
        }

        QString chunk = body;
        if (chunk.size() > budget) {
            chunk.truncate(budget);
            out += QStringLiteral("\n\n") + header + QLatin1Char('\n') + chunk +
                QLatin1Char('\n') + tr("…（附件内容过多，已截断）");
            budget = 0;
        } else {
            out += QStringLiteral("\n\n") + header + QLatin1Char('\n') + chunk;
            budget -= chunk.size();
        }
        out += QLatin1Char('\n') + tr("【附件结束】");
    }

    return out;
}

QJsonValue contentJson(const QString &userText, const QVector<ChatAttachment> &attachments)
{
    const QString text = promptText(userText, attachments);

    QJsonArray imageBlocks;
    for (const ChatAttachment &attachment : attachments) {
        if (!attachment.isImage()) {
            continue;
        }
        if (!attachment.fileId.isEmpty()) {
            imageBlocks.append(QJsonObject{
                { QStringLiteral("type"), QStringLiteral("file") },
                { QStringLiteral("file_id"), attachment.fileId },
            });
            continue;
        }
        const QString url = dataUrl(attachment);
        if (!url.isEmpty()) {
            imageBlocks.append(QJsonObject{
                { QStringLiteral("type"), QStringLiteral("image_url") },
                { QStringLiteral("image_url"),
                  QJsonObject{ { QStringLiteral("url"), url } } },
            });
        }
    }

    if (imageBlocks.isEmpty()) {
        return text;
    }

    QJsonArray blocks;
    if (!text.isEmpty()) {
        blocks.append(QJsonObject{ { QStringLiteral("type"), QStringLiteral("text") },
                                   { QStringLiteral("text"), text } });
    }
    for (const QJsonValue &block : imageBlocks) {
        blocks.append(block);
    }
    return blocks;
}

QString dataUrl(const ChatAttachment &attachment)
{
    if (!attachment.isImage() || attachment.path.isEmpty()) {
        return QString();
    }
    const QFileInfo info(attachment.path);
    if (!info.exists() || !info.isFile() || info.size() > kMaxInlineImageBytes) {
        return QString();
    }
    QFile file(attachment.path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) {
        return QString();
    }
    QString mime = attachment.mimeType;
    if (!mime.startsWith(QLatin1String("image/"))) {
        mime = QMimeDatabase().mimeTypeForFile(info).name();
    }
    if (!mime.startsWith(QLatin1String("image/"))) {
        mime = QStringLiteral("image/png");
    }
    return QStringLiteral("data:%1;base64,%2")
        .arg(mime, QString::fromLatin1(bytes.toBase64()));
}

} // namespace AttachmentUtils
