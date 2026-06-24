/*
 * window_wayland.c — Wayland backend for the cross-platform window API.
 * No X11. Linux targets Wayland only.
 * Build: gcc -c window_wayland.c $(pkg-config --cflags wayland-client wayland-egl xkbcommon gbm egl glesv2) -I include
 */
#include "window.h"
#include <wayland-client.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include "xdg-shell-client-protocol.h"

/* ── Window struct (must be defined before any function that dereferences it) ── */
struct window {
    int width, height;
    bool should_close, focused;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_surface *surface;
    struct xdg_wm_base *xdg_wm_base;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    struct wl_shm *shm;
    struct wl_output *outputs[8];
    int output_count;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    EGLConfig egl_config;
    struct gbm_device *gbm_device;
    int drm_fd;
    struct xkb_context *xkb_context;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;
    window_event_t event_queue[64];
    int event_head, event_tail;
    char title[WINDOW_MAX_TITLE];
    window_cursor_t cursor;
    float scale;
};

/* ── Listeners ─────────────────────────────────────────────────────────── */
static void xdg_wm_base_handle_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    (void)data; xdg_wm_base_pong(xdg_wm_base, serial);
}
static const struct xdg_wm_base_listener xdg_wm_base_listener = { .ping = xdg_wm_base_handle_ping };

static void xdg_surface_handle_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    (void)data; xdg_surface_ack_configure(xdg_surface, serial);
}
static const struct xdg_surface_listener xdg_surface_listener = { .configure = xdg_surface_handle_configure };

static void xdg_toplevel_handle_configure(void *data, struct xdg_toplevel *toplevel,
        int32_t width, int32_t height, struct wl_array *states) {
    (void)toplevel; (void)states;
    struct window *w = (struct window *)data;
    if (w) { if (width > 0) w->width = width; if (height > 0) w->height = height; }
}
static void xdg_toplevel_handle_close(void *data, struct xdg_toplevel *toplevel) {
    (void)toplevel;
    struct window *w = (struct window *)data;
    if (w) w->should_close = true;
}
static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_handle_configure, .close = xdg_toplevel_handle_close
};

static void seat_handle_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    struct window *w = (struct window *)data;
    if (!w) return;
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !w->pointer) w->pointer = wl_seat_get_pointer(seat);
    if (!(caps & WL_SEAT_CAPABILITY_POINTER) && w->pointer) { wl_pointer_release(w->pointer); w->pointer = NULL; }
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !w->keyboard) w->keyboard = wl_seat_get_keyboard(seat);
    if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && w->keyboard) { wl_keyboard_release(w->keyboard); w->keyboard = NULL; }
}
static void seat_handle_name(void *data, struct wl_seat *seat, const char *name) { (void)data; (void)seat; (void)name; }
static const struct wl_seat_listener seat_listener = { .capabilities = seat_handle_capabilities, .name = seat_handle_name };

static void registry_handle_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    struct window *w = (struct window *)data;
    if (!w) return;
    if (!strcmp(interface, wl_compositor_interface.name))
        w->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    else if (!strcmp(interface, xdg_wm_base_interface.name)) {
        w->xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(w->xdg_wm_base, &xdg_wm_base_listener, w);
    } else if (!strcmp(interface, wl_seat_interface.name)) {
        w->seat = wl_registry_bind(registry, name, &wl_seat_interface, 7);
        wl_seat_add_listener(w->seat, &seat_listener, w);
    } else if (!strcmp(interface, wl_shm_interface.name))
        w->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    else if (!strcmp(interface, wl_output_interface.name)) {
        if (w->output_count < 8)
            w->outputs[w->output_count++] = wl_registry_bind(registry, name, &wl_output_interface, 2);
    }
}
static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) { (void)data; (void)registry; (void)name; }
static const struct wl_registry_listener registry_listener = { .global = registry_handle_global, .global_remove = registry_handle_global_remove };

/* ── Platform info ─────────────────────────────────────────────────────── */
const char *window_platform_name(void) { return "wayland"; }
bool window_platform_has_gpu(void) { return true; }

