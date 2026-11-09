// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include "aicore.h"
#include "chatclient.h"

/** Names conversations with the model, each in a throwaway AI Core session. */
class ChatTitler : public QObject
{
    Q_OBJECT

public:
    explicit ChatTitler(QObject *parent = nullptr);

    /** \a done gets the title as a string, or an error. */
    void summarize(const QString &question, const QString &answer, const ChatProvider &provider,
                   AiCore::Callback done);

private:
    void useProvider(const QString &sessionId, const QString &text, const ChatProvider &provider,
                     AiCore::Callback done);
    void ask(const QString &sessionId, const QString &text, AiCore::Callback done);
    void handleUpdate(const QString &sessionId, const QJsonObject &update);

    QHash<QString, QString> m_replies; // by session id
};
