// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>

class QProcess;

/** A tool call GXDE AI Core wants the user to approve. */
struct AiCorePermission
{
    QJsonValue rpcId; // id of the session/request_permission request
    QString sessionId;
    QJsonObject toolCall; // ACP ToolCall: toolCallId, title, kind, content, …
    QList<QJsonObject> options; // ACP PermissionOption: optionId, name, kind
};

/**
 * The connection to GXDE AI Core (`gxde-ai-core --acp`).
 *
 * One agent process serves the whole application: it owns the provider
 * accounts (API keys live in its keychain), the chat sessions and their
 * tools.  The process is started on first use and again after it died;
 * requests sent before the ACP handshake finished are queued.
 *
 * Messages are JSON-RPC 2.0, one per line on stdio.
 */
class AiCore : public QObject
{
    Q_OBJECT

public:
    /** \a error is empty on success. */
    using Callback = std::function<void(const QJsonValue &result, const QString &error)>;

    static AiCore *instance();
    ~AiCore() override;

    /** The agent binary: $GXDE_AI_SEE_AICORE, else gxde-ai-core from PATH. */
    static QString program();
    /** Working directory of the sessions (the user's home). */
    static QString workspace();

    bool running() const;
    void start();
    void stop();

    void call(const QString &method, const QJsonObject &params, Callback callback = {});
    void notify(const QString &method, const QJsonObject &params);

    /** Answer a permission request; an empty \a optionId cancels it. */
    void answerPermission(const QJsonValue &rpcId, const QString &optionId);

    // gxde-ai-core/auth/providers/...
    void listAccounts(Callback callback);
    void currentAccount(Callback callback);
    /** \a apiKey empty keeps the stored key of an existing account. */
    void saveAccount(const QJsonObject &account, const QString &apiKey, Callback callback);
    void deleteAccount(const QString &id, Callback callback);
    void activateAccount(const QString &id, const QString &model, Callback callback);

    /** Turn ACP / provider errors into something a user can act on. */
    static QString friendlyError(const QString &error);

signals:
    void sessionUpdate(const QString &sessionId, const QJsonObject &update);
    void permissionRequested(const AiCorePermission &request);
    /** The provider accounts changed (saved, deleted or activated here). */
    void accountsChanged();
    /** The agent process went away: every session it had is gone. */
    void stopped(const QString &reason);

private:
    explicit AiCore(QObject *parent = nullptr);

    void write(const QJsonObject &message);
    void handleLine(const QByteArray &line);
    void handleRequest(const QJsonObject &message);
    void handleExit();
    QByteArray takeStderrTail();
    void removeStderrFile();
    void failPending(const QString &error);

    QProcess *m_process = nullptr;
    QByteArray m_buffer;
    QString m_stderrPath;
    bool m_initialized = false;
    int m_nextId = 1;
    QHash<int, Callback> m_callbacks;
    /* requests waiting for the initialize handshake */
    QList<QJsonObject> m_queue;
};
