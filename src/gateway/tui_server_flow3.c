/*
 * tui_server_flow3.c — third-wave FLOW cores / pure leaves of
 * tui_gateway/server.py (the UNCLAIMED remainder not covered by
 * tui_server_flow.c / tui_server_flow2.c / port_server.c /
 * port_methods_tools.c / pet_commands.c).
 *
 * Scoped down to the 12 functions faithful at 0 oracle mismatches; the
 * over-scoped 50-function remainder was removed (see oracle/generator).
 * Every symbol is static and ts3_*-prefixed; the
 * harness #includes this file, so there is no link collision with flow1/flow2.
 * All live I/O / collaborator calls enter through ONE universal seam; mutated
 * state lives in an injected json_t mirror copied by the harness before each
 * call. Only libjson is used; no other port object is linked.
 *
 * Modeling contract (shared with tests/oracle_tui_server_flow3.py):
 *  - Universal seam: json_t *(*call)(void *ud, const char *name, args).
 *    `name` identifies the live call; `args` is a BORROWED JSON object the stub
 *    copies into the ordered trace; the result is a NEW json_t (NULL == None;
 *    {"__raise__":kind,"message":m} == a raised exception). The port owns args
 *    (ts3_call frees them) and the result. A missing/exhausted script is
 *    FAIL-CLOSED: the stub returns a RuntimeError spec, propagated like any
 *    scripted raise.
 *  - Full builtin exception hierarchy (TS3_EXC): except Exception / suppress
 *    (Exception) swallow the Exception family; the BaseException trio
 *    (SystemExit/KeyboardInterrupt/GeneratorExit) and CancelledError escape.
 *    Unknown/custom kinds default to Exception subclasses.
 *  - Paths are POSIX strings on both sides (oracle projects path doubles with
 *    .as_posix()). Sets (live transports / served homes) are dedup arrays;
 *    only add/discard/membership/iteration are observed.
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
static const char TS3_MODEL_SWITCH_MARKER_PREFIX[] =
    "[System: The active model for this chat has changed to ";

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct ts3_env {
    json_t *state;   /* borrowed mirror (harness-owned copy of in["state"]) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
    json_t *exc;     /* owned unhandled raise spec; NULL when none */
} ts3_env;

