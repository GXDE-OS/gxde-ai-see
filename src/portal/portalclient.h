// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

/**
 * Client for the compositor side "portal" (org.gxde.Portal).
 *
 * Start with \a beginPortal() while a drag hovers the portal zone.  The
 * compositor opens the window overview in portal mode and forwards the drag
 * to the window under the cursor, so the target application receives a real
 * wl_data_device drop at the cursor position.
 */
class PortalClient : public QObject
{
    Q_OBJECT

public:
    static PortalClient *instance();

    bool available() const { return m_available; }

    /** @return true when the compositor opened the overview. */
    bool beginPortal();
    void cancelPortal();

    /** Filter the drag mime types we want to react to. */
    static bool mimeInteresting(const QString &mimeTypes);

signals:
    void dragStarted(const QString &mimeTypes);
    void dragEnded();
    void dropped(const QString &uuid, bool accepted);

private slots:
    void onDragStarted(const QString &mimeTypes);
    void onDragEnded();
    void onDropped(const QString &uuid, bool accepted);

private:
    explicit PortalClient(QObject *parent = nullptr);

    bool m_available = false;
};
