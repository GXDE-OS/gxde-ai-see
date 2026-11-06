// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WEB_CHAT_WINDOW_H
#define WEB_CHAT_WINDOW_H

#include <DMainWindow>

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

private:
    QWebEngineView *m_view = nullptr;
};

# endif  /* WEB_CHAT_WINDOW_H */
