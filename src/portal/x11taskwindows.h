// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taskwindows.h"

#include <QHash>
#include <QByteArray>

#include <xcb/xcb.h>

/** EWMH backed TaskWindows, works with any compliant X11 window manager. */
class X11TaskWindows : public TaskWindows
{
    Q_OBJECT

public:
    explicit X11TaskWindows(xcb_connection_t *connection, QObject *parent = nullptr);

    bool available() const override;
    QList<TaskWindow> windows() const override;
    void minimize(const QString &id) override;
    void unminimize(const QString &id) override;
    void activate(const QString &id) override;

private:
    xcb_atom_t atom(const char *name) const;
    QList<xcb_window_t> windowList(xcb_atom_t property) const;
    QList<xcb_atom_t> atomList(xcb_window_t window, xcb_atom_t property) const;
    QByteArray property(xcb_window_t window, xcb_atom_t property, xcb_atom_t type,
                        uint32_t maxLength = 4096) const;
    bool listable(xcb_window_t window) const;
    QString title(xcb_window_t window) const;
    QString appId(xcb_window_t window) const;
    QImage icon(xcb_window_t window) const;
    void sendRootMessage(xcb_window_t window, xcb_atom_t type, uint32_t d0, uint32_t d1 = 0,
                         uint32_t d2 = 0);

    xcb_connection_t *m_connection = nullptr;
    xcb_window_t m_root = 0;
    mutable QHash<QByteArray, xcb_atom_t> m_atoms;
};
