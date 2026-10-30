// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Minimal wlr-virtual-pointer driver used by the portal integration test.
// Reads commands from stdin, one per line:
//   move <x> <y>   absolute motion in output coordinates
//   press          left button down
//   release        left button up
//   sleep <ms>
//   size           print "size <w> <h>"
//   quit
// After each command it prints "ok <line>" so the orchestrator can sync.

#define _POSIX_C_SOURCE 200809L

#include <linux/input-event-codes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <wayland-client.h>

#include "virtual-pointer-client.h"

struct state {
    struct wl_display *display;
    struct wl_seat *seat;
    struct zwlr_virtual_pointer_manager_v1 *manager;
    struct zwlr_virtual_pointer_v1 *pointer;
    uint32_t width;
    uint32_t height;
};

static uint32_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y,
                            int32_t physical_w, int32_t physical_h, int32_t subpixel,
                            const char *make, const char *model, int32_t transform)
{
    (void)data;
    (void)output;
    (void)x;
    (void)y;
    (void)physical_w;
    (void)physical_h;
    (void)subpixel;
    (void)make;
    (void)model;
    (void)transform;
}

static void output_mode(void *data, struct wl_output *output, uint32_t flags, int32_t width,
                        int32_t height, int32_t refresh)
{
    (void)output;
    (void)flags;
    (void)refresh;
    struct state *state = data;
    if (width > 0 && height > 0) {
        state->width = (uint32_t)width;
        state->height = (uint32_t)height;
    }
}

static void output_done(void *data, struct wl_output *output)
{
    (void)data;
    (void)output;
}

static void output_scale(void *data, struct wl_output *output, int32_t factor)
{
    (void)data;
    (void)output;
    (void)factor;
}

static void output_name(void *data, struct wl_output *output, const char *name)
{
    (void)data;
    (void)output;
    (void)name;
}

static void output_description(void *data, struct wl_output *output, const char *description)
{
    (void)data;
    (void)output;
    (void)description;
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description,
};

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
    (void)data;
    (void)seat;
    (void)caps;
}

static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
    (void)data;
    (void)seat;
    (void)name;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

static void registry_global(void *data, struct wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t version)
{
    struct state *state = data;

    if (strcmp(interface, wl_output_interface.name) == 0) {
        struct wl_output *output =
            wl_registry_bind(registry, name, &wl_output_interface, version < 2 ? version : 2);
        wl_output_add_listener(output, &output_listener, state);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        state->seat = wl_registry_bind(registry, name, &wl_seat_interface, version < 5 ? version : 5);
        wl_seat_add_listener(state->seat, &seat_listener, state);
    } else if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
        state->manager = wl_registry_bind(registry, name,
                                          &zwlr_virtual_pointer_manager_v1_interface,
                                          version < 2 ? version : 2);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static void send_motion(struct state *state, double x, double y)
{
    zwlr_virtual_pointer_v1_motion_absolute(state->pointer, now_ms(), wl_fixed_from_double(x),
                                            wl_fixed_from_double(y), state->width,
                                            state->height);
    zwlr_virtual_pointer_v1_frame(state->pointer);
    wl_display_roundtrip(state->display);
}

static void send_button(struct state *state, uint32_t button, uint32_t button_state)
{
    zwlr_virtual_pointer_v1_button(state->pointer, now_ms(), button, button_state);
    zwlr_virtual_pointer_v1_frame(state->pointer);
    wl_display_roundtrip(state->display);
}

int main(void)
{
    struct state state = { 0 };
    state.display = wl_display_connect(NULL);
    if (!state.display) {
        fprintf(stderr, "cannot connect to wayland display\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(registry, &registry_listener, &state);
    wl_display_roundtrip(state.display);
    wl_display_roundtrip(state.display);

    if (!state.seat || !state.manager) {
        fprintf(stderr, "missing seat or virtual pointer manager\n");
        return 1;
    }
    if (state.width == 0 || state.height == 0) {
        state.width = 1280;
        state.height = 720;
    }

    state.pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(state.manager, state.seat);
    if (!state.pointer) {
        fprintf(stderr, "cannot create virtual pointer\n");
        return 1;
    }
    wl_display_roundtrip(state.display);

    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        double x = 0.0;
        double y = 0.0;
        int ms = 0;

        if (sscanf(line, "move %lf %lf", &x, &y) == 2) {
            send_motion(&state, x, y);
        } else if (strncmp(line, "press", 5) == 0) {
            send_button(&state, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
        } else if (strncmp(line, "release", 7) == 0) {
            send_button(&state, BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
        } else if (sscanf(line, "sleep %d", &ms) == 1) {
            struct timespec delay = { ms / 1000, (long)(ms % 1000) * 1000000L };
            nanosleep(&delay, NULL);
        } else if (strncmp(line, "size", 4) == 0) {
            printf("size %u %u\n", state.width, state.height);
        } else if (strncmp(line, "quit", 4) == 0) {
            break;
        }

        printf("ok %s", line);
        fflush(stdout);
    }

    zwlr_virtual_pointer_v1_destroy(state.pointer);
    zwlr_virtual_pointer_manager_v1_destroy(state.manager);
    wl_display_disconnect(state.display);
    return 0;
}
