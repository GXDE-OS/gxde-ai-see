// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "aboutdialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QPointer>

#include <DAboutDialog>
#include <DApplication>

namespace {

QPointer<Dtk::Widget::DAboutDialog> s_aboutDialog;

QString defaultDescription()
{
    return QObject::tr("GXDE 桌面的 AI 助手：传送门、内容工厂、待办与小埃同学对话。");
}

void configureAboutDialog(Dtk::Widget::DAboutDialog *dialog, const QString &description)
{
    dialog->setWindowTitle(QObject::tr("关于小埃同学"));
    dialog->setProductIcon(QApplication::windowIcon());
    dialog->setProductName(QObject::tr("GXDE 小埃同学"));
    dialog->setVersion(QCoreApplication::applicationVersion());
    dialog->setDescription(description);
    dialog->setLicense(QStringLiteral("GPL-3.0-or-later"));
    dialog->setWebsiteName(QStringLiteral("gitee.com/GXDE-OS/gxde-ai-see"));
    dialog->setWebsiteLink(QStringLiteral("https://gitee.com/GXDE-OS/gxde-ai-see"));
}

/* One dialog for every entry point: the titlebar menu, the chat overflow menu
 * and the panel context menu all open this instance, so none of them ends up
 * with a stock, empty DTK dialog. */
Dtk::Widget::DAboutDialog *ensureAboutDialog()
{
    if (s_aboutDialog) {
        return s_aboutDialog;
    }

    auto *dialog = new Dtk::Widget::DAboutDialog;
    s_aboutDialog = dialog;
    if (auto *app = qobject_cast<Dtk::Widget::DApplication *>(QCoreApplication::instance())) {
        app->setAboutDialog(dialog);
    }
    return dialog;
}

} // namespace

void setupAboutDialog()
{
    configureAboutDialog(ensureAboutDialog(), defaultDescription());
}

void showAboutDialog(const QString &description)
{
    auto *dialog = ensureAboutDialog();
    configureAboutDialog(dialog, description);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}