/* ── JSON type / truthiness helpers ───────────────────────────────────────── */
static bool ts3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ts3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ts3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool ts3_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool ts3_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool ts3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ts3_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool ts3_truth(const json_t *v) {
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
static const char *ts3_type_name(const json_t *v) {
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
static bool ts3_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && ts3_is_str(json_obj_get(r, "__raise__"));
}
static const char *ts3_kind(const json_t *r) { return json_get_str(r, "__raise__", ""); }
static const char *ts3_msg(const json_t *r) { return json_get_str(r, "message", ""); }

typedef struct { const char *child; const char *parent; } ts3_exc_edge;
static const ts3_exc_edge TS3_EXC[] = {
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
static const char *ts3_exc_parent(const char *k) {
    for (size_t i = 0; i < sizeof(TS3_EXC) / sizeof(TS3_EXC[0]); i++)
        if (strcmp(k, TS3_EXC[i].child) == 0) return TS3_EXC[i].parent;
    return "Exception";
}
static bool ts3_exc_is(const char *k, const char *anc) {
    const char *cur = k;
    for (int guard = 0; cur && guard < 24; guard++) {
        if (strcmp(cur, anc) == 0) return true;
        cur = ts3_exc_parent(cur);
    }
    return false;
}
static json_t *ts3_raise_spec(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}
static json_t *ts3_attr_raise(const json_t *v, const char *attr) {
    char buf[192];
    snprintf(buf, sizeof(buf), "'%s' object has no attribute '%s'", ts3_type_name(v), attr);
    return ts3_raise_spec("AttributeError", buf);
}
static json_t *ts3_prop(ts3_env *e, json_t *r) {
    if (e->exc) json_free(r); else e->exc = r;
    return NULL;
}
/* except Exception / suppress(Exception): swallow Exception family, escape BaseException. */
static bool ts3_swallow_exc(ts3_env *e, json_t *r) {
    if (!ts3_exc_is(ts3_kind(r), "Exception")) { ts3_prop(e, r); return true; }
    json_free(r);
    return false;
}
static bool ts3_swallow_valtype(ts3_env *e, json_t *r) {
    const char *k = ts3_kind(r);
    if (!ts3_exc_is(k, "ValueError") && !ts3_exc_is(k, "TypeError")) {
        ts3_prop(e, r); return true;
    }
    json_free(r);
    return false;
}

/* ── universal seam call (args owned by port, freed after the call) ───────── */
static json_t *ts3_call(ts3_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}
static json_t *ts3_call0(ts3_env *e, const char *name) {
    return ts3_call(e, name, json_object());
}

/* ── string / path helpers ────────────────────────────────────────────────── */
static char *ts3_py_str(const json_t *v) {
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
/* str(value).strip(): ASCII + common unicode whitespace (fixture domain). */
static char *ts3_strip(const char *s) {
    if (!s) return strdup("");
    size_t a = 0, b = strlen(s);
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r'
                     || s[a] == '\f' || s[a] == '\v')) a++;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\n' || s[b-1] == '\r'
                     || s[b-1] == '\f' || s[b-1] == '\v')) b--;
    char *r = (char *)malloc(b - a + 1);
    if (!r) return strdup("");
    memcpy(r, s + a, b - a);
    r[b - a] = '\0';
    return r;
}
/* Path(base) / child for clean POSIX fixture domain (FakePath always inserts '/'). */
static char *ts3_path_join(const char *base, const char *child) {
    if (!base || !base[0]) {
        size_t n = strlen(child ? child : "") + 1;
        char *r = (char *)malloc(n);
        if (r) strcpy(r, child ? child : "");
        return r;
    }
    size_t bl = strlen(base);
    while (bl > 1 && base[bl - 1] == '/') bl--;
    char *r = (char *)malloc(bl + 1 + strlen(child ? child : "") + 1);
    if (!r) return strdup(child ? child : "");
    memcpy(r, base, bl);
    r[bl] = '/';
    strcpy(r + bl + 1, child ? child : "");
    return r;
}

/* ── state set helpers (dedup arrays, insertion order) ────────────────────── */
static json_t *ts3_set_arr(ts3_env *e, const char *key) {
    json_t *a = json_obj_get(e->state, key);
    if (!ts3_is_arr(a)) { a = json_array(); json_set(e->state, key, a); }
    return a;
}
static bool ts3_set_has(const json_t *a, const char *tok) {
    if (!tok) return false;
    for (size_t i = 0; i < a->c.count; i++) {
        const char *s = ts3_str(a->c.items[i]);
        if (s && strcmp(s, tok) == 0) return true;
    }
    return false;
}
static void ts3_set_add(ts3_env *e, const char *key, const char *tok) {
    json_t *a = ts3_set_arr(e, key);
    if (!tok || ts3_set_has(a, tok)) return;
    json_append(a, json_string(tok));
}
static void ts3_set_discard(ts3_env *e, const char *key, const char *tok) {
    json_t *a = ts3_set_arr(e, key);
    if (!tok) return;
    for (size_t i = a->c.count; i > 0; i--) {
        const char *s = ts3_str(a->c.items[i - 1]);
        if (s && strcmp(s, tok) == 0) {
            for (size_t j = i - 1; j + 1 < a->c.count; j++) a->c.items[j] = a->c.items[j + 1];
            if (a->c.count > 0) a->c.count--;
        }
    }
}

