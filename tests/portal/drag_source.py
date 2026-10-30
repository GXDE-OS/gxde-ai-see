#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 GXDE OS Team
# SPDX-License-Identifier: GPL-3.0-or-later
"""GTK3 drag source used by the portal integration test."""

import gi

gi.require_version("Gtk", "3.0")

from gi.repository import Gdk, Gtk  # noqa: E402


class Source(Gtk.Window):
    def __init__(self):
        super().__init__(title="PortalDragSource")
        self.set_default_size(420, 280)
        self.set_position(Gtk.WindowPosition.CENTER)

        label = Gtk.Label()
        label.set_markup("<span size='x-large'>DRAG ME</span>\n\nportal test payload")
        label.set_hexpand(True)
        label.set_vexpand(True)

        box = Gtk.EventBox()
        box.add(label)
        self.add(box)
        box.add_events(
            Gdk.EventMask.BUTTON_PRESS_MASK
            | Gdk.EventMask.BUTTON_RELEASE_MASK
            | Gdk.EventMask.POINTER_MOTION_MASK
        )
        box.connect("button-press-event", lambda *args: print("PRESS", flush=True))
        box.connect("button-release-event", lambda *args: print("RELEASE", flush=True))
        box.connect("motion-notify-event", lambda *args: print("MOTION", flush=True))

        box.drag_source_set(Gdk.ModifierType.BUTTON1_MASK, [], Gdk.DragAction.COPY)
        box.drag_source_add_text_targets()
        box.connect("drag-data-get", self.on_drag_data_get)
        box.connect("drag-begin", lambda *args: print("DRAG-BEGIN", flush=True))
        box.connect("drag-end", lambda *args: print("DRAG-END", flush=True))

        self.connect("destroy", Gtk.main_quit)
        self.show_all()

    def on_drag_data_get(self, widget, context, data, info, time):
        data.set_text("portal-payload-42", -1)


Source()
Gtk.main()
