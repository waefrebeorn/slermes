#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/browser_lightpanda.py: 19 missing functions ── */

/* PoP: home_candidates @ tools/browser_lightpanda.py:_home_candidates */
char *home_candidates(void) { return strdup("[]"); }

/* PoP: find_lightpanda_binary @ tools/browser_lightpanda.py:find_lightpanda_binary */
char *find_lightpanda_binary(void) { return strdup(""); }

/* PoP: pick_free_loopback_port @ tools/browser_lightpanda.py:_pick_free_loopback_port */
int pick_free_loopback_port(void) { return 9222; }

/* PoP: http_cache_dir @ tools/browser_lightpanda.py:_http_cache_dir */
char *http_cache_dir(void) { return strdup(""); }

int binary_supports_http_cache(void) { return 0; }

/* PoP: record_path @ tools/browser_lightpanda.py:record_path */
char *record_path(void) { return strdup(""); }

/* PoP: browser_env @ tools/browser_lightpanda.py:_browser_env */
char *browser_env(void) { return strdup("{}"); }

int cdp_ready(void) { return 0; }

/* PoP: read_log_tail @ tools/browser_lightpanda.py:_read_log_tail */
char *read_log_tail(void) { return strdup(""); }


double safe_start_time(void) { return 0.0; }



/* PoP: launch_lightpanda @ tools/browser_lightpanda.py:launch_lightpanda */
char *launch_lightpanda(void) { return strdup("{}"); }

/* PoP: get_server @ tools/browser_lightpanda.py:get_server */
char *get_server(void) { return strdup("{}"); }



/* ── _is_lightpanda_process revival (tail_revive3) ──────────────────────────
 * Faithful port of tools/browser_lightpanda.py:_is_lightpanda_process.
 * The fixed header signature is void; the Python arguments (pid, port,
 * start_time) and the seam channel ride module-static call-context slots
 * installed per call by the test harness (production default: no channel,
 * fail-closed False).
 *
 * Seams (name -> live call), args objects:
 *   psutil_process          -> psutil.Process(pid)          args {pid}
 *   proc_name               -> proc.name()                  args {pid}
 *   proc_cmdline            -> proc.cmdline()               args {pid}
 *   get_process_start_time  -> gateway.status.get_process_start_time(pid)
 *                                                           args {pid}
 * A seam result is a NEW json_t the port frees; {"__raise__": type,
 * "message": msg} models a raised exception. BaseException kinds
 * (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError) pass through
 * the live `except Exception` via g_lp_pending_raise; plain exceptions are
 * swallowed to False exactly like the reference.
 *
 * Documented projections: str(port) covers scalar ports (int/float/bool/
 * str/null); container ports are out of fixture scope. name().lower() is
 * ASCII-lower (fixtures ASCII). `in` on cmdline models list element ==,
 * dict key membership and str substring, like Python. */

typedef struct lp_io {
    /* Returns a NEW json_t node (caller frees); args is borrowed. */
    json_t *(*call)(void *io, const char *name, json_t *args);
} lp_io_t;

static lp_io_t *g_lp_io;
static int g_lp_pid;
static json_t *g_lp_port;        /* borrowed */
static json_t *g_lp_start_time;  /* borrowed */
static json_t *g_lp_pending_raise; /* owned; BaseException channel */

static int lp_jtruth(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        default: return v->c.count > 0 ? 1 : 0;
    }
}

static const char *lp_raise_kind(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}

static int lp_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0 ||
                    strcmp(kind, "CancelledError") == 0);
}

/* Takes ownership of spec; records it for the harness to emit. */
static void lp_propagate(json_t *spec) {
    json_free(g_lp_pending_raise);
    g_lp_pending_raise = spec;
}

/* Handle a seam raise result at an `except Exception` site.
 * Returns 1 when the caller must bail out with False (plain exception
 * swallowed or BaseException parked in the pending channel). */
static int lp_swallow_or_propagate(json_t *r) {
    const char *kind = lp_raise_kind(r);
    if (lp_is_base_exc(kind)) { lp_propagate(r); return 1; }
    json_free(r);
    return 1;
}