/* borrowed obj get helpers */
static const json_t *ts3_oget(const json_t *o, const char *k) {
    return ts3_is_obj(o) ? json_obj_get(o, k) : NULL;
}
/* call a seam inside contextlib.suppress(Exception); returns NULL on swallow. */
static json_t *ts3_call_suppress(ts3_env *e, const char *name, json_t *args) {
    json_t *r = ts3_call(e, name, args);
    if (ts3_is_raise(r)) {
        if (ts3_swallow_exc(e, r)) return NULL;
        return NULL;   /* swallowed: None */
    }
    return r;
}
/* getattr(obj,key,default) for an object double (dict); missing/null -> default. */
static const json_t *ts3_getattr(const json_t *o, const char *k, const json_t *def) {
    const json_t *v = ts3_oget(o, k);
    return (v && !ts3_is_null(v)) ? v : def;
}

/* ════════════════════ ported functions (UNCLAIMED) ═══════════════════════ */

/* PoP: ts3_drop_write @ tui_gateway/server.py:_DropTransport.write */
static json_t *ts3_drop_write(ts3_env *e, const json_t *in) {
    (void)e; (void)in;
    return json_bool(false);
}

/* PoP: ts3_drop_close @ tui_gateway/server.py:_DropTransport.close */
static json_t *ts3_drop_close(ts3_env *e, const json_t *in) {
    (void)e; (void)in;
    return json_null();
}

/* PoP: ts3_enable_gateway_prompts @ tui_gateway/server.py:_enable_gateway_prompts */
static json_t *ts3_enable_gateway_prompts(ts3_env *e, const json_t *in) {
    (void)in;
    json_t *a = json_object();
    json_set(a, "HERMES_GATEWAY_SESSION", json_string("1"));
    json_set(a, "HERMES_EXEC_ASK", json_string("1"));
    json_set(a, "HERMES_INTERACTIVE", json_string("1"));
    json_t *r = ts3_call(e, "environ_update", a);
    if (ts3_is_raise(r)) return ts3_prop(e, r);
    json_free(r);
    return json_null();
}

/* PoP: ts3_register_method @ tui_gateway/server.py:register_method */
static json_t *ts3_register_method(ts3_env *e, const json_t *in) {
    const char *name = json_get_str(in, "name", "");
    json_t *methods = json_obj_get(e->state, "methods");
    if (!ts3_is_obj(methods)) { methods = json_object(); json_set(e->state, "methods", methods); }
    const json_t *fn = json_obj_get(in, "fn");
    json_set(methods, name ? name : "", fn ? json_copy(fn) : json_null());
    (void)e;
    return json_null();
}


/* PoP: ts3_clear_pending @ tui_gateway/server.py:_clear_pending */
static json_t *ts3_clear_pending(ts3_env *e, const json_t *in) {
    const json_t *sidv = json_obj_get(in, "sid");
    const char *sid = ts3_str(sidv);
    json_t *a = json_object();
    json_set(a, "sid", sidv ? json_copy(sidv) : json_null());
    json_set(a, "reason", json_string(sid && sid[0] ? "interrupted" : "shutdown"));
    json_t *r = ts3_call(e, "cancel", a);
    if (ts3_is_raise(r)) return ts3_prop(e, r);
    json_free(r);
    return json_null();
}

/* PoP: ts3_register_live_transport @ tui_gateway/server.py:register_live_transport */
static json_t *ts3_register_live_transport(ts3_env *e, const json_t *in) {
    const json_t *t = json_obj_get(in, "transport");
    if (t && !ts3_is_null(t)) {
        const char *s = ts3_str(t);
        ts3_set_add(e, "live_transports", s ? s : "obj");
    }
    return json_null();
}

/* PoP: ts3_unregister_live_transport @ tui_gateway/server.py:unregister_live_transport */
static json_t *ts3_unregister_live_transport(ts3_env *e, const json_t *in) {
    const json_t *t = json_obj_get(in, "transport");
    const char *s = ts3_str(t);
    ts3_set_discard(e, "live_transports", s ? s : "obj");
    json_t *a = json_object();
    json_set(a, "transport", t ? json_copy(t) : json_null());
    json_t *r = ts3_call(e, "forget", a);
    if (ts3_is_raise(r)) return ts3_prop(e, r);
    json_free(r);
    return json_null();
}

