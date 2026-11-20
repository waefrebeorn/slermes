/*
 * web_profiles_flow2.c — FLOW port of the UNCLAIMED remainder of
 * hermes_cli/web_routers/profiles.py (LIVE authority). Self-contained: libjson
 * only. Every function is a static wp2_*; the harness (#include
 * "cli/web_profiles_flow2.c") wires ONE universal seam to scripted results and
 * records the ordered trace.
 *
 * SCOPE (CRLF-aware PoP grep at authoring time): 17 of the module's 67 AST defs
 * are claimed by profiles.c / profiles_gaps.c / web_server.c /
 * web_server_wrappers.c / ws_revive2.c / ws_revive4.c. This file claims a
 * coherent set of the UNCLAIMED pure/leaf helpers below (each PoP-annotated on
 * the line immediately before its definition). Claimed siblings (_merge_by_id,
 * _profile_to_dict, _write_profile_model, ...) are NOT re-ported.
 *
 * SEAM DOCTRINE: call(ctx,name,args) returns a NEW json_t* (NULL == None). A
 * raising live call is scripted {"__raise__":"<Class>","msg":...}; the cores
 * propagate it exactly where the live try/except lets it escape. A MISSING key
 * fail-closes (harness delivers RuntimeError); a present JSON null is a valid
 * None. A JSON array script is one result PER CALL (last repeats).
 *
 * FULL BUILTIN EXCEPTION HIERARCHY: wp2_exc_parent mirrors py3.14's parent
 * chain. wp2_caught walks it, so an `except Exception` (wp2_best_effort) can
 * never swallow the BaseException trio or BaseExceptionGroup; ExceptionGroup
 * (an Exception subclass via multiple inheritance) IS caught.
 *
 * Ownership: seam results / returned json_t* are malloc'd (caller frees);
 * inputs are borrowed. Nothing here mutates its input — observable state
 * changes are returned as fresh json_t* trees.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* ── universal seam ── */
typedef struct wp2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} wp2_seams_t;

static json_t *wp2_seam(const wp2_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}

/* ── JSON value helpers ── */
static bool wp2_jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static const char *wp2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool wp2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool wp2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool wp2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
/* Python dict.get(): NULL for a MISSING key; a present null is a JSON_NULL node. */
static const json_t *wp2_get(const json_t *obj, const char *key, const json_t *def) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return v ? v : def;
}
static json_t *wp2_copy(const json_t *v) { return v ? json_copy(v) : json_null(); }

#include <stdarg.h>
static char *wp2_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = (char *)malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

