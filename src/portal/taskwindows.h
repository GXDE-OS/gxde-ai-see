// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QString>

/** One toplevel window of another application, as seen by a taskbar. */
struct TaskWindow
{
    QString id;      // backend specific, stable while the window lives
    QString title;
    QString appId;   // WM_CLASS / xdg app_id, used to look up a theme icon
    QImage icon;     // window supplied icon, may be null
    bool minimized = false;
    bool active = false;
};

/**
 * Taskbar style access to the other windows on the desktop.
 *
 * X11 goes through EWMH; Wayland (treeland, wlroots based compositors) goes
 * through wlr-foreign-toplevel-management.  windows() is ordered bottom to
 * top: the real stacking order on X11, the activation history on Wayland.
 */
class TaskWindows : public QObject
{
    Q_OBJECT

public:
    /** @return the backend for the running platform, or nullptr. */
    static TaskWindows *instance();

    virtual bool available() const = 0;
    virtual QList<TaskWindow> windows() const = 0;
    virtual void minimize(const QString &id) = 0;
    virtual void unminimize(const QString &id) = 0;
    /** Raise, unminimize and focus. */
    virtual void activate(const QString &id) = 0;

    /** Theme icon name for an app id, resolved through its .desktop file. */
    static QString iconNameForApp(const QString &appId);

signals:
    void windowsChanged();

protected:
    explicit TaskWindows(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
};
