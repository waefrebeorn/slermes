/*
 * runtime.c - C11 port of upstream Hermes tools/bot_desktop/runtime.py.
 *
 * Gates sharing this file:
 *   w30_runtime_3588a2_p1 - host inquiry, state paths, display band, published env
 *   w30_runtime_3588a2_p2 - lifecycle decisions, idle math, status/placement shaping
 *
 * PoP: w30_rt_p1_state_dir              @ tools/bot_desktop/runtime.py:state_dir
 * PoP: w30_rt_p1_is_supported_host      @ tools/bot_desktop/runtime.py:is_supported_host
 * PoP: w30_rt_p1_missing_binaries       @ tools/bot_desktop/runtime.py:missing_binaries
 * PoP: w30_rt_p1_package_manager        @ tools/bot_desktop/runtime.py:package_manager
 * PoP: w30_rt_p1_is_root                @ tools/bot_desktop/runtime.py:is_root
 * PoP: w30_rt_p1_installable            @ tools/bot_desktop/runtime.py:installable
 * PoP: w30_rt_p1_install_command        @ tools/bot_desktop/runtime.py:install_command
 * PoP: w30_rt_p1_launcher_identity      @ tools/bot_desktop/runtime.py:_launcher_identity
 * PoP: w30_rt_p1_launcher_pid           @ tools/bot_desktop/runtime.py:_launcher_pid
 * PoP: w30_rt_p1_recorded_launcher_pid  @ tools/bot_desktop/runtime.py:_recorded_launcher_pid
 * PoP: w30_rt_p1_x_lock_pid             @ tools/bot_desktop/runtime.py:_x_lock_pid
 * PoP: w30_rt_p1_x_socket_bound         @ tools/bot_desktop/runtime.py:_x_socket_bound
 * PoP: w30_rt_p1_display_in_use         @ tools/bot_desktop/runtime.py:_display_in_use
 * PoP: w30_rt_p1_pick_display           @ tools/bot_desktop/runtime.py:_pick_display
 * PoP: w30_rt_p1_published_env          @ tools/bot_desktop/runtime.py:published_env
 * PoP: w30_rt_p1_rfb_socket_path        @ tools/bot_desktop/runtime.py:rfb_socket_path
 * PoP: w30_rt_p1_is_running             @ tools/bot_desktop/runtime.py:is_running
 * PoP: w30_rt_p1_host_published_env     @ tools/bot_desktop/runtime.py:host_published_env
 * PoP: w30_rt_p1_desktop_env            @ tools/bot_desktop/runtime.py:desktop_env
 * PoP: w30_rt_p1_geometry               @ tools/bot_desktop/runtime.py:geometry
 * PoP: w30_rt_p2_idle_stop_seconds      @ tools/bot_desktop/runtime.py:idle_stop_seconds
 * PoP: w30_rt_p2_idle_seconds           @ tools/bot_desktop/runtime.py:idle_seconds
 * PoP: w30_rt_p2_idle_due               @ tools/bot_desktop/runtime.py:_idle_due
 * PoP: w30_rt_p2_should_auto_start      @ tools/bot_desktop/runtime.py:_should_auto_start
 * PoP: w30_rt_p2_tool_placement         @ tools/bot_desktop/runtime.py:tool_placement
 * PoP: w30_rt_p2_ensure_started_for_tool @ tools/bot_desktop/runtime.py:ensure_started_for_tool
 * PoP: w30_rt_p2_start                  @ tools/bot_desktop/runtime.py:start
 * PoP: w30_rt_p2_status                 @ tools/bot_desktop/runtime.py:status
 * PoP: w30_rt_p2_sandbox_status         @ tools/bot_desktop/runtime.py:_sandbox_status
 * PoP: w30_rt_p2_stop                   @ tools/bot_desktop/runtime.py:stop
 * PoP: w30_rt_p2_in_sandbox             @ tools/bot_desktop/runtime.py:in_sandbox
 *
 * The desktop itself is the seam: psutil (liveness, create_time), shutil.which,
 * os.geteuid, sys.platform, /proc/net/unix, the X lock files, the lease file,
 * the config loader, placement.resolve(), the launcher Popen, the sandbox host
 * and the clock are all answered by the caller and handed in as request fields.
 * What is ported here is what those answers decide: which display number is
 * free, which path the profile's state lives at, which env a spawn on this
 * screen gets, whether an idle screen is due, where a tool's browser must run,
 * what status the pane shows and what chore stop() performs.
 */
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w30_runtime_3588a2_p1.h"
#include "noc_w30_runtime_3588a2_p2.h"

/* ====================================================================== */
/* Python value helpers                                                   */
/* ====================================================================== */

