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
#include "ui/aboutdialog.h"

DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("GXDE"));
    app.setApplicationName(QStringLiteral("gxde-ai-see"));
    app.setApplicationDisplayName(QStringLiteral("GXDE 小埃同学"));
    app.setApplicationVersion(QStringLiteral("1.0.0~beta5"));
    QIcon appIcon = QIcon::fromTheme(QStringLiteral("gxde-ai-see"));
    if (appIcon.isNull()) {
        appIcon = QIcon(QStringLiteral("/usr/share/icons/hicolor/256x256/apps/gxde-ai-see.png"));
    }
    app.setWindowIcon(appIcon);
    app.setQuitOnLastWindowClosed(false);

    /* Metadata for the stock about dialog that the DTK titlebar hamburger menu
     * opens.  Without it DTK shows a tiny, empty dialog. */
    app.setProductName(QStringLiteral("GXDE 小埃同学"));
    app.setProductIcon(appIcon);
    app.setApplicationDescription(
        QStringLiteral("GXDE 桌面的 AI 助手：传送门、内容工厂、待办与小埃同学对话。"));
    app.setApplicationLicense(QStringLiteral("GPL-3.0-or-later"));
    app.setApplicationHomePage(QStringLiteral("https://gitee.com/GXDE-OS/gxde-ai-see"));
    setupAboutDialog();

    const QString locale = QLocale::system().name();
    auto *translator = new QTranslator(&app);
    const QStringList translationDirs = {
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/translations"),
        QStringLiteral("/usr/share/gxde-ai-see/translations"),
        QStringLiteral("/usr/local/share/gxde-ai-see/translations"),
    };
    QStringList candidates{ locale, locale.section(QLatin1Char('_'), 0, 0) };
    /* ensure unsupported locales fall back to English, never to the Chinese
     * source strings */
    if (!locale.startsWith(QLatin1String("zh"))) {
        candidates.append(QStringLiteral("en"));
    }
    for (const QString &dir : translationDirs) {
        bool loaded = false;
        for (const QString &candidate : candidates) {
            if (translator->load(QStringLiteral("gxde-ai-see_") + candidate, dir)) {
                app.installTranslator(translator);
                loaded = true;
                break;
            }
        }
        if (loaded) {
            break;
        }
    }

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
