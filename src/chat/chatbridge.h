// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHAT_BRIDGE_H
#define CHAT_BRIDGE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "aicore.h"

class ChatClient;
class QWebEnginePage;

// This class is responsible for Web UI IPC (Qt backend <---> React Web frontend)
class ChatBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    explicit ChatBridge(QObject *parent = nullptr);

    void install(QWebEnginePage *page);

    bool busy() const { return !m_turnConversation.isEmpty(); }
    QString mode() const;
    void setMode(const QString &modeId);
    void ask(const QString &text, const QStringList &files);
    Q_INVOKABLE void ready();
    Q_INVOKABLE QJsonArray modes() const;
    /** Empty \a providerAccountId answers with the default account. */
    Q_INVOKABLE void send(const QString &conversationId, const QJsonArray &history,
                          const QString &providerAccountId, const QString &model);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void forget(const QString &conversationId);
    Q_INVOKABLE void resetSessions();
    Q_INVOKABLE void answerPermission(const QString &key, const QString &optionId);
    Q_INVOKABLE QJsonObject loadConversations() const;
    Q_INVOKABLE bool saveConversations(const QJsonObject &store);
    Q_INVOKABLE void listAccounts(int callId);
    Q_INVOKABLE void currentAccount(int callId);
    Q_INVOKABLE void saveAccount(int callId, const QJsonObject &account, const QString &apiKey);
    Q_INVOKABLE void deleteAccount(int callId, const QString &id);
    Q_INVOKABLE void activateAccount(int callId, const QString &id, const QString &model);
    /** {mcpServers, systemPrompt}, the chat settings outside AI Core. */
    Q_INVOKABLE QJsonObject settings() const;
    /** Empty on success, otherwise why nothing was saved. */
    Q_INVOKABLE QString saveSettings(const QJsonObject &settings);
    /** Store a dropped image (base64) for the model to read; returns its path. */
    Q_INVOKABLE QString saveImage(const QString &base64);

    /** Bring the conversation of the running turn into view. */
    void revealTurn();
    /** Stop AI Core; the next request starts it again. */
    void restartCore();

signals:
    void busyChanged();
    void modeChanged();

    void delta(const QString &conversationId, const QString &text);
    void thought(const QString &conversationId, const QString &text);
    /** \a status: pending, in_progress, completed, failed; \a content: ACP content or null */
    void toolCall(const QString &conversationId, const QString &toolCallId, const QString &title,
                  const QString &kind, const QString &status, const QJsonValue &content);

    void permissionRequested(const QString &conversationId, const QString &key,
                             const QJsonObject &toolCall, const QJsonArray &options);
    void permissionDropped(const QString &key);
    void finished(const QString &conversationId, const QString &stopReason);
    void failed(const QString &conversationId, const QString &error);
    void activity(const QString &conversationId);

    void accountsChanged();
    void askRequested(const QString &text, const QStringList &files);
    /* QWebChannel sends a QJsonValue argument as {}, a QVariant as JSON */
    void replied(int callId, const QVariant &result, const QString &error);

    void permissionPending(const QString &title);
    void coreRestarted();
    void turnRevealRequested();
    /** URLs dropped at (x, y) in page coordinates; local files are file:// URLs. */
    void urlsDropped(const QStringList &urls, double x, double y);

private:
    AiCore::Callback replyTo(int callId);
    void endTurn(const QString &stopReason);
    void pageReset();

    ChatClient *m_client = nullptr;
    QString m_turnConversation;
    QString m_storePath;
    bool m_pageReady = false;
    QList<QPair<QString, QStringList>> m_pendingAsks;
};

#endif  /* CHAT_BRIDGE_H */