/* ── Lifecycle ─────────────────────────────────────────────────────────── */
window_t *window_create(const window_config_t *config) {
    struct window *w = calloc(1, sizeof(struct window));
    if (!w) return NULL;
    w->width = config && config->width > 0 ? config->width : 1280;
    w->height = config && config->height > 0 ? config->height : 900;
    w->scale = 1.0f; w->should_close = false; w->focused = true;
    w->event_head = 0; w->event_tail = 0; w->output_count = 0;
    if (config && config->title) strncpy(w->title, config->title, WINDOW_MAX_TITLE - 1);
    else strncpy(w->title, "Slermes", WINDOW_MAX_TITLE - 1);

    w->display = wl_display_connect(NULL);
    if (!w->display) { fprintf(stderr, "[wayland] No display\n"); free(w); return NULL; }
    w->registry = wl_display_get_registry(w->display);
    wl_registry_add_listener(w->registry, &registry_listener, w);
    wl_display_roundtrip(w->display);
    if (!w->compositor) { fprintf(stderr, "[wayland] No compositor\n"); window_destroy(w); return NULL; }
    if (!w->xdg_wm_base) { fprintf(stderr, "[wayland] No xdg_wm_base\n"); window_destroy(w); return NULL; }

    w->surface = wl_compositor_create_surface(w->compositor);
    w->xdg_surface = xdg_wm_base_get_xdg_surface(w->xdg_wm_base, w->surface);
    xdg_surface_add_listener(w->xdg_surface, &xdg_surface_listener, w);
    w->xdg_toplevel = xdg_surface_get_toplevel(w->xdg_surface);
    xdg_toplevel_add_listener(w->xdg_toplevel, &xdg_toplevel_listener, w);
    xdg_toplevel_set_title(w->xdg_toplevel, w->title);
    xdg_toplevel_set_app_id(w->xdg_toplevel, "com.nousresearch.hermes");
    if (config) {
        if (config->min_width > 0 && config->min_height > 0)
            xdg_toplevel_set_min_size(w->xdg_toplevel, config->min_width, config->min_height);
        if (config->fullscreen) xdg_toplevel_set_fullscreen(w->xdg_toplevel, NULL);
    }
    wl_surface_commit(w->surface);
    wl_display_roundtrip(w->display);
    w->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    fprintf(stderr, "[wayland] Window %dx%d\n", w->width, w->height);
    return w;
}

void window_destroy(window_t *w) {
    if (!w) return;
    if (w->xkb_state) xkb_state_unref(w->xkb_state);
    if (w->xkb_keymap) xkb_keymap_unref(w->xkb_keymap);
    if (w->xkb_context) xkb_context_unref(w->xkb_context);
    if (w->xdg_toplevel) xdg_toplevel_destroy(w->xdg_toplevel);
    if (w->xdg_surface) xdg_surface_destroy(w->xdg_surface);
    if (w->surface) wl_surface_destroy(w->surface);
    for (int i = 0; i < w->output_count; i++) wl_output_destroy(w->outputs[i]);
    if (w->pointer) wl_pointer_release(w->pointer);
    if (w->keyboard) wl_keyboard_release(w->keyboard);
    if (w->seat) wl_seat_release(w->seat);
    if (w->shm) wl_shm_destroy(w->shm);
    if (w->xdg_wm_base) xdg_wm_base_destroy(w->xdg_wm_base);
    if (w->compositor) wl_compositor_destroy(w->compositor);
    if (w->registry) wl_registry_destroy(w->registry);
    if (w->display) wl_display_disconnect(w->display);
    free(w);
}

void window_set_title(window_t *w, const char *title) {
    if (!w || !title) return;
    strncpy(w->title, title, WINDOW_MAX_TITLE - 1);
    if (w->xdg_toplevel) xdg_toplevel_set_title(w->xdg_toplevel, w->title);
}
void window_show(window_t *w) { if (w && w->surface) { wl_surface_commit(w->surface); wl_display_flush(w->display); } }
void window_hide(window_t *w) { if (w && w->surface) { wl_surface_attach(w->surface, NULL, 0, 0); wl_surface_commit(w->surface); } }
void window_focus(window_t *w) { (void)w; }
void window_request_close(window_t *w) { if (w) w->should_close = true; }

/* ── Size / position ───────────────────────────────────────────────────── */
void window_get_size(window_t *w, int *width, int *height) { if (!w) return; if (width) *width = w->width; if (height) *height = w->height; }
void window_set_size(window_t *w, int width, int height) { if (w) { w->width = width; w->height = height; } }
void window_get_position(window_t *w, int *x, int *y) { (void)w; if (x) *x = 0; if (y) *y = 0; }
void window_set_position(window_t *w, int x, int y) { (void)w; (void)x; (void)y; }
float window_get_scale(window_t *w) { return w ? w->scale : 1.0f; }
void window_set_fullscreen(window_t *w, bool fs) { if (!w || !w->xdg_toplevel) return; if (fs) xdg_toplevel_set_fullscreen(w->xdg_toplevel, NULL); else xdg_toplevel_unset_fullscreen(w->xdg_toplevel); }
bool window_is_fullscreen(window_t *w) { (void)w; return false; }
void window_set_min_size(window_t *w, int min_w, int min_h) { if (w && w->xdg_toplevel) xdg_toplevel_set_min_size(w->xdg_toplevel, min_w, min_h); }