/* Python truthiness of a JSON value. */
static int rt_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python's numeric view: int, float or bool (None is none of those). */
static int rt_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { if (out) *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { if (out) *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

static int rt_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.splitlines() breaks on these; str.split() breaks on those plus space/tab. */
static int rt_nl(char c) {
    return c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *rt_dup_range(const char *b, const char *e) {
    size_t n = (size_t)(e - b);
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    if (n) memcpy(s, b, n);
    s[n] = '\0';
    return s;
}

static char *rt_strdup(const char *s) {
    size_t n;
    char *o;
    if (!s) s = "";
    n = strlen(s);
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

/* Python str.strip() in place (ASCII whitespace is what a pid/env file holds). */
static void rt_strip_inplace(char *s) {
    size_t a = 0, b, i;
    if (!s) return;
    b = strlen(s);
    while (a < b && rt_ws(s[a])) a++;
    while (b > a && rt_ws(s[b - 1])) b--;
    for (i = 0; a + i < b; i++) s[i] = s[a + i];
    s[i] = '\0';
}

/* _read(path): the text, stripped, empty -> None. */
static char *rt_read_text(const json_t *v) {
    char *s;
    if (!v || v->type != JSON_STRING) return NULL;
    s = rt_strdup(v->str_val ? v->str_val : "");
    if (!s) return NULL;
    rt_strip_inplace(s);
    if (!s[0]) { free(s); return NULL; }
    return s;
}

/* str.isdigit() over ASCII digits (the only ones launcher.pid/display hold). */
static int rt_is_digits(const char *s) {
    if (!s || !*s) return 0;
    for (; *s; s++)
        if (*s < '0' || *s > '9') return 0;
    return 1;
}

/* Python float(s); ok=0 mirrors ValueError/TypeError. */
static double rt_py_float(const char *s, int *ok) {
    char buf[128];
    size_t n;
    char *end = NULL;
    double v;
    *ok = 0;
    if (!s) return 0.0;
    n = strlen(s);
    if (n >= sizeof(buf)) return 0.0;
    memcpy(buf, s, n + 1);
    rt_strip_inplace(buf);
    if (!buf[0]) return 0.0;
    v = strtod(buf, &end);
    if (!end || *end) return 0.0; /* float() consumes the whole string */
    *ok = 1;
    return v;
}

/* Python int(s) for the shapes a lock file can hold; ok=0 mirrors ValueError. */
static long long rt_py_int(const char *s, int *ok) {
    const char *p;
    long long v = 0;
    int neg = 0, digits = 0;
    *ok = 0;
    if (!s) return 0;
    p = s;
    while (rt_ws(*p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    while (*p >= '0' && *p <= '9') {
        if (v > (LLONG_MAX - 9) / 10) return 0; /* beyond any real pid/display */
        v = v * 10 + (*p - '0');
        p++;
        digits++;
    }
    if (!digits) return 0;
    while (rt_ws(*p)) p++;
    if (*p) return 0;
    *ok = 1;
    return neg ? -v : v;
}

/* Python str() for the numbers this module renders (display numbers, geometry). */
static void rt_num_str(double v, char *buf, size_t n) {
    if (isfinite(v) && fabs(v) < 1e15 && v == floor(v))
        snprintf(buf, n, "%.0f", v);
    else
        snprintf(buf, n, "%.17g", v);
}

static char *rt_join(const char *base, const char *name) {
    size_t bl, nl;
    char *out;
    if (!base) base = "";
    bl = strlen(base);
    while (bl > 1 && base[bl - 1] == '/') bl--;
    nl = strlen(name);
    out = (char *)malloc(bl + nl + 2);
    if (!out) return NULL;
    if (bl == 0) {
        memcpy(out, name, nl + 1);
        return out;
    }
    memcpy(out, base, bl);
    if (base[bl - 1] == '/') {
        memcpy(out + bl, name, nl + 1);
    } else {
        out[bl] = '/';
        memcpy(out + bl + 1, name, nl + 1);
    }
    return out;
}

/* Is `name` in a JSON array of strings? (shutil.which(name) is not None) */
static int rt_has_str(const json_t *arr, const char *name) {
    size_t i, n;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    n = json_len(arr);
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val ? v->str_val : "", name)) return 1;
    }
    return 0;
}

/* Is the number `num` in a JSON array of numbers? */
static int rt_has_num(const json_t *arr, double num) {
    size_t i, n;
    double d;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    n = json_len(arr);
    for (i = 0; i < n; i++)
        if (rt_number(json_get(arr, i), &d) && d == num) return 1;
    return 0;
}

/* ---- growable string (the advisory sentences are assembled, not fixed) ---- */
typedef struct {
    char *p;
    size_t len, cap;
} rt_sb;

static void rt_sb_init(rt_sb *b) {
    b->p = NULL;
    b->len = b->cap = 0;
}

static int rt_sb_reserve(rt_sb *b, size_t extra) {
    size_t need = b->len + extra + 1;
    char *np;
    if (need <= b->cap) return 1;
    if (b->cap == 0) b->cap = 128;
    while (b->cap < need) b->cap *= 2;
    np = (char *)realloc(b->p, b->cap);
    if (!np) return 0;
    b->p = np;
    return 1;
}

static void rt_sb_add(rt_sb *b, const char *s) {
    size_t n;
    if (!s) return;
    n = strlen(s);
    if (!rt_sb_reserve(b, n)) return;
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void rt_sb_addc(rt_sb *b, char c) {
    if (!rt_sb_reserve(b, 1)) return;
    b->p[b->len++] = c;
    b->p[b->len] = '\0';
}

static char *rt_sb_take(rt_sb *b) {
    char *p = b->p;
    if (!p) p = rt_strdup("");
    b->p = NULL;
    b->len = b->cap = 0;
    return p;
}

/* json_copy for a value that may be absent: a missing key is Python None. */
static json_t *rt_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* A "!raise: <message>" marker stands in for the RuntimeError the truth raises,
 * because the differential harness speaks JSON lines, not exceptions. */
static json_t *rt_raise(const char *msg) {
    rt_sb b;
    json_t *out;
    char *s;
    rt_sb_init(&b);
    rt_sb_add(&b, "!raise: ");
    rt_sb_add(&b, msg ? msg : "");
    s = rt_sb_take(&b);
    out = json_string(s ? s : "!raise: ");
    free(s);
    return out;
}

/* ', '.join(values) */
static char *rt_join_comma(const json_t *arr) {
    rt_sb b;
    size_t i, n;
    rt_sb_init(&b);
    rt_sb_add(&b, "");
    n = (arr && arr->type == JSON_ARRAY) ? json_len(arr) : 0;
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(arr, i);
        const char *s = (v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : "";
        if (i) rt_sb_add(&b, ", ");
        rt_sb_add(&b, s);
    }
    return rt_sb_take(&b);
}

/* ====================================================================== */
/* Gate w30_runtime_3588a2_p1                                             */
/* ====================================================================== */

/* REQUIRED_BINARIES */
static const char *const RT_REQUIRED[W30_RT_REQUIRED_COUNT] = {
    "Xvnc", "xfwm4", "xfce4-panel", "xfdesktop", "xfsettingsd",
    "dbus-run-session", "xauth", "xdpyinfo", "setxkbmap", "xprop",
};

/* PACKAGES - per distro family, in the order install_command() joins them. */
static const char *const RT_PKGS_APT[] = {
    "tigervnc-standalone-server", "xfce4-panel", "xfwm4", "xfdesktop4", "xfce4-settings",
    "xfce4-terminal", "dbus-x11", "x11-xserver-utils", "x11-utils", "x11-xkb-utils", "xauth",
    "fonts-dejavu-core",
};
static const char *const RT_PKGS_DNF[] = {
    "tigervnc-x11-server", "xfce4-panel", "xfwm4", "xfdesktop", "xfce4-settings",
    "xfce4-terminal", "dbus-daemon", "xsetroot", "xset", "xdpyinfo", "xprop", "xorg-x11-xauth",
    "setxkbmap", "dejavu-sans-fonts",
};
static const char *const RT_PKGS_PACMAN[] = {
    "tigervnc", "xfce4-panel", "xfwm4", "xfdesktop", "xfce4-settings", "xfce4-terminal", "dbus",
    "xorg-xsetroot", "xorg-xset", "xorg-xdpyinfo", "xorg-xprop", "xorg-xauth", "xorg-setxkbmap",
    "ttf-dejavu",
};

static char *rt_join_words(const char *const *w, size_t n) {
    rt_sb b;
    size_t i;
    rt_sb_init(&b);
    rt_sb_add(&b, "");
    for (i = 0; i < n; i++) {
        if (i) rt_sb_addc(&b, ' ');
        rt_sb_add(&b, w[i]);
    }
    return rt_sb_take(&b);
}

/* package_manager(): first of apt-get/dnf/pacman that resolves, apt-get -> "apt". */
static const char *rt_pm_name(const json_t *which) {
    if (rt_has_str(which, "apt-get")) return "apt";
    if (rt_has_str(which, "dnf")) return "dnf";
    if (rt_has_str(which, "pacman")) return "pacman";
    return NULL;
}

/* is_root(): hasattr(os, "geteuid") and os.geteuid() == 0 */
static int rt_root(const json_t *req) {
    double d;
    if (!rt_number(json_obj_get(req, "euid"), &d)) return 0;
    return d == 0.0;
}

json_t *w30_rt_p1_state_dir(const json_t *req) {
    const char *home = json_get_str(req, "home", "");
    char *p = rt_join(home, "bot-desktop");
    json_t *out = json_string(p ? p : "bot-desktop");
    free(p);
    return out;
}

json_t *w30_rt_p1_is_supported_host(const json_t *req) {
    const char *platform = json_get_str(req, "platform", "");
    return json_bool(strncmp(platform, "linux", 5) == 0);
}

json_t *w30_rt_p1_missing_binaries(const json_t *req) {
    const json_t *present = json_obj_get(req, "present");
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < W30_RT_REQUIRED_COUNT; i++)
        if (!rt_has_str(present, RT_REQUIRED[i])) json_append(out, json_string(RT_REQUIRED[i]));
    return out;
}

json_t *w30_rt_p1_package_manager(const json_t *req) {
    const char *pm = rt_pm_name(json_obj_get(req, "which"));
    return pm ? json_string(pm) : json_null();
}

json_t *w30_rt_p1_is_root(const json_t *req) {
    return json_bool(rt_root(req));
}

json_t *w30_rt_p1_installable(const json_t *req) {
    const json_t *which = json_obj_get(req, "which");
    if (!rt_pm_name(which)) return json_bool(0);
    if (rt_root(req)) return json_bool(1);
    return json_bool(rt_has_str(which, "sudo"));
}

json_t *w30_rt_p1_install_command(const json_t *req) {
    const json_t *which = json_obj_get(req, "which");
    const char *pm = rt_pm_name(which);
    rt_sb b;
    char *pkgs, *body;
    json_t *j;

    if (!pm) return json_null();
    if (!strcmp(pm, "apt"))
        pkgs = rt_join_words(RT_PKGS_APT, sizeof(RT_PKGS_APT) / sizeof(RT_PKGS_APT[0]));
    else if (!strcmp(pm, "dnf"))
        pkgs = rt_join_words(RT_PKGS_DNF, sizeof(RT_PKGS_DNF) / sizeof(RT_PKGS_DNF[0]));
    else
        pkgs = rt_join_words(RT_PKGS_PACMAN, sizeof(RT_PKGS_PACMAN) / sizeof(RT_PKGS_PACMAN[0]));

    rt_sb_init(&b);
    if (!rt_root(req)) rt_sb_add(&b, "sudo ");
    if (!strcmp(pm, "apt")) {
        rt_sb_add(&b, "apt-get install -y --no-install-recommends ");
    } else if (!strcmp(pm, "dnf")) {
        rt_sb_add(&b, "dnf install -y ");
    } else {
        rt_sb_add(&b, "pacman -S --needed --noconfirm ");
    }
    rt_sb_add(&b, pkgs ? pkgs : "");
    body = rt_sb_take(&b);
    j = json_string(body ? body : "");
    free(body);
    free(pkgs);
    return j;
}

json_t *w30_rt_p1_launcher_identity(const json_t *req) {
    char *raw = rt_read_text(json_obj_get(req, "pidfile"));
    const char *sp;
    char *pid_s = NULL;
    const char *born_s = "";
    long long pid = 0;
    double born = 0.0, ct = 0.0;
    int ok = 0;
    json_t *out = NULL;

    if (raw) {
        sp = strchr(raw, ' ');
        if (sp) {
            pid_s = rt_dup_range(raw, sp);
            born_s = sp + 1;
        } else {
            pid_s = rt_strdup(raw);
            born_s = "";
        }
    }
    if (!pid_s || !rt_is_digits(pid_s) || !*born_s) goto done;
    ok = 0;
    pid = rt_py_int(pid_s, &ok);
    if (!ok) goto done;
    born = rt_py_float(born_s, &ok);
    if (!ok) goto done;
    /* a recycled pid with a different start time is somebody else's process */
    if (!rt_number(json_obj_get(req, "create_time"), &ct)) goto done;
    if (fabs(ct - born) >= 0.01) goto done;
    if (!rt_truthy(json_obj_get(req, "alive"))) goto done;
    out = json_object();
    json_set(out, "pid", json_number((double)pid));
    json_set(out, "create_time", json_number(born));
done:
    free(raw);
    free(pid_s);
    return out ? out : json_null();
}

json_t *w30_rt_p1_launcher_pid(const json_t *req) {
    json_t *id = w30_rt_p1_launcher_identity(req);
    json_t *out;
    if (!id || id->type != JSON_OBJECT) { json_free(id); return json_null(); }
    out = json_copy(json_obj_get(id, "pid"));
    json_free(id);
    return out ? out : json_null();
}

json_t *w30_rt_p1_recorded_launcher_pid(const json_t *req) {
    char *raw = rt_read_text(json_obj_get(req, "pidfile"));
    const char *sp;
    json_t *out = json_null();
    if (!raw) return out;
    sp = strchr(raw, ' ');
    if (sp && sp != raw && sp[1]) {
        char *pid_s = rt_dup_range(raw, sp);
        if (rt_is_digits(pid_s)) {
            int ok = 0;
            long long pid = rt_py_int(pid_s, &ok);
            if (ok) {
                json_free(out);
                out = json_number((double)pid);
            }
        }
        free(pid_s);
    }
    free(raw);
    return out;
}

json_t *w30_rt_p1_x_lock_pid(const json_t *req) {
    const json_t *v = json_obj_get(req, "raw");
    int ok = 0;
    long long pid;
    if (!v || v->type != JSON_STRING) return json_null();
    pid = rt_py_int(v->str_val ? v->str_val : "", &ok);
    return ok ? json_number((double)pid) : json_null();
}

json_t *w30_rt_p1_x_socket_bound(const json_t *req) {
    const json_t *tv = json_obj_get(req, "unix_table");
    const char *text;
    char needle[64];
    double num = 0.0;
    const char *p;
    int found = 0;

    if (!tv || tv->type != JSON_STRING) return json_bool(0); /* no /proc/net/unix read: False */
    text = tv->str_val ? tv->str_val : "";
    rt_number(json_obj_get(req, "num"), &num);
    snprintf(needle, sizeof(needle), "/tmp/.X11-unix/X%.0f", num);

    p = text;
    while (*p && !found) {
        const char *e = p, *b, *t;
        while (*e && !rt_nl(*e)) e++;           /* one splitlines() line */
        b = e;
        while (b > p && rt_ws(b[-1])) b--;      /* trailing whitespace */
        t = p;
        while (t < b && rt_ws(*t)) t++;         /* `if line.strip()` */
        if (t < b) {
            const char *s = b;
            while (s > t && !rt_ws(s[-1])) s--; /* line.split()[-1] */
            while (s < b && *s == '@') s++;     /* .lstrip("@") */
            if ((size_t)(b - s) == strlen(needle) && !strncmp(s, needle, strlen(needle)))
                found = 1;
        }
        p = e;
        if (*p) p++;
    }
    return json_bool(found);
}

json_t *w30_rt_p1_display_in_use(const json_t *req) {
    double pid = 0.0;
    int have_pid = rt_number(json_obj_get(req, "lock_pid"), &pid);
    int alive = rt_truthy(json_obj_get(req, "alive"));
    /* A lock left by a crashed server (dead pid, no socket) does not count. */
    if (have_pid && alive) return json_bool(1);
    return json_bool(rt_truthy(json_obj_get(req, "socket_bound")));
}

json_t *w30_rt_p1_pick_display(const json_t *req) {
    char *rec = rt_read_text(json_obj_get(req, "recorded"));
    const json_t *in_use = json_obj_get(req, "in_use");
    json_t *out = NULL;
    int num;

    if (rec && rt_is_digits(rec)) {
        int ok = 0;
        long long v = rt_py_int(rec, &ok);
        if (ok && !rt_has_num(in_use, (double)v)) out = json_number((double)v);
    }
    free(rec);
    if (out) return out;
    for (num = W30_RT_DISPLAY_MIN; num <= W30_RT_DISPLAY_MAX; num++)
        if (!rt_has_num(in_use, (double)num)) return json_number((double)num);
    return rt_raise("no free X display number in the Bot Desktop band");
}

/* out[key.strip()] = value.strip() for every "key=value" line. */
static void rt_env_lines(json_t *out, const char *raw) {
    const char *p = raw;
    while (p && *p) {
        const char *e = p;
        const char *eq;
        while (*e && !rt_nl(*e)) e++;
        eq = (const char *)memchr(p, '=', (size_t)(e - p));
        if (eq) {
            char *k = rt_dup_range(p, eq);
            char *v = rt_dup_range(eq + 1, e);
            if (k) rt_strip_inplace(k);
            if (v) rt_strip_inplace(v);
            if (k && v) json_set(out, k, json_string(v));
            free(k);
            free(v);
        }
        p = e;
        if (*p) p++;
    }
}

json_t *w30_rt_p1_published_env(const json_t *req) {
    const json_t *lp;
    json_t *out;
    char *raw;

    if (rt_truthy(json_obj_get(req, "marker"))) {
        const json_t *sp = json_obj_get(req, "sandbox_published");
        return (sp && sp->type == JSON_OBJECT) ? json_copy(sp) : json_object();
    }
    lp = json_obj_get(req, "launcher_pid");
    if (!lp || lp->type != JSON_NUMBER) return json_object();
    out = json_object();
    raw = rt_read_text(json_obj_get(req, "raw"));
    if (!raw) return out;
    rt_env_lines(out, raw);
    free(raw);
    return out;
}

json_t *w30_rt_p1_rfb_socket_path(const json_t *req) {
    const json_t *lp;
    char *joined;
    json_t *path;

    if (rt_truthy(json_obj_get(req, "marker"))) return json_null();
    lp = json_obj_get(req, "launcher_pid");
    if (!lp || lp->type != JSON_NUMBER) return json_null();
    if (!rt_truthy(json_obj_get(req, "exists"))) return json_null();
    joined = rt_join(json_get_str(req, "state_dir", ""), "rfb.sock");
    path = json_string(joined ? joined : "rfb.sock");
    free(joined);
    return path;
}

json_t *w30_rt_p1_is_running(const json_t *req) {
    const json_t *env = json_obj_get(req, "env");
    return json_bool(rt_truthy(env && env->type == JSON_OBJECT ? json_obj_get(env, "DISPLAY") : NULL));
}

json_t *w30_rt_p1_host_published_env(const json_t *req) {
    const json_t *pub;
    if (rt_truthy(json_obj_get(req, "marker"))) return json_object();
    pub = json_obj_get(req, "published");
    return pub && pub->type == JSON_OBJECT ? json_copy(pub) : json_object();
}

json_t *w30_rt_p1_desktop_env(const json_t *req) {
    const json_t *base = json_obj_get(req, "base");
    const json_t *pub_in = json_obj_get(req, "published");
    int marker = rt_truthy(json_obj_get(req, "marker"));
    json_t *env = (base && base->type == JSON_OBJECT) ? json_copy(base) : json_object();
    json_t *published = (marker || !pub_in || pub_in->type != JSON_OBJECT) ? json_object() : json_copy(pub_in);

    if (rt_truthy(published)) {
        size_t i, n = json_object_size(published);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(published, i);
            if (k) json_set(env, k, json_copy(json_object_get_at(published, i)));
        }
        json_obj_del(env, "WAYLAND_DISPLAY"); /* X11 desktop; a leaked Wayland socket flips GTK backends */
    } else if (marker) {
        json_obj_del(env, "DISPLAY");
        json_obj_del(env, "XAUTHORITY");
        json_obj_del(env, "DBUS_SESSION_BUS_ADDRESS");
        json_obj_del(env, "WAYLAND_DISPLAY");
    }
    json_free(published);
    return env;
}

json_t *w30_rt_p1_geometry(const json_t *req) {
    const json_t *cfg = json_obj_get(req, "cfg");
    const json_t *v = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "geometry") : NULL;
    char buf[64];

    if (!rt_truthy(v)) return json_string(W30_RT_DEFAULT_GEOMETRY);
    if (v->type == JSON_STRING) return json_string(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        rt_num_str(v->num_val, buf, sizeof(buf));
        return json_string(buf);
    }
    if (v->type == JSON_BOOL) return json_string(v->bool_val ? "True" : "False");
    return json_string(W30_RT_DEFAULT_GEOMETRY);
}

/* ====================================================================== */
/* Gate w30_runtime_3588a2_p2                                             */
/* ====================================================================== */

/* float(cfg.get(key, default)) with TypeError/ValueError -> default. */
static double rt_config_float(const json_t *cfg, const char *key, double def) {
    const json_t *v = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, key) : NULL;
    double d;
    int ok = 0;

    if (!v) return def;
    if (rt_number(v, &d)) return d;
    if (v->type == JSON_STRING) {
        d = rt_py_float(v->str_val, &ok);
        return ok ? d : def;
    }
    return def; /* None / list / dict -> TypeError -> default */
}

