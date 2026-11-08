/*
 * computer_use_x11.c — X11 computer-use backend.
 *
 * Split from computer_use.c (soul.md: self-contained modules). Uses xdotool
 * for input and ImageMagick `import` for capture. No file-scope state.
 */
#include "computer_use_x11.h"
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


static bool x11_is_available(void) {
    if (!getenv("DISPLAY")) return false;
    return _has_cmd("import") || _has_cmd("xdotool");
}


static bool x11_start(void) { return true; }

static void x11_stop(void) {}


static cu_capture_t *x11_capture(const char *mode, const char *app) {
    (void)app;
    cu_capture_t *cap = (cu_capture_t *)calloc(1, sizeof(cu_capture_t));
    if (!cap) return NULL;
    snprintf(cap->mode, sizeof(cap->mode), "%s", mode ? mode : "som");
    cap->width = 0;
    cap->height = 0;
    cap->png_b64 = NULL;

    if (mode && strcmp(mode, "ax") == 0) {
        snprintf(cap->window_title, sizeof(cap->window_title),
                 "X11 Desktop (no accessibility tree available)");
        return cap;
    }

    /* Try ImageMagick import for screenshot */
    if (_has_cmd("import")) {
        char tmp_path[256];
        snprintf(tmp_path, sizeof(tmp_path),
                 "/tmp/cu_capture_%d.png", getpid());

        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "import -window root -quality 85 %s 2>/dev/null", tmp_path);
        int ret = system(cmd);
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
            /* Get dimensions via identify */
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
            unlink(tmp_path);
        }
    }
    return cap;
}


static cu_action_t *x11_make_action(const char *action_name, bool ok,
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


static cu_action_t *x11_click(int element, int x, int y,
                               const char *button, int click_count,
                               const char *modifiers) {
    (void)element;
    (void)modifiers;
    if (!_has_cmd("xdotool")) {
        return x11_make_action("click", false,
            "xdotool not available - install: sudo apt install xdotool");
    }
    char cmd[512];
    int btn_num = 1;
    if (button) {
        if (strcmp(button, "middle") == 0) btn_num = 2;
        else if (strcmp(button, "right") == 0) btn_num = 3;
    }
    snprintf(cmd, sizeof(cmd),
             "xdotool mousemove %d %d click %d 2>/dev/null", x, y, btn_num);
    if (click_count > 1 && btn_num == 1) {
        snprintf(cmd, sizeof(cmd),
                 "xdotool mousemove %d %d click --repeat %d 1 2>/dev/null",
                 x, y, click_count);
    }
    int ret = system(cmd);
    return x11_make_action("click", ret == 0,
        ret == 0 ? "clicked at (%d,%d)" : "click failed",
        x, y);
}


/* PoP: x11_type_text @ tools/computer_use/backend.py:ComputerUseBackend.type_text */
static cu_action_t *x11_type_text(const char *text) {
    if (!_has_cmd("xdotool")) {
        return x11_make_action("type", false, "xdotool not available");
    }
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "xdotool type -- '%s' 2>/dev/null", text ? text : "");
    int ret = system(cmd);
    return x11_make_action("type", ret == 0,
        ret == 0 ? "typed %zu characters" : "type failed",
        text ? strlen(text) : 0);
}


static cu_action_t *x11_key(const char *keys) {
    if (!_has_cmd("xdotool")) {
        return x11_make_action("key", false, "xdotool not available");
    }
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "xdotool key '%s' 2>/dev/null", keys ? keys : "");
    int ret = system(cmd);
    return x11_make_action("key", ret == 0,
        ret == 0 ? "sent key %s" : "key failed",
        keys ? keys : "");
}


static cu_action_t *x11_scroll(const char *dir, int amount,
                                int element, int x, int y,
                                const char *modifiers) {
    (void)amount;
    (void)element; (void)modifiers;
    if (!_has_cmd("xdotool")) {
        return x11_make_action("scroll", false, "xdotool not available");
    }
    if (x > 0 || y > 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "xdotool mousemove %d %d 2>/dev/null", x, y);
        int r = system(cmd);
        (void)r;
    }
    int btn = 5; /* down */
    if (dir && (strcmp(dir, "up") == 0 || strcmp(dir, "left") == 0))
        btn = 4;
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "xdotool click %d 2>/dev/null", btn);
    int ret = system(cmd);
    return x11_make_action("scroll", ret == 0,
        ret == 0 ? "scrolled %s" : "scroll failed",
        dir ? dir : "down");
}


