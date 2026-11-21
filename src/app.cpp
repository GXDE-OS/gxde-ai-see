// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app.h"

#include "chat/webchatwindow.h"
#include "config.h"
#include "portal/portalclient.h"
#include "portal/dragwatcher.h"
#include "portal/portalsession.h"
#include "staging/stagingmodel.h"
#include "todo/todomodel.h"
#include "todo/todowindow.h"
#include "ui/portalpanel.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QLocalServer>
#include <QMenu>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QLocalSocket>

#include <unistd.h>

namespace {

/* 托盘里的「退出」也要把 k9 保活关掉，否则几秒后又被拉起来 */
void disableK9KeepAlive()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    QFile marker(dir + QStringLiteral("/k9-disabled"));
    if (marker.open(QIODevice::WriteOnly | QIODevice::Text)) {
        marker.write("# created by gxde-ai-see: the user quit from the todo tray\n");
    }
}

} // namespace

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
                    togglePanel();
                }
            });
            connect(socket, &QLocalSocket::disconnected, socket,
                    &QLocalSocket::deleteLater);
        }
    });

    m_model = new StagingModel(this);

    /* gxde-k9 keeps restarting us; when the desktop has no portal support we
     * warn once and then stay idle instead of exiting (which would make k9
     * relaunch us every few seconds).  The Wayland window list binds
     * asynchronously, so look a moment later. */
    if (daemon && !Config::instance()->getBool(QStringLiteral("warned/unsupportedEnv"), false)) {
        QTimer::singleShot(1500, this, []() {
            if (PortalClient::instance()->available() || PortalSession::supported()) {
                return;
            }
            QDBusMessage notification = QDBusMessage::createMethodCall(
                QStringLiteral("org.freedesktop.Notifications"),
                QStringLiteral("/org/freedesktop/Notifications"),
                QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
            notification << QStringLiteral("gxde-ai-see") << 0u
                         << QStringLiteral("gxde-ai-see")
                         << tr("小埃同学")
                         << tr("当前桌面不支持传送门（需要 X11 窗口管理器，或支持 "
                               "wlr-foreign-toplevel 的 Wayland 合成器）。"
                               "小埃同学会继续在后台待命，暂存区、内容工厂和对话仍然可用。")
                         << QStringList() << QVariantMap() << 6000;
            QDBusConnection::sessionBus().send(notification);
            Config::instance()->set(QStringLiteral("warned/unsupportedEnv"), true);
            Config::instance()->sync();
        });
    }

    PortalClient *portal = PortalClient::instance();
    connect(portal, &PortalClient::dragStarted, this, [this](const QString &mimeTypes) {
        if (PortalClient::mimeInteresting(mimeTypes)) {
            onDragStarted();
        }
    });

    connect(portal, &PortalClient::dragEnded, this, &App::onDragEnded);
    if (DragWatcher *watcher = DragWatcher::instance()) {
        connect(watcher, &DragWatcher::dragStarted, this, &App::onDragStarted);
        connect(watcher, &DragWatcher::dragEnded, this, &App::onDragEnded);
    }

    connect(portal, &PortalClient::fullscreenChanged, this, [this](bool fullscreen) {
        if (!Config::instance()->getBool(QStringLiteral("panel/hideOnFullscreen"), false)) {
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
        if (!m_model->isEmpty() && m_panel) {
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
    
    const bool shownByHand = forceShow || (!daemon && !showChat && !showTodo);
    if (shownByHand) {
        m_panelPinned = true;
        showPanel();
    } else if (!m_model->isEmpty()) {
        showPanel();
    }

    return true;
}

void App::onDragStarted()
{
    if (m_dragging) {
        return;
    }

    /* 对话设置里关掉「传送门快捷入口」后，拖拽不再唤起面板/传送门。
     * 显式打开的面板（--show、菜单）和暂存区常显不受影响。 */
    if (!Config::instance()->getBool(QStringLiteral("portal/enabled"), true)) {
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
}

void App::onDragEnded()
{
    if (!m_dragging) {
        return;
    }

    m_dragging = false;
    if (m_panel) {
        m_panel->stopPortalSuppression();
        m_panel->setDragging(false);
    }

    if (m_model->isEmpty() && !m_panelPinned && m_panel && m_panel->isVisible()) {
        m_panel->dismiss();
    }
}

void App::ensurePanel()
{
    if (m_panel) {
        return;
    }

    m_panel = new PortalPanel(m_model);
    connect(m_panel, &PortalPanel::openChatRequested, this, &App::showChatWindow);
    connect(m_panel, &PortalPanel::hideRequested, this, [this]() {
        m_panelPinned = false;
        m_panel->dismiss();
    });
    connect(m_panel, &PortalPanel::todoAdded, this, &App::onTodoAdded);
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

void App::togglePanel()
{
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
    if (!m_chatWindow) {
        m_chatWindow = new WebChatWindow;
    }
    m_chatWindow->activateForAsk();
}

/* 内容工厂里新建待办：只有待办窗口还没打开时才弹出来，
 * 已经开着就别再抢用户的窗口。 */
void App::onTodoAdded()
{
    if (m_todoWindow && m_todoWindow->isVisible()) {
        return;
    }
    /* 第一次打开时 TodoWindow（含 layer-shell / ukui-shell / 托盘）是现建的：
     * 不能在拖拽的 drop 事件里做——拖拽 grab 还没释放，窗口映射会把整个
     * 拖拽卡住，看起来像传送门没反应。等这一轮事件处理完再弹。 */
    QTimer::singleShot(120, this, [this]() {
        if (m_todoWindow && m_todoWindow->isVisible()) {
            return;
        }
        showTodoWindow();
    });
}

void App::showTodoWindow()
{
    ensureTray();
    if (!m_todoWindow) {
        m_todoWindow = new TodoWindow(TodoModel::instance());
        connect(m_todoWindow, &TodoWindow::recreateRequested, this, [this]() {
            if (m_todoWindow) {
                m_todoWindow->hide();
                m_todoWindow->deleteLater();
                m_todoWindow = nullptr;
            }
            showTodoWindow();
        });
        connect(m_todoWindow, &TodoWindow::hiddenToTray, this, [this]() {
            if (!m_tray || m_trayHintShown) {
                return;
            }
            m_trayHintShown = true;
            m_tray->showMessage(tr("待办已收进托盘"),
                                tr("点托盘里的待办图标可以重新打开。"), QSystemTrayIcon::Information,
                                3000);
        });
    }
    m_todoWindow->popup();
}

void App::toggleTodoWindow()
{
    if (m_todoWindow && m_todoWindow->isVisible()) {
        m_todoWindow->hide();
        return;
    }
    showTodoWindow();
}

void App::addTodoEntry()
{
    showTodoWindow();
    m_todoWindow->addEntry();
}

void App::ensureTray()
{
    if (m_tray) {
        return;
    }
    QIcon icon = QIcon::fromTheme(QStringLiteral("gxde-ai-see-todo"),
                                  QIcon(QStringLiteral("/usr/share/icons/hicolor/256x256/apps/"
                                                        "gxde-ai-see-todo.png")));
    m_tray = new QSystemTrayIcon(icon, this);
    m_tray->setToolTip(tr("GXDE 待办"));

    auto *menu = new QMenu;
    menu->addAction(tr("显示待办"), this, &App::showTodoWindow);
    menu->addAction(tr("新建待办…"), this, &App::addTodoEntry);
    menu->addAction(tr("隐藏待办"), this, [this]() {
        if (m_todoWindow) {
            m_todoWindow->hide();
        }
    });
    menu->addSeparator();
    menu->addAction(tr("退出小埃同学"), this, [this]() {
        disableK9KeepAlive();
        QCoreApplication::quit();
    });
    m_tray->setContextMenu(menu);

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger ||
                    reason == QSystemTrayIcon::DoubleClick) {
                    toggleTodoWindow();
                }
            });
    m_tray->show();
}