json_t *w30_rt_p2_idle_stop_seconds(const json_t *req) {
    const json_t *cfg = json_obj_get(req, "cfg");
    double minutes = rt_config_float(cfg, "idle_stop_minutes", W30_RT_DEFAULT_IDLE_STOP_MINUTES);
    double seconds = minutes * 60.0;
    /* .inf ("never") is off, like 0, and so are .nan and a window too large: none can be due. */
    return json_number(isfinite(seconds) && seconds > 0.0 ? seconds : 0.0);
}

json_t *w30_rt_p2_idle_seconds(const json_t *req) {
    double now = json_get_num(req, "now", 0.0);
    const json_t *a = json_obj_get(req, "activity_mtime");
    const json_t *e = json_obj_get(req, "env_mtime");
    double d;

    /* "activity" first, then the env file's publish time, then None. */
    if (rt_number(a, &d)) return json_number(now - d > 0.0 ? now - d : 0.0);
    if (rt_number(e, &d)) return json_number(now - d > 0.0 ? now - d : 0.0);
    return json_null();
}

json_t *w30_rt_p2_idle_due(const json_t *req) {
    double limit = json_get_num(req, "limit", 0.0);
    int running = rt_truthy(json_obj_get(req, "running"));
    const json_t *launcher = json_obj_get(req, "launcher");
    const json_t *current = json_obj_get(req, "current");
    const char *holder = json_get_str(req, "holder", "");
    double idle = 0.0, abandoned;

    if (limit <= 0.0 || !running) return json_null();
    if (launcher && launcher->type == JSON_OBJECT) {
        double lp = json_get_num(launcher, "pid", 0.0);
        double lc = json_get_num(launcher, "create_time", 0.0);
        double cp, cc;
        if (!current || current->type != JSON_OBJECT) return json_null();
        cp = json_get_num(current, "pid", 0.0);
        cc = json_get_num(current, "create_time", 0.0);
        if (cp != lp || fabs(cc - lc) >= 0.01) return json_null(); /* a replacement is never due */
    }
    if (!rt_number(json_obj_get(req, "idle"), &idle)) return json_null();
    if (idle < limit) return json_null();
    abandoned = (limit * W30_RT_HUMAN_LEASE_ABANDONED_WINDOWS > W30_RT_HUMAN_LEASE_ABANDONED_MIN_S)
                    ? limit * W30_RT_HUMAN_LEASE_ABANDONED_WINDOWS
                    : W30_RT_HUMAN_LEASE_ABANDONED_MIN_S;
    if (holder && !strcmp(holder, "human") && idle < abandoned) return json_null();
    return json_number(idle);
}

