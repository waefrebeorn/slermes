/*
 * web_profiles_flow3.c — FLOW port of the UNCLAIMED remainder of
 * hermes_cli/web_routers/profiles.py (LIVE authority): async endpoints and the
 * sidebar profile-cache writers. Complements web_profiles_flow2.c (the 14
 * unclaimed pure/leaf helpers). Self-contained: libjson only. Every function is
 * a static wp3_*; the harness (#include "cli/web_profiles_flow3.c") wires ONE
 * universal seam to scripted results and records the ordered trace.
 *
 * SCOPE (CRLF-aware PoP grep at authoring time): 17 of the module's 67 AST defs
 * are claimed by profiles.c / profiles_gaps.c / web_server.c /
 * web_server_wrappers.c / ws_revive2.c / ws_revive4.c; a further 14 (the pure
 * helpers) are claimed by web_profiles_flow2.c. This file PoP-claims a coherent
 * set of the STILL-UNCLAIMED async endpoints + sidebar-cache writers below.
 * Claimed siblings (_stat_fingerprint, _sidebar_profile_cache_get,
 * _sidebar_singleflight_cache, _write_profile_model, _profile_to_dict, ...) are
 * NOT re-ported; they are driven through the seam.
 *
 * SEAM DOCTRINE: call(ctx,name,args) returns a NEW json_t* (NULL == None). A
 * raising live call is scripted {"__raise__":"<Class>","msg":...} (HTTPException
 * carries status_code+detail); the cores propagate/map it exactly where the
 * live try/except lets it. A MISSING key fail-closes (harness delivers
 * RuntimeError); a present JSON null is a valid None. A JSON array script is
 * one result PER CALL (last repeats).
 *
 * FULL BUILTIN EXCEPTION HIERARCHY: wp3_exc_parent mirrors py3.14's parent
 * chain. wp3_caught walks it, so `except Exception` never swallows the
 * BaseException trio or BaseExceptionGroup; ExceptionGroup (an Exception
 * subclass via multiple inheritance) IS caught. Custom exception names fall
 * through to Exception (matching a normal Exception subclass).
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
#include <stdarg.h>
#include "hermes_json.h"

/* ── universal seam ── */
typedef struct wp3_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} wp3_seams_t;

static json_t *wp3_seam(const wp3_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}

