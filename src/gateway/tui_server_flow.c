/*
 * tui_server_flow.c — FLOW cores + pure leaves of tui_gateway/server.py.
 *
 * Ports the DECISION sequences of the config-I/O cluster (_display_cfg,
 * _load_cfg, _load_cfg_raw, _save_cfg, _drop_cfg_cache, _write_config_*),
 * the blocking-prompt bridge cluster (_ask, _clarify_block,
 * _clarify_timeout_seconds, _tour_request, _preview_action_request,
 * _status_update), the dashboard isolation read, the request normalizer,
 * the model-switch markers and several pure leaves. All live I/O enters
 * through ONE universal seam; module state lives in an injected json_t
 * mirror mutated in place.
 *
 * Modeling contract (shared with tests/oracle_tui_server_flow.py):
 *  - Universal seam: json_t *(*call)(void *ud, const char *name, args).
 *    name identifies the live call (table below); args is a BORROWED JSON
 *    object the stub copies into the trace; the result is a NEW json_t
 *    (NULL == None; {"__raise__":kind,"message":m} == a raised exception).
 *    The port owns args (ts_call frees them after the call) and the result.
 *  - Seam names -> live calls:
 *      _load_cfg / _display_cfg / _active_config_path / _load_cfg_raw /
 *      _save_cfg / _drop_cfg_cache / _write_config_changes /
 *      _write_config_keys / _coerce_int_config_value /
 *      _is_model_switch_marker / _clarify_timeout_seconds / _ask / _ok /
 *      _err                         -> the named sibling function
 *      is_truthy_value              -> utils.is_truthy_value(value, default)
 *      get_hermes_home_override     -> hermes_constants.get_hermes_home_override()
 *      load_user_config_effective   -> hermes_cli.config_effective.load_user_config_effective(path)
 *      config_cache_version         -> hermes_cli.config_backend.config_cache_version(path)
 *      config_exists                -> hermes_cli.config_backend.config_exists(path)
 *      read_user_config_raw         -> hermes_cli.config.read_user_config_raw(path)
 *      atomic_config_replace        -> hermes_cli.config.atomic_config_replace(path, cfg)
 *      supports_file_tooling        -> hermes_cli.config_backend.supports_file_tooling()
 *      write_changes                -> _config_backend.write_changes(path, Changes(set,unset))
 *      _refuse_managed_keys         -> methods_config_set._refuse_managed_keys(*keys)
 *      get_clarify_timeout          -> tools.clarify_gateway.get_clarify_timeout()
 *      send                         -> server_requests.send(method, sid, params, timeout=, qids=)
 *      monotonic                    -> time.monotonic()
 *      _emit                        -> _emit(event, sid, payload)
 *      resolve_command              -> hermes_cli.commands.resolve_command(name)
 *      save_config_value            -> cli.save_config_value(key, value)
 *      log                          -> logger.<level>(fmt, *args)  (raw fmt + args)
 *  - State mirror (in["state"], copied by the harness before mutation):
 *      cfg:      {cache: obj|null, sig: any|null, path: str|null}
 *      home:     {hermes_home: str, at_import: str}
 *      sessions: {sid: {tour_bridge, preview_action_bridge,
 *                 preview_action_bridge_retry_at, ...}}
 *      reprobe:  {sid: true}   (presence of an in-flight reprobe token)
 *      mcp:      {loaded_rev: str}
 *  - Paths are POSIX strings; the oracle projects Path results with
 *    .as_posix() so Windows slash-flipping never reaches the trace.
 *  - FailedConfigRead (a dict subclass) is modelled as the empty object {}
 *    it reads as; the read_error attribute is not part of the dict content.
 *  - BaseException kinds (SystemExit, KeyboardInterrupt, GeneratorExit,
 *    CancelledError) AND ConfigBackendUnavailable (a SystemExit subclass)
 *    pass through every `except Exception` / suppress(Exception) site via
 *    env->exc. _load_cfg_raw's `except (Exception, ConfigBackendUnavailable)`
 *    additionally swallows ConfigBackendUnavailable; _save_cfg's inner
 *    `except Exception` does NOT (it propagates). Exception kinds are
 *    swallowed exactly where live swallows them.
 *  - Scripted seam results: a missing/exhausted script is fail-closed — the
 *    harness dispatcher RETURNS a {"__raise__":"RuntimeError",...} spec and
 *    records it, so the port propagates it like any scripted raise.
 *  - Self-contained: the harness #includes this .c; every symbol is static
 *    and ts_*-prefixed; only libjson is linked.
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
static const char TS_MARKER_PREFIX[] = "[System: The active model for this chat has changed to ";
static const char TS_BARE_HINT[] = "bare `hermes` is interactive \xe2\x80\x94 use `/hermes chat -q \xe2\x80\xa6` or run `hermes` in another terminal";
static const char TS_SETUP_HINT[] = "`hermes setup` needs a full terminal \xe2\x80\x94 run it outside the TUI";
static const char TS_GATEWAY_HINT[] = "`hermes gateway` is long-running \xe2\x80\x94 run it in another terminal";
static const char TS_SESS_HINT[] = "`hermes sessions browse` is interactive \xe2\x80\x94 use /resume here, or run browse in another terminal";
static const char TS_CONFIG_HINT[] = "`hermes config edit` needs $EDITOR in a real terminal";
static const char TS_TOUR_UNAVAIL[] = "{\"success\": false, \"error\": \"No Hermes Desktop window answered the tour request. The tour is driven by the desktop app's renderer, which updates separately from this backend, so an app build older than the tour tool has nothing listening. Update the Hermes Desktop app and start a new session. Do not retry tour in this session.\"}";
static const char TS_PREVIEW_UNAVAIL[] = "{\"success\": false, \"error\": \"No Hermes Desktop window answered the preview action request. The drive_preview / annotate_preview bridge is served by the desktop app's renderer, which updates separately from this backend, so an app build older than the tool has nothing listening. Update the Hermes Desktop app, open a page with open_preview, and try again in this session after a short cooldown.\"}";

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct ts_env {
    json_t *state;   /* borrowed mirror (harness-owned copy of in["state"]) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
    json_t *exc;     /* owned unhandled raise spec; NULL when none */
} ts_env;

