// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include <DApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>

#include "app.h"

DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("GXDE"));
    app.setApplicationName(QStringLiteral("gxde-ai-see"));
    app.setApplicationDisplayName(QStringLiteral("GXDE 小埃同学"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("GXDE 小埃同学"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption daemonOption(QStringLiteral("daemon"),
                                    QStringLiteral("后台常驻，需要时再显示面板"));
    QCommandLineOption showOption(QStringLiteral("show"),
                                  QStringLiteral("启动后立即显示传送门面板"));
    parser.addOption(daemonOption);
    parser.addOption(showOption);
    parser.process(app);

    App gxde;
    if (!gxde.init(parser.isSet(daemonOption), parser.isSet(showOption))) {
        return 0;
    }

    return app.exec();
}
