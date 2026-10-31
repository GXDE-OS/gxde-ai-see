// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dragwatcher.h"

#include <QGuiApplication>
#include <QTimer>

#include <memory>

#ifdef GXDE_HAVE_XCB_XFIXES
#include <QAbstractNativeEventFilter>

#include <xcb/xcb.h>
#include <xcb/xfixes.h>

#include <cstdlib>
#endif

#ifdef GXDE_HAVE_WAYLAND_CLIENT
#include <QWaylandClientExtensionTemplate>

#include "qwayland-treeland-dde-shell-v1.h"
#endif

#ifdef GXDE_HAVE_XCB_XFIXES
namespace {

// X11
class X11DragWatcher : public DragWatcher, public QAbstractNativeEventFilter
{
public:
    X11DragWatcher(xcb_connection_t *connection, QObject *parent)
        : DragWatcher(parent)
        , m_connection(connection)
    {
        const xcb_setup_t *setup = xcb_get_setup(m_connection);
        xcb_screen_iterator_t screens = xcb_setup_roots_iterator(setup);
        if (screens.rem == 0) {
            return;
        }
        m_root = screens.data->root;

        const xcb_query_extension_reply_t *ext = xcb_get_extension_data(m_connection, &xcb_xfixes_id);
        if (!ext || !ext->present) {
            return;
        }
        m_eventBase = ext->first_event;
        free(xcb_xfixes_query_version_reply(
            m_connection, xcb_xfixes_query_version(m_connection, 1, 0), nullptr));

        static const char name[] = "XdndSelection";
        xcb_intern_atom_reply_t *atom = xcb_intern_atom_reply(
            m_connection, xcb_intern_atom(m_connection, false, sizeof(name) - 1, name), nullptr);
        if (!atom) {
            return;
        }
        m_xdnd = atom->atom;
        free(atom);

        /* every drag source claims XdndSelection when its drag starts */
        xcb_xfixes_select_selection_input(m_connection, m_root, m_xdnd,
                                          XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER);
        xcb_flush(m_connection);
        qApp->installNativeEventFilter(this);

        /* the drag source holds the pointer: the drag lasts while a button does */
        m_poll.setInterval(100);
        connect(&m_poll, &QTimer::timeout, this, [this]() {
            if (!buttonsDown()) {
                m_poll.stop();
                /* let the drop (maybe onto the panel) arrive before reacting */
                QTimer::singleShot(400, this, [this]() {
                    if (!m_poll.isActive()) {
                        Q_EMIT dragEnded();
                    }
                });
            }
        });
    }

    ~X11DragWatcher() override
    {
        if (qApp) {
            qApp->removeNativeEventFilter(this);
        }
    }

    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *) override
    {
        if (type != "xcb_generic_event_t") {
            return false;
        }
        auto *event = static_cast<xcb_generic_event_t *>(message);
        if ((event->response_type & ~0x80) != m_eventBase + XCB_XFIXES_SELECTION_NOTIFY) {
            return false;
        }
        auto *notify = reinterpret_cast<xcb_xfixes_selection_notify_event_t *>(event);
        if (notify->selection == m_xdnd && notify->owner != XCB_NONE && !m_poll.isActive()) {
            QMetaObject::invokeMethod(this, [this]() {
                if (!m_poll.isActive() && buttonsDown()) {
                    m_poll.start();
                    Q_EMIT dragStarted();
                }
            }, Qt::QueuedConnection);
        }

        return false;
    }

private:
    bool buttonsDown() const
    {
        xcb_query_pointer_reply_t *reply =
            xcb_query_pointer_reply(m_connection, xcb_query_pointer(m_connection, m_root), nullptr);
        if (!reply) {
            return false;
        }
        const bool down = reply->mask & (XCB_BUTTON_MASK_1 | XCB_BUTTON_MASK_2 | XCB_BUTTON_MASK_3);
        free(reply);
        return down;
    }

    xcb_connection_t *m_connection = nullptr;
    xcb_window_t m_root = XCB_NONE;
    xcb_atom_t m_xdnd = XCB_NONE;
    uint8_t m_eventBase = 0;
    QTimer m_poll;
};

} // namespace
#endif

// GXWM/FlakeWM
#ifdef GXDE_HAVE_WAYLAND_CLIENT
namespace {

class TreelandDragWatcher : public DragWatcher
{
public:
    explicit TreelandDragWatcher(QObject *parent)
        : DragWatcher(parent)
        , m_manager(std::make_unique<Manager>())
    {
        connect(m_manager.get(), &QWaylandClientExtension::activeChanged, this,
                &TreelandDragWatcher::bind);
        bind();
    }

private:
    /* bound at version 1, which has no destructor request: it lives with the
     * connection */
    class Manager : public QWaylandClientExtensionTemplate<Manager>,
                    public QtWayland::treeland_dde_shell_manager_v1
    {
    public:
        Manager()
            : QWaylandClientExtensionTemplate<Manager>(1)
        {
        }
    };

    class Active : public QtWayland::treeland_dde_active_v1
    {
    public:
        Active(::treeland_dde_active_v1 *object, DragWatcher *owner)
            : QtWayland::treeland_dde_active_v1(object)
            , m_owner(owner)
        {
        }

        ~Active() override
        {
            if (object()) {
                destroy();
            }
        }

    protected:
        void treeland_dde_active_v1_start_drag() override { Q_EMIT m_owner->dragStarted(); }
        void treeland_dde_active_v1_drop() override { Q_EMIT m_owner->dragEnded(); }

    private:
        DragWatcher *m_owner = nullptr;
    };

    void bind()
    {
        if (!m_manager->isActive()) {
            m_active.reset();
            return;
        }
        auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        if (m_active || !wayland || !wayland->seat()) {
            return;
        }
        m_active = std::make_unique<Active>(m_manager->get_treeland_dde_active(wayland->seat()),
                                            this);
    }

    std::unique_ptr<Manager> m_manager;
    std::unique_ptr<Active> m_active;
};

} // namespace
#endif

DragWatcher *DragWatcher::instance()
{
    static DragWatcher *self = []() -> DragWatcher * {
        const QString platform = QGuiApplication::platformName();
#ifdef GXDE_HAVE_XCB_XFIXES
        if (platform == QLatin1String("xcb") || platform == QLatin1String("dxcb")) {
            if (auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
                return new X11DragWatcher(x11->connection(), qGuiApp);
            }
        }
#endif
#ifdef GXDE_HAVE_WAYLAND_CLIENT
        if (platform.startsWith(QLatin1String("wayland"))) {
            return new TreelandDragWatcher(qGuiApp);
        }
#endif
        Q_UNUSED(platform)
        return nullptr;
    }();
    return self;
}