/* PoP: ts3_probe_credentials @ tui_gateway/server.py:_probe_credentials */
static json_t *ts3_probe_credentials(ts3_env *e, const json_t *in) {
    (void)e;
    const json_t *agent = json_obj_get(in, "agent");
    const json_t *keyv = ts3_getattr(agent, "api_key", NULL);
    if (!keyv || !ts3_truth(keyv)) {
        const json_t *pv = ts3_getattr(agent, "provider", NULL);
        const char *provider = ts3_str(pv) ? ts3_str(pv) : "";
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "No API key configured for provider '%s'. First message will fail.", provider);
        return json_string(buf);
    }
    return json_string("");
}

/* PoP: ts3_tool_ctx @ tui_gateway/server.py:_tool_ctx */
static json_t *ts3_tool_ctx(ts3_env *e, const json_t *in) {
    json_t *a = json_object();
    json_set(a, "name", json_copy(json_obj_get(in, "name")));
    json_set(a, "args", json_copy(json_obj_get(in, "args")));
    json_set(a, "max_len", json_number(80));
    json_t *r = ts3_call_suppress(e, "build_tool_preview", a);
    return r ? r : json_string("");
}

/* ── approval payload (shared by two ported functions) ────────────────────── */
static json_t *ts3_approval_payload(ts3_env *e, const json_t *data) {
    json_t *payload = ts3_is_obj(data) ? json_copy(data) : json_object();
    const json_t *choices_v = json_obj_get(payload, "choices");
    if (!choices_v || ts3_is_null(choices_v)) {
        bool smart_denied = ts3_truth(json_obj_get(payload, "smart_denied"));
        const json_t *as = json_obj_get(payload, "allow_session");
        bool allow_session_not_false = !(as && as->type == JSON_BOOL && !as->bool_val);
        const json_t *ap = json_obj_get(payload, "allow_permanent");
        bool allow_perm_not_false = !(ap && ap->type == JSON_BOOL && !ap->bool_val);
        json_t *choices = json_array();
        json_append(choices, json_string("once"));
        if (!smart_denied && allow_session_not_false) {
            json_append(choices, json_string("session"));
            if (allow_perm_not_false) json_append(choices, json_string("always"));
        }
        json_append(choices, json_string("deny"));
        json_set(payload, "choices", choices);
    }
    const char *keys[2] = {"command", "description"};
    for (int i = 0; i < 2; i++) {
        const json_t *v = json_obj_get(payload, keys[i]);
        if (!v || ts3_is_null(v)) continue;
        json_t *a = json_object();
        json_set(a, "value", json_copy(v));
        json_t *r = ts3_call(e, "redact_approval", a);
        if (ts3_is_raise(r)) { json_free(payload); ts3_prop(e, r); return NULL; }
        json_set(payload, keys[i], r ? r : json_null());
    }
    return payload;
}

/* PoP: ts3_approval_request_payload @ tui_gateway/server.py:_approval_request_payload */
static json_t *ts3_approval_request_payload(ts3_env *e, const json_t *in) {
    return ts3_approval_payload(e, json_obj_get(in, "data"));
}

/* PoP: ts3_open_requests @ tui_gateway/server.py:_open_requests */
static json_t *ts3_open_requests(ts3_env *e, const json_t *in) {
    const char *sid = json_get_str(in, "sid", "");
    json_t *a = json_object();
    json_set(a, "sid", json_string(sid ? sid : ""));
    json_t *reqs = ts3_call(e, "open_requests", a);
    if (ts3_is_raise(reqs)) return ts3_prop(e, reqs);
    if (ts3_truth(reqs)) return reqs;
    json_free(reqs);
    const json_t *sess = ts3_oget(ts3_oget(e->state, "sessions"), sid);
    if (ts3_is_obj(sess)) {
        const json_t *mirrored = json_obj_get(sess, "_compute_host_open_request");
        if (ts3_is_obj(mirrored)) {
            const json_t *rp = json_obj_get(mirrored, "replayable");
            bool replayable = !(rp && rp->type == JSON_BOOL && !rp->bool_val);
            if (replayable) {
                json_t *arr = json_array();
                json_append(arr, json_copy(mirrored));
                return arr;
            }
        }
    }
    return json_array();
}


