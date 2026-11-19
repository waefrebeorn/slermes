#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/shutdown_watchdog.py: 2 missing functions ── */

/* ── rest_revive1 seam infrastructure (shutdown_watchdog_gaps3) ─────────────
 * json_t *fn(sw_io_t *io, json_t *in): io->call is the injected I/O seam.
 * sw_seam records {ev, args, ret} into io->trace in call order and returns
 * the scripted result (caller frees; args are ADOPTED). A result
 * {"__raise__": Type, "message": msg} models a Python exception; there is
 * no try/except in the live fn, so every raise propagates. NULL == None.
 *
 * get_loop_tick_socket_path: pid = int(pid if pid is not None else
 * os.getpid()) — "os.getpid" seam fires only for a None pid; int() follows
 * Python rules (truncate toward zero, string parse with underscore/sign
 * rules, exact py3.14 error messages, probed). _home(home) resolves a None
 * home through the "_process_hermes_home" seam. Path results are projected
 * to POSIX strings (oracle norm()s with as_posix()); sw_path_norm mirrors
 * Windows pathlib (leading separator run preserved, '.'/empty dropped,
 * internal runs collapsed, trailing seps dropped, backslashes folded).
 * Production default (no seam channel installed) is fail-closed. */
typedef struct sw_io {
    json_t *(*call)(struct sw_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} sw_io_t;

static int sw_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *sw_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static const char *sw_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *sw_seam(sw_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        if (args) json_free(args);
        char msg[160];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return sw_raise("RuntimeError", msg);
    }
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static unsigned sw_cp_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned cp;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *p += 2; return cp;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p += 3; return cp;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x07) << 18) | ((unsigned)(s[1] & 0x3F) << 12) |
             ((unsigned)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *p += 4; return cp;
    }
    *p += 1; return 0xFFFD;
}

static int sw_isspace_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F)) return 1;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    return cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

static char *sw_strip_dup(const char *s) {
    if (!s) s = "";
    const char *start = s;
    while (*start) {
        const char *n = start;
        unsigned cp = sw_cp_next(&n);
        if (!sw_isspace_cp(cp)) break;
        start = n;
    }
    const char *end = s + strlen(s);
    while (end > start) {
        const char *p = start;
        const char *last = p;
        unsigned cp = 0;
        while (p < end) { last = p; cp = sw_cp_next(&p); }
        if (!sw_isspace_cp(cp)) break;
        end = last;
    }
    size_t len = (size_t)(end - start);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

/* int(v) — Python rules; returns a number node or a raise spec. */
static json_t *sw_py_int(const json_t *v) {
    if (v && v->type == JSON_NUMBER)
        return json_number((double)(long long)v->num_val);
    if (v && v->type == JSON_BOOL)
        return json_number(v->bool_val ? 1.0 : 0.0);
    if (v && v->type == JSON_STRING) {
        char *s = sw_strip_dup(v->str_val);
        const char *p = s;
        int neg = 0;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        const char *digits = p;
        int ndigit = 0, prev_digit = 0, ok = 1;
        for (; *p; p++) {
            if (*p >= '0' && *p <= '9') { ndigit++; prev_digit = 1; }
            else if (*p == '_') {
                if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) { ok = 0; break; }
                prev_digit = 0;
            } else { ok = 0; break; }
        }
        if (!ok || ndigit == 0) {
            char *msg = malloc(strlen(v->str_val) + 80);
            snprintf(msg, strlen(v->str_val) + 80,
                "invalid literal for int() with base 10: '%s'", v->str_val);
            json_t *e = sw_raise("ValueError", msg);
            free(msg);
            free(s);
            return e;
        }
        char *clean = malloc(strlen(digits) + 1);
        size_t k = 0;
        for (const char *q = digits; *q; q++)
            if (*q != '_') clean[k++] = *q;
        clean[k] = '\0';
        long long val = strtoll(clean, NULL, 10);
        free(clean);
        free(s);
        return json_number((double)(neg ? -val : val));
    }
    char msg[160];
    snprintf(msg, sizeof msg,
        "int() argument must be a string, a bytes-like object or a real number, not '%s'",
        sw_typename(v));
    return sw_raise("TypeError", msg);
}