json_t *w30_rt_p2_should_auto_start(const json_t *req) {
    int supported = rt_truthy(json_obj_get(req, "supported"));
    const json_t *env = json_obj_get(req, "env");
    const json_t *missing = json_obj_get(req, "missing");

    if (!supported) return json_bool(0);
    if (env && env->type == JSON_OBJECT) {
        if (rt_truthy(json_obj_get(env, "DISPLAY"))) return json_bool(0);
        if (rt_truthy(json_obj_get(env, "WAYLAND_DISPLAY"))) return json_bool(0);
    }
    if (missing && missing->type == JSON_ARRAY && json_len(missing) > 0) return json_bool(0);
    return json_bool(rt_truthy(json_obj_get(req, "auto_start")));
}

json_t *w30_rt_p2_tool_placement(const json_t *req) {
    const char *where = json_get_str(req, "where", "");
    if (!strcmp(where, W30_RT_PLACEMENT_REFUSED)) return rt_raise(json_get_str(req, "reason", ""));
    if (!strcmp(where, W30_RT_PLACEMENT_GATEWAY)) return json_string(W30_RT_PLACEMENT_GATEWAY);
    return json_string(W30_RT_PLACEMENT_TERMINAL);
}

json_t *w30_rt_p2_ensure_started_for_tool(const json_t *req) {
    const json_t *env = json_obj_get(req, "env");
    const char *where = json_get_str(req, "where", "");
    int start_calls = 0, touch_calls = 0;
    json_t *out, *auto_start;

    if (env && env->type == JSON_OBJECT && rt_truthy(json_obj_get(env, "DISPLAY"))) {
        touch_calls = 1; /* a browser / cua-driver spawn is the agent using its screen */
    } else if (!strcmp(where, W30_RT_PLACEMENT_REFUSED)) {
        /* tool_placement() raises the reason at the spawn site: nothing starts here. */
    } else if (!strcmp(where, W30_RT_PLACEMENT_TERMINAL)) {
        start_calls = 1; /* the sandbox is the boundary: bring its screen up, log a failure */
    } else {
        auto_start = w30_rt_p2_should_auto_start(req);
        if (rt_truthy(auto_start)) start_calls = 1;
        json_free(auto_start);
    }
    out = json_object();
    json_set(out, "start_calls", json_number((double)start_calls));
    json_set(out, "touch_calls", json_number((double)touch_calls));
    return out;
}

