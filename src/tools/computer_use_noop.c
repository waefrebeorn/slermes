/*
 * computer_use_noop.c - Noop computer-use backend.
 *
 * Split from computer_use.c (soul.md: self-contained modules). Returns a
 * fixed 1024x768 capture and echoes actions; used for tests, CI and as the
 * fallback when no platform backend is available. No file-scope state.
 */
#include "computer_use_noop.h"
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <windows.h>


static bool noop_is_available(void) { return true; }

static bool noop_start(void) { return true; }

static void noop_stop(void) {}


static cu_capture_t *noop_capture(const char *mode, const char *app) {
    (void)mode;
    cu_capture_t *cap = (cu_capture_t *)calloc(1, sizeof(cu_capture_t));
    if (!cap) return NULL;
    snprintf(cap->mode, sizeof(cap->mode), "%s", mode ? mode : "som");
    cap->width = 1024;
    cap->height = 768;
    if (app) snprintf(cap->app, sizeof(cap->app), "%s", app);
    snprintf(cap->window_title, sizeof(cap->window_title), "Noop Desktop");
    cap->elements = NULL;
    cap->element_count = 0;
    cap->png_b64 = NULL;
    return cap;
}


static cu_action_t *noop_make_action(const char *action_name, bool ok,
                                      const char *fmt, ...) {
    cu_action_t *act = (cu_action_t *)calloc(1, sizeof(cu_action_t));
    if (!act) return NULL;
    snprintf(act->action, sizeof(act->action), "%s", action_name);
    act->ok = ok;
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(act->message, sizeof(act->message), fmt, ap);
        va_end(ap);
    }
    return act;
}


static cu_action_t *noop_click(int element, int x, int y,
                                const char *button, int click_count,
                                const char *modifiers) {
    (void)modifiers;
    if (element > 0)
        return noop_make_action("click", true, "clicked element #%d", element);
    return noop_make_action("click", true, "clicked at (%d,%d) [%s] x%d",
                             x, y, button ? button : "left", click_count);
}


static cu_action_t *noop_drag(int from_e, int to_e,
                               int fx, int fy, int tx, int ty,
                               const char *button, const char *modifiers) {
    (void)button; (void)modifiers;
    if (from_e > 0 && to_e > 0)
        return noop_make_action("drag", true, "dragged element #%d to #%d",
                                 from_e, to_e);
    return noop_make_action("drag", true, "dragged (%d,%d) - (%d,%d)",
                             fx, fy, tx, ty);
}


static cu_action_t *noop_scroll(const char *dir, int amount,
                                 int element, int x, int y,
                                 const char *modifiers) {
    (void)element; (void)x; (void)y; (void)modifiers;
    return noop_make_action("scroll", true, "scrolled %s x%d",
                             dir ? dir : "down", amount > 0 ? amount : 3);
}


/* PoP: type_text @ tools/computer_use/cua_backend.py:type_text */
static cu_action_t *noop_type_text(const char *text) {
    return noop_make_action("type", true, "typed %zu chars",
                             text ? strlen(text) : 0);
}


static cu_action_t *noop_key(const char *keys) {
    return noop_make_action("key", true, "sent key %s", keys ? keys : "?");
}


static cu_action_t *noop_list_apps(void) {
    return noop_make_action("list_apps", true, "3 apps running");
}


static cu_action_t *noop_focus_app(const char *app, bool raise) {
    return noop_make_action("focus_app", true, "focused %s%s",
                             app ? app : "(frontmost)",
                             raise ? " (raised)" : "");
}


static cu_action_t *noop_set_value(const char *value, int element) {
    return noop_make_action("set_value", true, "set element #%d to %s",
                             element, value ? value : "");
}


static cu_action_t *noop_wait(double seconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
    return noop_make_action("wait", true, "waited %.2fs", seconds);
}


/* PoP: make_noop_backend @ tools/computer_use/cua_backend.py:CuaDriverBackend.is_available */ cu_backend_t *make_noop_backend(void) {
    cu_backend_t *b = (cu_backend_t *)calloc(1, sizeof(cu_backend_t));
    if (!b) return NULL;
    b->is_available = noop_is_available;
    b->start = noop_start;
    b->stop = noop_stop;
    b->capture = noop_capture;
    b->click = noop_click;
    b->drag = noop_drag;
    b->scroll = noop_scroll;
    b->type_text = noop_type_text;
    b->key = noop_key;
    b->list_apps = noop_list_apps;
    b->focus_app = noop_focus_app;
    b->set_value = noop_set_value;
    b->wait = noop_wait;
    b->state = calloc(1, sizeof(noop_state_t));
    return b;
}
