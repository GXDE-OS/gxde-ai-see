#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 GXDE OS Team
# SPDX-License-Identifier: GPL-3.0-or-later
"""GTK3 drop target used by the portal integration test.

GtkTextView accepts text drops out of the box; every insertion is printed as
``DROP <text> at <offset>`` so the orchestrator can assert that the *native*
drop landed in this window.
"""

import gi

gi.require_version("Gtk", "3.0")

from gi.repository import Gtk  # noqa: E402


class Target(Gtk.Window):
    def __init__(self):
        super().__init__(title="PortalDropTarget")
        self.set_default_size(420, 280)
        self.set_position(Gtk.WindowPosition.CENTER)

        view = Gtk.TextView()
        view.set_wrap_mode(Gtk.WrapMode.WORD)
        self.buffer = view.get_buffer()
        self.buffer.connect("insert-text", self.on_insert)

        scroller = Gtk.ScrolledWindow()
        scroller.add(view)
        self.add(scroller)

        self.connect("destroy", Gtk.main_quit)
        self.show_all()

    def on_insert(self, buffer, location, text, length):
        print("DROP", repr(text), "at", location.get_offset(), flush=True)


Target()
Gtk.main()