/* ── JSON type / truthiness helpers ───────────────────────────────────────── */
static bool ts_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ts_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ts_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool ts_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool ts_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ts_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool ts_truth(const json_t *v) {
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
/* Python type name for AttributeError messages. */
static const char *ts_type_name(const json_t *v) {
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

/* ── raise specs ──────────────────────────────────────────────────────────── */
static bool ts_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && ts_is_str(json_obj_get(r, "__raise__"));
}
static const char *ts_kind(const json_t *r) { return json_get_str(r, "__raise__", ""); }
static const char *ts_msg(const json_t *r) { return json_get_str(r, "message", ""); }
/* BaseException-only kinds pass through `except Exception`. ConfigBackendUnavailable
 * is a SystemExit subclass, so it too passes a bare `except Exception`. */
static bool ts_is_base(const char *k) {
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0 || strcmp(k, "CancelledError") == 0
        || strcmp(k, "ConfigBackendUnavailable") == 0;
}
static json_t *ts_raise_spec(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}
static json_t *ts_attr_raise(const json_t *v, const char *attr) {
    char buf[160];
    snprintf(buf, sizeof(buf), "'%s' object has no attribute '%s'", ts_type_name(v), attr);
    return ts_raise_spec("AttributeError", buf);
}
static json_t *ts_keyerror_raise(const char *key) {
    char buf[160];
    snprintf(buf, sizeof(buf), "'%s'", key);
    return ts_raise_spec("KeyError", buf);
}
/* Park an unhandled raise (first wins) and return NULL. */
static json_t *ts_prop(ts_env *e, json_t *r) {
    if (e->exc) json_free(r); else e->exc = r;
    return NULL;
}
/* `except Exception` / suppress(Exception): swallow plain Exception (free r),
 * propagate BaseException. Returns true when the caller must propagate. */
static bool ts_swallow_exc(ts_env *e, json_t *r) {
    if (ts_is_base(ts_kind(r))) { ts_prop(e, r); return true; }
    json_free(r);
    return false;
}
/* _load_cfg_raw's `except (Exception, ConfigBackendUnavailable)`: also swallows
 * ConfigBackendUnavailable; only the other BaseException kinds propagate. */
static bool ts_swallow_cfgread(ts_env *e, json_t *r) {
    const char *k = ts_kind(r);
    if (ts_is_base(k) && strcmp(k, "ConfigBackendUnavailable") != 0) { ts_prop(e, r); return true; }
    json_free(r);
    return false;
}

/* ── universal seam call (args owned by port, freed after the call) ───────── */
static json_t *ts_call(ts_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}

/* ── state mirror access (borrowed) ───────────────────────────────────────── */
static json_t *ts_cfg(ts_env *e) { return json_obj_get(e->state, "cfg"); }
static json_t *ts_home(ts_env *e) { return json_obj_get(e->state, "home"); }
static json_t *ts_sessions(ts_env *e) { return json_obj_get(e->state, "sessions"); }
static json_t *ts_reprobe(ts_env *e) { return json_obj_get(e->state, "reprobe"); }
static json_t *ts_mcp(ts_env *e) { return json_obj_get(e->state, "mcp"); }

/* ── deep JSON equality (order-independent objects) ───────────────────────── */
static bool ts_json_eq(const json_t *a, const json_t *b) {
    if (ts_is_null(a) && ts_is_null(b)) return true;
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL:   return true;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!ts_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !ts_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        }
    }
    return false;
}

/* ── unicode string helpers ───────────────────────────────────────────────── */
static unsigned long ts_cp(const char *s, size_t *adv) {
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
static bool ts_ws(unsigned long cp) {
    return (cp >= 0x09 && cp <= 0x0D) || cp == 0x20 || (cp >= 0x1C && cp <= 0x1F)
        || cp == 0x85 || cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)
        || cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}
static char *ts_strip(const char *s) {
    if (!s) return strdup("");
    size_t start = 0, last = 0, i = 0;
    bool any = false;
    while (s[i]) {
        size_t adv; unsigned long cp = ts_cp(s + i, &adv);
        if (!ts_ws(cp)) { if (!any) { start = i; any = true; } last = i + adv; }
        i += adv;
    }
    if (!any) return strdup("");
    char *r = (char *)malloc(last - start + 1);
    if (!r) return strdup("");
    memcpy(r, s + start, last - start);
    r[last - start] = '\0';
    return r;
}
static char *ts_lower(const char *s) {
    char *r = strdup(s ? s : "");
    for (char *p = r; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return r;
}
/* Path(base) / child for the clean absolute-path fixture domain: strip trailing
 * slashes from base, append "/" + child; empty base -> child (relative). */
static char *ts_path_join(const char *base, const char *child) {
    if (!base || !base[0]) return strdup(child);
    size_t bl = strlen(base);
    while (bl > 1 && base[bl - 1] == '/') bl--;
    char *r = (char *)malloc(bl + 1 + strlen(child) + 1);
    if (!r) return strdup(child);
    memcpy(r, base, bl);
    r[bl] = '/';
    strcpy(r + bl + 1, child);
    return r;
}

/* ── int()/float() coercion (Python semantics) ────────────────────────────── */
/* int(value): bool->0/1, number->truncate toward zero, str->base-10 parse with
 * underscores-between-digits; null/container -> TypeError (false). */
static bool ts_int_coerce(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return true; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) return false;   /* OverflowError domain (not fixtured) */
        *out = (long long)d;              /* C cast truncates toward zero */
        return true;
    }
    if (v->type != JSON_STRING) return false;
    char *s = ts_strip(v->str_val ? v->str_val : "");
    const char *p = s;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!*p) { free(s); return false; }
    /* digits with underscores strictly between digits */
    char digits[128]; size_t dn = 0;
    const char *q = p;
    bool prev_digit = false;
    for (; *q; q++) {
        if (*q >= '0' && *q <= '9') {
            if (dn < sizeof(digits) - 1) digits[dn++] = *q;
            prev_digit = true;
        } else if (*q == '_') {
            if (!prev_digit || !(q[1] >= '0' && q[1] <= '9')) { free(s); return false; }
            prev_digit = false;
        } else { free(s); return false; }
    }
    if (!prev_digit || dn == 0) { free(s); return false; }  /* trailing '_' or empty */
    digits[dn] = '\0';
    char *end = NULL;
    long long val = strtoll(digits, &end, 10);
    free(s);
    if (!end || *end) return false;
    *out = neg ? -val : val;
    return true;
}
/* float(value): number->double, bool->0/1, str->strtod after strip + underscore
 * cleanup; null/container -> TypeError (false). Invalid str -> ValueError (false). */
static bool ts_float_coerce(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return true; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return true; }
    if (v->type != JSON_STRING) return false;
    char *s = ts_strip(v->str_val ? v->str_val : "");
    /* remove underscores between digits (Python float accepts 1_0.5) */
    char clean[128]; size_t cn = 0;
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

