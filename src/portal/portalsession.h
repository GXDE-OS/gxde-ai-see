// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QQuickImageProvider>

#include "taskwindows.h"

/**
 * One portal "jump": while a drag hovers the portal every window is
 * minimized and listed in a grid next to the panel.  Hovering a tile
 * activates that window so the drag can continue into it; leaving the portal
 * puts every window back the way it was.
 *
 * Rows are ordered top-most window first.
 */
class PortalSession : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(int count READ count NOTIFY activeChanged)

public:
    enum Roles {
        WindowIdRole = Qt::UserRole + 1,
        TitleRole,
        IconSourceRole,
    };

    explicit PortalSession(QObject *parent = nullptr);
    ~PortalSession() override;

    static bool supported();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool active() const { return m_active; }
    int count() const { return int(m_windows.size()); }

    /** Minimize everything and fill the grid; false when nothing is open. */
    bool begin();
    /** Restore the other windows and bring \a windowId to the front. */
    Q_INVOKABLE void activate(const QString &windowId);
    /** Restore every window as it was before begin(). */
    Q_INVOKABLE void cancel();

    QImage iconFor(const QString &windowId) const;

signals:
    void activeChanged();

private:
    void restore(const QString &front);
    void reset();

    bool m_active = false;
    QList<TaskWindow> m_windows; // bottom to top
    QString m_previouslyActive;
};

/** Serves image://taskwindow/<id> for the portal grid. */
class TaskWindowIconProvider : public QQuickImageProvider
{
public:
    explicit TaskWindowIconProvider(PortalSession *session);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    PortalSession *m_session = nullptr;
};
