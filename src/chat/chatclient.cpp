// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "config.h"

namespace {

bool prepareRequest(const QVector<ChatMessage> &messages, bool stream,
                    QNetworkRequest *request, QByteArray *body, QString *error)
{
    const QString apiKey =
        Config::instance()->get(QStringLiteral("chat/apiKey")).toString().trimmed();
    if (apiKey.isEmpty()) {
        *error = QObject::tr("还没有配置 API Key。请先点右上角设置，填入 DeepSeek API Key。");
        return false;
    }

    QString baseUrl = Config::instance()
                          ->get(QStringLiteral("chat/baseUrl"),
                                QStringLiteral("https://api.deepseek.com"))
                          .toString()
                          .trimmed();
    while (baseUrl.endsWith(QLatin1Char('/'))) {
        baseUrl.chop(1);
    }
    if (!baseUrl.endsWith(QStringLiteral("/chat/completions"))) {
        baseUrl += QStringLiteral("/chat/completions");
    }

    QJsonArray jsonMessages;
    for (const ChatMessage &message : messages) {
        QJsonObject object;
        object.insert(QStringLiteral("role"), message.role);
        object.insert(QStringLiteral("content"), message.content);
        jsonMessages.append(object);
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("model"),
                   Config::instance()
                       ->get(QStringLiteral("chat/model"), QStringLiteral("deepseek-chat"))
                       .toString());
    payload.insert(QStringLiteral("messages"), jsonMessages);
    payload.insert(QStringLiteral("stream"), stream);
    payload.insert(QStringLiteral("temperature"), 0.7);

    *request = QNetworkRequest{QUrl(baseUrl)};
    request->setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request->setRawHeader("Authorization", "Bearer " + apiKey.toUtf8());
    request->setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                          QNetworkRequest::NoLessSafeRedirectPolicy);
    *body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    return true;
}

QString errorFromBody(const QByteArray &body, const QString &fallback)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (document.isObject()) {
        const QString detail = document.object()
                                   .value(QStringLiteral("error"))
                                   .toObject()
                                   .value(QStringLiteral("message"))
                                   .toString();
        if (!detail.isEmpty()) {
            return detail;
        }
    }
    return fallback;
}

} // namespace

ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
{
    m_manager.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

void ChatClient::send(const QVector<ChatMessage> &messages)
{
    cancel();
    m_buffer.clear();
    m_fullText.clear();
    m_done = false;

    QNetworkRequest request;
    QByteArray body;
    QString error;
    if (!prepareRequest(messages, true, &request, &body, &error)) {
        Q_EMIT failed(error);
        return;
    }

    m_reply = m_manager.post(request, body);
    connect(m_reply, &QNetworkReply::readyRead, this, &ChatClient::handleReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &ChatClient::handleFinished);
}

void ChatClient::sendOnce(const QVector<ChatMessage> &messages)
{
    if (m_onceReply) {
        m_onceReply->abort();
        m_onceReply->deleteLater();
        m_onceReply = nullptr;
    }

    QNetworkRequest request;
    QByteArray body;
    QString error;
    if (!prepareRequest(messages, false, &request, &body, &error)) {
        Q_EMIT onceFailed(error);
        return;
    }

    m_onceReply = m_manager.post(request, body);
    connect(m_onceReply, &QNetworkReply::finished, this, [this]() {
        if (!m_onceReply) {
            return;
        }
        QNetworkReply *reply = m_onceReply;
        m_onceReply = nullptr;

        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT onceFailed(errorFromBody(reply->readAll(), reply->errorString()));
            reply->deleteLater();
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray choices =
            document.object().value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) {
            Q_EMIT onceFailed(tr("模型没有返回内容"));
            reply->deleteLater();
            return;
        }
        const QString content = choices.first()
                                    .toObject()
                                    .value(QStringLiteral("message"))
                                    .toObject()
                                    .value(QStringLiteral("content"))
                                    .toString();
        Q_EMIT onceFinished(content);
        reply->deleteLater();
    });
}

void ChatClient::cancel()
{
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    if (m_onceReply) {
        QNetworkReply *reply = m_onceReply;
        m_onceReply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}

void ChatClient::handleReadyRead()
{
    if (!m_reply) {
        return;
    }
    m_buffer.append(m_reply->readAll());

    int newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (!line.startsWith("data:")) {
            continue;
        }
        QByteArray payload = line.mid(5).trimmed();
        if (payload == "[DONE]") {
            m_done = true;
            continue;
        }
        handlePayload(payload);
    }
}

void ChatClient::handlePayload(const QByteArray &payload)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        return;
    }
    const QJsonObject object = document.object();

    if (object.contains(QStringLiteral("error"))) {
        const QJsonObject error = object.value(QStringLiteral("error")).toObject();
        Q_EMIT failed(error.value(QStringLiteral("message")).toString(tr("请求失败")));
        return;
    }

    const QJsonArray choices = object.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        return;
    }
    const QJsonObject deltaObject =
        choices.first().toObject().value(QStringLiteral("delta")).toObject();
    const QString content = deltaObject.value(QStringLiteral("content")).toString();
    if (!content.isEmpty()) {
        m_fullText += content;
        Q_EMIT delta(content);
    }
}

void ChatClient::handleFinished()
{
    if (!m_reply) {
        return;
    }
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError &&
        reply->error() != QNetworkReply::OperationCanceledError) {
        QString message = errorFromBody(reply->readAll(), reply->errorString());
        if (status == 401) {
            message = tr("API Key 无效或已过期");
        }
        Q_EMIT failed(message);
        reply->deleteLater();
        return;
    }

    if (!m_buffer.isEmpty()) {
        QByteArray line = m_buffer.trimmed();
        if (line.startsWith("data:")) {
            handlePayload(line.mid(5).trimmed());
        }
        m_buffer.clear();
    }

    Q_EMIT finished(m_fullText);
    reply->deleteLater();
}