/* ── Python json.dumps(value, ensure_ascii=False), default separators ─────── */
static void ts_dumps_str(const char *s, char **buf, size_t *len, size_t *cap) {
    #define TS_PUT(ch) do { if (*len + 1 >= *cap) { *cap = *cap * 2 + 16; *buf = (char*)realloc(*buf, *cap); } (*buf)[(*len)++] = (ch); } while (0)
    TS_PUT('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '"') { TS_PUT('\\'); TS_PUT('"'); }
        else if (c == '\\') { TS_PUT('\\'); TS_PUT('\\'); }
        else if (c == '\n') { TS_PUT('\\'); TS_PUT('n'); }
        else if (c == '\r') { TS_PUT('\\'); TS_PUT('r'); }
        else if (c == '\t') { TS_PUT('\\'); TS_PUT('t'); }
        else if (c == '\b') { TS_PUT('\\'); TS_PUT('b'); }
        else if (c == '\f') { TS_PUT('\\'); TS_PUT('f'); }
        else if (c < 0x20) {
            char hex[8]; snprintf(hex, sizeof(hex), "\\u%04x", c);
            for (char *h = hex; *h; h++) TS_PUT(*h);
        } else TS_PUT((char)c);   /* ensure_ascii=False: raw UTF-8 */
    }
    TS_PUT('"');
    #undef TS_PUT
}
static void ts_dumps(const json_t *v, char **buf, size_t *len, size_t *cap) {
    #define TS_PUTS(s) do { const char *_s = (s); for (; *_s; _s++) { if (*len + 1 >= *cap) { *cap = *cap * 2 + 16; *buf = (char*)realloc(*buf, *cap); } (*buf)[(*len)++] = *_s; } } while (0)
    if (!v || v->type == JSON_NULL) { TS_PUTS("null"); return; }
    switch (v->type) {
        case JSON_NULL: TS_PUTS("null"); return;
        case JSON_BOOL: TS_PUTS(v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            char num[64]; double d = v->num_val;
            if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
                snprintf(num, sizeof(num), "%lld", (long long)d);
            else
                snprintf(num, sizeof(num), "%.17g", d);
            TS_PUTS(num); return;
        }
        case JSON_STRING: ts_dumps_str(v->str_val ? v->str_val : "", buf, len, cap); return;
        case JSON_ARRAY: {
            TS_PUTS("[");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) TS_PUTS(", ");
                ts_dumps(v->c.items[i], buf, len, cap);
            }
            TS_PUTS("]"); return;
        }
        case JSON_OBJECT: {
            TS_PUTS("{");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) TS_PUTS(", ");
                ts_dumps_str(v->c.keys[i], buf, len, cap);
                TS_PUTS(": ");
                ts_dumps(v->c.items[i], buf, len, cap);
            }
            TS_PUTS("}"); return;
        }
    }
    #undef TS_PUTS
}
static char *ts_json_dumps(const json_t *v) {
    size_t cap = 64, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return strdup("");
    ts_dumps(v, &buf, &len, &cap);
    if (len + 1 >= cap) { cap = len + 2; buf = (char *)realloc(buf, cap); }
    buf[len] = '\0';
    return buf;
}
/* ── JSON-RPC envelope leaves ─────────────────────────────────────────────── */
/* PoP: ts_ok @ tui_gateway/server.py:_ok */
static json_t *ts_ok(ts_env *e, const json_t *in) {
    (void)e;
    const json_t *rid = json_obj_get(in, "rid");
    const json_t *result = json_obj_get(in, "result");
    json_t *o = json_object();
    json_set(o, "jsonrpc", json_string("2.0"));
    json_set(o, "id", rid ? json_copy(rid) : json_null());
    json_set(o, "result", result ? json_copy(result) : json_null());
    return o;
}

/* _err is a CLAIMED sibling (port_methods_tools.c) — seamed, not re-ported. */
static json_t *ts_err_call(ts_env *e, const json_t *rid, int code, const char *msg) {
    json_t *a = json_object();
    json_set(a, "rid", rid ? json_copy(rid) : json_null());
    json_set(a, "code", json_number(code));
    json_set(a, "msg", json_string(msg));
    json_t *r = ts_call(e, "_err", a);
    if (ts_is_raise(r)) return ts_prop(e, r);
    return r;
}

/* PoP: ts_normalize_request @ tui_gateway/server.py:_normalize_request */
static json_t *ts_normalize_request(ts_env *e, const json_t *in) {
    const json_t *req = json_obj_get(in, "req");
    if (!ts_is_obj(req))
        return ts_err_call(e, NULL, -32600, "invalid request: expected an object");
    const json_t *rid = json_obj_get(req, "id");
    const json_t *method = json_obj_get(req, "method");
    if (!ts_is_str(method) || !method->str_val || !method->str_val[0])
        return ts_err_call(e, rid, -32600, "invalid request: method must be a non-empty string");
    /* params = req.get("params", {}): absent -> {}, present-null -> None */
    bool absent = !json_has(req, "params");
    json_t *pval = absent ? json_object() : json_copy(json_obj_get(req, "params"));
    if (pval->type != JSON_NULL && pval->type != JSON_OBJECT) {
        json_free(pval);
        return ts_err_call(e, rid, -32602, "invalid params: expected an object");
    }
    json_t *ret = json_array();
    json_append(ret, rid ? json_copy(rid) : json_null());
    json_append(ret, json_copy(method));
    if (pval->type == JSON_NULL) { json_free(pval); json_append(ret, json_object()); }
    else json_append(ret, pval);   /* adopts */
    return ret;
}

/* PoP: ts_coerce_int_config_value @ tui_gateway/server.py:_coerce_int_config_value */
static json_t *ts_coerce_int_config_value(ts_env *e, const json_t *in) {
    (void)e;
    const json_t *value = json_obj_get(in, "value");
    const json_t *def = json_obj_get(in, "default");
    double minv = json_get_num(in, "min_value", 0);
    long long coerced;
    if (!ts_int_coerce(value, &coerced))   /* TypeError/ValueError -> default */
        return def ? json_copy(def) : json_null();
    if ((double)coerced >= minv) return json_number((double)coerced);
    return def ? json_copy(def) : json_null();
}

/* ── config reads ─────────────────────────────────────────────────────────── */
/* PoP: ts_display_cfg @ tui_gateway/server.py:_display_cfg */
static json_t *ts_display_cfg(ts_env *e, const json_t *in) {
    (void)in;
    json_t *cfg = ts_call(e, "_load_cfg", json_object());
    if (ts_is_raise(cfg)) return ts_prop(e, cfg);
    if (!ts_is_obj(cfg)) { json_t *ar = ts_attr_raise(cfg, "get"); json_free(cfg); return ts_prop(e, ar); }
    json_t *display = json_obj_get(cfg, "display");
    json_t *ret = ts_is_obj(display) ? json_copy(display) : json_object();
    json_free(cfg);
    return ret;
}