json_t *w30_rt_p2_start(const json_t *req) {
    const char *where = json_get_str(req, "where", "");
    const json_t *missing = json_obj_get(req, "missing");
    const char *pm = json_get_str(req, "pm", "");
    int has_missing = missing && missing->type == JSON_ARRAY && json_len(missing) > 0;
    char *need, *pkgs;
    rt_sb b;
    json_t *out;

    if (!strcmp(where, W30_RT_PLACEMENT_REFUSED)) return rt_raise(json_get_str(req, "reason", ""));
    if (!strcmp(where, W30_RT_PLACEMENT_TERMINAL)) return json_string("sandbox");
    if (!rt_truthy(json_obj_get(req, "supported")))
        return rt_raise("Bot Desktop runs on Linux gateway hosts only");
    if (!has_missing) return json_string("proceed");

    /* Three dead ends: an operator told "unprivileged, no sudo" while running as root hunts the wrong bug. */
    pkgs = rt_join_comma(missing);
    rt_sb_init(&b);
    rt_sb_add(&b, "Bot Desktop needs ");
    rt_sb_add(&b, pkgs ? pkgs : "");
    rt_sb_add(&b, " on the gateway host");
    need = rt_sb_take(&b);

    if (!pm || !*pm) {
        rt_sb_init(&b);
        rt_sb_add(&b, need ? need : "");
        rt_sb_add(&b, ", and no supported package manager (apt/dnf/pacman) is available to install them. "
                       "Install TigerVNC (Xvnc) and the Xfce core components with this distro's own tooling.");
    } else if (!rt_truthy(json_obj_get(req, "installable"))) {
        rt_sb_init(&b);
        rt_sb_add(&b, need ? need : "");
        rt_sb_add(&b, ", and this host cannot install them: the process is unprivileged and there is no sudo. "
                       "On the published Docker image the packages have to be baked in, so this needs a newer "
                       "image rather than an install.");
    } else {
        rt_sb_init(&b);
        rt_sb_add(&b, need ? need : "");
        rt_sb_add(&b, ". Install: ");
        rt_sb_add(&b, json_get_str(req, "install_command", ""));
    }
    out = rt_raise(rt_sb_take(&b));
    free(pkgs);
    free(need);
    return out;
}