/* PoP: x11_list_apps @ tools/computer_use/backend.py:ComputerUseBackend.list_apps */
static cu_action_t *x11_list_apps(void) {
    if (!_has_cmd("xdotool")) {
        return x11_make_action("list_apps", false, "xdotool not available");
    }
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "xdotool getactivewindow getwindowname 2>/dev/null");
    FILE *fp = popen(cmd, "r");
    char win_name[256] = "";
    if (fp) {
        if (fgets(win_name, sizeof(win_name), fp)) {
            size_t len = strlen(win_name);
            if (len > 0 && win_name[len-1] == '\n') win_name[len-1] = '\0';
        }
        pclose(fp);
    }
    return x11_make_action("list_apps", true,
        "active window: %s", win_name[0] ? win_name : "(unknown)");
}


/* PoP: x11_focus_app @ tools/computer_use/backend.py:ComputerUseBackend.focus_app */
static cu_action_t *x11_focus_app(const char *app, bool raise) {
    (void)raise;
    if (!_has_cmd("xdotool")) {
        return x11_make_action("focus_app", false, "xdotool not available");
    }
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "xdotool search --name '%s' windowactivate 2>/dev/null",
             app ? app : "");
    int ret = system(cmd);
    return x11_make_action("focus_app", ret == 0,
        ret == 0 ? "focused %s" : "no window found matching '%s'",
        app ? app : "", app ? app : "");
}


/* PoP: x11_drag @ tools/computer_use/backend.py:ComputerUseBackend.drag */
static cu_action_t *x11_drag(int from_e, int to_e,
                              int fx, int fy, int tx, int ty,
                              const char *button, const char *modifiers) {
    (void)from_e; (void)to_e; (void)modifiers;
    if (!_has_cmd("xdotool")) {
        return x11_make_action("drag", false, "xdotool not available");
    }
    int btn_num = 1;
    if (button) {
        if (strcmp(button, "middle") == 0) btn_num = 2;
        else if (strcmp(button, "right") == 0) btn_num = 3;
    }
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "xdotool mousemove %d %d mousedown %d "
             "mousemove %d %d mouseup %d 2>/dev/null",
             fx, fy, btn_num, tx, ty, btn_num);
    int ret = system(cmd);
    return x11_make_action("drag", ret == 0,
        ret == 0 ? "dragged (%d,%d)-(%d,%d)" : "drag failed",
        fx, fy, tx, ty);
}


static cu_action_t *x11_set_value(const char *value, int element) {
    (void)element;  /* X11 has no element-level targeting without accessibility */
    if (!value || !*value)
        return x11_make_action("set_value", true, "set empty value (no-op)");
    if (!_has_cmd("xdotool"))
        return x11_make_action("set_value", false,
            "xdotool not available - install: sudo apt install xdotool");
    /* Click to focus, then type */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "xdotool click 1 2>/dev/null && xdotool type --clearmodifiers '%s' 2>/dev/null",
             value);
    int ret = system(cmd);
    return x11_make_action("set_value", ret == 0,
        ret == 0 ? "typed value (%zu chars)" : "type command failed",
        strlen(value));
}


static cu_action_t *x11_wait(double seconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
    return x11_make_action("wait", true, "waited %.2fs", seconds);
}


/* PoP: make_x11_backend @ tools/computer_use/cua_backend.py:CuaDriverBackend.is_available */
cu_backend_t *make_x11_backend(void) {
    cu_backend_t *b = (cu_backend_t *)calloc(1, sizeof(cu_backend_t));
    if (!b) return NULL;
    b->is_available = x11_is_available;
    b->start = x11_start;
    b->stop = x11_stop;
    b->capture = x11_capture;
    b->click = x11_click;
    b->drag = x11_drag;
    b->scroll = x11_scroll;
    b->type_text = x11_type_text;
    b->key = x11_key;
    b->list_apps = x11_list_apps;
    b->focus_app = x11_focus_app;
    b->set_value = x11_set_value;
    b->wait = x11_wait;
    b->state = NULL;
    return b;
}
