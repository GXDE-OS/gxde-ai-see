// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "x11taskwindows.h"

#include <cstdlib>
#include <cstring>

namespace {

constexpr uint32_t kIconicState = 3;
constexpr uint32_t kSourcePager = 2;
constexpr uint32_t kAllDesktops = 0xFFFFFFFF;
constexpr int kWantedIconSize = 64;

QString windowId(xcb_window_t window)
{
    return QString::number(window);
}

xcb_window_t toWindow(const QString &id)
{
    return id.toUInt();
}

} // namespace

X11TaskWindows::X11TaskWindows(xcb_connection_t *connection, QObject *parent)
    : TaskWindows(parent)
    , m_connection(connection)
{
    if (m_connection) {
        const xcb_setup_t *setup = xcb_get_setup(m_connection);
        xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
        if (it.rem > 0) {
            m_root = it.data->root;
        }
    }
}

bool X11TaskWindows::available() const
{
    return m_connection && m_root && !windowList(atom("_NET_CLIENT_LIST")).isEmpty();
}

xcb_atom_t X11TaskWindows::atom(const char *name) const
{
    const QByteArray key(name);
    auto it = m_atoms.constFind(key);
    if (it != m_atoms.constEnd()) {
        return it.value();
    }
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(
        m_connection,
        xcb_intern_atom(m_connection, false, static_cast<uint16_t>(key.size()), name), nullptr);
    const xcb_atom_t value = reply ? reply->atom : XCB_ATOM_NONE;
    free(reply);
    m_atoms.insert(key, value);
    return value;
}

QByteArray X11TaskWindows::property(xcb_window_t window, xcb_atom_t property, xcb_atom_t type,
                                    uint32_t maxLength) const
{
    xcb_get_property_reply_t *reply = xcb_get_property_reply(
        m_connection, xcb_get_property(m_connection, false, window, property, type, 0, maxLength),
        nullptr);
    if (!reply) {
        return {};
    }
    QByteArray data;
    if (reply->type != XCB_ATOM_NONE) {
        data = QByteArray(static_cast<const char *>(xcb_get_property_value(reply)),
                          xcb_get_property_value_length(reply));
    }
    free(reply);
    return data;
}

QList<xcb_window_t> X11TaskWindows::windowList(xcb_atom_t prop) const
{
    const QByteArray data = property(m_root, prop, XCB_ATOM_WINDOW, 16384);
    QList<xcb_window_t> list;
    const auto *ids = reinterpret_cast<const xcb_window_t *>(data.constData());
    for (qsizetype i = 0; i < data.size() / qsizetype(sizeof(xcb_window_t)); ++i) {
        list.append(ids[i]);
    }
    return list;
}

QList<xcb_atom_t> X11TaskWindows::atomList(xcb_window_t window, xcb_atom_t prop) const
{
    const QByteArray data = property(window, prop, XCB_ATOM_ATOM);
    QList<xcb_atom_t> list;
    const auto *atoms = reinterpret_cast<const xcb_atom_t *>(data.constData());
    for (qsizetype i = 0; i < data.size() / qsizetype(sizeof(xcb_atom_t)); ++i) {
        list.append(atoms[i]);
    }
    return list;
}

bool X11TaskWindows::listable(xcb_window_t window) const
{
    /* the same rules a taskbar uses: plain application windows only */
    const QList<xcb_atom_t> types = atomList(window, atom("_NET_WM_WINDOW_TYPE"));
    if (!types.isEmpty() && !types.contains(atom("_NET_WM_WINDOW_TYPE_NORMAL"))) {
        return false;
    }
    if (atomList(window, atom("_NET_WM_STATE")).contains(atom("_NET_WM_STATE_SKIP_TASKBAR"))) {
        return false;
    }
    if (!property(window, XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW).isEmpty()) {
        return false;
    }

    const QByteArray current = property(m_root, atom("_NET_CURRENT_DESKTOP"), XCB_ATOM_CARDINAL);
    const QByteArray desktop = property(window, atom("_NET_WM_DESKTOP"), XCB_ATOM_CARDINAL);
    if (current.size() >= 4 && desktop.size() >= 4) {
        uint32_t currentDesktop = 0;
        uint32_t windowDesktop = 0;
        std::memcpy(&currentDesktop, current.constData(), 4);
        std::memcpy(&windowDesktop, desktop.constData(), 4);
        if (windowDesktop != kAllDesktops && windowDesktop != currentDesktop) {
            return false;
        }
    }
    return true;
}