/* status(): the gateway-host DesktopStatus, rendered by as_dict(). */
json_t *w30_rt_p2_status(const json_t *req) {
    const char *profile = json_get_str(req, "profile", "default");
    int supported = rt_truthy(json_obj_get(req, "supported"));
    const json_t *missing = json_obj_get(req, "missing");
    const json_t *pid = json_obj_get(req, "pid");
    const json_t *env = json_obj_get(req, "env");
    int has_missing = missing && missing->type == JSON_ARRAY && json_len(missing) > 0;
    int running;
    json_t *out = json_object();

    running = pid && pid->type == JSON_NUMBER &&
              rt_truthy(env && env->type == JSON_OBJECT ? json_obj_get(env, "DISPLAY") : NULL);

    json_set(out, "profile", json_string(profile));
    json_set(out, "supported", json_bool(supported));
    json_set(out, "installed", json_bool(!has_missing));
    json_set(out, "missing", missing && missing->type == JSON_ARRAY ? json_copy(missing) : json_array());
    json_set(out, "running", json_bool(running));
    json_set(out, "pid", (pid && pid->type == JSON_NUMBER) ? json_copy(pid) : json_null());
    json_set(out, "display", rt_copy_or_null(env && env->type == JSON_OBJECT ? json_obj_get(env, "DISPLAY") : NULL));
    json_set(out, "socket", rt_copy_or_null(json_obj_get(req, "socket")));
    json_set(out, "geometry", rt_copy_or_null(json_obj_get(req, "geometry")));
    /* install_command is only offered while something is missing */
    json_set(out, "install_command", has_missing ? rt_copy_or_null(json_obj_get(req, "install_command")) : json_null());
    json_set(out, "browser", supported ? rt_copy_or_null(json_obj_get(req, "browser")) : json_null());
    /* A running screen is never "blocked": the check guards the allocation, not the session. */
    json_set(out, "blocker",
             (running || has_missing || !supported) ? json_null() : rt_copy_or_null(json_obj_get(req, "blocker")));
    if (supported) {
        json_set(out, "memory_available_mb", rt_copy_or_null(json_obj_get(req, "memory_available_mb")));
        json_set(out, "memory_limit_mb", rt_copy_or_null(json_obj_get(req, "memory_limit_mb")));
    } else {
        json_set(out, "memory_available_mb", json_null()); /* resources.MemoryInfo(None, None) */
        json_set(out, "memory_limit_mb", json_null());
    }
    json_set(out, "placement", json_string(W30_RT_PLACEMENT_GATEWAY));
    json_set(out, "image_switch", json_null());
    return out;
}

