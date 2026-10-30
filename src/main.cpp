// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include <DApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QIcon>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>

#include "app.h"

DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("GXDE"));
    app.setApplicationName(QStringLiteral("gxde-ai-see"));
    app.setApplicationDisplayName(QStringLiteral("GXDE 小埃同学"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    QIcon appIcon = QIcon::fromTheme(QStringLiteral("gxde-ai-see"));
    if (appIcon.isNull()) {
        appIcon = QIcon(QStringLiteral("/usr/share/icons/hicolor/256x256/apps/gxde-ai-see.png"));
    }
    app.setWindowIcon(appIcon);
    app.setQuitOnLastWindowClosed(false);

    const QString locale = QLocale::system().name();
    auto *translator = new QTranslator(&app);
    const QStringList translationDirs = {
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/translations"),
        QStringLiteral("/usr/share/gxde-ai-see/translations"),
        QStringLiteral("/usr/local/share/gxde-ai-see/translations"),
    };
    bool translated = false;
    for (const QString &dir : translationDirs) {
        if (translator->load(QStringLiteral("gxde-ai-see_") + locale, dir) ||
            translator->load(QStringLiteral("gxde-ai-see_") + locale.section(QLatin1Char('_'), 0, 0),
                             dir)) {
            app.installTranslator(translator);
            translated = true;
            break;
        }
    }
    Q_UNUSED(translated)

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("GXDE 小埃同学"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption daemonOption(QStringLiteral("daemon"),
                                    QStringLiteral("后台常驻，需要时再显示面板"));
    QCommandLineOption showOption(QStringLiteral("show"),
                                  QStringLiteral("启动后立即显示传送门面板"));
    QCommandLineOption todoOption(QStringLiteral("todo"),
                                  QStringLiteral("打开待办窗口"));
    QCommandLineOption chatOption(QStringLiteral("chat"),
                                  QStringLiteral("打开小埃同学对话窗口"));
    parser.addOption(daemonOption);
    parser.addOption(showOption);
    parser.addOption(todoOption);
    parser.addOption(chatOption);
    parser.process(app);

    App gxde;
    if (!gxde.init(parser.isSet(daemonOption), parser.isSet(showOption),
                          parser.isSet(todoOption), parser.isSet(chatOption))) {
        return 0;
    }

    return app.exec();
}
