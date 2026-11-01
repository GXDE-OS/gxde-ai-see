// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QVector>

class QImage;
/**
 * One attachment of a chat message.
 *
 * Images are uploaded through the DeepSeek Files API and referenced by
 * \a fileId.  Text (source files, dropped long text) is carried inline in
 * \a text and injected into the prompt, because the Files API only accepts
 * images.  Other binaries keep their metadata and path.
 */
struct ChatAttachment
{
    enum Kind {
        File = 0,  ///< a local file (text files also carry inline text)
        Image = 1, ///< a local image, uploaded for the vision model
        Text = 2,  ///< long text produced by a drop/paste, no file on disk
    };

    QString id;        ///< stable local id (also used to match upload results)
    QString name;      ///< display name
    QString path;      ///< local path (cache copy for images)
    QString mimeType;  ///< sniffed MIME type
    qint64 size = 0;   ///< byte size (UTF-8 size for pure text)
    Kind kind = File;
    QString text;      ///< inline content for text files / long text
    QString fileId;    ///< DeepSeek Files API id once an image was uploaded
    bool truncated = false; ///< text / image content was clipped

    bool isImage() const { return kind == Image; }
    bool isValid() const
    {
        return !id.isEmpty() && (!name.isEmpty() || !path.isEmpty() || !text.isEmpty());
    }
};

namespace AttachmentUtils {

/** Text longer than this is turned into an attachment instead of the input. */
constexpr int kLongTextThreshold = 1000;

/** Upper bound for inline text from one attachment (characters). */
constexpr qint64 kMaxTextChars = 512 * 1024;

/** Upper bound for the whole attachment text injected into one prompt. */
constexpr qint64 kMaxPromptChars = 1024 * 1024;

/** DeepSeek rejects inline images larger than this. */
constexpr qint64 kMaxInlineImageBytes = 32LL * 1024 * 1024;

/** DeepSeek Files API accepts at most this much per image. */
constexpr qint64 kMaxUploadBytes = 64LL * 1024 * 1024;

QString newId();

/** Human readable byte size ("12.3 MB"). */
QString displaySize(qint64 bytes);

bool looksLikeText(const QString &mimeType, const QString &path);
bool isLongText(const QString &text);

/**
 * Build an attachment from a long text chunk.
 * The display name is derived from the first line.
 */
ChatAttachment fromText(const QString &text, const QString &name = QString());

/**
 * Build an attachment from a local file.
 *
 * @param imageStoreDir where dragged images are copied (so they survive the
 *                      original being moved); JPEG/PNG/GIF/WebP keep their
 *                      format, other decodable images are re-encoded as PNG.
 */
ChatAttachment fromFile(const QString &path, const QString &imageStoreDir);

/** Build an attachment from an in-memory image (clipboard drop). */
ChatAttachment fromImage(const QImage &image, const QString &suggestedName,
                         const QString &imageStoreDir);

/**
 * The user text with all text attachments expanded into clearly delimited
 * sections.  Image attachments are not represented here.
 */
QString promptText(const QString &userText, const QVector<ChatAttachment> &attachments);

/**
 * JSON value for a user message's "content": a plain string when there are no
 * images, otherwise an array of text/file/image blocks.
 */
QJsonValue contentJson(const QString &userText, const QVector<ChatAttachment> &attachments);

/** data: URL for an inline image, or an empty string on failure. */
QString dataUrl(const ChatAttachment &attachment);

} // namespace AttachmentUtils
