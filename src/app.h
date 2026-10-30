// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

class PortalPanel;
class QLocalServer;
class StagingModel;

/**
 * Application controller: owns the staging model, the panel and the single
 * instance machinery.
 */
class App : public QObject
{
    Q_OBJECT

public:
    explicit App(QObject *parent = nullptr);

    /**
     * @param daemon    run in the background, only show the panel when needed
     * @param forceShow always show the panel on startup
     * @return false when another instance is already running
     */
    bool init(bool daemon, bool forceShow);

private:
    void ensurePanel();
    void showPanel();

    StagingModel *m_model = nullptr;
    PortalPanel *m_panel = nullptr;
    QLocalServer *m_server = nullptr;
    bool m_daemon = false;
};