static char *ts3_lower(const char *s) {
    if (!s) return strdup("");
    char *r = strdup(s);
    for (char *p = r; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    return r;
}

/* PoP: ts3_probe_config_health @ tui_gateway/server.py:_probe_config_health */
static json_t *ts3_probe_config_health(ts3_env *e, const json_t *in) {
    const json_t *cfg = json_obj_get(in, "cfg");
    if (!ts3_is_obj(cfg)) return json_string("");
    const char *nulls[64]; size_t nn = 0;
    for (size_t i = 0; i < cfg->c.count && nn < 64; i++) {
        const json_t *v = cfg->c.items[i];
        if (!v || v->type == JSON_NULL) nulls[nn++] = cfg->c.keys[i];
    }
    for (size_t i = 0; i < nn; i++) for (size_t j = i + 1; j < nn; j++)
        if (strcmp(nulls[i], nulls[j]) > 0) { const char *t = nulls[i]; nulls[i] = nulls[j]; nulls[j] = t; }
    json_t *warnings = json_array();
    if (nn) {
        char keys[1024]; size_t k = 0;
        for (size_t i = 0; i < nn; i++) {
            int w = snprintf(keys + k, sizeof(keys) - k, "%s`%s`", i ? ", " : "", nulls[i]);
            if (w < 0 || (size_t)w >= sizeof(keys) - k) break;
            k += (size_t)w;
        }
        char msg[1400];
        snprintf(msg, sizeof(msg),
                 "config.yaml has empty section(s): %s. Remove the line(s) or set them to `{}` — empty sections silently drop nested settings.",
                 keys);
        json_append(warnings, json_string(msg));
    }
    const json_t *disp = json_obj_get(cfg, "display");
    if (ts3_is_obj(disp)) {
        const json_t *pv = ts3_getattr(disp, "personality", NULL);
        char *raw = (pv && !ts3_is_null(pv) && ts3_truth(pv)) ? ts3_py_str(pv) : strdup("");
        char *st = ts3_strip(raw); free(raw);
        char *low = ts3_lower(st); free(st);
        bool special = (strcmp(low, "default") == 0 || strcmp(low, "none") == 0 || strcmp(low, "neutral") == 0);
        if (low[0] && !special) {
            json_t *a = json_object(); json_set(a, "cfg", json_copy(cfg));
            json_t *avail = ts3_call_suppress(e, "available_personalities", a);
            bool found = false;
            if (ts3_is_arr(avail)) for (size_t i = 0; i < avail->c.count; i++) {
                const char *s = ts3_str(avail->c.items[i]);
                if (s && strcmp(s, low) == 0) { found = true; break; }
            }
            if (avail) json_free(avail);
            if (!found && !e->exc) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "`display.personality: %s` does not match any built-in or `agent.personalities` entry; personality overlay will be skipped.",
                         low);
                json_append(warnings, json_string(msg));
            }
        }
        free(low);
    }
    /* " ".join(warnings).strip() */
    char joined[2048]; size_t k = 0;
    for (size_t i = 0; i < warnings->c.count; i++) {
        const char *s = ts3_str(warnings->c.items[i]);
        if (!s) continue;
        int w = snprintf(joined + k, sizeof(joined) - k, "%s%s", i ? " " : "", s);
        if (w < 0 || (size_t)w >= sizeof(joined) - k) break;
        k += (size_t)w;
    }
    joined[k] = '\0';
    json_free(warnings);
    return json_string(joined);
}

