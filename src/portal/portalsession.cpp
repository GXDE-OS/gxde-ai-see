// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "portalsession.h"

#include <QIcon>
#include <QUrl>

PortalSession::PortalSession(QObject *parent)
    : QAbstractListModel(parent)
{
}

PortalSession::~PortalSession()
{
    /* never leave the desktop with everything minimized */
    cancel();
}

bool PortalSession::supported()
{
    TaskWindows *tasks = TaskWindows::instance();
    return tasks && tasks->available();
}

int PortalSession::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant PortalSession::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= count()) {
        return {};
    }
    /* the grid lists the top-most window first */
    const TaskWindow &window = m_windows.at(count() - 1 - index.row());
    switch (role) {
    case WindowIdRole:
        return window.id;
    case Qt::DisplayRole:
    case TitleRole:
        return window.title;
    case IconSourceRole:
        return QStringLiteral("image://taskwindow/") + QUrl::toPercentEncoding(window.id);
    default:
        return {};
    }
}

QHash<int, QByteArray> PortalSession::roleNames() const
{
    return {
        { WindowIdRole, "windowId" },
        { TitleRole, "title" },
        { IconSourceRole, "iconSource" },
    };
}

bool PortalSession::begin()
{
    TaskWindows *tasks = TaskWindows::instance();
    if (m_active || !tasks || !tasks->available()) {
        return m_active;
    }

    QList<TaskWindow> shown;
    QString previouslyActive;
    for (const TaskWindow &window : tasks->windows()) {
        if (window.minimized) {
            continue;
        }
        shown.append(window);
        if (window.active) {
            previouslyActive = window.id;
        }
    }
    if (shown.isEmpty()) {
        return false;
    }

    beginResetModel();
    m_windows = shown;
    m_previouslyActive = previouslyActive;
    endResetModel();

    for (const TaskWindow &window : std::as_const(m_windows)) {
        tasks->minimize(window.id);
    }

    m_active = true;
    Q_EMIT activeChanged();
    return true;
}

void PortalSession::activate(const QString &windowId)
{
    if (m_active) {
        restore(windowId);
    }
}

void PortalSession::cancel()
{
    if (m_active) {
        restore(m_previouslyActive);
    }
}

void PortalSession::restore(const QString &front)
{
    if (TaskWindows *tasks = TaskWindows::instance()) {
        /* bottom to top, so X11 (which can only "activate") ends up with the
         * original stacking; the front window goes last */
        for (const TaskWindow &window : std::as_const(m_windows)) {
            if (window.id != front) {
                tasks->unminimize(window.id);
            }
        }
        if (!front.isEmpty()) {
            tasks->activate(front);
        }
    }
    reset();
}

void PortalSession::reset()
{
    m_active = false;
    m_previouslyActive.clear();
    /* keep the rows until the grid has faded out; begin() replaces them */
    Q_EMIT activeChanged();
}

QImage PortalSession::iconFor(const QString &windowId) const
{
    for (const TaskWindow &window : m_windows) {
        if (window.id != windowId) {
            continue;
        }
        if (!window.icon.isNull()) {
            return window.icon;
        }
        QString name = TaskWindows::iconNameForApp(window.appId);
        QIcon icon = name.startsWith(QLatin1Char('/')) ? QIcon(name) : QIcon::fromTheme(name);
        if (icon.isNull()) {
            icon = QIcon::fromTheme(QStringLiteral("application-x-executable"));
        }
        return icon.pixmap(64, 64).toImage();
    }
    return {};
}

TaskWindowIconProvider::TaskWindowIconProvider(PortalSession *session)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_session(session)
{
}

QImage TaskWindowIconProvider::requestImage(const QString &id, QSize *size,
                                            const QSize &requestedSize)
{
    QImage image = m_session ? m_session->iconFor(QUrl::fromPercentEncoding(id.toUtf8()))
                             : QImage();
    if (!image.isNull() && requestedSize.isValid()) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (size) {
        *size = image.size();
    }
    return image;
}
