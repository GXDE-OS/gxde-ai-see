// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "taskwindows.h"

#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QStandardPaths>

#ifdef GXDE_HAVE_XCB
#include "x11taskwindows.h"
#endif
#ifdef GXDE_HAVE_WAYLAND_CLIENT
#include "wlrtaskwindows.h"
#endif

namespace {

QString desktopFileIcon(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    bool inEntry = false;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.startsWith('[')) {
            inEntry = line == "[Desktop Entry]";
            continue;
        }
        if (inEntry && line.startsWith("Icon=")) {
            return QString::fromUtf8(line.mid(5)).trimmed();
        }
    }
    return {};
}

} // namespace

TaskWindows *TaskWindows::instance()
{
    static TaskWindows *self = []() -> TaskWindows * {
        const QString platform = QGuiApplication::platformName();
#ifdef GXDE_HAVE_XCB
        if (platform == QLatin1String("xcb") || platform == QLatin1String("dxcb")) {
            if (auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
                return new X11TaskWindows(x11->connection(), qGuiApp);
            }
        }
#endif
#ifdef GXDE_HAVE_WAYLAND_CLIENT
        if (platform.startsWith(QLatin1String("wayland"))) {
            return new WlrTaskWindows(qGuiApp);
        }
#endif
        Q_UNUSED(platform)
        return nullptr;
    }();
    return self;
}

QString TaskWindows::iconNameForApp(const QString &appId)
{
    if (appId.isEmpty()) {
        return {};
    }

    static QHash<QString, QString> cache;
    auto it = cache.constFind(appId);
    if (it != cache.constEnd()) {
        return it.value();
    }

    /* app ids come as "org.gnome.Foo", "Dde-file-manager", "foo" ... */
    QStringList candidates{ appId, appId.toLower() };
    const QString last = appId.section(QLatin1Char('.'), -1);
    if (last != appId) {
        candidates << last << last.toLower();
    }

    QString name;
    for (const QString &candidate : std::as_const(candidates)) {
        const QString desktop = QStandardPaths::locate(
            QStandardPaths::ApplicationsLocation, candidate + QStringLiteral(".desktop"));
        if (!desktop.isEmpty()) {
            name = desktopFileIcon(desktop);
            if (!name.isEmpty()) {
                break;
            }
        }
    }
    if (name.isEmpty()) {
        for (const QString &candidate : std::as_const(candidates)) {
            if (QIcon::hasThemeIcon(candidate)) {
                name = candidate;
                break;
            }
        }
    }

    cache.insert(appId, name);
    return name;
}
