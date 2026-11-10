/*
 * computer_use_wayland.c — Wayland computer-use backend.
 *
 * Split from computer_use.c (soul.md: self-contained modules). Uses grim for
 * capture, ydotool for input and wtype for typing. No file-scope state.
 */
#include "computer_use_wayland.h"
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


/* ======================================================================
 *  Wayland Backend (Linux) — uses grim + ydotool + wtype
 * ====================================================================== */

static bool wayland_is_available(void) {
    /* Wayland detected via env var */
    if (!getenv("WAYLAND_DISPLAY")) return false;
    /* Need at least grim for screenshots and ydotool or wtype for input */
    return _has_cmd("grim");
}


static bool wayland_start(void) { return true; }

static void wayland_stop(void) {}


static cu_capture_t *wayland_capture(const char *mode, const char *app) {
    (void)app;
    cu_capture_t *cap = (cu_capture_t *)calloc(1, sizeof(cu_capture_t));
    if (!cap) return NULL;
    snprintf(cap->mode, sizeof(cap->mode), "%s", mode ? mode : "som");
    cap->width = 0;
    cap->height = 0;
    cap->png_b64 = NULL;

    if (mode && strcmp(mode, "ax") == 0) {
        snprintf(cap->window_title, sizeof(cap->window_title),
                 "Wayland Desktop (no accessibility tree available)");
        return cap;
    }

    /* grim -p outputs PNG to stdout; capture -o for the whole screen */
    /* Use grim with no args = full screen, outputs PNG to stdout */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path),
             "/tmp/cu_capture_%d.png", getpid());

    /* grim captures full screen, writes to file */
    char cmd[1024];
    if (_has_cmd("slurp") && _has_cmd("jq")) {
        /* Prefer focused monitor using slurp+jq */
        snprintf(cmd, sizeof(cmd),
                 "grim -o \"$(swaymsg -t get_outputs | jq -r "
                 "'.[] | select(.focused) | .name')\" %s 2>/dev/null",
                 tmp_path);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "grim %s 2>/dev/null", tmp_path);
    }
    int ret = system(cmd);
    if (ret != 0) {
        /* Fallback: try grim without args */
        snprintf(cmd, sizeof(cmd), "grim %s 2>/dev/null", tmp_path);
        ret = system(cmd);
    }

    if (ret == 0) {
        FILE *f = fopen(tmp_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (fsize > 0 && fsize < 50L * 1024L * 1024L) {
                unsigned char *raw = (unsigned char *)malloc((size_t)fsize);
                if (raw) {
                    size_t nread = fread(raw, 1, (size_t)fsize, f);
                    (void)nread;
                    cap->png_bytes = (size_t)fsize;

                    static const char b64t[] =
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz0123456789+/";
                    size_t b64len = ((size_t)fsize + 2) / 3 * 4;
                    cap->png_b64 = (char *)malloc(b64len + 1);
                    if (cap->png_b64) {
                        size_t i, j = 0;
                        for (i = 0; i < (size_t)fsize; i += 3) {
                            unsigned int val = (unsigned)raw[i] << 16;
                            if (i + 1 < (size_t)fsize)
                                val |= (unsigned)raw[i+1] << 8;
                            if (i + 2 < (size_t)fsize)
                                val |= raw[i+2];
                            cap->png_b64[j++] = b64t[(val >> 18) & 0x3F];
                            cap->png_b64[j++] = b64t[(val >> 12) & 0x3F];
                            cap->png_b64[j++] = (i+1 < (size_t)fsize)
                                ? b64t[(val >> 6) & 0x3F] : '=';
                            cap->png_b64[j++] = (i+2 < (size_t)fsize)
                                ? b64t[val & 0x3F] : '=';
                        }
                        cap->png_b64[j] = '\0';
                    }
                    free(raw);
                }
            }
            fclose(f);
        }
        /* Get dimensions via identify (ImageMagick) or file */
        if (_has_cmd("identify")) {
            char dim_cmd[512];
            snprintf(dim_cmd, sizeof(dim_cmd),
                     "identify -format '%%w %%h' %s 2>/dev/null", tmp_path);
            FILE *fp = popen(dim_cmd, "r");
            if (fp) {
                if (fscanf(fp, "%d %d", &cap->width, &cap->height) != 2) {
                    cap->width = 1920;
                    cap->height = 1080;
                }
                pclose(fp);
            }
        } else {
            cap->width = 1920;
            cap->height = 1080;
        }
        unlink(tmp_path);
    }
    return cap;
}