QString X11TaskWindows::title(xcb_window_t window) const
{
    const QByteArray name = property(window, atom("_NET_WM_NAME"), atom("UTF8_STRING"));
    if (!name.isEmpty()) {
        return QString::fromUtf8(name);
    }
    return QString::fromLocal8Bit(property(window, XCB_ATOM_WM_NAME, XCB_GET_PROPERTY_TYPE_ANY));
}

QString X11TaskWindows::appId(xcb_window_t window) const
{
    /* WM_CLASS is "instance\0class\0" */
    const QList<QByteArray> parts =
        property(window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING).split('\0');
    if (parts.size() >= 2 && !parts.at(1).isEmpty()) {
        return QString::fromLocal8Bit(parts.at(1));
    }
    return parts.isEmpty() ? QString() : QString::fromLocal8Bit(parts.first());
}

QImage X11TaskWindows::icon(xcb_window_t window) const
{
    /* _NET_WM_ICON: repeated { width, height, ARGB32[width * height] } */
    const QByteArray data =
        property(window, atom("_NET_WM_ICON"), XCB_ATOM_CARDINAL, 4 * 1024 * 1024);
    const auto *values = reinterpret_cast<const uint32_t *>(data.constData());
    const qsizetype count = data.size() / qsizetype(sizeof(uint32_t));

    qsizetype best = -1;
    uint32_t bestSize = 0;
    for (qsizetype i = 0; i + 2 <= count;) {
        const uint32_t width = values[i];
        const uint32_t height = values[i + 1];
        const qsizetype pixels = qsizetype(width) * height;
        if (width == 0 || height == 0 || i + 2 + pixels > count) {
            break;
        }
        /* smallest icon that is at least the wanted size, else the biggest */
        const bool better = bestSize == 0 ||
                            (bestSize < kWantedIconSize ? width > bestSize
                                                        : width >= kWantedIconSize && width < bestSize);
        if (better) {
            best = i;
            bestSize = width;
        }
        i += 2 + pixels;
    }
    if (best < 0) {
        return {};
    }

    const int width = int(values[best]);
    const int height = int(values[best + 1]);
    QImage image(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) {
        auto *line = reinterpret_cast<uint32_t *>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            line[x] = values[best + 2 + qsizetype(y) * width + x];
        }
    }
    return image;
}

QList<TaskWindow> X11TaskWindows::windows() const
{
    QList<TaskWindow> result;
    if (!m_connection) {
        return result;
    }

    QList<xcb_window_t> stacking = windowList(atom("_NET_CLIENT_LIST_STACKING"));
    if (stacking.isEmpty()) {
        stacking = windowList(atom("_NET_CLIENT_LIST"));
    }
    const QList<xcb_window_t> active = windowList(atom("_NET_ACTIVE_WINDOW"));
    const xcb_atom_t hidden = atom("_NET_WM_STATE_HIDDEN");

    for (xcb_window_t window : stacking) {
        if (!listable(window)) {
            continue;
        }
        TaskWindow task;
        task.id = windowId(window);
        task.title = title(window);
        task.appId = appId(window);
        task.icon = icon(window);
        task.minimized = atomList(window, atom("_NET_WM_STATE")).contains(hidden);
        task.active = !active.isEmpty() && active.first() == window;
        result.append(task);
    }
    return result;
}

void X11TaskWindows::sendRootMessage(xcb_window_t window, xcb_atom_t type, uint32_t d0,
                                     uint32_t d1, uint32_t d2)
{
    xcb_client_message_event_t event;
    std::memset(&event, 0, sizeof(event));
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = window;
    event.type = type;
    event.data.data32[0] = d0;
    event.data.data32[1] = d1;
    event.data.data32[2] = d2;
    xcb_send_event(m_connection, false, m_root,
                   XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                   reinterpret_cast<const char *>(&event));
    xcb_flush(m_connection);
}

void X11TaskWindows::minimize(const QString &id)
{
    if (m_connection) {
        sendRootMessage(toWindow(id), atom("WM_CHANGE_STATE"), kIconicState);
    }
}

void X11TaskWindows::unminimize(const QString &id)
{
    /* EWMH has no plain "deiconify" for pagers; activating is the portable way */
    activate(id);
}

void X11TaskWindows::activate(const QString &id)
{
    if (m_connection) {
        sendRootMessage(toWindow(id), atom("_NET_ACTIVE_WINDOW"), kSourcePager,
                        XCB_CURRENT_TIME);
    }
}
