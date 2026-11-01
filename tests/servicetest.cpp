// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Checks the portal service defaults and the gxde-k9 autostart file lifecycle.

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTemporaryDir>

#include "config.h"
#include "service.h"

static int failures = 0;

static void check(bool condition, const QString &what)
{
    QTextStream out(stdout);
    out << (condition ? "PASS " : "FAIL ") << what << "\n";
    out.flush();
    if (!condition) {
        ++failures;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("GXDE"));
    app.setApplicationName(QStringLiteral("gxde-ai-see"));

    QTemporaryDir tmp;
    qputenv("XDG_CONFIG_HOME", (tmp.path() + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_DATA_HOME", (tmp.path() + QStringLiteral("/data")).toUtf8());
    qputenv("XDG_CACHE_HOME", (tmp.path() + QStringLiteral("/cache")).toUtf8());
    Config::instance()->init();

    PortalService *service = PortalService::instance();
    const QString path = service->autostartPath();
    QFile::remove(path);

    service->init();
    check(service->enabled(), "service defaults to enabled");
    check(QFile::exists(path), "autostart slimy created by default");
    check(QFileInfo(path).isExecutable(), "autostart slimy is executable");
    check(Config::instance()->getBool(QStringLiteral("service/enabled"), false),
          "default persisted in config");

    bool signalValue = true;
    int signalCount = 0;
    QObject::connect(service, &PortalService::enabledChanged,
                     [&](bool enabled) {
                         signalValue = enabled;
                         ++signalCount;
                     });

    service->setEnabled(false);
    check(!service->enabled(), "service disabled");
    check(!QFile::exists(path), "autostart slimy removed when disabled");
    check(signalCount == 1 && !signalValue, "disable signal emitted");
    check(!Config::instance()->getBool(QStringLiteral("service/enabled"), true),
          "disabled state persisted");

    service->setEnabled(true);
    check(service->enabled(), "service re-enabled");
    check(QFile::exists(path), "autostart slimy restored");
    check(signalCount == 2 && signalValue, "enable signal emitted");

    /* A fresh init must honour the persisted off state. */
    Config::instance()->set(QStringLiteral("service/enabled"), false);
    service->init();
    check(!service->enabled(), "init honours the stored disabled state");
    check(!QFile::exists(path), "init removes the autostart file when disabled");

    QTextStream(stdout) << (failures == 0 ? "ALL PASS\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
