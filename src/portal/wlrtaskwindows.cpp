// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wlrtaskwindows.h"

#include <QGuiApplication>

#include <algorithm>
#include <functional>

namespace {
constexpr int kManagerVersion = 3;
}

class WlrToplevel : public QtWayland::zwlr_foreign_toplevel_handle_v1
{
public:
    WlrToplevel(::zwlr_foreign_toplevel_handle_v1 *handle, std::function<void()> changed,
                std::function<void(WlrToplevel *)> closed)
        : QtWayland::zwlr_foreign_toplevel_handle_v1(handle)
        , m_changed(std::move(changed))
        , m_closed(std::move(closed))
    {
    }

    ~WlrToplevel() override
    {
        if (object()) {
            destroy();
        }
    }

    QString id() const { return QString::number(reinterpret_cast<quintptr>(object()), 16); }

    TaskWindow task;
    bool ready = false;
    bool becameActive = false;

protected:
    void zwlr_foreign_toplevel_handle_v1_title(const QString &title) override
    {
        m_pending.title = title;
    }

    void zwlr_foreign_toplevel_handle_v1_app_id(const QString &appId) override
    {
        m_pending.appId = appId;
    }

    void zwlr_foreign_toplevel_handle_v1_state(wl_array *state) override
    {
        m_pending.minimized = false;
        m_pending.active = false;
        const auto *values = static_cast<const uint32_t *>(state->data);
        for (size_t i = 0; i < state->size / sizeof(uint32_t); ++i) {
            if (values[i] == state_minimized) {
                m_pending.minimized = true;
            } else if (values[i] == state_activated) {
                m_pending.active = true;
            }
        }
    }

    void zwlr_foreign_toplevel_handle_v1_done() override
    {
        becameActive = m_pending.active && !task.active;
        m_pending.id = id();
        task = m_pending;
        ready = true;
        m_changed();
    }

    void zwlr_foreign_toplevel_handle_v1_closed() override { m_closed(this); }

private:
    TaskWindow m_pending;
    std::function<void()> m_changed;
    std::function<void(WlrToplevel *)> m_closed;
};

class WlrTaskWindows::Manager
    : public QWaylandClientExtensionTemplate<WlrTaskWindows::Manager>
    , public QtWayland::zwlr_foreign_toplevel_manager_v1
{
public:
    explicit Manager(WlrTaskWindows *owner)
        : QWaylandClientExtensionTemplate<Manager>(kManagerVersion)
        , m_owner(owner)
    {
    }

    ~Manager() override
    {
        if (isActive() && object()) {
            stop();
        }
    }

protected:
    void zwlr_foreign_toplevel_manager_v1_toplevel(::zwlr_foreign_toplevel_handle_v1 *handle) override
    {
        m_owner->addToplevel(handle);
    }

private:
    WlrTaskWindows *m_owner = nullptr;
};

WlrTaskWindows::WlrTaskWindows(QObject *parent)
    : TaskWindows(parent)
    , m_manager(std::make_unique<Manager>(this))
{
}

WlrTaskWindows::~WlrTaskWindows()
{
    m_toplevels.clear();
}

bool WlrTaskWindows::available() const
{
    return m_manager->isActive();
}

void WlrTaskWindows::addToplevel(::zwlr_foreign_toplevel_handle_v1 *handle)
{
    auto toplevel = std::make_unique<WlrToplevel>(
        handle,
        [this]() {
            /* keep the vector in activation order: the newly active goes on top */
            auto it = std::find_if(m_toplevels.begin(), m_toplevels.end(),
                                   [](const auto &t) { return t->becameActive; });
            if (it != m_toplevels.end()) {
                (*it)->becameActive = false;
                std::rotate(it, it + 1, m_toplevels.end());
            }
            Q_EMIT windowsChanged();
        },
        [this](WlrToplevel *closed) {
            auto it = std::find_if(m_toplevels.begin(), m_toplevels.end(),
                                   [closed](const auto &t) { return t.get() == closed; });
            if (it == m_toplevels.end()) {
                return;
            }
            /* we are inside the handle's own event: delete it once that returned */
            WlrToplevel *doomed = it->release();
            m_toplevels.erase(it);
            QMetaObject::invokeMethod(this, [doomed]() { delete doomed; }, Qt::QueuedConnection);
            Q_EMIT windowsChanged();
        });
    m_toplevels.push_back(std::move(toplevel));
}

WlrToplevel *WlrTaskWindows::find(const QString &id) const
{
    for (const auto &toplevel : m_toplevels) {
        if (toplevel->ready && toplevel->task.id == id) {
            return toplevel.get();
        }
    }
    return nullptr;
}

QList<TaskWindow> WlrTaskWindows::windows() const
{
    QList<TaskWindow> result;
    for (const auto &toplevel : m_toplevels) {
        if (toplevel->ready) {
            result.append(toplevel->task);
        }
    }
    return result;
}

void WlrTaskWindows::minimize(const QString &id)
{
    if (WlrToplevel *toplevel = find(id)) {
        toplevel->set_minimized();
    }
}

void WlrTaskWindows::unminimize(const QString &id)
{
    if (WlrToplevel *toplevel = find(id)) {
        toplevel->unset_minimized();
    }
}

void WlrTaskWindows::activate(const QString &id)
{
    WlrToplevel *toplevel = find(id);
    auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!toplevel || !wayland || !wayland->seat()) {
        return;
    }
    toplevel->unset_minimized();
    toplevel->activate(wayland->seat());
}
