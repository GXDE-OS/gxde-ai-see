// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app.h"

#include "chat/chatwindow.h"
#include "config.h"
#include "portal/portalclient.h"
#include "service.h"
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
        /* Another instance owns the service: forward only what this command
         * line actually asked for.  A plain --daemon launch (k9 autostart, or
         * a second manual start) must stay silent, otherwise it would toggle
         * and pin the panel of the running instance. */
        QByteArray request;
        if (showChat) {
            request = "chat";
        } else if (showTodo) {
            request = "todo";
        } else if (forceShow || !daemon) {
            request = "show";
        }
        if (!request.isEmpty()) {
            probe.write(request);
            probe.waitForBytesWritten(200);
        }
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
                    togglePanel();
                }
            });
            connect(socket, &QLocalSocket::disconnected, socket,
                    &QLocalSocket::deleteLater);
        }
    });

    PortalService::instance()->init();

    m_model = new StagingModel(this);

    connect(PortalService::instance(), &PortalService::enabledChanged, this,
            [this](bool enabled) {
                if (!enabled) {
                    m_panelPinned = false;
                    m_dragging = false;
                    PortalClient::instance()->cancelPortal();
                    if (m_panel) {
                        m_panel->stopPortalSuppression();
                        m_panel->setDragging(false);
                        m_panel->dismiss();
                    }
                    return;
                }
                /* Re-enabled from chat settings: show the panel once so the
                 * user gets immediate feedback. */
                ensurePanel();
                m_panel->popup();
            });

    /* gxde-k9 keeps restarting us; when the compositor has no portal support we
     * warn once and then stay idle instead of exiting (which would make k9
     * relaunch us every few seconds). */
    if (daemon && PortalService::instance()->enabled() &&
        !PortalClient::instance()->available() &&
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
        if (!PortalService::instance()->enabled() ||
            !PortalClient::mimeInteresting(mimeTypes)) {
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
        QTimer::singleShot(80, this, [this]() {
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
        if (m_model->isEmpty() && !m_panelPinned && m_panel && m_panel->isVisible()) {
            m_panel->dismiss();
        }
    });

    connect(portal, &PortalClient::fullscreenChanged, this, [this](bool fullscreen) {
        if (!PortalService::instance()->enabled() ||
            !Config::instance()->getBool(QStringLiteral("panel/hideOnFullscreen"), false)) {
            return;
        }
        if (fullscreen) {
            if (m_panel && m_panel->isVisible()) {
                m_panel->dismiss();
            }
        } else if (m_panelPinned || !m_model->isEmpty()) {
            ensurePanel();
            m_panel->popup();
        }
    });

    connect(m_model, &StagingModel::countChanged, this, [this]() {
        if (!m_model->isEmpty() && m_panel && PortalService::instance()->enabled()) {
            m_panel->popup();
            return;
        }
        /* cleared: the panel exists for the staging list, hide it again */
        if (!m_dragging && !m_panelPinned && m_panel && m_panel->isVisible()) {
            m_panel->dismiss();
        }
    });

    if (showChat) {
        showChatWindow();
    }
    if (showTodo) {
        showTodoWindow();
    }

    /* The panel is an on-demand surface: an explicit desktop toggle (--show),
     * a plain launch, or staged content pins/opens it.  Opening the chat or
     * the todo window must not leave the panel covering the screen. */
    if (PortalService::instance()->enabled()) {
        const bool shownByHand = forceShow || (!daemon && !showChat && !showTodo);
        if (shownByHand) {
            m_panelPinned = true;
            showPanel();
        } else if (!m_model->isEmpty()) {
            showPanel();
        }
    } else if (forceShow) {
        /* The desktop portal toggle started us while the service is off:
         * guide the user to the chat settings where it can be re-enabled. */
        QTimer::singleShot(0, this, [this]() {
            showChatWindow();
            if (m_chatWindow) {
                m_chatWindow->showSettings();
            }
        });
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
    connect(m_panel, &PortalPanel::openChatSettingsRequested, this, [this]() {
        showChatWindow();
        if (m_chatWindow) {
            m_chatWindow->showSettings();
        }
    });
    connect(m_panel, &PortalPanel::hideRequested, this, [this]() {
        m_panelPinned = false;
        m_panel->dismiss();
    });
    connect(m_panel, &PortalPanel::todoAdded, this, &App::showTodoWindow);
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
    if (!PortalService::instance()->enabled()) {
        return;
    }
    ensurePanel();
    m_panel->popup();
}

void App::togglePanel()
{
    if (!PortalService::instance()->enabled()) {
        /* The desktop "portal" entry is the discoverable place to get the
         * service back: open the chat settings where it can be re-enabled. */
        showChatWindow();
        if (m_chatWindow) {
            m_chatWindow->showSettings();
        }
        return;
    }
    ensurePanel();
    if (m_panel->isVisible() && m_panelPinned) {
        m_panel->dismiss();
        m_panelPinned = false;
        return;
    }
    m_panelPinned = true;
    m_panel->popup();
}

void App::showChatWindow()
{
    /* Opening the chat must not keep an incidental panel on screen.  A panel
     * the user pinned from the desktop entry stays; an empty, unpinned one is
     * dismissed. */
    if (m_panel && m_panel->isVisible() && !m_panelPinned && !m_dragging &&
        m_model->isEmpty()) {
        m_panel->dismiss();
    }

    if (!m_chatWindow) {
        m_chatWindow = new ChatWindow;
    }
    m_chatWindow->activateForAsk();
}

void App::showTodoWindow()
{
    if (!m_todoWindow) {
        m_todoWindow = new TodoWindow(TodoModel::instance());
        connect(m_todoWindow, &TodoWindow::recreateRequested, this, [this]() {
            if (m_todoWindow) {
                m_todoWindow->close();
                m_todoWindow->deleteLater();
                m_todoWindow = nullptr;
            }
            showTodoWindow();
        });
    }
    m_todoWindow->popup();
}
