// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QList>
#include <QPair>
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

    /**
     * An inline card asking to approve a tool call.  \a options are
     * (optionId, button label) pairs; picking one emits permissionAnswered()
     * and leaves the card showing the decision.  The card survives
     * setMessages() until removePermission().
     */
    void showPermission(const QString &key, const QString &title, const QString &detail,
                        const QList<QPair<QString, QString>> &options);
    void removePermission(const QString &key);
    /** Hide the confirmation cards while another conversation is shown. */
    void setPermissionsVisible(bool visible);

    /**
     * A "thinking" card above the bubble of the assistant message \a index,
     * \a active while the model is still reasoning.  Empty \a text (or a
     * negative index) removes it.  The card survives setMessages().
     */
    void setThinking(int index, const QString &text, bool active);

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
    void permissionAnswered(const QString &key, const QString &optionId);

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
    QHash<QString, QWidget *> m_permissions;
    bool m_permissionsVisible = true;
    int m_thinkingIndex = -1;
    QString m_thinkingText;
    bool m_thinkingActive = false;
    QVector<ChatMessage> m_messages;
};
