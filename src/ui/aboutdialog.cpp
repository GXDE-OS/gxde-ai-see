// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "aboutdialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

#include <DAboutDialog>

void showAboutDialog(QWidget *parent, const QString &description)
{
    auto *dialog = new Dtk::Widget::DAboutDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QObject::tr("关于小埃同学"));
    dialog->setProductIcon(QApplication::windowIcon());
    dialog->setProductName(QObject::tr("GXDE 小埃同学"));
    dialog->setVersion(QCoreApplication::applicationVersion());
    dialog->setDescription(description);
    dialog->setLicense(QStringLiteral("GPL-3.0-or-later"));
    dialog->setWebsiteName(QStringLiteral("gitee.com/GXDE-OS/gxde-ai-see"));
    dialog->setWebsiteLink(QStringLiteral("https://gitee.com/GXDE-OS/gxde-ai-see"));
    dialog->show();
}