static cu_action_t *wayland_make_action(const char *action_name, bool ok,
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


static cu_action_t *wayland_click(int element, int x, int y,
                                   const char *button, int click_count,
                                   const char *modifiers) {
    (void)element;
    (void)modifiers;
    if (!_has_cmd("ydotool")) {
        return wayland_make_action("click", false,
            "ydotool not available - install: sudo apt install ydotool");
    }
    int btn = 1; /* left */
    if (button) {
        if (strcmp(button, "middle") == 0) btn = 2;
        else if (strcmp(button, "right") == 0) btn = 3;
    }
    /* ydotool click: move mouse, then button down/up */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ydotool mousemove %d %d click 0x%02x 2>/dev/null",
             x, y, btn);
    if (click_count > 1 && btn == 1) {
        snprintf(cmd, sizeof(cmd),
                 "ydotool mousemove %d %d click --repeat %d 0x01 2>/dev/null",
                 x, y, click_count);
    }
    int ret = system(cmd);
    return wayland_make_action("click", ret == 0,
        ret == 0 ? "clicked at (%d,%d)" : "click failed (is ydotool running?)",
        x, y);
}


static cu_action_t *wayland_type_text(const char *text) {
    if (!_has_cmd("wtype")) {
        return wayland_make_action("type", false,
            "wtype not available - install: sudo apt install wtype");
    }
    /* wtype handles special chars via -- options, but basic text works */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "wtype '%s' 2>/dev/null", text ? text : "");
    int ret = system(cmd);
    return wayland_make_action("type", ret == 0,
        ret == 0 ? "typed %zu characters" : "type failed",
        text ? strlen(text) : 0);
}


static cu_action_t *wayland_key(const char *keys) {
    if (!_has_cmd("ydotool")) {
        return wayland_make_action("key", false,
            "ydotool not available");
    }
    /* ydotool key accepts keys as comma-separated key codes or names */
    /* Minimal: pass through the key combo string */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ydotool key '%s' 2>/dev/null", keys ? keys : "");
    int ret = system(cmd);
    return wayland_make_action("key", ret == 0,
        ret == 0 ? "sent key %s" : "key failed",
        keys ? keys : "");
}


static cu_action_t *wayland_scroll(const char *dir, int amount,
                                    int element, int x, int y,
                                    const char *modifiers) {
    (void)element; (void)modifiers;
    if (!_has_cmd("ydotool")) {
        return wayland_make_action("scroll", false,
            "ydotool not available");
    }
    if (x > 0 || y > 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "ydotool mousemove %d %d 2>/dev/null", x, y);
        int __r = system(cmd);
        (void)__r;
    }
    /* ydotool uses key codes for scroll (e.g., 0xe0 for wheel down) */
    int keycode = 0xe0;  /* wheel down */
    if (dir && (strcmp(dir, "up") == 0 || strcmp(dir, "left") == 0))
        keycode = 0xd0;  /* wheel up */
    if (amount <= 0) amount = 3;
    for (int i = 0; i < amount; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "ydotool key 0x%02x 2>/dev/null", keycode);
        int __r2 = system(cmd);
        (void)__r2;
    }
    return wayland_make_action("scroll", true,
        "scrolled %s", dir ? dir : "down");
}