/* ── Events ────────────────────────────────────────────────────────────── */
static void push_event(window_t *w, const window_event_t *ev) {
    if (!w) return; int next = (w->event_tail + 1) % 64;
    if (next == w->event_head) return;
    w->event_queue[w->event_tail] = *ev; w->event_tail = next;
}
static bool pop_event(window_t *w, window_event_t *ev) {
    if (!w || w->event_head == w->event_tail) return false;
    *ev = w->event_queue[w->event_head]; w->event_head = (w->event_head + 1) % 64; return true;
}
static void dispatch_events(window_t *w) {
    if (!w || !w->display) return;
    while (wl_display_prepare_read(w->display) != 0) wl_display_dispatch_pending(w->display);
    wl_display_flush(w->display);
    struct pollfd pfd = { .fd = wl_display_get_fd(w->display), .events = POLLIN };
    if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) { wl_display_read_events(w->display); wl_display_dispatch_pending(w->display); }
    else wl_display_cancel_read(w->display);
    if (w->should_close) { window_event_t ev = { .type = EVENT_CLOSE }; push_event(w, &ev); }
}
bool window_poll_event(window_t *w, window_event_t *ev) { if (!w || !ev) return false; dispatch_events(w); return pop_event(w, ev); }
bool window_wait_event(window_t *w, window_event_t *ev) {
    if (!w || !ev) return false;
    while (!w->should_close) {
        dispatch_events(w); if (pop_event(w, ev)) return true;
        struct pollfd pfd = { .fd = wl_display_get_fd(w->display), .events = POLLIN }; poll(&pfd, 1, -1);
    }
    ev->type = EVENT_CLOSE; return true;
}
bool window_wait_event_timeout(window_t *w, window_event_t *ev, int timeout_ms) {
    if (!w || !ev) return false;
    dispatch_events(w); if (pop_event(w, ev)) return true;
    struct pollfd pfd = { .fd = wl_display_get_fd(w->display), .events = POLLIN };
    if (poll(&pfd, 1, timeout_ms) > 0) { dispatch_events(w); return pop_event(w, ev); }
    return false;
}

/* ── Rendering stubs ───────────────────────────────────────────────────── */
window_renderer_t *window_get_renderer(window_t *w) { (void)w; return NULL; }
void window_swap_buffers(window_t *w) { if (w && w->surface) { wl_surface_commit(w->surface); wl_display_flush(w->display); } }
void window_render_begin(window_t *w, float r, float g, float b, float a) { (void)w; (void)r; (void)g; (void)b; (void)a; }
void window_render_end(window_t *w) { (void)w; }
void window_clear(window_t *w, float r, float g, float b, float a) { (void)w; (void)r; (void)g; (void)b; (void)a; }
void window_draw_rect(window_t *w, float x, float y, float width, float height, float r, float g, float b, float a) { (void)w; (void)x; (void)y; (void)width; (void)height; (void)r; (void)g; (void)b; (void)a; }
void window_fill_rect(window_t *w, float x, float y, float width, float height, float r, float g, float b, float a) { (void)w; (void)x; (void)y; (void)width; (void)height; (void)r; (void)g; (void)b; (void)a; }
void window_fill_rect_rounded(window_t *w, float x, float y, float width, float height, float radius, float r, float g, float b, float a) { (void)w; (void)x; (void)y; (void)width; (void)height; (void)radius; (void)r; (void)g; (void)b; (void)a; }
void window_fill_circle(window_t *w, float cx, float cy, float radius, float r, float g, float b, float a) { (void)w; (void)cx; (void)cy; (void)radius; (void)r; (void)g; (void)b; (void)a; }
void window_draw_text(window_t *w, const char *text, float x, float y, float size, float r, float g, float b, float a) { (void)w; (void)text; (void)x; (void)y; (void)size; (void)r; (void)g; (void)b; (void)a; }
float window_text_width(window_t *w, const char *text, float size) { (void)w; (void)text; (void)size; return 0.0f; }
void window_draw_line(window_t *w, float x0, float y0, float x1, float y1, float lw, float r, float g, float b, float a) { (void)w; (void)x0; (void)y0; (void)x1; (void)y1; (void)lw; (void)r; (void)g; (void)b; (void)a; }
void window_draw_image(window_t *w, const uint8_t *pixels, int img_w, int img_h, float x, float y, float width, float height) { (void)w; (void)pixels; (void)img_w; (void)img_h; (void)x; (void)y; (void)width; (void)height; }
void window_set_cursor(window_t *w, window_cursor_t cursor) { if (w) w->cursor = cursor; }
void window_get_mouse_pos(window_t *w, int *x, int *y) { (void)w; if (x) *x = 0; if (y) *y = 0; }
bool window_get_mouse_button(window_t *w, mouse_button_t button) { (void)w; (void)button; return false; }
void window_set_mouse_pos(window_t *w, int x, int y) { (void)w; (void)x; (void)y; }
const char *window_clipboard_get(window_t *w) { (void)w; return NULL; }
void window_clipboard_set(window_t *w, const char *text) { (void)w; (void)text; }
void window_enable_dnd(window_t *w, bool enable) { (void)w; (void)enable; }
