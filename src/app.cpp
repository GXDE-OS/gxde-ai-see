// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app.h"

#include "chat/chatwindow.h"
#include "config.h"
#include "portal/portalclient.h"
#include "staging/stagingmodel.h"
#include "todo/todomodel.h"
#include "todo/todowindow.h"
#include "ui/portalpanel.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QLocalServer>
#include <QTimer>
#include <QLocalSocket>

#include <unistd.h>

App::App(QObject *parent)
    : QObject(parent)
{
}

bool App::init(bool daemon, bool forceShow, bool showTodo, bool showChat)
{
    Config::instance()->init();
    m_daemon = daemon;

    const QString serverName = QStringLiteral("gxde-ai-see-%1").arg(getuid());

    QLocalSocket probe;
    probe.connectToServer(serverName);
    if (probe.waitForConnected(200)) {
        probe.write(showChat ? "chat" : (showTodo ? "todo" : "show"));
        probe.waitForBytesWritten(200);
        probe.disconnectFromServer();
        return false;
    }

    QLocalServer::removeServer(serverName);
    m_server = new QLocalServer(this);
    m_server->listen(serverName);
    connect(m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
                const QByteArray data = socket->readAll();
                if (data.contains("chat")) {
                    showChatWindow();
                } else if (data.contains("todo")) {
                    showTodoWindow();
                } else if (data.contains("show")) {
                    showPanel();
                }
            });
            connect(socket, &QLocalSocket::disconnected, socket,
                    &QLocalSocket::deleteLater);
        }
    });

    m_model = new StagingModel(this);

    /* gxde-k9 keeps restarting us; when the compositor has no portal support we
     * warn once and then stay idle instead of exiting (which would make k9
     * relaunch us every few seconds). */
    if (daemon && !PortalClient::instance()->available() &&
        !Config::instance()->getBool(QStringLiteral("warned/unsupportedEnv"), false)) {
        QDBusMessage notification = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
        notification << QStringLiteral("gxde-ai-see") << 0u
                     << QStringLiteral("gxde-ai-see")
                     << tr("小埃同学") 
                     << tr("当前桌面不支持传送门（需要 GXDE Wayland 合成器）。"
                           "小埃同学会继续在后台待命，暂存区、内容工厂和对话仍然可用。")
                     << QStringList() << QVariantMap() << 6000;
        QDBusConnection::sessionBus().send(notification);
        Config::instance()->set(QStringLiteral("warned/unsupportedEnv"), true);
        Config::instance()->sync();
    }

    PortalClient *portal = PortalClient::instance();
    connect(portal, &PortalClient::dragStarted, this, [this](const QString &mimeTypes) {
        if (!PortalClient::mimeInteresting(mimeTypes)) {
            return;
        }
        m_dragging = true;
        if (m_panel) {
            m_panel->setDragging(true);
        }
        if (!m_model->isEmpty()) {
            if (m_panel && m_panel->isVisible()) {
                m_panel->suppressPortalIfUnderCursor();
            }
            return;
        }
        QTimer::singleShot(120, this, [this]() {
            if (m_dragging && m_model->isEmpty()) {
                showPanel();
                if (m_panel) {
                    m_panel->suppressPortalIfUnderCursor();
                }
            }
        });
    });
    connect(portal, &PortalClient::dragEnded, this, [this]() {
        m_dragging = false;
        if (m_panel) {
            m_panel->stopPortalSuppression();
            m_panel->setDragging(false);
        }
        if (m_model->isEmpty() && m_panel && m_panel->isVisible()) {
            m_panel->dismiss();
        }
    });

    connect(m_model, &StagingModel::countChanged, this, [this]() {
        if (!m_daemon && !m_model->isEmpty() && m_panel) {
            m_panel->popup();
        }
    });

    if (showChat) {
        showChatWindow();
    }
    if (showTodo) {
        showTodoWindow();
    }
    if (!daemon || forceShow || !m_model->isEmpty()) {
        showPanel();
    }

    return true;
}

void App::ensurePanel()
{
    if (m_panel) {
        return;
    }

    m_panel = new PortalPanel(m_model);
    connect(m_panel, &PortalPanel::openChatRequested, this, &App::showChatWindow);
    connect(m_panel, &PortalPanel::askWithContent, this,
            [this](const QString &text, const QStringList &files) {
                showChatWindow();
                if (m_chatWindow) {
                    m_chatWindow->startConversationWith(text, files);
                }
            });
}

void App::showPanel()
{
    ensurePanel();
    m_panel->popup();
}

void App::showChatWindow()
{
    if (!m_chatWindow) {
        m_chatWindow = new ChatWindow;
    }
    m_chatWindow->activateForAsk();
}

void App::showTodoWindow()
{
    if (!m_todoWindow) {
        m_todoWindow = new TodoWindow(TodoModel::instance());
    }
    m_todoWindow->popup();
}