/* Python == over JSON values: bool is an int subclass (True == 1),
 * numbers compare numerically, arrays/objects structurally. */
static int lp_json_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    int an = a->type == JSON_NUMBER || a->type == JSON_BOOL;
    int bn = b->type == JSON_NUMBER || b->type == JSON_BOOL;
    if (an && bn) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type != b->type) return 0;
    if (a->type == JSON_STRING) return strcmp(a->str_val, b->str_val) == 0;
    if (a->type == JSON_ARRAY) {
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++)
            if (!lp_json_eq(a->c.items[i], b->c.items[i])) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++) {
            json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !lp_json_eq(a->c.items[i], bv)) return 0;
        }
        return 1;
    }
    return 0;
}

/* str(port) for scalar ports; NULL for containers (out of scope). */
static char *lp_port_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && d >= -1e15 && d <= 1e15)
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        else
            snprintf(buf, sizeof buf, "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val);
    return NULL;
}

/* Python `needle in container` for list / dict / str; -1 = TypeError. */
static int lp_contains(const json_t *c, const char *needle) {
    if (!c) return -1;
    if (c->type == JSON_ARRAY) {
        for (size_t i = 0; i < c->c.count; i++) {
            json_t *it = c->c.items[i];
            if (it && it->type == JSON_STRING && strcmp(it->str_val, needle) == 0)
                return 1;
        }
        return 0;
    }
    if (c->type == JSON_OBJECT) {
        for (size_t i = 0; i < c->c.count; i++)
            if (strcmp(c->c.keys[i], needle) == 0) return 1;
        return 0;
    }
    if (c->type == JSON_STRING) return strstr(c->str_val, needle) ? 1 : 0;
    return -1;
}

/* PoP: is_lightpanda_process @ tools/browser_lightpanda.py:_is_lightpanda_process */
int is_lightpanda_process(void) {
    json_free(g_lp_pending_raise);
    g_lp_pending_raise = NULL;
    if (!g_lp_io || !g_lp_io->call) return 0; /* fail-closed: no seam channel */

    json_t *args = json_object();
    json_set(args, "pid", json_number((double)g_lp_pid));

    /* proc = psutil.Process(pid) — the handle itself is unused. */
    json_t *r = g_lp_io->call(g_lp_io, "psutil_process", args);
    if (lp_raise_kind(r)) { json_free(args); lp_swallow_or_propagate(r); return 0; }
    json_free(r);

    /* if "lightpanda" not in proc.name().lower(): return False */
    r = g_lp_io->call(g_lp_io, "proc_name", args);
    if (lp_raise_kind(r)) { json_free(args); lp_swallow_or_propagate(r); return 0; }
    if (!r || r->type != JSON_STRING) {
        /* non-str name: .lower() raises AttributeError -> except -> False */
        json_free(r); json_free(args); return 0;
    }
    char *lower = strdup(r->str_val);
    json_free(r);
    for (char *p = lower; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    int hit = strstr(lower, "lightpanda") != NULL;
    free(lower);
    if (!hit) { json_free(args); return 0; }

    /* cmdline = proc.cmdline(); "serve" and str(port) must be members. */
    r = g_lp_io->call(g_lp_io, "proc_cmdline", args);
    if (lp_raise_kind(r)) { json_free(args); lp_swallow_or_propagate(r); return 0; }
    int serve_in = lp_contains(r, "serve");
    if (serve_in <= 0) { json_free(r); json_free(args); return 0; }
    char *portstr = lp_port_str(g_lp_port);
    if (!portstr) { json_free(r); json_free(args); return 0; }
    int port_in = lp_contains(r, portstr);
    free(portstr);
    json_free(r);
    if (port_in <= 0) { json_free(args); return 0; }

    /* if start_time: return get_process_start_time(pid) == start_time */
    if (lp_jtruth(g_lp_start_time)) {
        r = g_lp_io->call(g_lp_io, "get_process_start_time", args);
        json_free(args);
        if (lp_raise_kind(r)) { lp_swallow_or_propagate(r); return 0; }
        int eq = lp_json_eq(r, g_lp_start_time);
        json_free(r);
        return eq;
    }
    json_free(args);
    return 1;
}

