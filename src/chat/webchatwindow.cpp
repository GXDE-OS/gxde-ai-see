// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "webchatwindow.h"

#include "config.h"
#include "aicoreaccounts.h"
#include "chatbridge.h"

#include <DTitlebar>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QScreen>
#include <QUrl>
#include <QWebEngineView>

namespace {

QUrl webUiUrl()
{
    /* Loaded from static dist. These files are pre-built by Vite. */
    const QString devUrl = qEnvironmentVariable("GXDE_AI_SEE_WEBUI_URL");
    if (!devUrl.isEmpty()) {
        return QUrl(devUrl);
    }
    return QUrl(QStringLiteral("qrc:/web_ui/index.html"));
}

void migrateLegacySettings()
{
    Config *config = Config::instance();
    const QString storedModel = config->get(QStringLiteral("chat/model")).toString();
    if (storedModel == QStringLiteral("deepseek-chat")) {
        config->set(QStringLiteral("chat/model"), QStringLiteral("deepseek-flash"));
        config->sync();
    } else if (storedModel == QStringLiteral("deepseek-reasoner")) {
        config->set(QStringLiteral("chat/model"), QStringLiteral("deepseek-v4-pro"));
        config->sync();
    }
    AiCoreAccountsPanel::importLegacyKey();
}

} // namespace

WebChatWindow::WebChatWindow(QWidget *parent)
    : Dtk::Widget::DMainWindow(parent)
    , m_bridge(new ChatBridge(this))
    , m_view(new QWebEngineView(this))
{
    setWindowTitle(tr("小埃同学"));
    titlebar()->setTitle(tr("小埃同学"));
    titlebar()->setIcon(QApplication::windowIcon());
    titlebar()->setAutoHideOnFullscreen(true);
    setMinimumSize(760, 520);
    resize(1000, 700);

    migrateLegacySettings();

    setCentralWidget(m_view);
    m_bridge->install(m_view->page());
    m_view->load(webUiUrl());

    // Send a notifition if user's permission is EXPLICITLY required
    // This is common when a tool call/shell command is pending
    connect(m_bridge, &ChatBridge::permissionPending, this, [this](const QString &title) {
        if (!isActiveWindow()) {
            notifyPermission(title);
        }
    });

    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("ActionInvoked"), this,
        SLOT(onNotificationAction(uint, QString)));

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        move(screen->availableGeometry().center() - rect().center());
    }
}

void WebChatWindow::activateForAsk()
{
    if (isMinimized()) {
        showNormal();
    } else {
        show();
    }

    raise();
    activateWindow();
}

void WebChatWindow::startConversationWith(const QString &text, const QStringList &files)
{
    activateForAsk();
    m_bridge->ask(text, files);
}

void WebChatWindow::notifyPermission(const QString &title)
{
    QDBusMessage notification = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    notification << QStringLiteral("gxde-ai-see") << m_notificationId
                 << QStringLiteral("gxde-ai-see") << tr("小埃同学需要你确认")
                 << tr("小埃想要执行：%1\n请回到对话窗口允许或拒绝。").arg(title)
                 << QStringList{ QStringLiteral("default"), tr("查看") } << QVariantMap()
                 << 0;
    QDBusConnection::sessionBus().callWithCallback(notification, this,
                                                   SLOT(onNotificationSent(uint)));
}

void WebChatWindow::onNotificationSent(uint id)
{
    m_notificationId = id;
}

void WebChatWindow::onNotificationAction(uint id, const QString &action)
{
    if (id == m_notificationId && !action.isEmpty()) {
        activateForAsk();
    }
}
