// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QObject>
#include <QString>

#include "attachment.h"

class QNetworkAccessManager;
class QNetworkReply;

/**
 * Uploads image attachments to the DeepSeek Files API (POST /files).
 *
 * One instance is owned by the chat window.  Results are delivered by local
 * attachment id so the composer and already-sent messages can be updated.
 */
class AttachmentUploader : public QObject
{
    Q_OBJECT

public:
    explicit AttachmentUploader(QObject *parent = nullptr);
    ~AttachmentUploader() override;

    void upload(const ChatAttachment &attachment);
    bool isUploading(const QString &attachmentId) const;
    void cancelAll();

signals:
    void uploaded(const QString &attachmentId, const QString &fileId);
    void failed(const QString &attachmentId, const QString &error);

private:
    QNetworkAccessManager *m_manager = nullptr;
    QHash<QString, QNetworkReply *> m_replies;
};