/* PoP: ts_load_interim_assistant_messages @ tui_gateway/server.py:_load_interim_assistant_messages */
static json_t *ts_load_interim_assistant_messages(ts_env *e, const json_t *in) {
    (void)in;
    json_t *dc = ts_call(e, "_display_cfg", json_object());
    if (ts_is_raise(dc)) return ts_prop(e, dc);
    if (!ts_is_obj(dc)) { json_t *ar = ts_attr_raise(dc, "get"); json_free(dc); return ts_prop(e, ar); }
    json_t *v = json_obj_get(dc, "interim_assistant_messages");
    json_t *a = json_object();
    json_set(a, "value", v ? json_copy(v) : json_bool(true));   /* .get(..., True) */
    json_set(a, "default", json_bool(false));
    json_free(dc);
    json_t *r = ts_call(e, "is_truthy_value", a);
    if (ts_is_raise(r)) return ts_prop(e, r);
    return r;
}

/* PoP: ts_load_dashboard_process_isolation_config @ tui_gateway/server.py:_load_dashboard_process_isolation_config */
static json_t *ts_load_dashboard_process_isolation_config(ts_env *e, const json_t *in) {
    const json_t *cfg = json_obj_get(in, "cfg");
    json_t *root;
    bool root_owned = false;
    if (ts_is_null(cfg)) { root = ts_call(e, "_load_cfg", json_object()); root_owned = true;
        if (ts_is_raise(root)) return ts_prop(e, root); }
    else root = (json_t *)cfg;   /* borrowed */
    json_t *dash = ts_is_obj(root) ? json_obj_get(root, "dashboard") : NULL;
    if (!ts_is_obj(dash)) dash = NULL;   /* non-dict -> {} */
    /* turn_isolation = is_truthy_value(dash.get("turn_isolation"), default=False) */
    json_t *ti = dash ? json_obj_get(dash, "turn_isolation") : NULL;
    json_t *a1 = json_object();
    json_set(a1, "value", ti ? json_copy(ti) : json_null());
    json_set(a1, "default", json_bool(false));
    json_t *r1 = ts_call(e, "is_truthy_value", a1);
    if (ts_is_raise(r1)) { if (root_owned) json_free(root); return ts_prop(e, r1); }
    /* heartbeat = _coerce_int_config_value(dash.get(...), 15, min_value=1) */
    json_t *hb = dash ? json_obj_get(dash, "compute_host_heartbeat_secs") : NULL;
    json_t *a2 = json_object();
    json_set(a2, "value", hb ? json_copy(hb) : json_null());
    json_set(a2, "default", json_number(15));
    json_set(a2, "min_value", json_number(1));
    json_t *r2 = ts_call(e, "_coerce_int_config_value", a2);
    if (ts_is_raise(r2)) { json_free(r1); if (root_owned) json_free(root); return ts_prop(e, r2); }
    /* respawn = _coerce_int_config_value(dash.get(...), 3, min_value=0) */
    json_t *rm = dash ? json_obj_get(dash, "compute_host_respawn_max") : NULL;
    json_t *a3 = json_object();
    json_set(a3, "value", rm ? json_copy(rm) : json_null());
    json_set(a3, "default", json_number(3));
    json_set(a3, "min_value", json_number(0));
    json_t *r3 = ts_call(e, "_coerce_int_config_value", a3);
    if (ts_is_raise(r3)) { json_free(r1); json_free(r2); if (root_owned) json_free(root); return ts_prop(e, r3); }
    json_t *ret = json_object();
    json_set(ret, "turn_isolation", r1);   /* adopts */
    json_set(ret, "compute_host_heartbeat_secs", r2);
    json_set(ret, "compute_host_respawn_max", r3);
    if (root_owned) json_free(root);
    return ret;
}

/* PoP: ts_active_config_path @ tui_gateway/server.py:_active_config_path */
static json_t *ts_active_config_path(ts_env *e, const json_t *in) {
    (void)in;
    json_t *ov = ts_call(e, "get_hermes_home_override", json_object());
    if (ts_is_raise(ov)) return ts_prop(e, ov);
    const char *base;
    if (ts_is_str(ov) && ov->str_val && ov->str_val[0]) base = ov->str_val;
    else base = json_get_str(ts_home(e), "hermes_home", "");
    char *p = ts_path_join(base, "config.yaml");
    json_free(ov);
    json_t *ret = json_string(p);
    free(p);
    return ret;
}

/* PoP: ts_load_cfg @ tui_gateway/server.py:_load_cfg */
static json_t *ts_load_cfg(ts_env *e, const json_t *in) {
    (void)in;
    json_t *p = ts_call(e, "_active_config_path", json_object());
    if (ts_is_raise(p)) { if (ts_swallow_exc(e, p)) return NULL; return json_object(); }
    json_t *a = json_object();
    json_set(a, "path", p);   /* adopts p */
    json_t *r = ts_call(e, "load_user_config_effective", a);
    if (ts_is_raise(r)) { if (ts_swallow_exc(e, r)) return NULL; return json_object(); }
    return r;   /* verbatim (fail-open {} only on Exception) */
}

/* PoP: ts_drop_cfg_cache @ tui_gateway/server.py:_drop_cfg_cache */
static json_t *ts_drop_cfg_cache(ts_env *e, const json_t *in) {
    (void)in;
    json_t *cfg = ts_cfg(e);
    json_set(cfg, "cache", json_null());
    json_set(cfg, "sig", json_null());
    json_set(cfg, "path", json_null());
    return json_null();
}

