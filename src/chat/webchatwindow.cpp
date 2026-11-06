// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "webchatwindow.h"

#include "config.h"
#include "aicoreaccounts.h"

#include <DTitlebar>
#include <QApplication>
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
    m_view->load(webUiUrl());

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
    // TODO: hand the content to the page once the QWebChannel bridge exists.
    Q_UNUSED(text)
    Q_UNUSED(files)
    activateForAsk();
}
