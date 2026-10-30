// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app.h"

#include "config.h"
#include "staging/stagingmodel.h"
#include "ui/portalpanel.h"

#include <QLocalServer>
#include <QLocalSocket>

#include <unistd.h>

App::App(QObject *parent)
    : QObject(parent)
{
}

bool App::init(bool daemon, bool forceShow)
{
    Config::instance()->init();
    m_daemon = daemon;

    const QString serverName = QStringLiteral("gxde-ai-see-%1").arg(getuid());

    QLocalSocket probe;
    probe.connectToServer(serverName);
    if (probe.waitForConnected(200)) {
        probe.write("show");
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
                if (socket->readAll().contains("show")) {
                    showPanel();
                }
            });
            connect(socket, &QLocalSocket::disconnected, socket,
                    &QLocalSocket::deleteLater);
        }
    });

    m_model = new StagingModel(this);

    connect(m_model, &StagingModel::countChanged, this, [this]() {
        if (!m_daemon && !m_model->isEmpty() && m_panel) {
            m_panel->popup();
        }
    });

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
    connect(m_panel, &PortalPanel::openChatRequested, this, []() {
        // TODO(M4): open the AI See chat window.
    });
}

void App::showPanel()
{
    ensurePanel();
    m_panel->popup();
}