static char *sw_path_norm(const char *s) {
    size_t n = strlen(s);
    char *tmp = malloc(n + 1);
    if (!tmp) return NULL;
    for (size_t i = 0; i < n; i++) tmp[i] = (s[i] == '\\') ? '/' : s[i];
    tmp[n] = '\0';
    size_t lead = 0;
    while (tmp[lead] == '/') lead++;
    char *out = malloc(n + 2);
    if (!out) { free(tmp); return NULL; }
    size_t k = 0;
    for (size_t i = 0; i < lead; i++) out[k++] = '/';
    char *p = tmp + lead;
    int have_comp = 0;
    while (*p) {
        char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        if (len == 0 || (len == 1 && p[0] == '.')) {
            /* drop */
        } else {
            if (have_comp) out[k++] = '/';
            memcpy(out + k, p, len);
            k += len;
            have_comp = 1;
        }
        if (!slash) break;
        p = slash + 1;
    }
    out[k] = '\0';
    free(tmp);
    return out;
}

static char *sw_path_join(const char *home, const char *rel) {
    char *norm = sw_path_norm(home);
    if (!norm) return NULL;
    if (!norm[0] || strcmp(norm, ".") == 0) {
        free(norm);
        return strdup(rel);
    }
    size_t hn = strlen(norm), rn = strlen(rel);
    int need_sep = norm[hn - 1] != '/';
    char *out = malloc(hn + rn + 2);
    if (!out) { free(norm); return NULL; }
    memcpy(out, norm, hn);
    size_t k = hn;
    if (need_sep) out[k++] = '/';
    memcpy(out + k, rel, rn + 1);
    free(norm);
    return out;
}

/* PoP: get_loop_tick_socket_path @ gateway/shutdown_watchdog.py:get_loop_tick_socket_path */
json_t *get_loop_tick_socket_path(sw_io_t *io, json_t *in) {
    /* pid = int(pid if pid is not None else os.getpid()) */
    json_t *pidv = json_obj_get(in, "pid");
    json_t *pid_src;
    int pid_owned = 0;
    if (!pidv || pidv->type == JSON_NULL) {
        pid_src = sw_seam(io, "os.getpid", NULL);
        pid_owned = 1;
        if (sw_is_raise(pid_src)) return pid_src;
    } else {
        pid_src = pidv;
    }
    json_t *pid_int = sw_py_int(pid_src);
    if (pid_owned) json_free(pid_src);
    if (sw_is_raise(pid_int)) return pid_int;
    /* _home(home): home if home is not None else _process_hermes_home() */
    json_t *home = json_obj_get(in, "home");
    int home_owned = 0;
    if (!home || home->type == JSON_NULL) {
        home = sw_seam(io, "_process_hermes_home", NULL);
        home_owned = 1;
        if (sw_is_raise(home)) { json_free(pid_int); return home; }
    }
    if (!home || home->type != JSON_STRING) {
        /* live: _home(home) / "state" — the / operator on a non-Path home
         * raises TypeError (unsupported operand), NOT a Path() TypeError. */
        char m[160];
        snprintf(m, sizeof m, "unsupported operand type(s) for /: '%s' and 'str'", sw_typename(home));
        if (home_owned) json_free(home);
        json_free(pid_int);
        return sw_raise("TypeError", m);
    }
    char tail[96];
    snprintf(tail, sizeof tail, "state/gateway.loop-tick.%lld.sock", (long long)pid_int->num_val);
    char *joined = sw_path_join(home->str_val, tail);
    json_t *out = json_string(joined ? joined : "");
    free(joined);
    if (home_owned) json_free(home);
    json_free(pid_int);
    return out;
}
