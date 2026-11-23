/*
 * tui_server_flow2.c — second-wave FLOW cores / pure leaves of
 * tui_gateway/server.py (the UNCLAIMED remainder not covered by
 * tui_server_flow.c / port_server.c / port_methods_tools.c / pet_commands.c).
 *
 * Ports 13 UNCLAIMED functions (CRLF-aware PoP audit: 32 names claimed, the
 * rest unclaimed). Every symbol here is static and ts2_*-prefixed; the harness
 * #includes this file, so there is no link collision with the flow1 symbols.
 * All live I/O / collaborator calls enter through ONE universal seam; the only
 * mutated state is the pet cancel set, carried in the injected json_t mirror
 * as an insertion-ordered, deduplicated array under state["cancel"] (the live
 * object is a set; these functions only ever add/discard/test membership, so
 * iteration order is never observed and both sides model it as a dedup list).
 *
 * Modeling contract (shared with tests/oracle_tui_server_flow2.py):
 *  - Universal seam: json_t *(*call)(void *ud, const char *name, args).
 *    `name` identifies the live call (table in the oracle); `args` is a BORROWED
 *    JSON object the stub copies into the trace; the result is a NEW json_t
 *    (NULL == None; {"__raise__":kind,"message":m} == a raised exception).
 *    The port owns args (ts2_call frees them) and the result. A missing or
 *    exhausted script is FAIL-CLOSED: the stub returns a RuntimeError spec,
 *    which the port propagates like any scripted raise.
 *  - Full builtin exception hierarchy (see TS2_EXC): except Exception /
 *    suppress(Exception) swallow every Exception subclass; the BaseException
 *    trio (SystemExit/KeyboardInterrupt/GeneratorExit) and CancelledError
 *    propagate. Unknown/custom kinds default to Exception subclasses, matching
 *    the oracle's `type(kind, (Exception,), {})` fallback.
 *  - Paths are POSIX strings on both sides (the oracle projects PurePosixPath
 *    / PathDouble results with .as_posix()).
 *  - Self-contained: only libjson is used; no other port object is linked.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "libjson/json.h"

/* ── live module constants (byte-exact) ───────────────────────────────────── */
static const char TS2_MODEL_SWITCH_PREFIX[] =
    "[System: The active model for this chat has changed to ";
static const char TS2_ENV_ORPHAN_GRACE[] = "HERMES_TUI_WS_ORPHAN_REAP_GRACE_S";
static const char TS2_CFG_ORPHAN_GRACE[] = "ws_orphan_reap_grace_s";
static const double TS2_ORPHAN_GRACE_DEFAULT = 20.0;
static const double TS2_DEFAULT_SCALE = 0.33;   /* agent.pet.constants.DEFAULT_SCALE */

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct ts2_env {
    json_t *state;   /* borrowed mirror (harness-owned copy of in["state"]) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
    json_t *exc;     /* owned unhandled raise spec; NULL when none */
} ts2_env;

/* ── JSON type / truthiness helpers ───────────────────────────────────────── */
static bool ts2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ts2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ts2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool ts2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool ts2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ts2_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool ts2_truth(const json_t *v) {
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
static const char *ts2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* ── raise specs + FULL builtin exception hierarchy ──────────────────────── */
static bool ts2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && ts2_is_str(json_obj_get(r, "__raise__"));
}
static const char *ts2_kind(const json_t *r) { return json_get_str(r, "__raise__", ""); }
static const char *ts2_msg(const json_t *r) { return json_get_str(r, "message", ""); }

/* child -> parent; covers every builtin exception plus the module's custom
 * classes. Anything not listed defaults to an Exception subclass. */
