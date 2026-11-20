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

/* PoP: ts2_spawn_tree_session_dir @ tui_gateway/server.py:_spawn_tree_session_dir */
static json_t *ts2_spawn_tree_session_dir(ts2_env *e, const json_t *in) {
    const char *sid = json_get_str(in, "session_id", "");
    if (!sid) sid = "";
    json_t *root = ts2_call0(e, "spawn_trees_root");
    if (ts2_is_raise(root)) return ts2_prop(e, root);
    const char *rs = ts2_str(root);
    /* sanitize: keep [A-Za-z0-9-_], replace everything else with '_' (ASCII domain). */
    size_t n = strlen(sid);
    char *san = (char *)malloc(n + 1);
    if (!san) { json_free(root); return json_null(); }
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)sid[i];
        bool keep = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                 || (c >= '0' && c <= '9') || c == '-' || c == '_';
        san[i] = keep ? (char)c : '_';
    }
    san[n] = '\0';
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
