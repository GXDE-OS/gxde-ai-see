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
#include <QDesktopServices>
#include <QDropEvent>
#include <QMenu>
#include <QGuiApplication>
#include <QMimeData>
#include <QScreen>
#include <QStyleHints>
#include <QUrl>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePage>
#include <QWebEngineSettings>
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

void paintSidebarFill(QWidget *window)
{
    const bool dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    QPalette palette = window->palette();
    palette.setColor(QPalette::Window, dark ? QColor(23, 23, 23) : QColor(247, 247, 248));
    window->setPalette(palette);
    window->setAutoFillBackground(true);
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
    titlebar()->setBackgroundTransparent(true);
    titlebar()->setSeparatorVisible(false);
    /* DTK adds its own About and Exit after these */
    auto *menu = new QMenu(this);
    menu->addAction(tr("重启 GXDE AI Core"), m_bridge, &ChatBridge::restartCore);
    titlebar()->setMenu(menu);
    paintSidebarFill(this);
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this]() { paintSidebarFill(this); });
    setMinimumSize(760, 520);
    resize(1000, 700);

    migrateLegacySettings();

    setCentralWidget(m_view);
    // Copy buttons use navigator.clipboard
    // Sadly, Qt's WebEngine prevents this by default :(
    m_view->settings()->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
    m_bridge->install(m_view->page());
    // Markdown links open with target=_blank.
    // So we have to hand them to system browser.
    connect(m_view->page(), &QWebEnginePage::newWindowRequested, this,
            [](QWebEngineNewWindowRequest &request) {
                QDesktopServices::openUrl(request.requestedUrl());
            });
    m_view->load(webUiUrl());
    // Chromium hides the paths of dropped files from the page, so drops with
    // local files are taken here; the widget under the cursor may be a child.
    m_view->installEventFilter(this);
    for (QObject *child : m_view->children()) {
        child->installEventFilter(this);
    }

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

bool WebChatWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ChildAdded && watched == m_view) {
        static_cast<QChildEvent *>(event)->child()->installEventFilter(this);
    }
    if (event->type() != QEvent::Drop || !watched->isWidgetType()) {
        return DMainWindow::eventFilter(watched, event);
    }

    auto *drop = static_cast<QDropEvent *>(event);
    const QList<QUrl> urls = drop->mimeData()->urls();
    if (std::none_of(urls.cbegin(), urls.cend(), [](const QUrl &url) { return url.isLocalFile(); })) {
        return DMainWindow::eventFilter(watched, event);
    }

    QStringList dropped;
    for (const QUrl &url : urls) {
        dropped.append(url.toString(QUrl::FullyEncoded));
    }
    auto *widget = static_cast<QWidget *>(watched);
    const QPointF at = QPointF(widget->mapTo(m_view, drop->position().toPoint())) / m_view->zoomFactor();
    drop->acceptProposedAction();
    /* end the page's drag as if it left */
    QDragLeaveEvent leave;
    QCoreApplication::sendEvent(watched, &leave);
    Q_EMIT m_bridge->urlsDropped(dropped, at.x(), at.y());
    return true;
}

void WebChatWindow::onNotificationSent(uint id)
{
    m_notificationId = id;
}

void WebChatWindow::onNotificationAction(uint id, const QString &action)
{
    if (id == m_notificationId && !action.isEmpty()) {
        activateForAsk();
        /* the confirmation waits in the conversation of the running turn */
        m_bridge->revealTurn();
    }
}
