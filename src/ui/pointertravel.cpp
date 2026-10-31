// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pointertravel.h"

#ifdef GXDE_HAVE_WAYLAND_CLIENT

#include <QGuiApplication>
#include <QWaylandClientExtensionTemplate>

#include "qwayland-relative-pointer-unstable-v1.h"

class PointerTravel::Manager
    : public QWaylandClientExtensionTemplate<PointerTravel::Manager>
    , public QtWayland::zwp_relative_pointer_manager_v1
{
public:
    Manager()
        : QWaylandClientExtensionTemplate<Manager>(1)
    {
    }

    ~Manager() override
    {
        if (isActive() && object()) {
            destroy();
        }
    }
};

class PointerTravel::Relative : public QtWayland::zwp_relative_pointer_v1
{
public:
    Relative(::zwp_relative_pointer_v1 *object, PointerTravel *owner)
        : QtWayland::zwp_relative_pointer_v1(object)
        , m_owner(owner)
    {
    }

    ~Relative() override
    {
        if (object()) {
            destroy();
        }
    }

protected:
    void zwp_relative_pointer_v1_relative_motion(uint32_t, uint32_t, wl_fixed_t dx, wl_fixed_t dy,
                                                 wl_fixed_t, wl_fixed_t) override
    {
        /* the accelerated motion: what moves the pointer on screen */
        m_owner->addMotion(QPointF(wl_fixed_to_double(dx), wl_fixed_to_double(dy)));
    }

private:
    PointerTravel *m_owner = nullptr;
};

PointerTravel::PointerTravel(QObject *parent)
    : QObject(parent)
{
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        m_manager = std::make_unique<Manager>();
    }
}

PointerTravel::~PointerTravel() = default;

bool PointerTravel::start()
{
    m_travel = QPointF();
    if (!m_relative) {
        /* kept for good: it only reports while the pointer is over our
         * surfaces, and the next drag then does not wait for the binding */
        auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        if (!m_manager || !m_manager->isActive() || !wayland || !wayland->pointer()) {
            return false;
        }
        m_relative = std::make_unique<Relative>(
            m_manager->get_relative_pointer(wayland->pointer()), this);
    }
    m_running = true;
    return true;
}

void PointerTravel::stop()
{
    m_running = false;
}

bool PointerTravel::running() const
{
    return m_running;
}

#else

class PointerTravel::Manager
{
};
class PointerTravel::Relative
{
};

PointerTravel::PointerTravel(QObject *parent)
    : QObject(parent)
{
}

PointerTravel::~PointerTravel() = default;

bool PointerTravel::start()
{
    m_travel = QPointF();
    return false;
}

void PointerTravel::stop() { }

bool PointerTravel::running() const
{
    return m_running;
}

#endif

void PointerTravel::addMotion(const QPointF &delta)
{
    if (!m_running) {
        return;
    }
    m_travel += delta;
    Q_EMIT moved(m_travel);
}
