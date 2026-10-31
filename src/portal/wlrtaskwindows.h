// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taskwindows.h"

#include <QWaylandClientExtensionTemplate>

#include "qwayland-wlr-foreign-toplevel-management-unstable-v1.h"

#include <memory>
#include <vector>

class WlrToplevel;

/**
 * TaskWindows on top of zwlr_foreign_toplevel_manager_v1, which treeland
 * (waylib's WForeignToplevel), wlcom and the other wlroots compositors offer.
 */
class WlrTaskWindows : public TaskWindows
{
    Q_OBJECT

public:
    explicit WlrTaskWindows(QObject *parent = nullptr);
    ~WlrTaskWindows() override;

    bool available() const override;
    QList<TaskWindow> windows() const override;
    void minimize(const QString &id) override;
    void unminimize(const QString &id) override;
    void activate(const QString &id) override;

private:
    class Manager;
    friend class Manager;

    void addToplevel(::zwlr_foreign_toplevel_handle_v1 *handle);
    WlrToplevel *find(const QString &id) const;

    std::unique_ptr<Manager> m_manager;
    /* bottom to top, by activation history */
    std::vector<std::unique_ptr<WlrToplevel>> m_toplevels;
};
