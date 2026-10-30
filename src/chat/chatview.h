// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QScrollArea>
#include <QVector>

#include "chatclient.h"

class QLabel;
class QMimeData;
class QUrl;
class QVBoxLayout;

/**
 * Scrollable message list with chat bubbles, avatars and inline actions.
 */
class ChatMessageList : public QScrollArea
{
    Q_OBJECT

public:
    explicit ChatMessageList(QWidget *parent = nullptr);

    void setMessages(const QVector<ChatMessage> &messages);
    void appendMessage(const ChatMessage &message);
    void updateMessage(int index, const QString &content);
    void appendNote(const QString &text);
    void clear();

    /** Rebuild every row after the light/dark theme changed. */
    void refreshTheme();

    void scrollToBottom();

signals:
    void dropped(const QMimeData *mime);
    void copyRequested(int index);
    void editRequested(int index);
    void forkRequested(int index);
    void retryRequested(int index);
    void linkActivated(const QUrl &url);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void rebuild(bool scrollToBottom = true);
    void restyleWelcome();

    QWidget *m_container = nullptr;
    QVBoxLayout *m_layout = nullptr;
    QWidget *m_welcome = nullptr;
    QLabel *m_welcomeAvatar = nullptr;
    QLabel *m_welcomeTitle = nullptr;
    QLabel *m_welcomeHint = nullptr;
    QVector<class MessageRow *> m_rows;
    QVector<QWidget *> m_notes;
    QVector<ChatMessage> m_messages;
};
