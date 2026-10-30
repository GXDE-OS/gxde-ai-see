// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QVector>

struct ChatMessage
{
    QString role; // system / user / assistant
    QString content;
};

/**
 * Minimal streaming client for the DeepSeek (OpenAI compatible) chat API.
 */
class ChatClient : public QObject
{
    Q_OBJECT

public:
    explicit ChatClient(QObject *parent = nullptr);

    bool busy() const { return m_reply != nullptr; }
    void send(const QVector<ChatMessage> &messages);
    void cancel();

signals:
    void delta(const QString &text);
    void finished(const QString &text);
    void failed(const QString &error);

private:
    void handleReadyRead();
    void handleFinished();
    void handlePayload(const QByteArray &payload);

    QNetworkAccessManager m_manager;
    class QNetworkReply *m_reply = nullptr;
    QByteArray m_buffer;
    QString m_fullText;
    bool m_done = false;
};
