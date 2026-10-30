// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonArray>
#include <QVector>
#include <DMainWindow>
#include <QWidget>

#include "chatclient.h"

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QFrame;
class ChatMessageList;
class QMimeData;
class QUrl;

struct ChatConversation
{
    QString id;
    QString title;
    qint64 updatedAt = 0;
    QVector<ChatMessage> messages;
};

/**
 * GXDE AI See chat window.
 *
 * Left: conversation list (also a drop target: empty area creates a new
 * conversation, an item targets that conversation).
 * Right: conversation view (drop targets the current conversation) and input.
 */
class ChatWindow : public Dtk::Widget::DMainWindow
{
    Q_OBJECT

public:
    explicit ChatWindow(QWidget *parent = nullptr);

    /** Bring the window up for the "ask AI" action. */
    void activateForAsk();

    /** New conversation pre-filled from a portal drop on the ask tile. */
    void startConversationWith(const QString &text, const QStringList &files);

    /** Insert dropped content into the input of the current/new conversation. */
    void handleDroppedMime(const QMimeData *mime);
    void handleDropOnConversation(int index, const QMimeData *mime);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void setupUi();
    void applyTheme();
    void load();
    void save() const;
    void newConversation();
    void selectConversation(int index);
    void renderConversation();
    void updateHeader();
    void updateSidebar();
    void reorderConversations();
    void deleteConversations(const QList<int> &rows);
    void renameConversation(int row);
    void sendMessage();
    void requestCompletion();
    void runToolCalls(const QVector<ToolCall> &calls, int index, const QJsonArray &results);
    void sendPayload();
    void editMessage(int index);
    void retryLast();
    void retryFrom(int index);
    void forkFrom(int index);
    void beginCompression(bool thenSend);
    void handleAnchor(const QUrl &url);
    void showAbout();
    bool needsCompression() const;
    void openSettings();
    void appendError(const QString &message);
    int currentIndex() const { return m_current; }

    QListWidget *m_sidebar = nullptr;
    ChatMessageList *m_messages = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_modelLabel = nullptr;
    QPlainTextEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    QLabel *m_status = nullptr;
    ChatClient *m_client = nullptr;

    /* themed chrome, restyled by applyTheme() */
    QWidget *m_sidebarPane = nullptr;
    QWidget *m_chatPane = nullptr;
    QWidget *m_header = nullptr;
    QFrame *m_inputFrame = nullptr;
    QLabel *m_brand = nullptr;
    QLabel *m_brandSub = nullptr;
    QLabel *m_hint = nullptr;
    QPushButton *m_newButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QPushButton *m_menuButton = nullptr;

    QVector<ChatConversation> m_conversations;
    int m_current = -1;
    int m_streamingIndex = -1;
    bool m_compressing = false;
    bool m_afterCompressSend = false;
    bool m_skipCompress = false;
    QJsonArray m_toolScaffold;
    int m_toolRounds = 0;
    QString m_storePath;
};
