// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ukuiwindow.h"

#include <QGuiApplication>
#include <QWindow>

#ifdef GXDE_HAVE_WAYLAND_CLIENT
#include <QWaylandClientExtensionTemplate>
#include <qpa/qplatformnativeinterface.h>

#include <functional>
#include <memory>
#include <wayland-client.h>

#include "qwayland-ukui-shell.h"

namespace {

constexpr int kShellVersion = 4;

class Shell : public QWaylandClientExtensionTemplate<Shell>
    , public QtWayland::ukui_shell
{
public:
    Shell()
        : QWaylandClientExtensionTemplate<Shell>(kShellVersion)
    {
    }
};

class Surface : public QtWayland::ukui_surface
{
public:
    using PositionCallback = std::function<void(int, int)>;

    Surface(::ukui_surface *surface, PositionCallback callback)
        : QtWayland::ukui_surface(surface)
        , m_callback(std::move(callback))
    {
    }

    ~Surface() override
    {
        if (object()) {
            destroy();
        }
    }

protected:
    void ukui_surface_position(int32_t x, int32_t y) override
    {
        if (m_callback) {
            m_callback(x, y);
        }
    }

private:
    PositionCallback m_callback;
};

} // namespace
#endif

class UkuiWindow::Private
{
public:
    ~Private()
    {
#ifdef GXDE_HAVE_WAYLAND_CLIENT
        m_surface.reset();
        delete m_shell;
#endif
    }

#ifdef GXDE_HAVE_WAYLAND_CLIENT
    Shell *m_shell = nullptr;
    std::unique_ptr<Surface> m_surface;
#endif
};

UkuiWindow::UkuiWindow(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
}

UkuiWindow::~UkuiWindow()
{
    delete d;
}

bool UkuiWindow::available() const
{
#ifdef GXDE_HAVE_WAYLAND_CLIENT
    if (!QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        return false;
    }
    if (!d->m_shell) {
        d->m_shell = new Shell;
    }
    return d->m_shell->isActive();
#else
    return false;
#endif
}

bool UkuiWindow::attach(QWindow *window)
{
#ifdef GXDE_HAVE_WAYLAND_CLIENT
    if (!window || !available() || !d->m_shell->isActive()) {
        return false;
    }
    if (!window->handle()) {
        window->create();
    }
    auto *native = QGuiApplication::platformNativeInterface();
    if (!native) {
        return false;
    }
    void *resource = native->nativeResourceForWindow(QByteArrayLiteral("wl_surface"), window);
    if (!resource) {
        resource = native->nativeResourceForWindow(QByteArrayLiteral("surface"), window);
    }
    auto *surface = static_cast<wl_surface *>(resource);
    if (!surface) {
        return false;
    }
    d->m_surface = std::make_unique<Surface>(d->m_shell->create_surface(surface),
                                             [this](int x, int y) {
                                                 Q_EMIT positionChanged(x, y);
                                             });
    setSkipTaskbar(true);
    return true;
#else
    Q_UNUSED(window)
    return false;
#endif
}

void UkuiWindow::setSkipTaskbar(bool skip)
{
#ifdef GXDE_HAVE_WAYLAND_CLIENT
    if (d->m_surface) {
        d->m_surface->set_skip_taskbar(skip ? 1 : 0);
        d->m_surface->set_skip_switcher(skip ? 1 : 0);
    }
#else
    Q_UNUSED(skip)
#endif
}

void UkuiWindow::setPosition(int x, int y)
{
#ifdef GXDE_HAVE_WAYLAND_CLIENT
    if (d->m_surface) {
        d->m_surface->set_position(x, y);
    }
#else
    Q_UNUSED(x)
    Q_UNUSED(y)
#endif
}

