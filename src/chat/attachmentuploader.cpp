// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "attachmentuploader.h"

#include "chatclient.h"
#include "config.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

QString apiError(const QByteArray &body, const QString &fallback)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (document.isObject()) {
        const QJsonValue error = document.object().value(QStringLiteral("error"));
        if (error.isObject()) {
            const QString message =
                error.toObject().value(QStringLiteral("message")).toString();
            if (!message.isEmpty()) {
                return message;
            }
        } else if (error.isString()) {
            const QString message = error.toString();
            if (!message.isEmpty()) {
                return message;
            }
        }
    }
    return fallback;
}

QString formFilename(QString name)
{
    name.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    name.replace(QLatin1Char('\r'), QLatin1Char(' '));
    name.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return name;
}

} // namespace

AttachmentUploader::AttachmentUploader(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

AttachmentUploader::~AttachmentUploader()
{
    cancelAll();
}

bool AttachmentUploader::isUploading(const QString &attachmentId) const
{
    return m_replies.contains(attachmentId);
}

void AttachmentUploader::upload(const ChatAttachment &attachment)
{
    if (!attachment.isImage()) {
        Q_EMIT failed(attachment.id, tr("只有图片需要上传"));
        return;
    }
    if (m_replies.contains(attachment.id)) {
        return;
    }

    const QFileInfo info(attachment.path);
    if (!info.exists() || !info.isFile()) {
        Q_EMIT failed(attachment.id, tr("图片文件不存在"));
        return;
    }
    if (info.size() > AttachmentUtils::kMaxUploadBytes) {
        Q_EMIT failed(attachment.id, tr("图片超过 64 MB，无法上传"));
        return;
    }
    if (chatApiKey().isEmpty()) {
        Q_EMIT failed(attachment.id, tr("还没有配置 API Key"));
        return;
    }

    auto *file = new QFile(info.absoluteFilePath());
    if (!file->open(QIODevice::ReadOnly)) {
        Q_EMIT failed(attachment.id, tr("无法读取图片文件"));
        delete file;
        return;
    }

    QString mimeType = attachment.mimeType;
    if (!mimeType.startsWith(QLatin1String("image/"))) {
        mimeType = QMimeDatabase().mimeTypeForFile(info).name();
    }
    if (!mimeType.startsWith(QLatin1String("image/"))) {
        mimeType = QStringLiteral("image/png");
    }

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart purposePart;
    purposePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QStringLiteral("form-data; name=\"purpose\""));
    purposePart.setBody(QByteArrayLiteral("user_data"));
    multiPart->append(purposePart);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"file\"; filename=\"%1\"")
                           .arg(formFilename(info.fileName())));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QNetworkRequest request{QUrl(chatApiBaseUrl() + QStringLiteral("/files"))};
    request.setRawHeader("Authorization", "Bearer " + chatApiKey().toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_manager->post(request, multiPart);
    multiPart->setParent(reply);
    m_replies.insert(attachment.id, reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, attachment]() {
        const QString attachmentId = attachment.id;
        if (m_replies.value(attachmentId) == reply) {
            m_replies.remove(attachmentId);
        }
        reply->deleteLater();

        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            Q_EMIT failed(attachmentId,
                          apiError(body, reply->errorString().isEmpty()
                                              ? tr("上传失败（HTTP %1）").arg(status)
                                              : reply->errorString()));
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(body);
        const QString fileId =
            document.object().value(QStringLiteral("id")).toString();
        if (fileId.isEmpty()) {
            Q_EMIT failed(attachmentId, tr("Files API 没有返回文件 id"));
            return;
        }
        Q_EMIT uploaded(attachmentId, fileId);
    });
}

void AttachmentUploader::cancelAll()
{
    const QList<QNetworkReply *> replies = m_replies.values();
    m_replies.clear();
    for (QNetworkReply *reply : replies) {
        if (!reply) {
            continue;
        }
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}
