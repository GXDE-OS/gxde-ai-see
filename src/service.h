// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

/**
 * Controls the portal background service.
 *
 * "Service enabled" means: start at login / stay alive through gxde-k9 (the
 * .slimy file) *and* let the portal panel react to drags.  Disabling it
 * removes the autostart file and turns the portal feature off without
 * quitting the process, so the chat stays usable (and can re-enable it).
 *
 * The service is enabled by default.
 */
class PortalService : public QObject
{
    Q_OBJECT

public:
    static PortalService *instance();

    /** Read the persisted setting and sync the autostart file. */
    void init();

    bool enabled() const { return m_enabled; }
    QString autostartPath() const;

    void setEnabled(bool enabled);

signals:
    void enabledChanged(bool enabled);

private:
    explicit PortalService(QObject *parent = nullptr);

    void writeAutostartFile();
    void removeAutostartFile();

    bool m_enabled = true;
};