static cu_action_t *wayland_list_apps(void) {
    if (_has_cmd("swaymsg")) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "swaymsg -t get_tree | jq -r "
                 "'.. | select(.app_id?) | .app_id' 2>/dev/null | "
                 "head -10 | tr '\\n' ','");
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char apps[512] = "";
            size_t n = fread(apps, 1, sizeof(apps) - 1, fp);
            apps[n] = '\0';
            pclose(fp);
            if (apps[0])
                return wayland_make_action("list_apps", true,
                    "apps: %s", apps);
        }
    }
    return wayland_make_action("list_apps", true,
        "active apps unknown (install swaymsg+jq)");
}


/* PoP: focus_app @ tools/computer_use/cua_backend.py:focus_app */
static cu_action_t *wayland_focus_app(const char *app, bool raise) {
    (void)raise;
    if (!_has_cmd("swaymsg") || !_has_cmd("jq")) {
        return wayland_make_action("focus_app", false,
            "swaymsg+jq required for app focus on Wayland");
    }
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "swaymsg '[app_id=\"%s\"]' focus 2>/dev/null",
             app ? app : "");
    int ret = system(cmd);
    return wayland_make_action("focus_app", ret == 0,
        ret == 0 ? "focused %s" : "no window found matching '%s'",
        app ? app : "", app ? app : "");
}


static cu_action_t *wayland_drag(int from_e, int to_e,
                                  int fx, int fy, int tx, int ty,
                                  const char *button, const char *modifiers) {
    (void)from_e; (void)to_e; (void)modifiers;
    if (!_has_cmd("ydotool")) {
        return wayland_make_action("drag", false,
            "ydotool not available");
    }
    int btn = 0x01;
    if (button) {
        if (strcmp(button, "middle") == 0) btn = 0x02;
        else if (strcmp(button, "right") == 0) btn = 0x04;
    }
    /* ydotool: mousemove to start, mousedown, mousemove to end, mouseup */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "ydotool mousemove %d %d mousedown 0x%02x "
             "mousemove %d %d mouseup 0x%02x 2>/dev/null",
             fx, fy, btn, tx, ty, btn);
    int ret = system(cmd);
    return wayland_make_action("drag", ret == 0,
        ret == 0 ? "dragged (%d,%d)-(%d,%d)" : "drag failed",
        fx, fy, tx, ty);
}


static cu_action_t *wayland_set_value(const char *value, int element) {
    (void)element;  /* Wayland compositor restricts per-element access */
    if (!value || !*value)
        return wayland_make_action("set_value", true, "set empty value (no-op)");
    if (!_has_cmd("ydotool"))
        return wayland_make_action("set_value", false,
            "ydotool not available - install: sudo apt install ydotool");
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "ydotool click 0x01 2>/dev/null && ydotool type '%s' 2>/dev/null",
             value);
    int ret = system(cmd);
    return wayland_make_action("set_value", ret == 0,
        ret == 0 ? "typed value (%zu chars)" : "type command failed",
        strlen(value));
}


static cu_action_t *wayland_wait(double seconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
    return wayland_make_action("wait", true, "waited %.2fs", seconds);
}


/* PoP: make_wayland_backend @ tools/computer_use/cua_backend.py:CuaDriverBackend.is_available */
cu_backend_t *make_wayland_backend(void) {
    cu_backend_t *b = (cu_backend_t *)calloc(1, sizeof(cu_backend_t));
    if (!b) return NULL;
    b->is_available = wayland_is_available;
    b->start = wayland_start;
    b->stop = wayland_stop;
    b->capture = wayland_capture;
    b->click = wayland_click;
    b->drag = wayland_drag;
    b->scroll = wayland_scroll;
    b->type_text = wayland_type_text;
    b->key = wayland_key;
    b->list_apps = wayland_list_apps;
    b->focus_app = wayland_focus_app;
    b->set_value = wayland_set_value;
    b->wait = wayland_wait;
    b->state = NULL;
    return b;
}