/* PoP: ts_load_cfg_raw @ tui_gateway/server.py:_load_cfg_raw */
static json_t *ts_load_cfg_raw(ts_env *e, const json_t *in) {
    (void)in;
    json_t *p = ts_call(e, "_active_config_path", json_object());
    if (ts_is_raise(p)) { if (ts_swallow_cfgread(e, p)) return NULL; return json_object(); }
    /* sig = config_cache_version(p); inner except FileNotFoundError -> None */
    json_t *sa = json_object(); json_set(sa, "path", json_copy(p));
    json_t *sig = ts_call(e, "config_cache_version", sa);
    if (ts_is_raise(sig)) {
        if (strcmp(ts_kind(sig), "FileNotFoundError") == 0) { json_free(sig); sig = json_null(); }
        else { if (ts_swallow_cfgread(e, sig)) { json_free(p); return NULL; } json_free(p); return json_object(); }
    }
    /* cache hit: _cfg_cache is not None and _cfg_sig == sig and _cfg_path == p */
    json_t *cfg = ts_cfg(e);
    json_t *cache = json_obj_get(cfg, "cache");
    const char *cpath = json_get_str(cfg, "path", NULL);
    const char *ps = ts_str(p);
    if (cache && cache->type != JSON_NULL && ts_json_eq(json_obj_get(cfg, "sig"), sig)
            && cpath && ps && strcmp(cpath, ps) == 0) {
        json_t *ret = json_copy(cache);
        json_free(sig); json_free(p);
        return ret;
    }
    /* data = read_user_config_raw(p) if config_exists(p) else {} */
    json_t *ea = json_object(); json_set(ea, "path", json_copy(p));
    json_t *ex = ts_call(e, "config_exists", ea);
    if (ts_is_raise(ex)) { if (ts_swallow_cfgread(e, ex)) { json_free(sig); json_free(p); return NULL; } json_free(sig); json_free(p); return json_object(); }
    json_t *data;
    if (ts_truth(ex)) {
        json_free(ex);
        json_t *ra = json_object(); json_set(ra, "path", json_copy(p));
        data = ts_call(e, "read_user_config_raw", ra);
        if (ts_is_raise(data)) { if (ts_swallow_cfgread(e, data)) { json_free(sig); json_free(p); return NULL; } json_free(sig); json_free(p); return json_object(); }
    } else { json_free(ex); data = json_object(); }
    /* cache the RAW config */
    json_set(cfg, "cache", json_copy(data));
    json_set(cfg, "sig", json_copy(sig));
    json_set(cfg, "path", json_copy(p));
    json_free(sig); json_free(p);
    return data;
}

/* PoP: ts_save_cfg @ tui_gateway/server.py:_save_cfg */
static json_t *ts_save_cfg(ts_env *e, const json_t *in) {
    const json_t *cfgin = json_obj_get(in, "cfg");
    json_t *p = ts_call(e, "_active_config_path", json_object());
    if (ts_is_raise(p)) return ts_prop(e, p);
    json_t *aa = json_object();
    json_set(aa, "path", json_copy(p));
    json_set(aa, "cfg", cfgin ? json_copy(cfgin) : json_null());
    json_t *ar = ts_call(e, "atomic_config_replace", aa);
    if (ts_is_raise(ar)) { json_free(p); return ts_prop(e, ar); }
    json_free(ar);
    json_t *sf = ts_call(e, "supports_file_tooling", json_object());
    if (ts_is_raise(sf)) { json_free(p); return ts_prop(e, sf); }
    bool sft = ts_truth(sf);
    json_free(sf);
    if (!sft) {
        json_t *dr = ts_call(e, "_drop_cfg_cache", json_object());
        if (ts_is_raise(dr)) { json_free(p); return ts_prop(e, dr); }
        json_free(dr); json_free(p);
        return json_null();
    }
    json_t *cfg = ts_cfg(e);
    json_set(cfg, "cache", cfgin ? json_copy(cfgin) : json_null());
    json_set(cfg, "path", json_copy(p));
    json_t *va = json_object(); json_set(va, "path", json_copy(p));
    json_t *vr = ts_call(e, "config_cache_version", va);
    if (ts_is_raise(vr)) {
        /* inner `except Exception`: ConfigBackendUnavailable (SystemExit) propagates */
        if (ts_is_base(ts_kind(vr))) { json_free(p); return ts_prop(e, vr); }
        json_free(vr);
        json_set(cfg, "sig", json_null());
    } else {
        json_set(cfg, "sig", vr);   /* adopts */
    }
    json_free(p);
    return json_null();
}

/* ── config writes ────────────────────────────────────────────────────────── */
/* PoP: ts_write_config_changes @ tui_gateway/server.py:_write_config_changes */
static json_t *ts_write_config_changes(ts_env *e, const json_t *in) {
    const json_t *sets = json_obj_get(in, "sets");
    const json_t *unsets = json_obj_get(in, "unsets");
    json_t *p = ts_call(e, "_active_config_path", json_object());
    json_t *wc_raise = NULL;
    if (ts_is_raise(p)) { wc_raise = p; }
    else {
        json_t *changes = json_object();
        json_set(changes, "set", sets ? json_copy(sets) : json_object());
        json_set(changes, "unset", unsets ? json_copy(unsets) : json_array());
        json_t *wa = json_object();
        json_set(wa, "path", json_copy(p));
        json_set(wa, "changes", changes);   /* adopts */
        json_t *wr = ts_call(e, "write_changes", wa);
        if (ts_is_raise(wr)) wc_raise = wr; else json_free(wr);
        json_free(p);
    }
    /* finally: _drop_cfg_cache() always runs; a finally raise replaces in-flight */
    json_t *dr = ts_call(e, "_drop_cfg_cache", json_object());
    if (ts_is_raise(dr)) { if (wc_raise) json_free(wc_raise); return ts_prop(e, dr); }
    json_free(dr);
    if (wc_raise) return ts_prop(e, wc_raise);
    return json_null();
}

/* PoP: ts_write_config_key @ tui_gateway/server.py:_write_config_key */
static json_t *ts_write_config_key(ts_env *e, const json_t *in) {
    const char *kp = json_get_str(in, "key_path", "");
    const json_t *val = json_obj_get(in, "value");
    json_t *updates = json_object();
    json_set(updates, kp, val ? json_copy(val) : json_null());
    json_t *a = json_object();
    json_set(a, "updates", updates);   /* adopts */
    json_t *r = ts_call(e, "_write_config_keys", a);
    if (ts_is_raise(r)) return ts_prop(e, r);
    json_free(r);
    return json_null();
}

/* split a dotted key into segments; returns count, fills segs (malloc'd). */
static size_t ts_split_dot(const char *kp, char **segs, size_t max) {
    size_t n = 0;
    const char *start = kp;
    for (const char *q = kp; ; q++) {
        if (*q == '.' || *q == '\0') {
            if (n < max) { segs[n] = strndup(start, (size_t)(q - start)); n++; }
            if (*q == '\0') break;
            start = q + 1;
        }
    }
    return n;
}