typedef struct { const char *child; const char *parent; } ts2_exc_edge;
static const ts2_exc_edge TS2_EXC[] = {
    {"BaseException", NULL},
    {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"}, {"CancelledError", "BaseException"},
    {"Exception", "BaseException"},
    {"ConfigBackendUnavailable", "SystemExit"},
    {"ProfileUnavailableError", "Exception"},
    {"ArithmeticError", "Exception"}, {"AssertionError", "Exception"},
    {"AttributeError", "Exception"}, {"BufferError", "Exception"},
    {"EOFError", "Exception"}, {"ImportError", "Exception"},
    {"LookupError", "Exception"}, {"MemoryError", "Exception"},
    {"NameError", "Exception"}, {"OSError", "Exception"},
    {"ReferenceError", "Exception"}, {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"}, {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"}, {"SystemError", "Exception"},
    {"TypeError", "Exception"}, {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"OverflowError", "ArithmeticError"}, {"ZeroDivisionError", "ArithmeticError"},
    {"FloatingPointError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
    {"FileNotFoundError", "OSError"}, {"FileExistsError", "OSError"},
    {"PermissionError", "OSError"}, {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"}, {"TimeoutError", "OSError"},
    {"ConnectionError", "OSError"}, {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"}, {"ProcessLookupError", "OSError"},
    {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
    {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
    {"UserWarning", "Warning"}, {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"}, {"SyntaxWarning", "Warning"},
    {"RuntimeWarning", "Warning"}, {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"}, {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"}, {"ResourceWarning", "Warning"},
};
static const char *ts2_exc_parent(const char *k) {
    for (size_t i = 0; i < sizeof(TS2_EXC) / sizeof(TS2_EXC[0]); i++)
        if (strcmp(k, TS2_EXC[i].child) == 0) return TS2_EXC[i].parent;
    return "Exception";   /* unknown custom kind -> Exception subclass */
}
/* True when `k` is `anc` or a descendant of it. */
static bool ts2_exc_is(const char *k, const char *anc) {
    const char *cur = k;
    for (int guard = 0; cur && guard < 24; guard++) {
        if (strcmp(cur, anc) == 0) return true;
        cur = ts2_exc_parent(cur);
    }
    return false;
}
static json_t *ts2_raise_spec(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}
static json_t *ts2_attr_raise(const json_t *v, const char *attr) {
    char buf[192];
    snprintf(buf, sizeof(buf), "'%s' object has no attribute '%s'", ts2_type_name(v), attr);
    return ts2_raise_spec("AttributeError", buf);
}
/* Park an unhandled raise (first wins) and return NULL. */
static json_t *ts2_prop(ts2_env *e, json_t *r) {
    if (e->exc) json_free(r); else e->exc = r;
    return NULL;
}
/* `except Exception` / suppress(Exception): swallow Exception family,
 * propagate the BaseException trio. Returns true when caller must propagate. */
static bool ts2_swallow_exc(ts2_env *e, json_t *r) {
    if (!ts2_exc_is(ts2_kind(r), "Exception")) { ts2_prop(e, r); return true; }  /* BaseException escapes */
    json_free(r);
    return false;
}
/* Narrow suppress(ValueError, TypeError). */
static bool ts2_swallow_valtype(ts2_env *e, json_t *r) {
    const char *k = ts2_kind(r);
    if (!ts2_exc_is(k, "ValueError") && !ts2_exc_is(k, "TypeError")) {
        ts2_prop(e, r);
        return true;
    }
    json_free(r);
    return false;
}

/* ── universal seam call (args owned by port, freed after the call) ───────── */
static json_t *ts2_call(ts2_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}
static json_t *ts2_call0(ts2_env *e, const char *name) {
    return ts2_call(e, name, json_object());
}

/* ── unicode / string helpers ─────────────────────────────────────────────── */
static unsigned long ts2_cp(const char *s, size_t *adv) {
    const unsigned char *u = (const unsigned char *)s;
    unsigned long cp; size_t n;
    if (u[0] < 0x80) { cp = u[0]; n = 1; }
    else if ((u[0] & 0xE0) == 0xC0) { cp = u[0] & 0x1F; n = 2; }
    else if ((u[0] & 0xF0) == 0xE0) { cp = u[0] & 0x0F; n = 3; }
    else if ((u[0] & 0xF8) == 0xF0) { cp = u[0] & 0x07; n = 4; }
    else { cp = u[0]; n = 1; }
    for (size_t i = 1; i < n && u[i]; i++) cp = (cp << 6) | (u[i] & 0x3F);
    *adv = n;
    return cp;
}
static bool ts2_ws(unsigned long cp) {
    return (cp >= 0x09 && cp <= 0x0D) || cp == 0x20 || (cp >= 0x1C && cp <= 0x1F)
        || cp == 0x85 || cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)
        || cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}
static char *ts2_strip(const char *s) {
    if (!s) return strdup("");
    size_t start = 0, last = 0, i = 0;
    bool any = false;
    while (s[i]) {
        size_t adv; unsigned long cp = ts2_cp(s + i, &adv);
        if (!ts2_ws(cp)) { if (!any) { start = i; any = true; } last = i + adv; }
        i += adv;
    }
    if (!any) return strdup("");
    char *r = (char *)malloc(last - start + 1);
    if (!r) return strdup("");
    memcpy(r, s + start, last - start);
    r[last - start] = '\0';
    return r;
}
/* str(value): strings verbatim; bool -> True/False; numbers -> repr-ish;
 * null -> None; containers are outside the fixture domain (-> ""). */
static char *ts2_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64]; double d = v->num_val;
        if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
            snprintf(b, sizeof(b), "%lld", (long long)d);
        else
            snprintf(b, sizeof(b), "%.15g", d);
        return strdup(b);
    }
    return strdup("");
}
/* Path(base) / child for the clean POSIX fixture domain. */
static char *ts2_path_join(const char *base, const char *child) {
    if (!base || !base[0]) return strdup(child);
    size_t bl = strlen(base);
    while (bl > 1 && base[bl - 1] == '/') bl--;
    bool sep = !(base[bl - 1] == '/');   /* "/" + child == "/child", not "//child" */
    char *r = (char *)malloc(bl + (sep ? 1 : 0) + strlen(child) + 1);
    if (!r) return strdup(child);
    memcpy(r, base, bl);
    if (sep) r[bl] = '/';
    strcpy(r + bl + (sep ? 1 : 0), child);
    return r;
}

/* ── float(value): Python-ish coercion; false on TypeError/ValueError ─────── */
static bool ts2_float_coerce(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return true; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return true; }
    if (v->type != JSON_STRING) return false;
    char *s = ts2_strip(v->str_val ? v->str_val : "");
    char clean[160]; size_t cn = 0;
    for (const char *q = s; *q; q++) {
        if (*q == '_') {
            bool pd = (q > s) && (q[-1] >= '0' && q[-1] <= '9');
            bool nd = (q[1] >= '0' && q[1] <= '9');
            if (!pd || !nd) { free(s); return false; }
            continue;
        }
        if (cn < sizeof(clean) - 1) clean[cn++] = *q;
    }
    clean[cn] = '\0';
    free(s);
    if (!cn) return false;
    char *end = NULL;
    double d = strtod(clean, &end);
    if (!end || *end || end == clean) return false;
    *out = d;
    return true;
}

/* ── pet cancel set on state["cancel"] (dedup list, insertion order) ──────── */
static json_t *ts2_cancel_arr(ts2_env *e) {
    json_t *a = json_obj_get(e->state, "cancel");
    if (!ts2_is_arr(a)) { a = json_array(); json_set(e->state, "cancel", a); }
    return a;
}
static bool ts2_cancel_has(const json_t *a, const char *tok) {
    if (!tok) return false;
    for (size_t i = 0; i < a->c.count; i++) {
        const char *s = ts2_str(a->c.items[i]);
        if (s && strcmp(s, tok) == 0) return true;
    }
    return false;
}
static void ts2_cancel_add(ts2_env *e, const char *tok) {
    json_t *a = ts2_cancel_arr(e);
    if (!tok || ts2_cancel_has(a, tok)) return;
    json_append(a, json_string(tok));
}
static void ts2_cancel_discard(ts2_env *e, const char *tok) {
    json_t *a = ts2_cancel_arr(e);
    if (!tok) return;
    for (size_t i = a->c.count; i > 0; i--) {
        const char *s = ts2_str(a->c.items[i - 1]);
        if (s && strcmp(s, tok) == 0) {
            /* shift left (libjson has no array-remove) preserving order */
            for (size_t j = i - 1; j + 1 < a->c.count; j++) a->c.items[j] = a->c.items[j + 1];
            if (a->c.count > 0) a->c.count--;
        }
    }
}

/* ════════════════════ ported functions (UNCLAIMED) ═══════════════════════ */

/* PoP: ts2_ws_orphan_setting @ tui_gateway/server.py:_ws_orphan_setting */
static json_t *ts2_ws_orphan_setting(ts2_env *e, const json_t *in) {
    const char *env_var = json_get_str(in, "env_var", "");
    const char *cfg_key = json_get_str(in, "cfg_key", "");
    const json_t *defv = json_obj_get(in, "default");
    double dflt;
    if (!ts2_float_coerce(defv, &dflt)) dflt = 0.0;

    json_t *a0 = json_object();
    json_set(a0, "name", json_string(env_var ? env_var : ""));
    json_t *raw = ts2_call(e, "environ_get", a0);
    if (ts2_is_raise(raw)) return ts2_prop(e, raw);

    bool raw_null = ts2_is_null(raw);
    if (!raw_null) {
        char *s = ts2_py_str(raw);
        char *st = ts2_strip(s);
        free(s);
        if (!st[0]) raw_null = true;
        free(st);
    }
    if (raw_null) {
        if (raw) json_free(raw);
        raw = NULL;
        json_t *lc = ts2_call0(e, "load_config");
        if (ts2_is_raise(lc)) {
            if (ts2_swallow_exc(e, lc)) return NULL;   /* BaseException escapes */
            /* suppressed -> raw stays None */
        } else {
            if (!ts2_is_obj(lc)) {
                /* None.get / list.get -> AttributeError, suppressed */
                json_free(lc);
            } else {
                json_t *dash = json_obj_get(lc, "dashboard");
                if (ts2_is_null(dash)) {
                    json_free(lc);
                } else if (!ts2_is_obj(dash)) {
                    json_free(lc);   /* truthy non-dict .get -> AttributeError, suppressed */
                } else {
                    json_t *v = json_obj_get(dash, cfg_key);
                    raw = v ? json_copy(v) : NULL;
                    json_free(lc);
                }
            }
        }
    }
    double result;
    if (raw) {
        if (ts2_float_coerce(raw, &result)) {
            /* ValueError/TypeError from float() is handled by ts2_float_coerce
             * returning false; a successful parse cannot raise. */
        } else {
            result = dflt;
        }
        json_free(raw);
    } else {
        result = dflt;
    }
    if (result < 0.0) result = 0.0;   /* max(0.0, x) */
    return json_number(result);
}

/* PoP: ts2_resolve_ws_orphan_reap_grace @ tui_gateway/server.py:_resolve_ws_orphan_reap_grace */
static json_t *ts2_resolve_ws_orphan_reap_grace(ts2_env *e, const json_t *in) {
    (void)in;
    json_t *a = json_object();
    json_set(a, "env_var", json_string(TS2_ENV_ORPHAN_GRACE));
    json_set(a, "cfg_key", json_string(TS2_CFG_ORPHAN_GRACE));
    json_set(a, "default", json_number(TS2_ORPHAN_GRACE_DEFAULT));
    json_t *r = ts2_call(e, "_ws_orphan_setting", a);
    if (ts2_is_raise(r)) return ts2_prop(e, r);
    return r;
}

/* PoP: ts2_configured_cwd_from_cfg @ tui_gateway/server.py:_configured_cwd_from_cfg */
static json_t *ts2_configured_cwd_from_cfg(ts2_env *e, const json_t *in) {
    const json_t *cfg = json_obj_get(in, "cfg");
    const json_t *terminal = ts2_is_obj(cfg) ? json_obj_get(cfg, "terminal") : NULL;
    const json_t *cwdv = ts2_is_obj(terminal) ? json_obj_get(terminal, "cwd") : NULL;
    char *raw = "";
    if (cwdv && ts2_truth(cwdv)) raw = ts2_py_str(cwdv);
    char *s = ts2_strip(raw);
    if (cwdv && ts2_truth(cwdv)) free(raw);
    if (!s[0] || strcmp(s, ".") == 0 || strcmp(s, "auto") == 0 || strcmp(s, "cwd") == 0) {
        free(s);
        return json_null();
    }
    json_t *a1 = json_object();
    json_set(a1, "raw", json_string(s));
    free(s);
    json_t *expanded = ts2_call(e, "expanduser", a1);
    if (ts2_is_raise(expanded)) return ts2_prop(e, expanded);
    json_t *a2 = json_object();
    json_set(a2, "path", expanded ? json_copy(expanded) : json_null());
    json_free(expanded);
    json_t *resolved = ts2_call(e, "abspath", a2);
    if (ts2_is_raise(resolved)) return ts2_prop(e, resolved);
    json_t *a3 = json_object();
    json_set(a3, "path", resolved ? json_copy(resolved) : json_null());
    json_t *isdir = ts2_call(e, "isdir", a3);
    if (ts2_is_raise(isdir)) { json_free(resolved); return ts2_prop(e, isdir); }
    bool ok = ts2_truth(isdir);
    json_free(isdir);
    if (ok) return resolved;   /* adopts */
    json_free(resolved);
    return json_null();
}

/* PoP: ts2_default_session_cwd @ tui_gateway/server.py:_default_session_cwd */
static json_t *ts2_default_session_cwd(ts2_env *e, const json_t *in) {
    (void)in;
    json_t *r = ts2_call0(e, "launch_configured_cwd");
    if (ts2_is_raise(r)) return ts2_prop(e, r);
    if (ts2_truth(r)) return r;
    json_free(r);
    json_t *a = json_object();
    json_set(a, "name", json_string("TERMINAL_CWD"));
    r = ts2_call(e, "getenv", a);
    if (ts2_is_raise(r)) return ts2_prop(e, r);
    if (ts2_truth(r)) return r;
    json_free(r);
    r = ts2_call0(e, "getcwd");
    if (ts2_is_raise(r)) return ts2_prop(e, r);
    return r;
}

/* PoP: ts2_pet_cfg @ tui_gateway/server.py:_pet_cfg */
static json_t *ts2_pet_cfg(ts2_env *e, const json_t *in) {
    (void)in;
    json_t *root = ts2_call0(e, "load_config");
    if (ts2_is_raise(root)) {
        if (ts2_swallow_exc(e, root)) return NULL;
        return json_object();
    }
    if (!ts2_is_obj(root)) { json_free(root); return json_object(); }
    json_t *display = json_obj_get(root, "display");
    if (!ts2_is_obj(display)) { json_free(root); return json_object(); }
    json_t *pet = json_obj_get(display, "pet");
    json_t *ret = ts2_is_obj(pet) ? json_copy(pet) : json_object();
    json_free(root);
    return ret;
}

/* PoP: ts2_pet_config_scale @ tui_gateway/server.py:_pet_config_scale */
static json_t *ts2_pet_config_scale(ts2_env *e, const json_t *in) {
    (void)in;
    json_t *cfg = ts2_call0(e, "pet_cfg");
    if (ts2_is_raise(cfg)) {
        if (ts2_swallow_exc(e, cfg)) return NULL;
        return json_number(TS2_DEFAULT_SCALE);
    }
    const json_t *scalev = ts2_is_obj(cfg) ? json_obj_get(cfg, "scale") : NULL;
    double val;
    bool ok = scalev && ts2_truth(scalev) && ts2_float_coerce(scalev, &val);
    if (!ok) val = TS2_DEFAULT_SCALE;
    json_free(cfg);
    return json_number(val);
}

/* PoP: ts2_pet_cancel_arm @ tui_gateway/server.py:_pet_cancel_arm */
static json_t *ts2_pet_cancel_arm(ts2_env *e, const json_t *in) {
    const char *tok = json_get_str(in, "token", "");
    ts2_cancel_discard(e, tok);
    return json_null();
}

/* PoP: ts2_pet_cancel_request @ tui_gateway/server.py:_pet_cancel_request */
static json_t *ts2_pet_cancel_request(ts2_env *e, const json_t *in) {
    const char *tok = json_get_str(in, "token", "");
    ts2_cancel_add(e, tok);
    return json_null();
}

/* PoP: ts2_pet_is_cancelled @ tui_gateway/server.py:_pet_is_cancelled */
static json_t *ts2_pet_is_cancelled(ts2_env *e, const json_t *in) {
    const char *tok = json_get_str(in, "token", "");
    json_t *a = ts2_cancel_arr(e);
    return json_bool(ts2_cancel_has(a, tok));
}

/* PoP: ts2_response_profile_name @ tui_gateway/server.py:_response_profile_name */
static json_t *ts2_response_profile_name(ts2_env *e, const json_t *in) {
    const json_t *prof = json_obj_get(in, "profile");
    char *s = prof && ts2_truth(prof) ? ts2_py_str(prof) : strdup("");
    char *name_s = ts2_strip(s);
    free(s);
    json_t *a0 = json_object();
    json_set(a0, "value", json_string(name_s));
    free(name_s);
    json_t *name = ts2_call(e, "canonical_profile_request", a0);
    if (ts2_is_raise(name)) return ts2_prop(e, name);
    if (!ts2_truth(name)) {
        json_free(name);
        return ts2_call0(e, "current_profile_name");   /* verbatim (raise already a spec) */
    }
    json_t *a1 = json_object();
    json_set(a1, "name", json_copy(name));
    json_t *home = ts2_call(e, "profile_home", a1);
    if (ts2_is_raise(home)) {
        if (strcmp(ts2_kind(home), "ProfileUnavailableError") == 0) {
            json_free(home);
            json_free(name);
            return ts2_call0(e, "current_profile_name");
        }
        json_free(name);
        return ts2_prop(e, home);
    }
    json_t *ret;
    if (!ts2_is_null(home)) ret = json_copy(name);
    else ret = ts2_call0(e, "current_profile_name");
    json_free(home);
    json_free(name);
    /* current_profile_name may itself have returned a raise spec: propagate. */
    if (ts2_is_raise(ret)) return ts2_prop(e, ret);
    return ret;
}

/* PoP: ts2_pet_gen_root @ tui_gateway/server.py:_pet_gen_root */
static json_t *ts2_pet_gen_root(ts2_env *e, const json_t *in) {
    (void)in;
    json_t *home = ts2_call0(e, "get_hermes_home");
    if (ts2_is_raise(home)) return ts2_prop(e, home);
    const char *hs = ts2_str(home);
    char *p1 = ts2_path_join(hs ? hs : "", "cache");
    char *p2 = ts2_path_join(p1, "pet-gen");
    free(p1);
    json_free(home);
    json_t *a = json_object();
    json_set(a, "path", json_string(p2));
    json_set(a, "parents", json_bool(true));
    json_set(a, "exist_ok", json_bool(true));
    json_t *r = ts2_call(e, "mkdir", a);
    if (ts2_is_raise(r)) { free(p2); return ts2_prop(e, r); }
    json_free(r);
    json_t *ret = json_string(p2);
    free(p2);
    return ret;
}

/* PoP: ts2_spawn_trees_root @ tui_gateway/server.py:_spawn_trees_root */
static json_t *ts2_spawn_trees_root(ts2_env *e, const json_t *in) {
    (void)in;
    json_t *home = ts2_call0(e, "get_hermes_home");
    if (ts2_is_raise(home)) return ts2_prop(e, home);
    const char *hs = ts2_str(home);
    char *p = ts2_path_join(hs ? hs : "", "spawn-trees");
    json_free(home);
    json_t *a = json_object();
    json_set(a, "path", json_string(p));
    json_set(a, "parents", json_bool(true));
    json_set(a, "exist_ok", json_bool(true));
    json_t *r = ts2_call(e, "mkdir", a);
    if (ts2_is_raise(r)) { free(p); return ts2_prop(e, r); }
    json_free(r);
    json_t *ret = json_string(p);
    free(p);
    return ret;
}

/* Unicode-aware sanitizer matching Python str.isalnum() per code point.
 * LIVE: "".join(c if c.isalnum() or c in "-_" else "_" for c in session_id). */
static const unsigned int TS2_ALNUM_RANGES[][2] = {
    {0xAA,0xAA}, {0xB2,0xB3}, {0xB5,0xB5}, {0xB9,0xBA}, {0xBC,0xBE}, {0xC0,0xD6},
    {0xD8,0xF6}, {0xF8,0x2C1}, {0x2C6,0x2D1}, {0x2E0,0x2E4}, {0x2EC,0x2EC}, {0x2EE,0x2EE},
    {0x370,0x374}, {0x376,0x377}, {0x37A,0x37D}, {0x37F,0x37F}, {0x386,0x386}, {0x388,0x38A},
    {0x38C,0x38C}, {0x38E,0x3A1}, {0x3A3,0x3F5}, {0x3F7,0x481}, {0x48A,0x52F}, {0x531,0x556},
    {0x559,0x559}, {0x560,0x588}, {0x5D0,0x5EA}, {0x5EF,0x5F2}, {0x620,0x64A}, {0x660,0x669},
    {0x66E,0x66F}, {0x671,0x6D3}, {0x6D5,0x6D5}, {0x6E5,0x6E6}, {0x6EE,0x6FC}, {0x6FF,0x6FF},
    {0x710,0x710}, {0x712,0x72F}, {0x74D,0x7A5}, {0x7B1,0x7B1}, {0x7C0,0x7EA}, {0x7F4,0x7F5},
    {0x7FA,0x7FA}, {0x800,0x815}, {0x81A,0x81A}, {0x824,0x824}, {0x828,0x828}, {0x840,0x858},
    {0x860,0x86A}, {0x870,0x887}, {0x889,0x88E}, {0x8A0,0x8C9}, {0x904,0x939}, {0x93D,0x93D},
    {0x950,0x950}, {0x958,0x961}, {0x966,0x96F}, {0x971,0x980}, {0x985,0x98C}, {0x98F,0x990},
    {0x993,0x9A8}, {0x9AA,0x9B0}, {0x9B2,0x9B2}, {0x9B6,0x9B9}, {0x9BD,0x9BD}, {0x9CE,0x9CE},
    {0x9DC,0x9DD}, {0x9DF,0x9E1}, {0x9E6,0x9F1}, {0x9F4,0x9F9}, {0x9FC,0x9FC}, {0xA05,0xA0A},
    {0xA0F,0xA10}, {0xA13,0xA28}, {0xA2A,0xA30}, {0xA32,0xA33}, {0xA35,0xA36}, {0xA38,0xA39},
    {0xA59,0xA5C}, {0xA5E,0xA5E}, {0xA66,0xA6F}, {0xA72,0xA74}, {0xA85,0xA8D}, {0xA8F,0xA91},
    {0xA93,0xAA8}, {0xAAA,0xAB0}, {0xAB2,0xAB3}, {0xAB5,0xAB9}, {0xABD,0xABD}, {0xAD0,0xAD0},
    {0xAE0,0xAE1}, {0xAE6,0xAEF}, {0xAF9,0xAF9}, {0xB05,0xB0C}, {0xB0F,0xB10}, {0xB13,0xB28},
    {0xB2A,0xB30}, {0xB32,0xB33}, {0xB35,0xB39}, {0xB3D,0xB3D}, {0xB5C,0xB5D}, {0xB5F,0xB61},
    {0xB66,0xB6F}, {0xB71,0xB77}, {0xB83,0xB83}, {0xB85,0xB8A}, {0xB8E,0xB90}, {0xB92,0xB95},
    {0xB99,0xB9A}, {0xB9C,0xB9C}, {0xB9E,0xB9F}, {0xBA3,0xBA4}, {0xBA8,0xBAA}, {0xBAE,0xBB9},
    {0xBD0,0xBD0}, {0xBE6,0xBF2}, {0xC05,0xC0C}, {0xC0E,0xC10}, {0xC12,0xC28}, {0xC2A,0xC39},
    {0xC3D,0xC3D}, {0xC58,0xC5A}, {0xC5D,0xC5D}, {0xC60,0xC61}, {0xC66,0xC6F}, {0xC78,0xC7E},
    {0xC80,0xC80}, {0xC85,0xC8C}, {0xC8E,0xC90}, {0xC92,0xCA8}, {0xCAA,0xCB3}, {0xCB5,0xCB9},
    {0xCBD,0xCBD}, {0xCDD,0xCDE}, {0xCE0,0xCE1}, {0xCE6,0xCEF}, {0xCF1,0xCF2}, {0xD04,0xD0C},
    {0xD0E,0xD10}, {0xD12,0xD3A}, {0xD3D,0xD3D}, {0xD4E,0xD4E}, {0xD54,0xD56}, {0xD58,0xD61},
    {0xD66,0xD78}, {0xD7A,0xD7F}, {0xD85,0xD96}, {0xD9A,0xDB1}, {0xDB3,0xDBB}, {0xDBD,0xDBD},
    {0xDC0,0xDC6}, {0xDE6,0xDEF}, {0xE01,0xE30}, {0xE32,0xE33}, {0xE40,0xE46}, {0xE50,0xE59},
    {0xE81,0xE82}, {0xE84,0xE84}, {0xE86,0xE8A}, {0xE8C,0xEA3}, {0xEA5,0xEA5}, {0xEA7,0xEB0},
    {0xEB2,0xEB3}, {0xEBD,0xEBD}, {0xEC0,0xEC4}, {0xEC6,0xEC6}, {0xED0,0xED9}, {0xEDC,0xEDF},
    {0xF00,0xF00}, {0xF20,0xF33}, {0xF40,0xF47}, {0xF49,0xF6C}, {0xF88,0xF8C}, {0x1000,0x102A},
    {0x103F,0x1049}, {0x1050,0x1055}, {0x105A,0x105D}, {0x1061,0x1061}, {0x1065,0x1066}, {0x106E,0x1070},
    {0x1075,0x1081}, {0x108E,0x108E}, {0x1090,0x1099}, {0x10A0,0x10C5}, {0x10C7,0x10C7}, {0x10CD,0x10CD},
    {0x10D0,0x10FA}, {0x10FC,0x1248}, {0x124A,0x124D}, {0x1250,0x1256}, {0x1258,0x1258}, {0x125A,0x125D},
    {0x1260,0x1288}, {0x128A,0x128D}, {0x1290,0x12B0}, {0x12B2,0x12B5}, {0x12B8,0x12BE}, {0x12C0,0x12C0},
    {0x12C2,0x12C5}, {0x12C8,0x12D6}, {0x12D8,0x1310}, {0x1312,0x1315}, {0x1318,0x135A}, {0x1369,0x137C},
    {0x1380,0x138F}, {0x13A0,0x13F5}, {0x13F8,0x13FD}, {0x1401,0x166C}, {0x166F,0x167F}, {0x1681,0x169A},
    {0x16A0,0x16EA}, {0x16EE,0x16F8}, {0x1700,0x1711}, {0x171F,0x1731}, {0x1740,0x1751}, {0x1760,0x176C},
    {0x176E,0x1770}, {0x1780,0x17B3}, {0x17D7,0x17D7}, {0x17DC,0x17DC}, {0x17E0,0x17E9}, {0x17F0,0x17F9},
    {0x1810,0x1819}, {0x1820,0x1878}, {0x1880,0x1884}, {0x1887,0x18A8}, {0x18AA,0x18AA}, {0x18B0,0x18F5},
    {0x1900,0x191E}, {0x1946,0x196D}, {0x1970,0x1974}, {0x1980,0x19AB}, {0x19B0,0x19C9}, {0x19D0,0x19DA},
    {0x1A00,0x1A16}, {0x1A20,0x1A54}, {0x1A80,0x1A89}, {0x1A90,0x1A99}, {0x1AA7,0x1AA7}, {0x1B05,0x1B33},
    {0x1B45,0x1B4C}, {0x1B50,0x1B59}, {0x1B83,0x1BA0}, {0x1BAE,0x1BE5}, {0x1C00,0x1C23}, {0x1C40,0x1C49},
    {0x1C4D,0x1C7D}, {0x1C80,0x1C8A}, {0x1C90,0x1CBA}, {0x1CBD,0x1CBF}, {0x1CE9,0x1CEC}, {0x1CEE,0x1CF3},
    {0x1CF5,0x1CF6}, {0x1CFA,0x1CFA}, {0x1D00,0x1DBF}, {0x1E00,0x1F15}, {0x1F18,0x1F1D}, {0x1F20,0x1F45},
    {0x1F48,0x1F4D}, {0x1F50,0x1F57}, {0x1F59,0x1F59}, {0x1F5B,0x1F5B}, {0x1F5D,0x1F5D}, {0x1F5F,0x1F7D},
    {0x1F80,0x1FB4}, {0x1FB6,0x1FBC}, {0x1FBE,0x1FBE}, {0x1FC2,0x1FC4}, {0x1FC6,0x1FCC}, {0x1FD0,0x1FD3},
    {0x1FD6,0x1FDB}, {0x1FE0,0x1FEC}, {0x1FF2,0x1FF4}, {0x1FF6,0x1FFC}, {0x2070,0x2071}, {0x2074,0x2079},
    {0x207F,0x2089}, {0x2090,0x209C}, {0x2102,0x2102}, {0x2107,0x2107}, {0x210A,0x2113}, {0x2115,0x2115},
    {0x2119,0x211D}, {0x2124,0x2124}, {0x2126,0x2126}, {0x2128,0x2128}, {0x212A,0x212D}, {0x212F,0x2139},
    {0x213C,0x213F}, {0x2145,0x2149}, {0x214E,0x214E}, {0x2150,0x2189}, {0x2460,0x249B}, {0x24EA,0x24FF},
    {0x2776,0x2793}, {0x2C00,0x2CE4}, {0x2CEB,0x2CEE}, {0x2CF2,0x2CF3}, {0x2CFD,0x2CFD}, {0x2D00,0x2D25},
    {0x2D27,0x2D27}, {0x2D2D,0x2D2D}, {0x2D30,0x2D67}, {0x2D6F,0x2D6F}, {0x2D80,0x2D96}, {0x2DA0,0x2DA6},
    {0x2DA8,0x2DAE}, {0x2DB0,0x2DB6}, {0x2DB8,0x2DBE}, {0x2DC0,0x2DC6}, {0x2DC8,0x2DCE}, {0x2DD0,0x2DD6},
    {0x2DD8,0x2DDE}, {0x2E2F,0x2E2F}, {0x3005,0x3007}, {0x3021,0x3029}, {0x3031,0x3035}, {0x3038,0x303C},
    {0x3041,0x3096}, {0x309D,0x309F}, {0x30A1,0x30FA}, {0x30FC,0x30FF}, {0x3105,0x312F}, {0x3131,0x318E},
    {0x3192,0x3195}, {0x31A0,0x31BF}, {0x31F0,0x31FF}, {0x3220,0x3229}, {0x3248,0x324F}, {0x3251,0x325F},
    {0x3280,0x3289}, {0x32B1,0x32BF}, {0x3400,0x4DBF}, {0x4E00,0xA48C}, {0xA4D0,0xA4FD}, {0xA500,0xA60C},
    {0xA610,0xA62B}, {0xA640,0xA66E}, {0xA67F,0xA69D}, {0xA6A0,0xA6EF}, {0xA717,0xA71F}, {0xA722,0xA788},
    {0xA78B,0xA7CD}, {0xA7D0,0xA7D1}, {0xA7D3,0xA7D3}, {0xA7D5,0xA7DC}, {0xA7F2,0xA801}, {0xA803,0xA805},
    {0xA807,0xA80A}, {0xA80C,0xA822}, {0xA830,0xA835}, {0xA840,0xA873}, {0xA882,0xA8B3}, {0xA8D0,0xA8D9},
    {0xA8F2,0xA8F7}, {0xA8FB,0xA8FB}, {0xA8FD,0xA8FE}, {0xA900,0xA925}, {0xA930,0xA946}, {0xA960,0xA97C},
    {0xA984,0xA9B2}, {0xA9CF,0xA9D9}, {0xA9E0,0xA9E4}, {0xA9E6,0xA9FE}, {0xAA00,0xAA28}, {0xAA40,0xAA42},
    {0xAA44,0xAA4B}, {0xAA50,0xAA59}, {0xAA60,0xAA76}, {0xAA7A,0xAA7A}, {0xAA7E,0xAAAF}, {0xAAB1,0xAAB1},
    {0xAAB5,0xAAB6}, {0xAAB9,0xAABD}, {0xAAC0,0xAAC0}, {0xAAC2,0xAAC2}, {0xAADB,0xAADD}, {0xAAE0,0xAAEA},
    {0xAAF2,0xAAF4}, {0xAB01,0xAB06}, {0xAB09,0xAB0E}, {0xAB11,0xAB16}, {0xAB20,0xAB26}, {0xAB28,0xAB2E},
    {0xAB30,0xAB5A}, {0xAB5C,0xAB69}, {0xAB70,0xABE2}, {0xABF0,0xABF9}, {0xAC00,0xD7A3}, {0xD7B0,0xD7C6},
    {0xD7CB,0xD7FB}, {0xF900,0xFA6D}, {0xFA70,0xFAD9}, {0xFB00,0xFB06}, {0xFB13,0xFB17}, {0xFB1D,0xFB1D},
    {0xFB1F,0xFB28}, {0xFB2A,0xFB36}, {0xFB38,0xFB3C}, {0xFB3E,0xFB3E}, {0xFB40,0xFB41}, {0xFB43,0xFB44},
    {0xFB46,0xFBB1}, {0xFBD3,0xFD3D}, {0xFD50,0xFD8F}, {0xFD92,0xFDC7}, {0xFDF0,0xFDFB}, {0xFE70,0xFE74},
    {0xFE76,0xFEFC}, {0xFF10,0xFF19}, {0xFF21,0xFF3A}, {0xFF41,0xFF5A}, {0xFF66,0xFFBE}, {0xFFC2,0xFFC7},
    {0xFFCA,0xFFCF}, {0xFFD2,0xFFD7}, {0xFFDA,0xFFDC}, {0x10000,0x1000B}, {0x1000D,0x10026}, {0x10028,0x1003A},
    {0x1003C,0x1003D}, {0x1003F,0x1004D}, {0x10050,0x1005D}, {0x10080,0x100FA}, {0x10107,0x10133}, {0x10140,0x10178},
    {0x1018A,0x1018B}, {0x10280,0x1029C}, {0x102A0,0x102D0}, {0x102E1,0x102FB}, {0x10300,0x10323}, {0x1032D,0x1034A},
    {0x10350,0x10375}, {0x10380,0x1039D}, {0x103A0,0x103C3}, {0x103C8,0x103CF}, {0x103D1,0x103D5}, {0x10400,0x1049D},
    {0x104A0,0x104A9}, {0x104B0,0x104D3}, {0x104D8,0x104FB}, {0x10500,0x10527}, {0x10530,0x10563}, {0x10570,0x1057A},
    {0x1057C,0x1058A}, {0x1058C,0x10592}, {0x10594,0x10595}, {0x10597,0x105A1}, {0x105A3,0x105B1}, {0x105B3,0x105B9},
    {0x105BB,0x105BC}, {0x105C0,0x105F3}, {0x10600,0x10736}, {0x10740,0x10755}, {0x10760,0x10767}, {0x10780,0x10785},
    {0x10787,0x107B0}, {0x107B2,0x107BA}, {0x10800,0x10805}, {0x10808,0x10808}, {0x1080A,0x10835}, {0x10837,0x10838},
    {0x1083C,0x1083C}, {0x1083F,0x10855}, {0x10858,0x10876}, {0x10879,0x1089E}, {0x108A7,0x108AF}, {0x108E0,0x108F2},
    {0x108F4,0x108F5}, {0x108FB,0x1091B}, {0x10920,0x10939}, {0x10980,0x109B7}, {0x109BC,0x109CF}, {0x109D2,0x10A00},
    {0x10A10,0x10A13}, {0x10A15,0x10A17}, {0x10A19,0x10A35}, {0x10A40,0x10A48}, {0x10A60,0x10A7E}, {0x10A80,0x10A9F},
    {0x10AC0,0x10AC7}, {0x10AC9,0x10AE4}, {0x10AEB,0x10AEF}, {0x10B00,0x10B35}, {0x10B40,0x10B55}, {0x10B58,0x10B72},
    {0x10B78,0x10B91}, {0x10BA9,0x10BAF}, {0x10C00,0x10C48}, {0x10C80,0x10CB2}, {0x10CC0,0x10CF2}, {0x10CFA,0x10D23},
    {0x10D30,0x10D39}, {0x10D40,0x10D65}, {0x10D6F,0x10D85}, {0x10E60,0x10E7E}, {0x10E80,0x10EA9}, {0x10EB0,0x10EB1},
    {0x10EC2,0x10EC4}, {0x10F00,0x10F27}, {0x10F30,0x10F45}, {0x10F51,0x10F54}, {0x10F70,0x10F81}, {0x10FB0,0x10FCB},
    {0x10FE0,0x10FF6}, {0x11003,0x11037}, {0x11052,0x1106F}, {0x11071,0x11072}, {0x11075,0x11075}, {0x11083,0x110AF},
    {0x110D0,0x110E8}, {0x110F0,0x110F9}, {0x11103,0x11126}, {0x11136,0x1113F}, {0x11144,0x11144}, {0x11147,0x11147},
    {0x11150,0x11172}, {0x11176,0x11176}, {0x11183,0x111B2}, {0x111C1,0x111C4}, {0x111D0,0x111DA}, {0x111DC,0x111DC},
    {0x111E1,0x111F4}, {0x11200,0x11211}, {0x11213,0x1122B}, {0x1123F,0x11240}, {0x11280,0x11286}, {0x11288,0x11288},
    {0x1128A,0x1128D}, {0x1128F,0x1129D}, {0x1129F,0x112A8}, {0x112B0,0x112DE}, {0x112F0,0x112F9}, {0x11305,0x1130C},
    {0x1130F,0x11310}, {0x11313,0x11328}, {0x1132A,0x11330}, {0x11332,0x11333}, {0x11335,0x11339}, {0x1133D,0x1133D},
    {0x11350,0x11350}, {0x1135D,0x11361}, {0x11380,0x11389}, {0x1138B,0x1138B}, {0x1138E,0x1138E}, {0x11390,0x113B5},
    {0x113B7,0x113B7}, {0x113D1,0x113D1}, {0x113D3,0x113D3}, {0x11400,0x11434}, {0x11447,0x1144A}, {0x11450,0x11459},
    {0x1145F,0x11461}, {0x11480,0x114AF}, {0x114C4,0x114C5}, {0x114C7,0x114C7}, {0x114D0,0x114D9}, {0x11580,0x115AE},
    {0x115D8,0x115DB}, {0x11600,0x1162F}, {0x11644,0x11644}, {0x11650,0x11659}, {0x11680,0x116AA}, {0x116B8,0x116B8},
    {0x116C0,0x116C9}, {0x116D0,0x116E3}, {0x11700,0x1171A}, {0x11730,0x1173B}, {0x11740,0x11746}, {0x11800,0x1182B},
    {0x118A0,0x118F2}, {0x118FF,0x11906}, {0x11909,0x11909}, {0x1190C,0x11913}, {0x11915,0x11916}, {0x11918,0x1192F},
    {0x1193F,0x1193F}, {0x11941,0x11941}, {0x11950,0x11959}, {0x119A0,0x119A7}, {0x119AA,0x119D0}, {0x119E1,0x119E1},
    {0x119E3,0x119E3}, {0x11A00,0x11A00}, {0x11A0B,0x11A32}, {0x11A3A,0x11A3A}, {0x11A50,0x11A50}, {0x11A5C,0x11A89},
    {0x11A9D,0x11A9D}, {0x11AB0,0x11AF8}, {0x11BC0,0x11BE0}, {0x11BF0,0x11BF9}, {0x11C00,0x11C08}, {0x11C0A,0x11C2E},
    {0x11C40,0x11C40}, {0x11C50,0x11C6C}, {0x11C72,0x11C8F}, {0x11D00,0x11D06}, {0x11D08,0x11D09}, {0x11D0B,0x11D30},
    {0x11D46,0x11D46}, {0x11D50,0x11D59}, {0x11D60,0x11D65}, {0x11D67,0x11D68}, {0x11D6A,0x11D89}, {0x11D98,0x11D98},
    {0x11DA0,0x11DA9}, {0x11EE0,0x11EF2}, {0x11F02,0x11F02}, {0x11F04,0x11F10}, {0x11F12,0x11F33}, {0x11F50,0x11F59},
    {0x11FB0,0x11FB0}, {0x11FC0,0x11FD4}, {0x12000,0x12399}, {0x12400,0x1246E}, {0x12480,0x12543}, {0x12F90,0x12FF0},
    {0x13000,0x1342F}, {0x13441,0x13446}, {0x13460,0x143FA}, {0x14400,0x14646}, {0x16100,0x1611D}, {0x16130,0x16139},
    {0x16800,0x16A38}, {0x16A40,0x16A5E}, {0x16A60,0x16A69}, {0x16A70,0x16ABE}, {0x16AC0,0x16AC9}, {0x16AD0,0x16AED},
    {0x16B00,0x16B2F}, {0x16B40,0x16B43}, {0x16B50,0x16B59}, {0x16B5B,0x16B61}, {0x16B63,0x16B77}, {0x16B7D,0x16B8F},
    {0x16D40,0x16D6C}, {0x16D70,0x16D79}, {0x16E40,0x16E96}, {0x16F00,0x16F4A}, {0x16F50,0x16F50}, {0x16F93,0x16F9F},
    {0x16FE0,0x16FE1}, {0x16FE3,0x16FE3}, {0x17000,0x187F7}, {0x18800,0x18CD5}, {0x18CFF,0x18D08}, {0x1AFF0,0x1AFF3},
    {0x1AFF5,0x1AFFB}, {0x1AFFD,0x1AFFE}, {0x1B000,0x1B122}, {0x1B132,0x1B132}, {0x1B150,0x1B152}, {0x1B155,0x1B155},
    {0x1B164,0x1B167}, {0x1B170,0x1B2FB}, {0x1BC00,0x1BC6A}, {0x1BC70,0x1BC7C}, {0x1BC80,0x1BC88}, {0x1BC90,0x1BC99},
    {0x1CCF0,0x1CCF9}, {0x1D2C0,0x1D2D3}, {0x1D2E0,0x1D2F3}, {0x1D360,0x1D378}, {0x1D400,0x1D454}, {0x1D456,0x1D49C},
    {0x1D49E,0x1D49F}, {0x1D4A2,0x1D4A2}, {0x1D4A5,0x1D4A6}, {0x1D4A9,0x1D4AC}, {0x1D4AE,0x1D4B9}, {0x1D4BB,0x1D4BB},
    {0x1D4BD,0x1D4C3}, {0x1D4C5,0x1D505}, {0x1D507,0x1D50A}, {0x1D50D,0x1D514}, {0x1D516,0x1D51C}, {0x1D51E,0x1D539},
    {0x1D53B,0x1D53E}, {0x1D540,0x1D544}, {0x1D546,0x1D546}, {0x1D54A,0x1D550}, {0x1D552,0x1D6A5}, {0x1D6A8,0x1D6C0},
    {0x1D6C2,0x1D6DA}, {0x1D6DC,0x1D6FA}, {0x1D6FC,0x1D714}, {0x1D716,0x1D734}, {0x1D736,0x1D74E}, {0x1D750,0x1D76E},
    {0x1D770,0x1D788}, {0x1D78A,0x1D7A8}, {0x1D7AA,0x1D7C2}, {0x1D7C4,0x1D7CB}, {0x1D7CE,0x1D7FF}, {0x1DF00,0x1DF1E},
    {0x1DF25,0x1DF2A}, {0x1E030,0x1E06D}, {0x1E100,0x1E12C}, {0x1E137,0x1E13D}, {0x1E140,0x1E149}, {0x1E14E,0x1E14E},
    {0x1E290,0x1E2AD}, {0x1E2C0,0x1E2EB}, {0x1E2F0,0x1E2F9}, {0x1E4D0,0x1E4EB}, {0x1E4F0,0x1E4F9}, {0x1E5D0,0x1E5ED},
    {0x1E5F0,0x1E5FA}, {0x1E7E0,0x1E7E6}, {0x1E7E8,0x1E7EB}, {0x1E7ED,0x1E7EE}, {0x1E7F0,0x1E7FE}, {0x1E800,0x1E8C4},
    {0x1E8C7,0x1E8CF}, {0x1E900,0x1E943}, {0x1E94B,0x1E94B}, {0x1E950,0x1E959}, {0x1EC71,0x1ECAB}, {0x1ECAD,0x1ECAF},
    {0x1ECB1,0x1ECB4}, {0x1ED01,0x1ED2D}, {0x1ED2F,0x1ED3D}, {0x1EE00,0x1EE03}, {0x1EE05,0x1EE1F}, {0x1EE21,0x1EE22},
    {0x1EE24,0x1EE24}, {0x1EE27,0x1EE27}, {0x1EE29,0x1EE32}, {0x1EE34,0x1EE37}, {0x1EE39,0x1EE39}, {0x1EE3B,0x1EE3B},
    {0x1EE42,0x1EE42}, {0x1EE47,0x1EE47}, {0x1EE49,0x1EE49}, {0x1EE4B,0x1EE4B}, {0x1EE4D,0x1EE4F}, {0x1EE51,0x1EE52},
    {0x1EE54,0x1EE54}, {0x1EE57,0x1EE57}, {0x1EE59,0x1EE59}, {0x1EE5B,0x1EE5B}, {0x1EE5D,0x1EE5D}, {0x1EE5F,0x1EE5F},
    {0x1EE61,0x1EE62}, {0x1EE64,0x1EE64}, {0x1EE67,0x1EE6A}, {0x1EE6C,0x1EE72}, {0x1EE74,0x1EE77}, {0x1EE79,0x1EE7C},
    {0x1EE7E,0x1EE7E}, {0x1EE80,0x1EE89}, {0x1EE8B,0x1EE9B}, {0x1EEA1,0x1EEA3}, {0x1EEA5,0x1EEA9}, {0x1EEAB,0x1EEBB},
    {0x1F100,0x1F10C}, {0x1FBF0,0x1FBF9}, {0x20000,0x2A6DF}, {0x2A700,0x2B739}, {0x2B740,0x2B81D}, {0x2B820,0x2CEA1},
    {0x2CEB0,0x2EBE0}, {0x2EBF0,0x2EE5D}, {0x2F800,0x2FA1D}, {0x30000,0x3134A}, {0x31350,0x323AF},
};

static bool ts2_cp_isalnum(unsigned int cp) {
    if ((cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) return true;
    if (cp < 0x80) return false;
    size_t lo = 0, hi = sizeof(TS2_ALNUM_RANGES) / sizeof(TS2_ALNUM_RANGES[0]);
    while (lo < hi) {
        size_t m = lo + (hi - lo) / 2;
        if (cp < TS2_ALNUM_RANGES[m][0]) hi = m;
        else if (cp > TS2_ALNUM_RANGES[m][1]) lo = m + 1;
        else return true;
    }
    return false;
}

/* Decode one UTF-8 code point from s[0..avail); invalid bytes decode to the
 * raw byte with *len == 1, so each bad byte maps to a single '_' like LIVE. */
static unsigned int ts2_utf8_cp(const unsigned char *s, size_t avail, size_t *len) {
    unsigned char c0 = s[0];
    if (c0 < 0x80) { *len = 1; return c0; }
    size_t need; unsigned int cp;
    if ((c0 & 0xE0) == 0xC0) { need = 2; cp = c0 & 0x1Fu; }
    else if ((c0 & 0xF0) == 0xE0) { need = 3; cp = c0 & 0x0Fu; }
    else if ((c0 & 0xF8) == 0xF0) { need = 4; cp = c0 & 0x07u; }
    else { *len = 1; return c0; }
    if (need > avail) { *len = 1; return c0; }
    for (size_t j = 1; j < need; j++) {
        if ((s[j] & 0xC0) != 0x80) { *len = 1; return c0; }
        cp = (cp << 6) | (s[j] & 0x3Fu);
    }
    *len = need;
    return cp;
}

/* PoP: ts2_spawn_tree_session_dir @ tui_gateway/server.py:_spawn_tree_session_dir */
static json_t *ts2_spawn_tree_session_dir(ts2_env *e, const json_t *in) {
    const char *sid = json_get_str(in, "session_id", "");
    if (!sid) sid = "";
    json_t *root = ts2_call0(e, "spawn_trees_root");
    if (ts2_is_raise(root)) return ts2_prop(e, root);
    const char *rs = ts2_str(root);
    /* sanitize per UNICODE CODE POINT, not per UTF-8 byte: one '_' for each
     * non-alnum code point (isalnum() or '-'/'_' preserved); an emoji (4-byte
     * code point) becomes a single '_'. */
    size_t n = strlen(sid);
    char *san = (char *)malloc(n + 1);
    if (!san) { json_free(root); return json_null(); }
    size_t o = 0;
    for (size_t i = 0; i < n; ) {
        size_t clen = 1;
        unsigned int cp = ts2_utf8_cp((const unsigned char *)sid + i, n - i, &clen);
        bool keep = ts2_cp_isalnum(cp) || cp == '-' || cp == '_';
        if (keep) { memcpy(san + o, sid + i, clen); o += clen; }
        else { san[o++] = '_'; }
        i += clen;
    }
    san[o] = '\0';
    const char *leaf = n ? san : "unknown";   /* borrow: freed exactly once below */
    char *p = ts2_path_join(rs ? rs : "", leaf);
    free(san);
    json_free(root);
    json_t *a = json_object();
    json_set(a, "path", json_string(p));
    json_set(a, "parents", json_bool(true));
    json_set(a, "exist_ok", json_bool(true));
    json_t *r = ts2_call(e, "mkdir", a);
    if (ts2_is_raise(r)) { free(p); return ts2_prop(e, r); }
    json_free(r);
    json_t *ret = json_string(p);
    free(p);
    return ret;
}
