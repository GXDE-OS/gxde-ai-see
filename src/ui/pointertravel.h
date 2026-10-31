// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointF>

#include <memory>

/**
 * How far the pointer travelled since start(), in logical pixels.
 *
 * A Wayland client never learns where the pointer is on screen, and the
 * surface-local positions it does get are useless while the surface itself
 * moves under the pointer (a layer-shell panel dragged by its margins).
 * zwp_relative_pointer_v1 reports the motion itself, independent of where
 * any surface is, so the panel can follow it like X11 follows QCursor::pos().
 */
class PointerTravel : public QObject
{
    Q_OBJECT

public:
    explicit PointerTravel(QObject *parent = nullptr);
    ~PointerTravel() override;

    /** Start counting from zero.  @return false when the compositor has no relative pointer. */
    bool start();
    void stop();
    bool running() const;

    QPointF travel() const { return m_travel; }

signals:
    void moved(const QPointF &travel);

private:
    class Manager;
    class Relative;

    void addMotion(const QPointF &delta);

    std::unique_ptr<Manager> m_manager;
    std::unique_ptr<Relative> m_relative;
    QPointF m_travel;
    bool m_running = false;
};
