// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "service.h"

#include "config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariant>

PortalService *PortalService::instance()
{
    static PortalService *self = new PortalService;
    return self;
}

PortalService::PortalService(QObject *parent)
    : QObject(parent)
{
}

QString PortalService::autostartPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
        QStringLiteral("/GXDE/gxde-k9/slimy/gxde-ai-see.slimy");
}

void PortalService::init()
{
    const QVariant stored =
        Config::instance()->get(QStringLiteral("service/enabled"));
    m_enabled = stored.isValid() ? stored.toBool() : true;
    if (!stored.isValid()) {
        /* Persist the default so a future default change does not silently
         * re-enable a service the user turned off. */
        Config::instance()->set(QStringLiteral("service/enabled"), m_enabled);
        Config::instance()->sync();
    }

    if (m_enabled) {
        writeAutostartFile();
    } else {
        removeAutostartFile();
    }
}

void PortalService::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        /* Keep the filesystem in sync if it was changed behind our back. */
        if (enabled) {
            writeAutostartFile();
        } else {
            removeAutostartFile();
        }
        return;
    }
    m_enabled = enabled;
    Config::instance()->set(QStringLiteral("service/enabled"), enabled);
    Config::instance()->sync();

    if (enabled) {
        writeAutostartFile();
    } else {
        removeAutostartFile();
    }
    Q_EMIT enabledChanged(enabled);
}

void PortalService::writeAutostartFile()
{
    const QString path = autostartPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    static const QByteArray script = R"SCRIPT(#!/bin/bash
PID_DIR="${PID_DIR:-/tmp/GXDE/gxde-k9/}"
mkdir -p "$PID_DIR"
# A manually started chat instance already provides the service; do not spawn
# a short-lived duplicate every k9 tick.
if pgrep -u "$(id -u)" -x gxde-ai-see >/dev/null 2>&1; then
  exit 0
fi
PID_FILE="$PID_DIR/gxde-ai-see.pid"
if [ -f "$PID_FILE" ]; then
  PID=$(cat "$PID_FILE" 2>/dev/null)
  [ -n "$PID" ] && [ -e "/proc/$PID" ] && exit 0
  rm -f "$PID_FILE"
fi
/usr/bin/gxde-ai-see --daemon >/dev/null 2>&1 &
echo $! > "$PID_FILE"
)SCRIPT";

    QFile file(path);
    if (file.exists() && file.open(QIODevice::ReadOnly) && file.readAll() == script) {
        file.close();
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                        QFileDevice::ExeOwner);
        return;
    }
    file.close();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(script);
    file.close();
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                    QFileDevice::ExeOwner);
}

void PortalService::removeAutostartFile()
{
    QFile::remove(autostartPath());
}
