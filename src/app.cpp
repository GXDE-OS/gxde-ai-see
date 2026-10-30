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

    PortalClient *portal = PortalClient::instance();
    connect(portal, &PortalClient::dragStarted, this, [this](const QString &mimeTypes) {
        if (!PortalClient::mimeInteresting(mimeTypes)) {
            return;
        }
        m_dragging = true;
        if (!m_model->isEmpty()) {
            return;
        }
        QTimer::singleShot(120, this, [this]() {
            if (m_dragging && m_model->isEmpty()) {
                showPanel();
            }
        });
    });
    connect(portal, &PortalClient::dragEnded, this, [this]() {
        m_dragging = false;
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