/* ── ASCII whitespace / strip ── */
static int wp2_isspace_ascii(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
/* Returns a malloc'd trimmed copy of s (Python str.strip over ASCII whitespace). */
static char *wp2_strip(const char *s) {
    if (!s) return strdup("");
    const char *a = s, *b = s + strlen(s);
    while (a < b && wp2_isspace_ascii(*a)) a++;
    while (b > a && wp2_isspace_ascii(*(b - 1))) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* ── exception model (FULL builtin parent hierarchy, py3.14) ── */
static const char *wp2_raise_class(const json_t *r) {
    if (wp2_is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool wp2_is_raise(const json_t *r) { return wp2_raise_class(r) != NULL; }

static json_t *wp2_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool wp2_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
static const char *wp2_exc_parent(const char *k) {
    if (!strcmp(k, "BaseException")) return NULL;
    if (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
        !strcmp(k, "GeneratorExit")) return "BaseException";
    if (!strcmp(k, "Exception")) return "BaseException";
    static const char *const EXC_CHILDREN[] = {
        "StopIteration", "StopAsyncIteration", "ArithmeticError", "AssertionError",
        "AttributeError", "BufferError", "EOFError", "ImportError", "LookupError",
        "MemoryError", "NameError", "OSError", "ReferenceError", "RuntimeError",
        "SyntaxError", "SystemError", "TypeError", "ValueError", "Warning", NULL
    };
    if (wp2_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (wp2_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (wp2_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (wp2_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (wp2_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (wp2_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = { "NotImplementedError", "RecursionError",
        "PythonFinalizationError", NULL };
    if (wp2_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (wp2_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (wp2_in_names(k, WARNING_CHILDREN)) return "Warning";
    static const char *const NAME_CHILDREN[] = { "UnboundLocalError", NULL };
    if (wp2_in_names(k, NAME_CHILDREN)) return "NameError";
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    return "Exception";
}
static bool wp2_caught(const json_t *r, const char *cls) {
    const char *k = wp2_raise_class(r);
    /* ExceptionGroup is a subclass of Exception by multiple inheritance. */
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = wp2_exc_parent(k);
    }
    return false;
}
static bool wp2_is_exception(const json_t *r) { return wp2_caught(r, "Exception"); }

/* ── arg builders ── */
static void wp2_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}

/* ═══════════════════════ WP2 FLOW 1: _recency ═══════════════════════ */
/* PoP: wp2_recency @ hermes_cli/web_routers/profiles.py:_recency */
static json_t *wp2_recency(const json_t *s) {
    const json_t *a = wp2_get(s, "last_active", NULL);
    if (wp2_jtruth(a)) return json_copy(a);
    const json_t *b = wp2_get(s, "started_at", NULL);
    if (wp2_jtruth(b)) return json_copy(b);
    return json_number(0);
}

/* ═══════════════════════ WP2 FLOW 2: _csv_list ═══════════════════════ */
/* PoP: wp2_csv_list @ hermes_cli/web_routers/profiles.py:_csv_list */
static json_t *wp2_csv_list(const json_t *value) {
    json_t *out = json_array();
    const char *s = wp2_jstr(value);
    if (!s) return out;                 /* (value or "") -> "" */
    const char *p = s;
    for (;;) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        char *tok = (char *)malloc(len + 1);
        memcpy(tok, p, len);
        tok[len] = '\0';
        char *t = wp2_strip(tok);
        free(tok);
        if (*t) json_append(out, json_string(t));
        free(t);
        if (!comma) break;
        p = comma + 1;
    }
    return out;
}

/* ═══════════════════════ WP2 FLOW 3: _slice_has_rows ═══════════════════════ */
/* PoP: wp2_slice_has_rows @ hermes_cli/web_routers/profiles.py:_slice_has_rows */
static json_t *wp2_slice_has_rows(const json_t *slices) {
    static const char *const KEYS[] = { "recents", "cron", "messaging", NULL };
    bool any = false;
    for (size_t i = 0; KEYS[i]; i++) {
        if (wp2_jtruth(wp2_get(slices, KEYS[i], NULL))) { any = true; break; }
    }
    return json_bool(any);
}

/* ═══════════════════════ WP2 FLOW 4: _failed_load_slice ═══════════════════════ */
/* PoP: wp2_failed_load_slice @ hermes_cli/web_routers/profiles.py:_failed_load_slice */
static json_t *wp2_failed_load_slice(const json_t *errors, const json_t *extra) {
    json_t *out = json_object();
    json_set(out, "failed", json_bool(true));
    json_set(out, "retry", json_bool(true));
    json_t *arr = json_array();
    if (wp2_is_arr(errors)) {
        for (size_t i = 0; i < errors->c.count; i++) {
            const json_t *e = json_get(errors, i);
            json_append(arr, wp2_is_obj(e) ? json_copy(e) : json_object());
        }
    }
    json_set(out, "errors", arr);
    if (wp2_is_obj(extra)) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(out, extra->c.keys[i], json_copy(extra->c.items[i]));
    }
    return out;
}

/* ═══════════════════════ WP2 FLOW 5: _profiles_failed ═══════════════════════ */
/* PoP: wp2_profiles_failed @ hermes_cli/web_routers/profiles.py:_profiles_failed */
static json_t *wp2_profiles_failed(const json_t *errors) {
    json_t *out = json_object();
    if (!wp2_is_arr(errors)) return out;
    for (size_t i = 0; i < errors->c.count; i++) {
        const json_t *e = json_get(errors, i);
        if (!wp2_is_obj(e)) continue;
        const json_t *p = wp2_get(e, "profile", NULL);
        if (!wp2_jtruth(p)) continue;          /* if err.get("profile") */
        const char *key = wp2_jstr(p);
        if (!key) continue;                    /* non-string key: out of ASCII domain */
        json_t *v = json_object();
        json_set(v, "failed", json_bool(true));
        json_set(v, "retry", json_bool(true));
        json_t *dv = json_string("");           /* err.get("error", "") */
        const json_t *ev = wp2_get(e, "error", dv);
        json_set(v, "error", json_copy(ev));
        json_free(dv);
        json_set(out, key, v);
    }
    return out;
}

/* ═══════════════════════ WP2 FLOW 6: _no_store_file ═══════════════════════ */
/* PoP: wp2_no_store_file @ hermes_cli/web_routers/profiles.py:_no_store_file */
static json_t *wp2_no_store_file(const char *name, const json_t *errors) {
    if (wp2_is_null(errors)) return json_null();   /* errors is None -> no-op */
    json_t *out = wp2_is_arr(errors) ? json_copy(errors) : json_array();
    json_t *e = json_object();
    json_set(e, "profile", name ? json_string(name) : json_null());
    json_set(e, "error", json_string(
        "the sidebar reads a profile's session store file, and this profile's store has no file"));
    json_append(out, e);
    return out;
}

/* ═══════════════════════ WP2 FLOW 7: _tag_rows ═══════════════════════ */
/* PoP: wp2_tag_rows @ hermes_cli/web_routers/profiles.py:_tag_rows */
static json_t *wp2_tag_rows(const json_t *rows, const char *name, double now) {
    json_t *out = wp2_is_arr(rows) ? json_copy(rows) : json_array();
    bool is_default = name && !strcmp(name, "default");
    for (size_t i = 0; i < out->c.count; i++) {
        json_t *s = json_get(out, i);
        if (!wp2_is_obj(s)) continue;
        json_set(s, "profile", name ? json_string(name) : json_null());
        json_set(s, "is_default_profile", json_bool(is_default));
        const json_t *ended = wp2_get(s, "ended_at", NULL);
        bool ended_none = wp2_is_null(ended);          /* get() is None: missing OR null */
        const json_t *chosen = wp2_get(s, "last_active", NULL);   /* presence, not truthiness */
        if (!chosen) chosen = wp2_get(s, "started_at", NULL);
        double last = (chosen && chosen->type == JSON_NUMBER) ? chosen->num_val : 0.0;
        bool active = ended_none && ((now - last) < 300.0);
        json_set(s, "is_active", json_bool(active));
        json_set(s, "archived", json_bool(wp2_jtruth(wp2_get(s, "archived", NULL))));
        json_set(s, "pinned", json_bool(wp2_jtruth(wp2_get(s, "pinned", NULL))));
    }
    return out;
}

/* ═══════════════════════ WP2 FLOW 8: _pinned_window ═══════════════════════ */
/* PoP: wp2_pinned_window @ hermes_cli/web_routers/profiles.py:_pinned_window */
static json_t *wp2_pinned_window(const json_t *rows, long offset, long cap) {
    json_t *out = json_array();
    long n = wp2_is_arr(rows) ? (long)rows->c.count : 0L;
    long start = offset < 0 ? 0 : offset;
    long end = offset + cap;
    if (end < 0) end = 0;
    /* window = rows[offset:offset+cap] */
    for (long i = start; i < end && i < n; i++)
        json_append(out, json_copy(json_get(rows, (size_t)i)));
    /* pinned rows past the window (rows are distinct dicts in this domain, so
     * the live id()-dedup against the window is a no-op). */
    if (n > end) {
        for (long i = end; i < n; i++) {
            const json_t *s = json_get(rows, (size_t)i);
            if (wp2_is_obj(s) && wp2_jtruth(wp2_get(s, "pinned", NULL)))
                json_append(out, json_copy(s));
        }
    }
    return out;
}

/* ═══════════════════════ WP2 FLOW 9: _linux_terminal_commands ═══════════════════════ */
/* PoP: wp2_linux_terminal_commands @ hermes_cli/web_routers/profiles.py:_linux_terminal_commands */
static json_t *wp2_linux_terminal_commands(const char *command) {
    /* (exe, flag): 0 => None, 1 => truthy flag, 2 => "" (falsy) */
    static const struct { const char *exe; int kind; const char *flag; } TERMS[] = {
        { "x-terminal-emulator", 1, "-e" }, { "gnome-terminal", 1, "--" },
        { "konsole", 1, "-e" }, { "xfce4-terminal", 0, NULL },
        { "mate-terminal", 0, NULL }, { "lxterminal", 0, NULL },
        { "tilix", 1, "-e" }, { "alacritty", 1, "-e" },
        { "kitty", 2, NULL }, { "xterm", 1, "-e" },
    };
    const char *cmd = command ? command : "";
    size_t qlen = strlen(cmd) + 32;
    char *quoted = (char *)malloc(qlen);
    snprintf(quoted, qlen, "sh -lc '%s'", cmd);
    json_t *out = json_array();
    for (size_t i = 0; i < sizeof(TERMS) / sizeof(TERMS[0]); i++) {
        json_t *args = json_array();
        json_append(args, json_string(TERMS[i].exe));
        if (TERMS[i].kind == 0) {                  /* flag is None */
            json_append(args, json_string("-e"));
            json_append(args, json_string(quoted));
        } else {
            if (TERMS[i].kind == 1) json_append(args, json_string(TERMS[i].flag));
            json_append(args, json_string("sh"));
            json_append(args, json_string("-lc"));
            json_append(args, json_string(cmd));
        }
        json_t *pair = json_array();
        json_append(pair, json_string(TERMS[i].exe));
        json_append(pair, args);
        json_append(out, pair);
    }
    free(quoted);
    return out;
}

/* ═══════════════════════ WP2 FLOW 10: _profile_state_db ═══════════════════════ */
/* PoP: wp2_profile_state_db @ hermes_cli/web_routers/profiles.py:_profile_state_db */
static json_t *wp2_profile_state_db(const wp2_seams_t *sp, const char *home) {
    json_t *a = json_object();
    wp2_aset_s(a, "home", home);
    json_t *r = wp2_seam(sp, "store_location", a);
    json_free(a);
    return r;
}

/* ═══════════════════════ WP2 FLOW 11: _corrupt_profile_stores ═══════════════════════ */
/* PoP: wp2_corrupt_profile_stores @ hermes_cli/web_routers/profiles.py:_corrupt_profile_stores */
static json_t *wp2_corrupt_profile_stores(const wp2_seams_t *sp, const json_t *targets) {
    json_t *out = json_object();
    if (!wp2_is_arr(targets)) return out;
    for (size_t i = 0; i < targets->c.count; i++) {
        const json_t *t = json_get(targets, i);
        if (!wp2_is_arr(t) || t->c.count < 2) continue;
        const char *name = wp2_jstr(json_get(t, 0));
        if (!name) continue;
        json_t *a = json_object();
        const char *hs = wp2_jstr(json_get(t, 1));
        wp2_aset_s(a, "home", hs ? hs : "");
        json_t *r = wp2_seam(sp, "storage_health", a);
        json_free(a);
        if (wp2_is_raise(r)) { json_free(out); return r; }
        const char *state = NULL;
        if (wp2_is_obj(r)) state = wp2_jstr(json_obj_get(r, "state"));
        if (!state) state = wp2_jstr(r);
        if (state && !strcmp(state, "corrupt")) json_set(out, name, json_string("corrupt"));
        if (r) json_free(r);
    }
    return out;
}

/* ═══════════════════════ WP2 FLOW 12: _retryable_profile_errors ═══════════════════════ */
/* PoP: wp2_retryable_profile_errors @ hermes_cli/web_routers/profiles.py:_retryable_profile_errors */
static json_t *wp2_retryable_profile_errors(const wp2_seams_t *sp,
                                            const json_t *errors, const json_t *scanned) {
    json_t *corrupt = wp2_corrupt_profile_stores(sp, scanned);
    if (wp2_is_raise(corrupt)) return corrupt;
    json_t *out = json_array();
    if (wp2_is_arr(errors)) {
        for (size_t i = 0; i < errors->c.count; i++) {
            const json_t *e = json_get(errors, i);
            if (!wp2_is_obj(e)) continue;
            const char *p = wp2_jstr(wp2_get(e, "profile", NULL));
            bool in = p && wp2_is_obj(corrupt) && json_obj_get(corrupt, p) ? true : false;
            if (!in) json_append(out, json_copy(e));
        }
    }
    json_free(corrupt);
    return out;
}

/* ═══════════════════════ WP2 FLOW 13: _scoped_profile_targets ═══════════════════════ */
/* PoP: wp2_scoped_profile_targets @ hermes_cli/web_routers/profiles.py:_scoped_profile_targets */
static json_t *wp2_scoped_profile_targets(const wp2_seams_t *sp,
                                          const char *log_label, const char *profile) {
    const char *raw = (profile && *profile) ? profile : "all";
    char *sc = wp2_strip(raw);
    if (!*sc) { free(sc); sc = strdup("all"); }
    json_t *a = json_object();
    wp2_aset_s(a, "log_label", log_label);
    json_t *targets = wp2_seam(sp, "profile_targets", a);
    json_free(a);
    if (wp2_is_raise(targets)) { free(sc); return targets; }
    json_t *out = json_array();
    bool all = !strcmp(sc, "all");
    if (wp2_is_arr(targets)) {
        for (size_t i = 0; i < targets->c.count; i++) {
            const json_t *t = json_get(targets, i);
            if (!wp2_is_arr(t) || t->c.count < 1) continue;
            const char *nm = wp2_jstr(json_get(t, 0));
            if (all || (nm && !strcmp(nm, sc))) json_append(out, json_copy(t));
        }
    }
    if (targets) json_free(targets);
    free(sc);
    return out;
}

/* ═══════════════════════ WP2 FLOW 14: _best_effort ═══════════════════════ */
/* PoP: wp2_best_effort @ hermes_cli/web_routers/profiles.py:_best_effort */
static json_t *wp2_best_effort(const wp2_seams_t *sp, const char *log_msg,
                               const json_t *args, const json_t *default_val) {
    json_t *a = json_object();
    json_t *r = wp2_seam(sp, "fn", a);
    json_free(a);
    if (wp2_is_raise(r)) {
        if (wp2_is_exception(r)) {               /* except Exception */
            json_t *la = json_object();
            wp2_aset_s(la, "log_msg", log_msg);
            json_set(la, "args", args ? json_copy(args) : json_array());
            json_t *ignored = wp2_seam(sp, "log_exception", la);
            json_free(la);
            if (ignored) json_free(ignored);
            json_free(r);
            return wp2_copy(default_val);         /* return default */
        }
        return r;                                 /* BaseException passthrough */
    }
    return r ? r : json_null();
}