/* PoP: ts_write_config_keys @ tui_gateway/server.py:_write_config_keys */
static json_t *ts_write_config_keys(ts_env *e, const json_t *in) {
    const json_t *updates = json_obj_get(in, "updates");
    /* _refuse_managed_keys(*updates) — splat the dict KEYS */
    json_t *keys = json_array();
    if (ts_is_obj(updates))
        for (size_t i = 0; i < updates->c.count; i++)
            json_append(keys, json_string(updates->c.keys[i]));
    json_t *ra = json_object(); json_set(ra, "keys", keys);
    json_t *rr = ts_call(e, "_refuse_managed_keys", ra);
    if (ts_is_raise(rr)) return ts_prop(e, rr);
    json_free(rr);
    json_t *sf = ts_call(e, "supports_file_tooling", json_object());
    if (ts_is_raise(sf)) return ts_prop(e, sf);
    bool sft = ts_truth(sf);
    json_free(sf);
    if (!sft) {
        json_t *ca = json_object();
        json_set(ca, "sets", updates ? json_copy(updates) : json_object());
        json_set(ca, "unsets", json_array());
        json_t *cr = ts_call(e, "_write_config_changes", ca);
        if (ts_is_raise(cr)) return ts_prop(e, cr);
        json_free(cr);
        return json_null();
    }
    json_t *cfg = ts_call(e, "_load_cfg_raw", json_object());
    if (ts_is_raise(cfg)) return ts_prop(e, cfg);
    if (!ts_is_obj(cfg)) { json_t *ar = ts_attr_raise(cfg, "get"); json_free(cfg); return ts_prop(e, ar); }
    /* for key_path, value in updates.items(): create parent dicts, set leaf */
    if (ts_is_obj(updates)) {
        for (size_t i = 0; i < updates->c.count; i++) {
            const char *kp = updates->c.keys[i];
            const json_t *value = updates->c.items[i];
            char *segs[32];
            size_t n = ts_split_dot(kp, segs, 32);
            json_t *current = cfg;
            for (size_t k = 0; k + 1 < n; k++) {
                json_t *existing = json_obj_get(current, segs[k]);
                if (!ts_is_obj(existing)) json_set(current, segs[k], json_object());
                current = json_obj_get(current, segs[k]);
            }
            if (n > 0) json_set(current, segs[n - 1], value ? json_copy(value) : json_null());
            for (size_t k = 0; k < n; k++) free(segs[k]);
        }
    }
    json_t *sa = json_object(); json_set(sa, "cfg", json_copy(cfg));
    json_t *sr = ts_call(e, "_save_cfg", sa);
    json_free(cfg);
    if (ts_is_raise(sr)) return ts_prop(e, sr);
    json_free(sr);
    return json_null();
}
/* ── model-switch markers ─────────────────────────────────────────────────── */
/* PoP: ts_is_model_switch_marker @ tui_gateway/server.py:_is_model_switch_marker */
static json_t *ts_is_model_switch_marker(ts_env *e, const json_t *in) {
    (void)e;
    const json_t *entry = json_obj_get(in, "entry");
    if (!ts_is_obj(entry)) return json_bool(false);
    const json_t *content = json_obj_get(entry, "content");
    if (!ts_is_str(content)) return json_bool(false);
    return json_bool(strncmp(content->str_val ? content->str_val : "",
                             TS_MARKER_PREFIX, sizeof(TS_MARKER_PREFIX) - 1) == 0);
}