/* ── JSON value helpers ── */
static bool wp3_jtruth(const json_t *v) {
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
static const char *wp3_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool wp3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool wp3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool wp3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static const json_t *wp3_get(const json_t *obj, const char *key, const json_t *def) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return v ? v : def;
}
static json_t *wp3_copy(const json_t *v) { return v ? json_copy(v) : json_null(); }
static json_t *wp3_args(void) { return json_object(); }
static void wp3_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static char *wp3_xsf(const char *fmt, ...) {
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
static int wp3_isspace_ascii(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static char *wp3_strip(const char *s) {
    if (!s) return strdup("");
    const char *a = s, *b = s + strlen(s);
    while (a < b && wp3_isspace_ascii(*a)) a++;
    while (b > a && wp3_isspace_ascii(*(b - 1))) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}
static bool wp3_equal(const json_t *a, const json_t *b) {
    if (a == b) return true;
    if (!a || !b || a->type != b->type) return false;
    char *sa = json_serialize(a), *sb = json_serialize(b);
    bool eq = sa && sb && !strcmp(sa, sb);
    free(sa); free(sb);
    return eq;
}

/* ── exception model (FULL builtin parent hierarchy, py3.14) ── */
static const char *wp3_raise_class(const json_t *r) {
    if (wp3_is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool wp3_is_raise(const json_t *r) { return wp3_raise_class(r) != NULL; }
static json_t *wp3_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *wp3_http_exc(long status, const char *detail) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string("HTTPException"));
    json_set(r, "status_code", json_number((double)status));
    json_set(r, "detail", json_string(detail ? detail : ""));
    return r;
}
static const char *wp3_exc_msg(const json_t *r) {
    const char *m = wp3_jstr(json_obj_get(r, "msg"));
    return m ? m : wp3_jstr(json_obj_get(r, "detail"));
}
static bool wp3_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
static const char *wp3_exc_parent(const char *k) {
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
    if (wp3_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (wp3_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (wp3_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (wp3_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (wp3_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (wp3_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = { "NotImplementedError", "RecursionError",
        "PythonFinalizationError", NULL };
    if (wp3_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (wp3_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (wp3_in_names(k, WARNING_CHILDREN)) return "Warning";
    if (!strcmp(k, "UnboundLocalError")) return "NameError";
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    return "Exception";  /* custom exception names are Exception subclasses */
}
static bool wp3_caught(const json_t *r, const char *cls) {
    const char *k = wp3_raise_class(r);
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = wp3_exc_parent(k);
    }
    return false;
}
static bool wp3_is_exception(const json_t *r) { return wp3_caught(r, "Exception"); }

/* Walk a JSON array of class-name strings; NULL array = empty. */
static bool wp3_caught_any(const json_t *r, const json_t *names) {
    if (!wp3_is_arr(names)) return false;
    for (size_t i = 0; i < names->c.count; i++) {
        const char *n = wp3_jstr(json_get(names, i));
        if (n && wp3_caught(r, n)) return true;
    }
    return false;
}

/* Mirror of the _profile_errors context manager: map a raise descriptor to an
 * HTTPException 404/400/500, or pass it through unchanged (HTTPException /
 * ProfileIdentitySettlementPending / BaseException family). */
static json_t *wp3_map_errors(const wp3_seams_t *sp, const json_t *exc,
                              const char *log_msg, const json_t *args,
                              const json_t *not_found, const json_t *bad_request) {
    (void)sp;
    if (!wp3_is_raise(exc)) return NULL;
    if (wp3_caught(exc, "HTTPException")) return json_copy(exc);
    if (wp3_caught_any(exc, not_found)) return wp3_http_exc(404, wp3_exc_msg(exc));
    if (wp3_caught_any(exc, bad_request)) return wp3_http_exc(400, wp3_exc_msg(exc));
    if (wp3_caught(exc, "ProfileIdentitySettlementPending")) return json_copy(exc);
    if (wp3_is_exception(exc)) {
        json_t *a = wp3_args();
        wp3_aset_s(a, "log_msg", log_msg);
        json_set(a, "args", args ? json_copy(args) : json_array());
        /* the live _log.exception is itself a seam */
        json_t *ign = wp3_seam(sp, "log_exception", a);
        json_free(ign);
        json_free(a);
        return wp3_http_exc(500, wp3_exc_msg(exc));
    }
    return json_copy(exc);  /* BaseException trio / groups pass through */
}

/* ── LRU cache helpers for the sidebar profile-cache writers ── */
static bool wp3_cache_key_match(const json_t *pair, const json_t *key) {
    return wp3_is_arr(pair) && json_len(pair) > 0 && wp3_equal(json_get(pair, 0), key);
}

/* ═══════════════════════ WP3 FLOW 1: _profile_targets ═══════════════════════ */
/* PoP: wp3_profile_targets @ hermes_cli/web_routers/profiles.py:_profile_targets */
static json_t *wp3_profile_targets(const wp3_seams_t *sp, const char *log_label) {
    json_t *a = wp3_args();
    wp3_aset_s(a, "log_label", log_label);
    json_t *r = wp3_seam(sp, "profile_targets", a);
    json_free(a);
    json_t *targets;
    if (wp3_is_raise(r)) {
        if (wp3_is_exception(r)) {
            json_t *la = wp3_args();
            wp3_aset_s(la, "log_msg", "%s: profile enumeration failed");
            json_t *arr = json_array();
            json_append(arr, json_string(log_label));
            json_set(la, "args", arr);
            json_t *ign = wp3_seam(sp, "log_exception", la);
            json_free(ign); json_free(la);
            targets = json_array();
            json_free(r);
        } else {
            return r;  /* BaseException family propagates */
        }
    } else {
        targets = wp3_is_arr(r) ? json_copy(r) : json_array();
        json_free(r);
    }
    if (json_len(targets) == 0) {
        json_t *da = wp3_args();
        wp3_aset_s(da, "name", "default");
        json_t *home = wp3_seam(sp, "get_profile_dir", da);
        json_free(da);
        if (wp3_is_raise(home)) { json_free(targets); return home; }
        json_t *pair = json_array();
        json_append(pair, json_string("default"));
        json_append(pair, home ? home : json_null());
        json_append(targets, pair);
    }
    json_t *sa = wp3_args();
    json_set(sa, "targets", json_copy(targets));
    json_t *out = wp3_seam(sp, "scope_to_launch_profile", sa);
    json_free(sa); json_free(targets);
    return out;
}

/* ═══════════════════════ WP3 FLOW 2: _sidebar_profile_cache_put ═════════════ */
/* PoP: wp3_sidebar_cache_put @ hermes_cli/web_routers/profiles.py:_sidebar_profile_cache_put */
static json_t *wp3_sidebar_cache_put(const wp3_seams_t *sp, const json_t *key,
                                     const json_t *value, const json_t *cache, long max_entries) {
    (void)sp;
    json_t *out = wp3_is_arr(cache) ? json_copy(cache) : json_array();
    if (!wp3_is_arr(key) || json_len(key) < 2) return out;
    const json_t *db_path = json_get(key, 0);
    const json_t *fp = json_get(key, 1);
    /* Eagerly drop every older parameter variant for this db_path. */
    for (size_t i = 0; i < json_len(out); ) {
        json_t *pair = json_get(out, i);
        const json_t *pk = wp3_is_arr(pair) && json_len(pair) > 0 ? json_get(pair, 0) : NULL;
        if (wp3_is_arr(pk) && json_len(pk) > 1 &&
            wp3_equal(json_get(pk, 0), db_path) && !wp3_equal(json_get(pk, 1), fp)) {
            json_array_remove(out, i);
        } else {
            i++;
        }
    }
    /* Upsert key -> deepcopy(value), then move_to_end. */
    bool found = false;
    for (size_t i = 0; i < json_len(out); i++) {
        json_t *pair = json_get(out, i);
        if (wp3_cache_key_match(pair, key)) {
            json_array_set(pair, 1, json_copy(value));
            json_t *moved = json_get(out, i);
            json_array_remove(out, i);
            json_append(out, json_copy(moved));
            found = true;
            break;
        }
    }
    if (!found) {
        json_t *pair = json_array();
        json_append(pair, json_copy(key));
        json_append(pair, json_copy(value));
        json_append(out, pair);
    }
    while ((long)json_len(out) > max_entries) json_array_remove(out, 0);
    return out;
}

/* ═══════════════════════ WP3 FLOW 3: _sidebar_profile_cache_clear ═══════════ */
/* PoP: wp3_sidebar_cache_clear @ hermes_cli/web_routers/profiles.py:_sidebar_profile_cache_clear */
static json_t *wp3_sidebar_cache_clear(const wp3_seams_t *sp, const json_t *cache) {
    (void)sp; (void)cache;
    return json_array();
}

/* ═══════════════════════ WP3 FLOW 4: _profile_errors ═════════════════════════ */
/* PoP: wp3_profile_errors @ hermes_cli/web_routers/profiles.py:_profile_errors */
static json_t *wp3_profile_errors(const wp3_seams_t *sp, const json_t *exc,
                                  const char *log_msg, const json_t *args,
                                  const json_t *not_found, const json_t *bad_request) {
    return wp3_map_errors(sp, exc, log_msg, args, not_found, bad_request);
}

/* ═══════════════════════ WP3 FLOW 5: _read_off_loop ══════════════════════════ */
/* PoP: wp3_read_off_loop @ hermes_cli/web_routers/profiles.py:_read_off_loop */
static json_t *wp3_read_off_loop(const wp3_seams_t *sp, const char *label,
                                 const json_t *errors, const char *read_name) {
    json_t *a = wp3_args();
    json_t *r = wp3_seam(sp, read_name, a);
    json_free(a);
    if (wp3_is_raise(r)) {
        if (wp3_caught_any(r, errors)) {
            char *msg = wp3_xsf("Could not read %s: %s", label, wp3_exc_msg(r) ? wp3_exc_msg(r) : "");
            json_t *h = wp3_http_exc(500, msg);
            free(msg); json_free(r);
            return h;
        }
        return r;  /* non-listed exception propagates */
    }
    return r;
}

/* ═══════════════════════ WP3 FLOW 6: get_profile_setup_command ══════════════ */
/* PoP: wp3_get_profile_setup_command @ hermes_cli/web_routers/profiles.py:get_profile_setup_command */
static json_t *wp3_get_profile_setup_command(const wp3_seams_t *sp, const char *name) {
    json_t *a = wp3_args();
    wp3_aset_s(a, "name", name);
    json_t *r = wp3_seam(sp, "resolve_profile_dir", a);
    json_free(a);
    if (wp3_is_raise(r)) return r;
    json_free(r);
    json_t *out = json_object();
    wp3_aset_s(out, "command", !strcmp(name, "default") ? "hermes setup" : NULL);
    if (strcmp(name, "default")) {
        char *cmd = wp3_xsf("%s setup", name);
        wp3_aset_s(out, "command", cmd);
        free(cmd);
    }
    return out;
}

/* ═══════════════════════ WP3 FLOW 7: get_active_profile_endpoint ════════════ */
/* PoP: wp3_get_active_profile @ hermes_cli/web_routers/profiles.py:get_active_profile_endpoint */
static json_t *wp3_get_active_profile(const wp3_seams_t *sp) {
    json_t *a = wp3_args();
    json_t *r1 = wp3_seam(sp, "get_active_profile", a);
    if (wp3_is_raise(r1) && !wp3_is_exception(r1)) { json_free(a); return r1; }
    json_t *r2 = wp3_seam(sp, "get_active_profile_name", a);
    json_free(a);
    if (wp3_is_raise(r2) && !wp3_is_exception(r2)) { json_free(r1); return r2; }
    json_t *out = json_object();
    if (wp3_is_raise(r1)) wp3_aset_s(out, "active", "default");
    else wp3_aset_s(out, "active", wp3_jtruth(r1) ? wp3_jstr(r1) : "default");
    if (wp3_is_raise(r2)) wp3_aset_s(out, "current", "custom");
    else wp3_aset_s(out, "current", wp3_jtruth(r2) ? wp3_jstr(r2) : "default");
    json_free(r1); json_free(r2);
    return out;
}

/* ═══════════════════════ WP3 FLOW 8: set_active_profile_endpoint ════════════ */
/* PoP: wp3_set_active_profile @ hermes_cli/web_routers/profiles.py:set_active_profile_endpoint */
static json_t *wp3_set_active_profile(const wp3_seams_t *sp, const char *name) {
    json_t *a = wp3_args();
    wp3_aset_s(a, "name", name);
    json_t *r = wp3_seam(sp, "set_active_profile", a);
    json_free(a);
    if (wp3_is_raise(r)) {
        json_t *nf = json_array(); json_append(nf, json_string("FileNotFoundError"));
        json_t *br = json_array(); json_append(br, json_string("ValueError"));
        json_t *mapped = wp3_map_errors(sp, r, "POST /api/profiles/active failed", NULL, nf, br);
        json_free(br); json_free(nf); json_free(r);
        return mapped;
    }
    json_free(r);
    json_t *na = wp3_args();
    wp3_aset_s(na, "name", name);
    json_t *norm = wp3_seam(sp, "normalize_profile_name", na);
    json_free(na);
    if (wp3_is_raise(norm)) return norm;
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "active", norm ? norm : json_null());
    return out;
}

/* ═══════════════════════ WP3 FLOW 9: rename_profile_endpoint ════════════════ */
/* PoP: wp3_rename_profile @ hermes_cli/web_routers/profiles.py:rename_profile_endpoint */
static json_t *wp3_rename_profile(const wp3_seams_t *sp, const char *name, const char *new_name) {
    json_t *a = wp3_args();
    wp3_aset_s(a, "name", name);
    wp3_aset_s(a, "new_name", new_name);
    json_t *r = wp3_seam(sp, "rename_profile", a);
    json_free(a);
    if (wp3_is_raise(r)) {
        json_t *bad = json_array();
        json_append(bad, json_string("ValueError"));
        json_append(bad, json_string("FileExistsError"));
        json_t *eargs = json_array();
        json_append(eargs, json_string(name));
        json_t *nf = json_array(); json_append(nf, json_string("FileNotFoundError"));
        json_t *mapped = wp3_map_errors(sp, r, "PATCH /api/profiles/%s failed", eargs, nf, bad);
        json_free(nf); json_free(eargs); json_free(bad); json_free(r);
        return mapped;
    }
    /* path = str(result) */
    json_t *path = r;
    /* is_default = normalize(name) == "default"  (ValueError -> False) */
    json_t *na = wp3_args();
    wp3_aset_s(na, "name", name);
    json_t *n1 = wp3_seam(sp, "normalize_profile_name", na);
    json_free(na);
    bool is_default = false;
    if (!wp3_is_raise(n1)) {
        const char *s = wp3_jstr(n1);
        is_default = s && !strcmp(s, "default");
    } else if (wp3_caught(n1, "ValueError")) {
        /* swallowed -> is_default stays false */
    }
    json_free(n1);
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    if (is_default) {
        wp3_aset_s(out, "name", "default");
        char *t = wp3_strip(new_name);
        wp3_aset_s(out, "display_name", t);
        free(t);
        json_set(out, "path", wp3_copy(path));
    } else {
        json_t *nb = wp3_args();
        wp3_aset_s(nb, "name", new_name);
        json_t *n2 = wp3_seam(sp, "normalize_profile_name", nb);
        json_free(nb);
        if (wp3_is_raise(n2)) { json_free(out); json_free(path); return n2; }
        json_set(out, "name", n2 ? n2 : json_null());
        json_set(out, "path", wp3_copy(path));
    }
    json_free(path);
    return out;
}

/* ═══════════════════════ WP3 FLOW 10: delete_profile_endpoint ═══════════════ */
/* PoP: wp3_delete_profile @ hermes_cli/web_routers/profiles.py:delete_profile_endpoint */
static json_t *wp3_delete_profile(const wp3_seams_t *sp, const char *name) {
    json_t *a = wp3_args();
    wp3_aset_s(a, "name", name);
    json_t *r = wp3_seam(sp, "delete_profile", a);
    json_free(a);
    if (wp3_is_raise(r)) {
        /* _profile_errors passes ProfileIdentitySettlementPending through */
        json_t *eargs = json_array();
        json_append(eargs, json_string(name));
        json_t *nf = json_array(); json_append(nf, json_string("FileNotFoundError"));
        json_t *br = json_array(); json_append(br, json_string("ValueError"));
        json_t *mapped = wp3_map_errors(sp, r, "DELETE /api/profiles/%s failed", eargs, nf, br);
        json_free(br); json_free(nf); json_free(eargs); json_free(r);
        if (mapped && wp3_caught(mapped, "ProfileIdentitySettlementPending")) {
            json_t *out = json_object();
            json_set(out, "ok", json_bool(true));
            json_set(out, "path", wp3_copy(json_obj_get(mapped, "path")));
            json_set(out, "identity_settled", json_bool(false));
            json_set(out, "settlement_pending", json_bool(true));
            json_set(out, "retry_command", wp3_copy(json_obj_get(mapped, "retry_command")));
            json_free(mapped);
            return out;
        }
        return mapped;
    }
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "path", r ? r : json_null());
    return out;
}

/* ═══════════════════════ WP3 FLOW 11: get_profile_soul ═══════════════════════ */
/* PoP: wp3_get_profile_soul @ hermes_cli/web_routers/profiles.py:get_profile_soul */
static json_t *wp3_get_profile_soul(const wp3_seams_t *sp, const char *name) {
    json_t *out = json_object();
    json_t *a = wp3_args();
    wp3_aset_s(a, "name", name);
    json_t *d = wp3_seam(sp, "resolve_profile_dir", a);
    json_free(a);
    if (wp3_is_raise(d)) { json_free(out); return d; }
    json_free(d);
    /* exists probe (OSError -> 500 via _read_off_loop) */
    json_t *ea = wp3_args();
    wp3_aset_s(ea, "name", name);
    json_t *ex = wp3_seam(sp, "soul_exists", ea);
    json_free(ea);
    if (wp3_is_raise(ex)) {
        if (wp3_caught(ex, "OSError")) {
            char *msg = wp3_xsf("Could not read SOUL.md: %s", wp3_exc_msg(ex) ? wp3_exc_msg(ex) : "");
            json_t *h = wp3_http_exc(500, msg);
            free(msg); json_free(ex); json_free(out);
            return h;
        }
        json_free(out); return ex;
    }
    bool exists = wp3_jtruth(ex);
    json_free(ex);
    if (!exists) {
        wp3_aset_s(out, "content", "");
        json_set(out, "exists", json_bool(false));
        return out;
    }
    json_t *ra = wp3_args();
    wp3_aset_s(ra, "name", name);
    json_t *content = wp3_seam(sp, "soul_read_text", ra);
    json_free(ra);
    if (wp3_is_raise(content)) {
        if (wp3_caught(content, "OSError")) {
            char *msg = wp3_xsf("Could not read SOUL.md: %s", wp3_exc_msg(content) ? wp3_exc_msg(content) : "");
            json_t *h = wp3_http_exc(500, msg);
            free(msg); json_free(content); json_free(out);
            return h;
        }
        json_free(out); return content;
    }
    json_set(out, "content", content ? content : json_null());
    json_set(out, "exists", json_bool(true));
    return out;
}

/* ═══════════════════════ WP3 FLOW 12: update_profile_soul ═══════════════════ */
/* PoP: wp3_update_profile_soul @ hermes_cli/web_routers/profiles.py:update_profile_soul */
static json_t *wp3_update_profile_soul(const wp3_seams_t *sp, const char *name, const char *content) {
    json_t *pa = wp3_args();
    wp3_aset_s(pa, "name", name);
    json_t *rp = wp3_seam(sp, "resolve_profile_dir", pa);
    json_free(pa);
    if (wp3_is_raise(rp)) return rp;
    json_free(rp);
    json_t *a = wp3_args();
    wp3_aset_s(a, "name", name);
    wp3_aset_s(a, "content", content);
    json_t *r = wp3_seam(sp, "atomic_write_text", a);
    json_free(a);
    if (wp3_is_raise(r)) {
        if (wp3_caught(r, "OSError")) {
            json_t *la = wp3_args();
            wp3_aset_s(la, "log_msg", "PUT /api/profiles/%s/soul failed");
            json_t *arr = json_array(); json_append(arr, json_string(name));
            json_set(la, "args", arr);
            json_t *ign = wp3_seam(sp, "log_exception", la);
            json_free(ign); json_free(la);
            char *msg = wp3_xsf("Could not write SOUL.md: %s", wp3_exc_msg(r) ? wp3_exc_msg(r) : "");
            json_t *h = wp3_http_exc(500, msg);
            free(msg); json_free(r);
            return h;
        }
        return r;
    }
    json_free(r);
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    return out;
}

/* ═══════════════════════ WP3 FLOW 13: update_profile_model_endpoint ═════════ */
/* PoP: wp3_update_profile_model @ hermes_cli/web_routers/profiles.py:update_profile_model_endpoint */
static json_t *wp3_update_profile_model(const wp3_seams_t *sp, const char *name,
                                        const char *provider_raw, const char *model_raw) {
    json_t *a = wp3_args();
    wp3_aset_s(a, "name", name);
    json_t *d = wp3_seam(sp, "resolve_profile_dir", a);
    json_free(a);
    if (wp3_is_raise(d)) return d;
    json_free(d);
    char *provider = wp3_strip(provider_raw);
    char *model = wp3_strip(model_raw);
    if (!*provider || !*model) {
        json_t *h = wp3_http_exc(400, "provider and model are required");
        free(provider); free(model);
        return h;
    }
    json_t *wa = wp3_args();
    wp3_aset_s(wa, "name", name);
    wp3_aset_s(wa, "provider", provider);
    wp3_aset_s(wa, "model", model);
    json_t *r = wp3_seam(sp, "write_profile_model", wa);
    json_free(wa);
    if (wp3_is_raise(r)) {
        json_t *bad = json_array();
        json_append(bad, json_string("ModelAssignmentRejected"));
        json_t *eargs = json_array();
        json_append(eargs, json_string(name));
        json_t *mapped = wp3_map_errors(sp, r, "PUT /api/profiles/%s/model failed", eargs, NULL, bad);
        json_free(eargs); json_free(bad); json_free(r);
        free(provider); free(model);
        return mapped;
    }
    json_free(r);
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    wp3_aset_s(out, "provider", provider);
    wp3_aset_s(out, "model", model);
    free(provider); free(model);
    return out;
}

/* ═══════════════════════ WP3 FLOW 14: describe_profile_auto_endpoint ════════ */
/* PoP: wp3_describe_profile_auto @ hermes_cli/web_routers/profiles.py:describe_profile_auto_endpoint */
static json_t *wp3_describe_profile_auto(const wp3_seams_t *sp, const char *name, bool overwrite) {
    json_t *a = wp3_args();
    wp3_aset_s(a, "name", name);
    json_t *d = wp3_seam(sp, "resolve_profile_dir", a);
    json_free(a);
    if (wp3_is_raise(d)) return d;
    json_free(d);
    json_t *da = wp3_args();
    wp3_aset_s(da, "name", name);
    json_set(da, "overwrite", json_bool(overwrite));
    json_t *r = wp3_seam(sp, "describe_profile", da);
    json_free(da);
    if (wp3_is_raise(r)) {
        json_t *eargs = json_array();
        json_append(eargs, json_string(name));
        json_t *mapped = wp3_map_errors(sp, r, "POST /api/profiles/%s/describe-auto failed", eargs, NULL, NULL);
        json_free(eargs); json_free(r);
        return mapped;
    }
    bool ok = wp3_jtruth(wp3_get(r, "ok", NULL));
    json_t *out = json_object();
    json_set(out, "ok", json_bool(ok));
    json_set(out, "reason", wp3_copy(wp3_get(r, "reason", NULL)));
    json_set(out, "description", wp3_copy(wp3_get(r, "description", NULL)));
    json_set(out, "description_auto", json_bool(ok));
    json_free(r);
    return out;
}

/* ═══════════════════════ WP3 FLOW 15: create_profile_endpoint ════════════════ */
/* PoP: wp3_create_profile @ hermes_cli/web_routers/profiles.py:create_profile_endpoint */
static json_t *wp3_create_profile(const wp3_seams_t *sp, const json_t *body) {
    const char *clone_from_default_raw = wp3_jstr(wp3_get(body, "clone_from", NULL));
    bool clone_all = wp3_jtruth(wp3_get(body, "clone_all", NULL));
    bool cfd = wp3_jtruth(wp3_get(body, "clone_from_default", NULL));
    char *explicit = wp3_strip(clone_from_default_raw);
    const char *clone_from = NULL;
    bool clone_config = false;
    if (*explicit) {
        clone_from = explicit;
        clone_config = !clone_all;
    } else if (clone_all) {
        clone_from = "default";
        clone_config = false;
    } else {
        clone_from = cfd ? "default" : NULL;
        clone_config = cfd;
    }
    (void)clone_config;  /* carried only inside the provision request (seamed) */
    char *provider = wp3_strip(wp3_jstr(wp3_get(body, "provider", NULL)));
    char *model = wp3_strip(wp3_jstr(wp3_get(body, "model", NULL)));
    json_t *pa = wp3_args();
    wp3_aset_s(pa, "name", wp3_jstr(wp3_get(body, "name", NULL)));
    wp3_aset_s(pa, "clone_from", clone_from);
    json_set(pa, "clone_all", json_bool(clone_all));
    json_t *r = wp3_seam(sp, "provision_profile", pa);
    json_free(pa);
    if (wp3_is_raise(r)) {
        json_t *bad = wp3_is_arr(wp3_get(body, "refusals", NULL)) ?
                      json_copy(wp3_get(body, "refusals", NULL)) : json_array();
        json_t *mapped = wp3_map_errors(sp, r, "POST /api/profiles failed", NULL, NULL, bad);
        json_free(bad); json_free(r);
        free(explicit); free(provider); free(model);
        return mapped;
    }
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    wp3_aset_s(out, "name", wp3_jstr(wp3_get(body, "name", NULL)));
    json_set(out, "path", wp3_copy(wp3_get(r, "path", NULL)));
    json_set(out, "model_set", wp3_copy(wp3_get(r, "model_set", NULL)));
    json_set(out, "model_error", wp3_copy(wp3_get(r, "model_error", NULL)));
    json_set(out, "mcp_written", wp3_copy(wp3_get(r, "mcp_written", NULL)));
    json_set(out, "skills_disabled", wp3_copy(wp3_get(r, "skills_disabled", NULL)));
    json_set(out, "soul_set", json_bool(!wp3_is_null(wp3_get(body, "soul", NULL))));
    json_set(out, "warnings", wp3_copy(wp3_get(r, "warnings", NULL)));
    /* hub installs: _best_effort(spawn_install) -> default None on Exception */
    json_t *installs = json_array();
    const json_t *skills = wp3_get(body, "hub_skills", NULL);
    if (wp3_is_arr(skills)) {
        for (size_t i = 0; i < skills->c.count; i++) {
            char *ident = wp3_strip(wp3_jstr(json_get(skills, i)));
            if (!*ident) { free(ident); continue; }
            json_t *ia = wp3_args();
            wp3_aset_s(ia, "ident", ident);
            json_t *pid = wp3_seam(sp, "spawn_install", ia);
            json_free(ia);
            if (wp3_is_raise(pid)) {
                if (wp3_is_exception(pid)) {
                    json_t *la = wp3_args();
                    wp3_aset_s(la, "log_msg", "Spawning hub-skill install %s for new profile %s failed");
                    json_t *arr = json_array();
                    json_append(arr, json_string(ident));
                    json_append(arr, json_string(wp3_jstr(wp3_get(body, "name", NULL)) ? wp3_jstr(wp3_get(body, "name", NULL)) : ""));
                    json_set(la, "args", arr);
                    json_t *ign = wp3_seam(sp, "log_exception", la);
                    json_free(ign); json_free(la);
                    json_free(pid);
                    pid = json_null();
                }
                /* BaseException family propagates out of _best_effort */
                else { json_free(installs); json_free(out); json_free(r);
                       free(explicit); free(provider); free(model); return pid; }
            }
            json_t *item = json_object();
            wp3_aset_s(item, "identifier", ident);
            json_set(item, "pid", pid ? pid : json_null());
            json_append(installs, item);
            free(ident);
        }
    }
    json_set(out, "hub_installs", installs);
    json_free(r);
    free(explicit); free(provider); free(model);
    return out;
}

/* ═══════════════════════ WP3 FLOW 16: open_profile_terminal_endpoint ════════ */
/* PoP: wp3_open_profile_terminal @ hermes_cli/web_routers/profiles.py:open_profile_terminal_endpoint */
static json_t *wp3_open_profile_terminal(const wp3_seams_t *sp, const char *name, const char *platform) {
    json_t *out = json_object();
    json_t *a = wp3_args();
    wp3_aset_s(a, "name", name);
    json_t *cmd_r = wp3_seam(sp, "resolve_profile_dir", a);
    json_free(a);
    if (wp3_is_raise(cmd_r)) {
        json_t *eargs = json_array();
        json_append(eargs, json_string(name));
        json_t *nf = json_array(); json_append(nf, json_string("FileNotFoundError"));
        json_t *br = json_array(); json_append(br, json_string("ValueError"));
        json_t *mapped = wp3_map_errors(sp, cmd_r, "POST /api/profiles/%s/open-terminal failed", eargs, nf, br);
        json_free(br); json_free(nf); json_free(eargs); json_free(cmd_r); json_free(out);
        return mapped;
    }
    const char *command = wp3_jstr(cmd_r);
    json_set(out, "command", json_copy(cmd_r));
    bool is_win = platform && !strncmp(platform, "win", 3);
    bool is_mac = platform && !strcmp(platform, "darwin");
    json_t *pa = wp3_args();
    if (is_win) {
        json_t *arr = json_array();
        json_append(arr, json_string("cmd.exe"));
        json_append(arr, json_string("/c"));
        json_append(arr, json_string("start"));
        json_append(arr, json_string(""));
        json_append(arr, json_string(command ? command : ""));
        json_set(pa, "args", arr);
        json_t *p = wp3_seam(sp, "popen", pa);
        json_free(p);
    } else if (is_mac) {
        char *esc = wp3_xsf("tell application \"Terminal\"\nactivate\ndo script \"%s\"\nend tell", command ? command : "");
        json_t *arr = json_array();
        json_append(arr, json_string("osascript"));
        json_append(arr, json_string("-e"));
        json_append(arr, json_string(esc));
        json_set(pa, "args", arr);
        json_t *p = wp3_seam(sp, "popen", pa);
        json_free(p); free(esc);
    } else {
        static const char *const EXES[] = {
            "x-terminal-emulator", "gnome-terminal", "konsole", "xfce4-terminal",
            "mate-terminal", "lxterminal", "tilix", "alacritty", "kitty", "xterm", NULL
        };
        bool launched = false;
        for (size_t i = 0; EXES[i]; i++) {
            json_t *wa = wp3_args();
            wp3_aset_s(wa, "exe", EXES[i]);
            json_t *w = wp3_seam(sp, "which", wa);
            json_free(wa);
            if (wp3_is_raise(w)) {
                json_t *eargs = json_array();
                json_append(eargs, json_string(name));
                json_t *nf = json_array(); json_append(nf, json_string("FileNotFoundError"));
                json_t *br = json_array(); json_append(br, json_string("ValueError"));
                json_t *mapped = wp3_map_errors(sp, w, "POST /api/profiles/%s/open-terminal failed",
                                               eargs, nf, br);
                json_free(br); json_free(nf); json_free(eargs); json_free(w);
                json_free(pa); json_free(cmd_r); json_free(out);
                return mapped;
            }
            bool ok = false;
            if (w && w->type == JSON_NUMBER) ok = (w->num_val == 0.0);
            else ok = !wp3_jtruth(w);
            json_free(w);
            if (ok) {
                char *quoted = wp3_xsf("sh -lc '%s'", command ? command : "");
                bool takes_flag_none = !strcmp(EXES[i], "xfce4-terminal") ||
                    !strcmp(EXES[i], "mate-terminal") || !strcmp(EXES[i], "lxterminal");
                bool takes_empty = !strcmp(EXES[i], "kitty");
                json_t *arr = json_array();
                json_append(arr, json_string(EXES[i]));
                if (takes_flag_none) {
                    json_append(arr, json_string("-e"));
                    json_append(arr, json_string(quoted));
                } else if (takes_empty) {
                    json_append(arr, json_string("sh"));
                    json_append(arr, json_string("-lc"));
                    json_append(arr, json_string(command ? command : ""));
                } else {
                    const char *flag = !strcmp(EXES[i], "gnome-terminal") ? "--" : "-e";
                    json_append(arr, json_string(flag));
                    json_append(arr, json_string("sh"));
                    json_append(arr, json_string("-lc"));
                    json_append(arr, json_string(command ? command : ""));
                }
                free(quoted);
                json_set(pa, "args", arr);
                json_t *p = wp3_seam(sp, "popen", pa);
                json_free(p);
                launched = true;
                break;
            }
        }
        if (!launched) {
            json_free(pa); json_free(cmd_r); json_free(out);
            return wp3_http_exc(400, "No supported terminal emulator found");
        }
    }
    json_free(pa); json_free(cmd_r);
    json_set(out, "ok", json_bool(true));
    return out;
}

/* ═══════════════════════ WP3 FLOW 17: _read_profiles ═════════════════════════ */
/* PoP: wp3_read_profiles @ hermes_cli/web_routers/profiles.py:_read_profiles */
static json_t *wp3_read_profiles(const wp3_seams_t *sp) {
    json_t *a = wp3_args();
    json_t *r = wp3_seam(sp, "list_profiles", a);
    json_free(a);
    json_t *profiles;
    if (wp3_is_raise(r)) {
        if (!wp3_is_exception(r)) { return r; }
        json_t *la = wp3_args();
        wp3_aset_s(la, "log_msg", "GET /api/profiles failed; falling back to profile directory scan");
        json_set(la, "args", json_array());
        json_t *ign = wp3_seam(sp, "log_exception", la);
        json_free(ign); json_free(la); json_free(r);
        json_t *fa = wp3_args();
        profiles = wp3_seam(sp, "fallback_profile_dicts", fa);
        json_free(fa);
        if (wp3_is_raise(profiles)) return profiles;
    } else {
        profiles = r;
    }
    json_t *sa = wp3_args();
    json_set(sa, "profiles", profiles ? profiles : json_null());
    json_t *scoped = wp3_seam(sp, "scope_to_launch_profile", sa);
    json_free(sa);
    json_t *wrap = json_object();
    json_set(wrap, "profiles", scoped ? scoped : json_null());
    return wrap;
}

/* ═══════════════════════ WP3 FLOW 18: list_profiles_endpoint ════════════════ */
/* PoP: wp3_list_profiles_endpoint @ hermes_cli/web_routers/profiles.py:list_profiles_endpoint */
static json_t *wp3_list_profiles_endpoint(const wp3_seams_t *sp) {
    return wp3_read_profiles(sp);
}

/* ═══════════════════════ WP3 FLOW 19: _read_desktop_overlay ══════════════════ */
/* PoP: wp3_read_desktop_overlay @ hermes_cli/web_routers/profiles.py:_read_desktop_overlay */
static json_t *wp3_read_desktop_overlay(const wp3_seams_t *sp, const char *profile_dir) {
    json_t *a = wp3_args();
    wp3_aset_s(a, "profile_dir", profile_dir);
    wp3_aset_s(a, "file", "desktop.json");
    json_t *r = wp3_seam(sp, "read_text", a);
    json_free(a);
    if (wp3_is_raise(r)) return r;  /* raises when unreadable */
    const char *s = wp3_jstr(r);
    if (!s) { json_free(r); return json_null(); }
    char *err = NULL;
    json_t *parsed = json_parse(s, &err);
    free(err); json_free(r);
    return parsed ? parsed : json_null();
}
