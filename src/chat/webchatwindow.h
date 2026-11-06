// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WEB_CHAT_WINDOW_H
#define WEB_CHAT_WINDOW_H

#include <DMainWindow>

class ChatBridge;
class QWebEngineView;

class WebChatWindow : public Dtk::Widget::DMainWindow
{
    Q_OBJECT

public:
    explicit WebChatWindow(QWidget *parent = nullptr);

    /* Active the window for "ask AI" section */
    void activateForAsk();

    /* Start conversation with given prompts and files */
    void startConversationWith(const QString &text, const QStringList &files);

private slots:
    void onNotificationSent(uint id);
    void onNotificationAction(uint id, const QString &action);

private:
    void notifyPermission(const QString &title);
    bool eventFilter(QObject *watched, QEvent *event) override;

    ChatBridge *m_bridge = nullptr;
    QWebEngineView *m_view = nullptr;
    uint m_notificationId = 0;
};

# endif  /* WEB_CHAT_WINDOW_H */