/* PoP: ts_is_pivot_marker @ tui_gateway/server.py:_is_pivot_marker */
static json_t *ts_is_pivot_marker(ts_env *e, const json_t *in) {
    const json_t *entry = json_obj_get(in, "entry");
    json_t *a = json_object();
    json_set(a, "entry", entry ? json_copy(entry) : json_null());
    json_t *r = ts_call(e, "_is_model_switch_marker", a);
    if (ts_is_raise(r)) return ts_prop(e, r);
    bool msm = ts_truth(r);
    json_free(r);
    if (msm) return json_bool(true);
    if (ts_is_obj(entry)) {
        const json_t *dk = json_obj_get(entry, "display_kind");
        if (ts_is_str(dk) && strcmp(dk->str_val, "personality_switch") == 0)
            return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: ts_turn_started_at @ tui_gateway/server.py:_turn_started_at */
static json_t *ts_turn_started_at(ts_env *e, const json_t *in) {
    const json_t *session = json_obj_get(in, "session");
    const json_t *inflight = NULL;
    if (ts_truth(session)) {
        if (!ts_is_obj(session)) return ts_prop(e, ts_attr_raise(session, "get"));
        inflight = json_obj_get(session, "inflight_turn");
    }
    if (ts_is_obj(inflight)) {
        const json_t *sa = json_obj_get(inflight, "started_at");
        if (ts_truth(sa)) {
            double d;
            if (!ts_float_coerce(sa, &d)) {
                char buf[160];
                snprintf(buf, sizeof(buf), "could not convert string to float: '%s'",
                         ts_str(sa) ? ts_str(sa) : "");
                return ts_prop(e, ts_raise_spec("ValueError", buf));
            }
            return json_number(d);
        }
    }
    return json_null();
}

/* ── cli.exec argv guard ──────────────────────────────────────────────────── */
static const char *ts_cli_lookup1(const char *h0) {
    if (!h0) return NULL;
    if (strcmp(h0, "setup") == 0) return TS_SETUP_HINT;
    if (strcmp(h0, "gateway") == 0) return TS_GATEWAY_HINT;
    return NULL;
}
static const char *ts_cli_lookup2(const char *h0, const char *h1) {
    if (!h0 || !h1) return NULL;
    if (strcmp(h0, "sessions") == 0 && strcmp(h1, "browse") == 0) return TS_SESS_HINT;
    if (strcmp(h0, "config") == 0 && strcmp(h1, "edit") == 0) return TS_CONFIG_HINT;
    return NULL;
}

/* PoP: ts_cli_exec_blocked @ tui_gateway/server.py:_cli_exec_blocked */
static json_t *ts_cli_exec_blocked(ts_env *e, const json_t *in) {
    (void)e;
    const json_t *argv = json_obj_get(in, "argv");
    if (!ts_truth(argv)) return json_string(TS_BARE_HINT);
    if (!ts_is_arr(argv)) return ts_prop(e, ts_attr_raise(argv, "lower"));
    size_t n = json_len(argv);
    char *h0 = NULL, *h1 = NULL;
    if (n > 0) {
        const json_t *a0 = json_get(argv, 0);
        if (!ts_is_str(a0)) return ts_prop(e, ts_attr_raise(a0, "lower"));
        h0 = ts_lower(a0->str_val);
    }
    if (n > 1) {
        const json_t *a1 = json_get(argv, 1);
        if (!ts_is_str(a1)) { free(h0); return ts_prop(e, ts_attr_raise(a1, "lower")); }
        h1 = ts_lower(a1->str_val);
    }
    const char *r1 = ts_cli_lookup1(h0);
    const char *r2 = (n >= 2) ? ts_cli_lookup2(h0, h1) : r1;
    const char *hit = r1 ? r1 : r2;
    json_t *ret = hit ? json_string(hit) : json_null();
    free(h0); free(h1);
    return ret;
}

/* ── blocking-prompt bridge ───────────────────────────────────────────────── */
/* PoP: ts_clarify_timeout_seconds @ tui_gateway/server.py:_clarify_timeout_seconds */
static json_t *ts_clarify_timeout_seconds(ts_env *e, const json_t *in) {
    (void)in;
    json_t *r = ts_call(e, "get_clarify_timeout", json_object());
    if (ts_is_raise(r)) { if (ts_swallow_exc(e, r)) return NULL; return json_number(300); }
    /* timeout > 0 ? timeout : None — non-number comparison raises TypeError -> 300 */
    if (ts_is_num(r)) {
        if (r->num_val > 0) return r;
        json_free(r); return json_null();
    }
    if (r && r->type == JSON_BOOL) {
        if (r->bool_val) return r;
        json_free(r); return json_null();
    }
    json_free(r);
    return json_number(300);
}

/* PoP: ts_ask @ tui_gateway/server.py:_ask */
static json_t *ts_ask(ts_env *e, const json_t *in) {
    const json_t *params = json_obj_get(in, "params");
    const json_t *to = json_obj_get(in, "timeout");
    json_t *a = json_object();
    json_set(a, "method", json_string(json_get_str(in, "method", "")));
    json_set(a, "sid", json_string(json_get_str(in, "sid", "")));
    json_set(a, "params", params ? json_copy(params) : json_null());
    json_set(a, "timeout", to ? json_copy(to) : json_number(300));
    json_t *result = ts_call(e, "send", a);
    if (ts_is_raise(result)) return ts_prop(e, result);
    /* value = (result or {}).get("value", "") */
    json_t *value;
    if (!ts_truth(result)) {
        value = json_string("");
    } else if (ts_is_obj(result)) {
        const json_t *v = json_obj_get(result, "value");
        value = v ? json_copy(v) : json_string("");
    } else {
        json_t *ar = ts_attr_raise(result, "get");
        json_free(result);
        return ts_prop(e, ar);
    }
    json_free(result);
    /* return value if isinstance(value, str) else json.dumps(value, ensure_ascii=False) */
    if (value->type == JSON_STRING) return value;
    char *s = ts_json_dumps(value);
    json_free(value);
    json_t *ret = json_string(s);
    free(s);
    return ret;
}

/* PoP: ts_clarify_block @ tui_gateway/server.py:_clarify_block */
static json_t *ts_clarify_block(ts_env *e, const json_t *in) {
    const json_t *questions = json_obj_get(in, "questions");
    const char *sid = json_get_str(in, "sid", "");
    /* wire = [{qid, question, choices, multi_select: bool} for e in questions] */
    json_t *wire = json_array();
    if (ts_is_arr(questions)) {
        for (size_t i = 0; i < json_len(questions); i++) {
            const json_t *qe = json_get(questions, i);
            const json_t *qid = json_obj_get(qe, "qid");
            if (!qid) { json_free(wire); return ts_prop(e, ts_keyerror_raise("qid")); }
            const json_t *question = json_obj_get(qe, "question");
            if (!question) { json_free(wire); return ts_prop(e, ts_keyerror_raise("question")); }
            const json_t *choices = json_obj_get(qe, "choices");
            if (!choices) { json_free(wire); return ts_prop(e, ts_keyerror_raise("choices")); }
            const json_t *ms = json_obj_get(qe, "multi_select");
            if (!ms) { json_free(wire); return ts_prop(e, ts_keyerror_raise("multi_select")); }
            json_t *w = json_object();
            json_set(w, "qid", json_copy(qid));
            json_set(w, "question", json_copy(question));
            json_set(w, "choices", json_copy(choices));
            json_set(w, "multi_select", json_bool(ts_truth(ms)));
            json_append(wire, w);
        }
    }
    /* timeout = _clarify_timeout_seconds() fires during send kwarg evaluation */
    json_t *tmo = ts_call(e, "_clarify_timeout_seconds", json_object());
    if (ts_is_raise(tmo)) { json_free(wire); return ts_prop(e, tmo); }
    json_t *qids = json_array();
    if (ts_is_arr(questions))
        for (size_t i = 0; i < json_len(questions); i++)
            json_append(qids, json_copy(json_obj_get(json_get(questions, i), "qid")));
    json_t *params = json_object(); json_set(params, "questions", wire);   /* adopts wire */
    json_t *a = json_object();
    json_set(a, "method", json_string("clarify"));
    json_set(a, "sid", json_string(sid));
    json_set(a, "params", params);
    json_set(a, "timeout", tmo ? tmo : json_null());   /* adopts; None is still a kwarg */
    json_set(a, "qids", qids);
    json_t *result = ts_call(e, "send", a);
    if (ts_is_raise(result)) return ts_prop(e, result);
    if (ts_truth(result)) return result;
    json_free(result);
    json_t *d = json_object();
    json_set(d, "answers", json_object());
    json_set(d, "outcome", json_string("undelivered"));
    return d;
}

/* PoP: ts_tour_request @ tui_gateway/server.py:_tour_request */
static json_t *ts_tour_request(ts_env *e, const json_t *in) {
    const char *sid = json_get_str(in, "sid", "");
    const json_t *payload = json_obj_get(in, "payload");
    json_t *session = json_obj_get(ts_sessions(e), sid);
    json_t *throwaway = NULL;
    if (!session) { throwaway = json_object(); session = throwaway; }
    const char *ss = ts_str(json_obj_get(session, "tour_bridge"));
    if (ss && strcmp(ss, "unanswered") == 0) {
        if (throwaway) json_free(throwaway);
        return json_string(TS_TOUR_UNAVAIL);
    }
    double timeout = (ss && strcmp(ss, "answered") == 0) ? 45 : 10;
    json_t *a = json_object();
    json_set(a, "method", json_string("tour"));
    json_set(a, "sid", json_string(sid));
    json_set(a, "payload", payload ? json_copy(payload) : json_null());
    json_set(a, "timeout", json_number(timeout));
    json_t *answer = ts_call(e, "_ask", a);
    if (ts_is_raise(answer)) { if (throwaway) json_free(throwaway); return ts_prop(e, answer); }
    if (ts_truth(answer)) {
        json_set(session, "tour_bridge", json_string("answered"));
    } else if (!(ss && strcmp(ss, "answered") == 0)) {
        json_set(session, "tour_bridge", json_string("unanswered"));
    }
    json_t *ret;
    if (ts_truth(answer)) ret = answer;
    else { json_free(answer); ret = json_string(TS_TOUR_UNAVAIL); }
    if (throwaway) json_free(throwaway);
    return ret;
}

/* PoP: ts_preview_action_request @ tui_gateway/server.py:_preview_action_request */
static json_t *ts_preview_action_request(ts_env *e, const json_t *in) {
    const char *sid = json_get_str(in, "sid", "");
    const json_t *payload = json_obj_get(in, "payload");
    json_t *reprobe = ts_reprobe(e);
    json_t *session = json_obj_get(ts_sessions(e), sid);
    json_t *throwaway = NULL;
    if (!session) { throwaway = json_object(); session = throwaway; }
    const char *ss = ts_str(json_obj_get(session, "preview_action_bridge"));
    bool unanswered = ss && strcmp(ss, "unanswered") == 0;
    bool answered = ss && strcmp(ss, "answered") == 0;
    json_t *now = ts_call(e, "monotonic", json_object());
    if (ts_is_raise(now)) { if (throwaway) json_free(throwaway); return ts_prop(e, now); }
    double nowv = ts_is_num(now) ? now->num_val : 0;
    json_free(now);
    if (unanswered) {
        const json_t *ra = json_obj_get(session, "preview_action_bridge_retry_at");
        double retry = ts_is_num(ra) ? ra->num_val : 0;
        if (nowv < retry) { if (throwaway) json_free(throwaway); return json_string(TS_PREVIEW_UNAVAIL); }
        const json_t *tok = json_obj_get(reprobe, sid);
        if (tok && tok->type != JSON_NULL) { if (throwaway) json_free(throwaway); return json_string(TS_PREVIEW_UNAVAIL); }
        json_set(reprobe, sid, json_bool(true));
        json_set(session, "preview_action_bridge_retry_at", json_number(nowv + 30));
    }
    double timeout = answered ? 45 : 10;
    json_t *a = json_object();
    json_set(a, "method", json_string("preview.act"));
    json_set(a, "sid", json_string(sid));
    json_set(a, "payload", payload ? json_copy(payload) : json_null());
    json_set(a, "timeout", json_number(timeout));
    json_t *answer = ts_call(e, "_ask", a);
    /* finally: if state == "unanswered": reprobe.pop(sid) */
    if (unanswered) json_obj_del(reprobe, sid);
    if (ts_is_raise(answer)) { if (throwaway) json_free(throwaway); return ts_prop(e, answer); }
    if (ts_truth(answer)) {
        json_set(session, "preview_action_bridge", json_string("answered"));
    } else {
        const char *cs = ts_str(json_obj_get(session, "preview_action_bridge"));
        if (!(cs && strcmp(cs, "answered") == 0)) {
            json_set(session, "preview_action_bridge", json_string("unanswered"));
            json_t *m2 = ts_call(e, "monotonic", json_object());
            if (ts_is_raise(m2)) { if (throwaway) json_free(throwaway); json_free(answer); return ts_prop(e, m2); }
            double m2v = ts_is_num(m2) ? m2->num_val : 0;
            json_free(m2);
            json_set(session, "preview_action_bridge_retry_at", json_number(m2v + 30));
        }
    }
    json_t *ret;
    if (ts_truth(answer)) ret = answer;
    else { json_free(answer); ret = json_string(TS_PREVIEW_UNAVAIL); }
    if (throwaway) json_free(throwaway);
    return ret;
}

/* PoP: ts_status_update @ tui_gateway/server.py:_status_update */
static json_t *ts_status_update(ts_env *e, const json_t *in) {
    const char *sid = json_get_str(in, "sid", "");
    const json_t *text = json_obj_get(in, "text");
    const json_t *kindn = json_obj_get(in, "kind");
    bool text_present = text && text->type != JSON_NULL;
    const json_t *src = text_present ? text : kindn;
    if (!ts_is_str(src)) return ts_prop(e, ts_attr_raise(src, "strip"));
    char *body = ts_strip(src->str_val);
    if (!body[0]) { free(body); return json_null(); }
    json_t *payload = json_object();
    json_set(payload, "kind", text_present ? (kindn ? json_copy(kindn) : json_null()) : json_string("status"));
    json_set(payload, "text", json_string(body));
    free(body);
    json_t *a = json_object();
    json_set(a, "event", json_string("status.update"));
    json_set(a, "sid", json_string(sid));
    json_set(a, "payload", payload);
    json_t *r = ts_call(e, "_emit", a);
    if (ts_is_raise(r)) return ts_prop(e, r);
    json_free(r);
    return json_null();
}

/* PoP: ts_resolve_name @ tui_gateway/server.py:_resolve_name */
static json_t *ts_resolve_name(ts_env *e, const json_t *in) {
    const char *name = json_get_str(in, "name", "");
    json_t *a = json_object(); json_set(a, "name", json_string(name));
    json_t *r = ts_call(e, "resolve_command", a);
    if (ts_is_raise(r)) { if (ts_swallow_exc(e, r)) return NULL; return json_string(name); }
    if (!ts_truth(r)) { json_free(r); return json_string(name); }
    if (ts_is_obj(r)) {
        const json_t *nm = json_obj_get(r, "name");
        if (nm) { json_t *ret = json_copy(nm); json_free(r); return ret; }
    }
    /* truthy without a usable .name -> AttributeError -> suppress -> name */
    json_free(r);
    return json_string(name);
}

/* PoP: ts_finish_reload @ tui_gateway/server.py:_finish_reload */
static json_t *ts_finish_reload(ts_env *e, const json_t *in) {
    const json_t *params = json_obj_get(in, "params");
    const json_t *rid = json_obj_get(in, "rid");
    bool coalesced = json_get_bool(in, "coalesced", false);
    const json_t *always = ts_is_obj(params) ? json_obj_get(params, "always") : NULL;
    if (ts_truth(always)) {
        json_t *a = json_object();
        json_set(a, "key", json_string("approvals.mcp_reload_confirm"));
        json_set(a, "value", json_bool(false));
        json_t *r = ts_call(e, "save_config_value", a);
        if (ts_is_raise(r)) {
            if (ts_is_base(ts_kind(r))) return ts_prop(e, r);   /* except Exception */
            json_t *la = json_object();
            json_set(la, "level", json_number(30));
            json_set(la, "fmt", json_string("Failed to persist mcp_reload_confirm=false: %s"));
            json_t *args = json_array(); json_append(args, json_string(ts_msg(r)));
            json_set(la, "args", args);
            json_free(r);
            json_t *lr = ts_call(e, "log", la);
            if (ts_is_raise(lr)) return ts_prop(e, lr);
            json_free(lr);
        } else json_free(r);
    }
    json_t *result = json_object();
    json_set(result, "status", json_string("reloaded"));
    json_set(result, "loaded_rev", json_string(json_get_str(ts_mcp(e), "loaded_rev", "")));
    if (coalesced) json_set(result, "coalesced", json_bool(true));
    json_t *a = json_object();
    json_set(a, "rid", rid ? json_copy(rid) : json_null());
    json_set(a, "result", result);
    json_t *r = ts_call(e, "_ok", a);
    if (ts_is_raise(r)) return ts_prop(e, r);
    return r;
}