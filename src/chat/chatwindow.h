// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QVector>
#include <QWidget>

#include "chatclient.h"

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
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
class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWindow(QWidget *parent = nullptr);

    /** Bring the window up for the "ask AI" action. */
    void activateForAsk();

    /** Insert dropped content into the input of the current/new conversation. */
    void handleDroppedMime(const QMimeData *mime);
    void handleDropOnConversation(int index, const QMimeData *mime);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void setupUi();
    void load();
    void save() const;
    void newConversation();
    void selectConversation(int index);
    void renderConversation();
    void updateSidebar();
    void sendMessage();
    void requestCompletion();
    void sendPayload();
    void editMessage(int index);
    void retryLast();
    void forkFrom(int index);
    void beginCompression(bool thenSend);
    void handleAnchor(const QUrl &url);
    void showAbout();
    bool needsCompression() const;
    void openSettings();
    void appendError(const QString &message);
    int currentIndex() const { return m_current; }

    QListWidget *m_sidebar = nullptr;
    QTextBrowser *m_view = nullptr;
    QPlainTextEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    QLabel *m_status = nullptr;
    ChatClient *m_client = nullptr;

    QVector<ChatConversation> m_conversations;
    int m_current = -1;
    int m_streamingIndex = -1;
    bool m_compressing = false;
    bool m_afterCompressSend = false;
    bool m_skipCompress = false;
    QString m_storePath;
};