/* upper() of the first advice letter; ASCII is what these sentences are written in. */
static char rt_upper1(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

json_t *w30_rt_p2_sandbox_status(const json_t *req) {
    const char *profile = json_get_str(req, "profile", "default");
    const char *backend = json_get_str(req, "backend", "");
    const json_t *missing = json_obj_get(req, "missing");
    const json_t *published = json_obj_get(req, "published");
    const json_t *sw = json_obj_get(req, "switch");
    int has_missing = missing && missing->type == JSON_ARRAY && json_len(missing) > 0;
    int running = rt_truthy(published && published->type == JSON_OBJECT ? json_obj_get(published, "DISPLAY") : NULL);
    json_t *out = json_object();
    json_t *image_switch = NULL;
    char *blocker = NULL;

    if (has_missing) {
        char *head = rt_join_comma(missing);
        const char *advice = json_get_str(req, "advice", "");
        rt_sb b, p;

        rt_sb_init(&b);
        rt_sb_add(&b, "The terminal backend's sandbox image lacks ");
        rt_sb_add(&b, head ? head : "");
        rt_sb_add(&b, ". ");
        if (advice && *advice) {
            rt_sb_addc(&b, rt_upper1(advice[0]));
            rt_sb_add(&b, advice + 1);
        } else {
            rt_sb_add(&b, "Use ");
            rt_sb_add(&b, json_get_str(req, "image_hint", ""));
            rt_sb_add(&b, " as terminal.");
            rt_sb_add(&b, backend);
            rt_sb_add(&b, "_image (the default sandbox base plus the desktop stack)");
        }
        rt_sb_add(&b, ", or set bot_desktop.placement: gateway.");
        blocker = rt_sb_take(&b);
        free(head);

        if (!strcmp(backend, "docker") && sw && sw->type == JSON_OBJECT) {
            const json_t *containers = json_obj_get(sw, "containers");
            char *msg;

            rt_sb_init(&p);
            rt_sb_add(&p, "Your sandbox container still runs ");
            rt_sb_add(&p, json_get_str(sw, "current_image", ""));
            rt_sb_add(&p, ", which has no desktop. Switch it to ");
            rt_sb_add(&p, json_get_str(sw, "target_image", ""));
            rt_sb_add(&p, ": files in /root and /workspace stay, packages installed inside the "
                          "container are reinstalled on demand.");
            msg = rt_sb_take(&p);
            free(blocker);
            blocker = msg;

            image_switch = json_object();
            json_set(image_switch, "current_image", json_copy(json_obj_get(sw, "current_image")));
            json_set(image_switch, "target_image", json_copy(json_obj_get(sw, "target_image")));
            json_set(image_switch, "containers",
                     json_number(containers && containers->type == JSON_ARRAY ? (double)json_len(containers) : 0.0));
        }
    }

    json_set(out, "profile", json_string(profile));
    json_set(out, "supported", json_bool(1));
    json_set(out, "installed", json_bool(1));
    json_set(out, "missing", missing && missing->type == JSON_ARRAY ? json_copy(missing) : json_array());
    json_set(out, "running", json_bool(running));
    json_set(out, "pid", json_null());
    json_set(out, "display",
             rt_copy_or_null(published && published->type == JSON_OBJECT ? json_obj_get(published, "DISPLAY") : NULL));
    json_set(out, "socket", json_null());
    json_set(out, "geometry", rt_copy_or_null(json_obj_get(req, "geometry")));
    json_set(out, "install_command", json_null()); /* the pane's Install would apt on the wrong machine */
    json_set(out, "browser", json_null());
    json_set(out, "blocker", blocker ? json_string(blocker) : json_null());
    json_set(out, "memory_available_mb", json_null());
    json_set(out, "memory_limit_mb", json_null());
    {
        rt_sb tag_sb;
        char *tag;
        rt_sb_init(&tag_sb);
        rt_sb_add(&tag_sb, W30_RT_PLACEMENT_TERMINAL);
        rt_sb_add(&tag_sb, ":");
        rt_sb_add(&tag_sb, backend);
        tag = rt_sb_take(&tag_sb);
        json_set(out, "placement", json_string(tag ? tag : W30_RT_PLACEMENT_TERMINAL));
        free(tag);
    }
    json_set(out, "image_switch", image_switch ? image_switch : json_null());
    free(blocker);
    return out;
}

json_t *w30_rt_p2_stop(const json_t *req) {
    int marker = rt_truthy(json_obj_get(req, "marker"));
    const char *where = json_get_str(req, "where", "");
    const json_t *still_due = json_obj_get(req, "still_due");
    const json_t *lp = json_obj_get(req, "launcher_pid");
    /* None means "no callable"; explicit null is treated the same way. */
    int have_due = still_due && still_due->type != JSON_NULL;

    if (marker || !strcmp(where, W30_RT_PLACEMENT_TERMINAL)) {
        if (have_due && !rt_truthy(still_due)) return json_bool(0); /* changed while it waited: kept */
        return json_bool(rt_truthy(json_obj_get(req, "sandbox_stopped")));
    }
    if (!rt_truthy(json_obj_get(req, "supported"))) return json_bool(0);
    if (have_due && !rt_truthy(still_due)) return json_bool(0);
    if (lp && lp->type == JSON_NUMBER) return json_bool(1); /* the group was signalled */
    return json_bool(rt_truthy(json_obj_get(req, "reaped")));
}

json_t *w30_rt_p2_in_sandbox(const json_t *req) {
    return json_bool(!strcmp(json_get_str(req, "where", ""), W30_RT_PLACEMENT_TERMINAL));
}
