// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QVector>

#include "aicore.h"

struct ChatMessage
{
    QString role; // system / user / assistant
    QString content;
};

/**
 * Runs the chat turns of the conversations on GXDE AI Core.
 *
 * Every conversation gets its own AI Core session, which keeps the history,
 * compresses it and runs the tools.  The conversation list stays the source
 * of truth: when it no longer matches what the session saw (a message was
 * edited, an answer retried, the agent restarted …) the next turn starts a
 * fresh session and hands it the earlier messages as context.
 */
class ChatClient : public QObject
{
    Q_OBJECT

public:
    /** ACP session modes, in the order the UI offers them. */
    static QStringList modeIds();
    static QString modeLabel(const QString &modeId);

    explicit ChatClient(QObject *parent = nullptr);
    ~ChatClient() override;

    bool busy() const { return !m_turnConversation.isEmpty(); }

    /**
     * Answer the last message of \a history, which must be the user's.
     * \a conversationId ties the turn to its AI Core session.
     */
    void send(const QString &conversationId, const QVector<ChatMessage> &history);
    void cancel();

    /** Drop the session of a conversation that was deleted. */
    void forget(const QString &conversationId);
    /** Start every conversation on a fresh session next time (settings changed). */
    void resetSessions();

    QString mode() const { return m_mode; }
    /** Applies to the running sessions and every new one. */
    void setMode(const QString &modeId);

    /** Answer a pending permission request; empty \a optionId rejects. */
    void answerPermission(const QString &key, const QString &optionId);

signals:
    void delta(const QString &text);
    /** A tool started, progressed or ended (\a status: pending, in_progress, completed, failed). */
    void toolCall(const QString &toolCallId, const QString &title, const QString &kind,
                  const QString &status);
    /** \a key identifies the request for answerPermission(). */
    void permissionRequested(const QString &key, const AiCorePermission &request);
    /** The request disappeared without an answer from here (cancelled turn). */
    void permissionDropped(const QString &key);
    void finished(const QString &stopReason);
    void failed(const QString &error);
    /**
     * The running turn's session reported something (text, thoughts, tools,
     * usage).  AI Core retries failed model requests for minutes without a
     * word, so a long silence is worth telling the user about.
     */
    void activity();

private:
    struct Session
    {
        QString id;
        /* conversation messages the session has seen */
        int synced = 0;
    };

    void startTurn(const QString &conversationId, const QVector<ChatMessage> &history);
    void prompt(const QString &conversationId, const QString &sessionId,
                const QJsonArray &blocks, int syncedAfter);
    void endTurn();
    void dropPermissions();
    void handleUpdate(const QString &sessionId, const QJsonObject &update);
    void handlePermission(const AiCorePermission &request);
    QJsonArray mcpServers() const;

    QString m_mode;
    QHash<QString, Session> m_sessions; // by conversation id

    /* the running turn */
    QString m_turnConversation;
    QString m_turnSession;
    int m_turnToken = 0;
    QHash<QString, AiCorePermission> m_permissions; // by key
    int m_nextPermission = 1;
};
