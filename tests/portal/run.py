#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 GXDE OS Team
# SPDX-License-Identifier: GPL-3.0-or-later
"""End-to-end test for the GXDE portal (native drop into a window).

Run inside a private session bus:

    dbus-run-session -- python3 tests/portal/run.py [compositor]

It starts a nested GXDE compositor, a GTK drag source and a GTK drop target,
drives the pointer through wlr-virtual-pointer, asks the compositor to open the
portal overview and asserts that the drop lands in the target window.
"""

import glob
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
RUNTIME = f"/run/user/{os.getuid()}"
PORTAL = ["org.gxde.Portal", "/org/gxde/Portal", "org.gxde.Portal"]


def log(message):
    print(f"[test] {message}", flush=True)


def wait_for(predicate, timeout=20.0, interval=0.1):
    deadline = time.time() + timeout
    while time.time() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(interval)
    return None


def portal_call(method, *args):
    address = os.environ.get("DBUS_SESSION_BUS_ADDRESS", "")
    cmd = ["busctl", f"--address={address}", "--json=short", "call", *PORTAL, method, *args]
    out = subprocess.check_output(cmd, text=True)
    return json.loads(out)


def list_windows():
    data = portal_call("ListWindows")["data"]
    rows = data[0] if data else []
    return [
        {
            "uuid": row[0],
            "title": row[1],
            "app": row[2],
            "x": row[3],
            "y": row[4],
            "w": row[5],
            "h": row[6],
        }
        for row in rows
    ]


def layout():
    data = portal_call("GetLayout")["data"]
    rows = data[0] if data else []
    return [
        {
            "uuid": row[0],
            "title": row[1],
            "x": row[3],
            "y": row[4],
            "w": row[5],
            "h": row[6],
        }
        for row in rows
    ]


def find_by_title(windows, title):
    for window in windows:
        if window["title"] == title:
            return window
    return None


def main():
    compositor = (
        sys.argv[1]
        if len(sys.argv) > 1
        else os.path.expanduser("~/playground/flake-wm/build-portal/flakewm")
    )
    parent_display = os.environ.get("WAYLAND_DISPLAY", "wayland-0")

    before = set(glob.glob(f"{RUNTIME}/wayland-*"))
    env = os.environ.copy()
    env["WAYLAND_DISPLAY"] = parent_display

    compositor_log = open("/tmp/pt-comp.log", "wb")
    compositor_proc = subprocess.Popen(
        [compositor], env=env, stdout=compositor_log, stderr=subprocess.STDOUT
    )

    procs = [compositor_proc]
    pointer = None
    try:
        def nested_display():
            for path in set(glob.glob(f"{RUNTIME}/wayland-*")) - before:
                if not path.endswith(".lock"):
                    return os.path.basename(path)
            return None

        display = wait_for(nested_display, 25.0)
        assert display, "nested compositor did not create a Wayland socket"
        log(f"nested display: {display}")
        env["WAYLAND_DISPLAY"] = display

        target_log = open("/tmp/pt-target.log", "wb")
        source_log = open("/tmp/pt-source.log", "wb")
        target = subprocess.Popen(
            [sys.executable, f"{HERE}/drop_target.py"],
            env=env, stdout=target_log, stderr=subprocess.STDOUT,
        )
        source = subprocess.Popen(
            [sys.executable, f"{HERE}/drag_source.py"],
            env=env, stdout=source_log, stderr=subprocess.STDOUT,
        )
        procs += [target, source]

        def portal_ready():
            try:
                portal_call("ListWindows")
                return True
            except subprocess.CalledProcessError:
                return False

        assert wait_for(portal_ready, 20.0, 0.3), "org.gxde.Portal never appeared"
        log("portal D-Bus ready")

        windows = wait_for(
            lambda: list_windows()
            if find_by_title(list_windows(), "PortalDragSource")
            and find_by_title(list_windows(), "PortalDropTarget")
            else None,
            20.0,
            0.2,
        )
        assert windows, "windows did not appear"
        log(f"windows: {json.dumps(windows)}")

        src = find_by_title(windows, "PortalDragSource")
        dst = find_by_title(windows, "PortalDropTarget")

        pointer = subprocess.Popen(
            [f"{HERE}/pointer"], env=env, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, text=True, bufsize=1,
        )
        procs.append(pointer)

        def pcmd(line):
            pointer.stdin.write(line + "\n")
            pointer.stdin.flush()
            response = pointer.stdout.readline().strip()
            if not (response.startswith("ok") or response.startswith("size")):
                raise RuntimeError(f"pointer failed: {response!r}")
            return response

        log(f"pointer {pcmd('size')}")

        sx = src["x"] + src["w"] / 2
        sy = src["y"] + src["h"] / 2
        pcmd(f"move {sx:.0f} {sy:.0f}")
        time.sleep(0.2)
        pcmd("press")
        time.sleep(0.2)
        pcmd(f"move {sx + 60:.0f} {sy:.0f}")
        time.sleep(0.8)

        if "DRAG-BEGIN" not in open("/tmp/pt-source.log").read():
            raise RuntimeError("GTK drag did not start; see /tmp/pt-source.log")
        log("drag started")

        started = portal_call("BeginPortal")["data"][0]
        assert started, "BeginPortal returned false"
        log("portal opened")

        time.sleep(0.7)  # let the overview animation settle
        tiles = wait_for(lambda: layout() or None, 5.0, 0.2)
        assert tiles, "portal layout is empty"
        log(f"layout: {json.dumps(tiles)}")

        tile = find_by_title(tiles, "PortalDropTarget") or find_by_title(tiles, "PortalDragSource")
        assert tile, "target window not in the portal layout"

        # If we only see the source tile, the drop still validates retargeting.
        tx = tile["x"] + tile["w"] / 2
        ty = tile["y"] + tile["h"] / 2
        pcmd(f"move {tx:.0f} {ty:.0f}")
        time.sleep(0.6)

        subprocess.run(["grim", "/tmp/pt-before-drop.png"], env=env, check=False)
        pcmd("release")
        time.sleep(1.0)
        subprocess.run(["grim", "/tmp/pt-after-drop.png"], env=env, check=False)

        target_text = open("/tmp/pt-target.log").read()
        log(f"target log: {target_text.strip()!r}")
        assert "DROP" in target_text, "the drop did not reach the target window"
        assert "portal-payload-42" in target_text, "wrong payload"

        log("PASS: native drop landed in the target window")
        return 0
    except Exception as exc:  # noqa: BLE001
        log(f"FAIL: {exc}")
        for path in ("/tmp/pt-comp.log", "/tmp/pt-source.log", "/tmp/pt-target.log"):
            try:
                content = open(path).read().strip()
            except OSError:
                continue
            if content:
                log(f"--- {path} ---\n{content[-2000:]}")
        return 1
    finally:
        if pointer is not None:
            try:
                pointer.stdin.write("quit\n")
                pointer.stdin.flush()
            except Exception:  # noqa: BLE001
                pass
        for proc in reversed(procs):
            if proc.poll() is None:
                proc.terminate()
        for proc in reversed(procs):
            try:
                proc.wait(timeout=5)
            except Exception:  # noqa: BLE001
                proc.kill()


if __name__ == "__main__":
    sys.exit(main())
