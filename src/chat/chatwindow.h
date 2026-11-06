// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QElapsedTimer>
#include <QSet>
#include <QVector>
#include <DMainWindow>
#include <QWidget>

#include "chatclient.h"

class QTimer;

class QComboBox;
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

private slots:
    void onNotificationSent(uint id);
    void onNotificationAction(uint id, const QString &action);

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
    void endTurn();
    void noteTurnActivity();
    void onTurnWait();
    void updateSendButton();
    void refreshAccountLabel();
    void askPermission(const QString &key, const AiCorePermission &request);
    void notifyPermission(const QString &title);
    void editMessage(int index);
    void retryLast();
    void retryFrom(int index);
    void forkFrom(int index);
    void handleAnchor(const QUrl &url);
    void showAbout();
    void openSettings();
    void appendError(const QString &message);
    int currentIndex() const { return m_current; }

    QListWidget *m_sidebar = nullptr;
    ChatMessageList *m_messages = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_modelLabel = nullptr;
    QComboBox *m_modeBox = nullptr;
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
    /* tool calls already shown in the streaming answer */
    QSet<QString> m_turnTools;
    /* confirmation cards of the running turn */
    QSet<QString> m_permissionKeys;
    /* "account · model" of the active AI Core account */
    QString m_accountLabel;
    uint m_notificationId = 0;
    /* silence watch of the running turn, see onTurnWait() */
    QTimer *m_waitTimer = nullptr;
    QElapsedTimer m_lastActivity;
    bool m_waitNoteShown = false;
    QString m_storePath;
};
