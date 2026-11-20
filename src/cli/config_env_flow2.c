/*
 * config_env_flow2.c — hermes_cli/web_routers/config_env.py UNCLAIMED remainder (C11 port).
 * LIVE authority: C:/Users/eman5/hermes-agent/hermes_cli/web_routers/config_env.py
 * (the task's "hermes_cli/config_env.py" shorthand; the live file lives under
 * hermes_cli/web_routers/ — PoP lines carry the LIVE path so coverage scans credit them).
 *
 * Sibling of the ws_revive1..4 / web_server.c ports. Every function claimed by an
 * existing PoP (grep PoP first, CRLF-aware) is SEAMED here, never re-ported:
 *   CLAIMED: _get_env_vars_sync (web_server.c), _custom_endpoint_response (ws_revive1),
 *            _catalog_provider_env_metadata (ws_revive1), _models_from_custom_endpoint_entry
 *            (ws_revive2), _api_key_display (ws_revive3), _custom_endpoint_id (ws_revive4),
 *            _config_api_key_is_env_ref (ws_revive4), get_config / get_schema (web_server.py
 *            re-exports, ws_get_config / wbs_get_schema).
 * This file ports 17 defs NO existing PoP claims:
 *   _provider_card                  pure catalog-card dict builder (d.slug / d.label)
 *   _endpoint_api_mode              api_mode/transport read order + _canonical_api_mode seam
 *   _endpoint_row                   row builder; CLAIMED _api_key_display seamed,
 *                                   _endpoint_api_mode composed REAL
 *   _model_names_provider           coerce_provider_id seam x1..x3, custom: removeprefix
 *   _resolve_custom_endpoint_entry  find_provider_entry seam, CLAIMED _custom_endpoint_id seam
 *   _raw_provider_api_key           read_raw_config + find_provider_entry seams
 *   _pop_legacy_custom_provider     CLAIMED _custom_endpoint_id seam; mutates cfg (list pop)
 *   _detach_main_model_from_provider  _model_names_provider composed REAL; mutates cfg
 *   _drop_stale_addresses           pure ENDPOINT_ADDRESS_FIELDS pop (mutates entry)
 *   _store_endpoint_key             store_secret seam; ValueError -> HTTPException 400
 *   _apply_endpoint_key             the key write/clear/migrate decision tree; mutates entry
 *   _save_env_credential            load_env / save_provider_env_credential /
 *                                   web_setup_surface / record_api_key_saved seams
 *   _drop_carried_route_state       drop_carried_route_state seam x1..xN (model/delegation/aux)
 *   _no_models_probe_message        resp.json() seam (ValueError swallowed); 3 message shapes
 *   _auto_api_mode                  _detect_api_mode_for_url seam + chat_completions fallback
 *   _env_write_errors               contextmanager exit classification (HTTPException pass,
 *                                   ValueError->400, Exception->log+500, BaseException pass)
 *   _record_custom_endpoint_setup   web_setup_surface / record_provider_setup_done seams
 *
 * Self-contained, libjson-only. Every live effect (foreign-module collaborators and the
 * CLAIMED siblings) enters through ONE universal seam; the cores perform NO I/O:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON object
 * (the seam records a deep copy and must not free it); the result is a NEW json_t* the
 * caller frees. NULL == Python None (never a JSON_NULL node);
 * {"__raise__": cls, "msg": msg} == a raised exception (HTTPException specs also carry
 * "status_code"). The harness records {"ev": name, "args": args, "ret": result} per call
 * in order; the oracle's recording stubs emit the identical shape driving LIVE Python
 * with the same scripts. BaseException kinds (SystemExit / KeyboardInterrupt /
 * GeneratorExit / BaseExceptionGroup) PASS THROUGH every catch site; only the SPECIFIC
 * named except clause (ValueError in _store_endpoint_key / _env_write_errors /
 * _no_models_probe_message, HTTPException then Exception in _env_write_errors) swallows
 * its own class family. With no seam installed the dispatcher is fail-closed (RuntimeError
 * raise spec); the harness script_call is ALSO fail-closed: a MISSING script returns a
 * RuntimeError raise spec (a scripted null is a valid None result).
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__": ...} object == a propagated exception. Tuple returns
 * (_resolve_custom_endpoint_entry) project to 2-element arrays. Mutation flows
 * (_pop_legacy_custom_provider, _detach_main_model_from_provider, _drop_stale_addresses,
 * _store_endpoint_key, _apply_endpoint_key) mutate the BORROWED carrier (cfg / entry)
 * IN PLACE — the harness passes a private copy and emits it as "state" post-hoc,
 * including on raise (partial mutation is observable).
 *
 * Seam name table (name -> live call; args key order is part of the trace; None-valued
 * args keys are dropped on BOTH sides):
 *   find_provider_entry            find_provider_entry(providers, key)
 *                                  args{providers, key} ret [stored_key, entry]|raise
 *   coerce_provider_id             coerce_provider_id(value)        args{value} ret str
 *   read_raw_config                read_raw_config()                args{} ret doc
 *   _canonical_api_mode            _canonical_api_mode(api_mode)    args{api_mode} ret str
 *   _api_key_display               _api_key_display(entry)          args{entry} ret [has, preview]
 *                                  (CLAIMED sibling)
 *   _custom_endpoint_id            _custom_endpoint_id(raw)         args{raw} ret str
 *                                  (CLAIMED sibling; call sites pass raw only, fallback default)
 *   _config_api_key_is_env_ref     _config_api_key_is_env_ref(endpoint_id)
 *                                  args{endpoint_id} ret bool       (CLAIMED sibling)
 *   is_redacted_credential_preview is_redacted_credential_preview(submitted)
 *                                  args{submitted} ret bool
 *   store_secret                   store_secret(env_name, value, base_url, own_provider)
 *                                  args{env_name, value, base_url, own_provider} ret str|raise
 *   dashboard_endpoint_key_env     dashboard_endpoint_key_env(entry, endpoint_id)
 *                                  args{entry, endpoint_id} ret str
 *   env_reference_owners           env_reference_owners(cfg, env_name)
 *                                  args{cfg, env_name} ret [blocks]
 *                                  (identity: a block {"__own__": true} denotes the SAME
 *                                  object as raw["providers"].get(stored_key); any other
 *                                  block is a distinct object — except JSON null when own
 *                                  is None: the None singleton is identical to itself)
 *   remove_env_value               remove_env_value(key)            args{key} ret bool|None
 *   load_env                       load_env()                       args{} ret dict
 *   save_provider_env_credential   save_provider_env_credential(env_var, value)
 *                                  args{env_var, value} ret result
 *   web_setup_surface              web_setup_surface()              args{} ret str
 *   record_api_key_saved           record_api_key_saved(env_var, value, previous, surface,
 *                                  connecting=)  args{env_var, value, previous, surface,
 *                                  connecting}
 *   record_provider_setup_done     record_provider_setup_done(surface, provider, hermes_home=,
 *                                  background=)  args{surface, provider, hermes_home, background}
 *   drop_carried_route_state       drop_carried_route_state(old, new, config)
 *                                  args{old, new, config} ret None|raise
 *   _detect_api_mode_for_url       _detect_api_mode_for_url(base_url) args{base_url} ret str|None
 *   _log.exception                 _log.exception(log_msg)          args{log_msg}
 *   resp_json                      resp.json()                      args{} ret payload|raise
 *
 * Representation contract (fixture domain, identical on both sides):
 *  - ASCII text inputs; POSIX lexical paths; the em-dash in _no_models_probe_message's
 *    live wording is emitted as UTF-8 (\xe2\x80\x94) exactly as live.
 *  - integral JSON numbers read as int (libjson has no int/float identity); numbers in
 *    str()-computed positions are integral (Python str(float) repr is outside the domain).
 *  - carrier dicts (cfg / entry / model_cfg / existing / merged / resp.headers / d) are
 *    JSON objects; a non-object carrier raises the live AttributeError at FIRST touch
 *    (get / pop / headers / slug / label — d is a SimpleNamespace double, so its missing
 *    attribute message is module-qualified 'types.SimpleNamespace').
 *  - body doubles (CustomEndpointUpdate.api_key, catalog d) expose exactly the attributes
 *    the live body reads; api_key is str or None.
 *  - entry.api_key in the _apply_endpoint_key migrate branch is str or falsy (the live
 *    entry["api_key"].strip() would AttributeError on a truthy non-str).
 *  - find_provider_entry / _api_key_display scripts are 2-element arrays (tuple projection).
 *  - env_reference_owners own is a dict or None (providers.<stored_key> is a dict entry
 *    or absent/null); blocks are {"__own__": true} markers, dicts, strings, or null.
 *  - in-function imports resolve silently (sys.modules stubs on the live side); import
 *    FAILURE is outside the fixture domain.
 *  - _env_write_errors: the with-block body projects to a scripted outcome (body_raise
 *    spec or null = normal completion); the generator yield/throw is modeled by the
 *    exit classification. HTTPException raise specs carry status_code + msg(=detail).
 *  - ENDPOINT_ADDRESS_FIELDS = ("base_url", "url", "api", "baseUrl") — derived LIVE from
 *    config_providers._CAMEL_ALIASES (baseUrl is the only camel alias of base_url).
 *  - _ENV_REF_RE = re.compile(r"\${([^}]+)}") (hermes_cli/config.py:1385), fullmatch.
 *  - REDACTED_CREDENTIAL_WRITE_DETAIL is the live _common.py constant (ASCII).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */
static bool ce3_jtruth(const json_t *v) {
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
static bool ce3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ce3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ce3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static const json_t *ce3_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT && k) ? json_obj_get(o, k) : NULL;
}
static char *ce3_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}
/* Python str() projection for the fixture domain (scalars only; containers are
 * outside the domain — str(dict/list) repr is not modeled). Integral numbers
 * format as ints (libjson has no int/float identity). */
static char *ce3_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 9.2233720368547758e18)
                return ce3_xsf("%.0f", d);
            return ce3_xsf("%.17g", d);  /* outside the fixture domain */
        }
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default:          return strdup("");  /* containers: outside the domain */
    }
}
/* Python type name for AttributeError messages (integral number == int). */
static const char *ce3_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == floor(d) && fabs(d) < 9.2233720368547758e18) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
/* Python str.strip() over the ASCII whitespace set (fixture domain is ASCII). */
static char *ce3_strip(const char *s) {
    if (!s) return strdup("");
    const char *ws = " \t\n\r\v\f";
    const char *a = s;
    while (*a && strchr(ws, *a)) a++;
    const char *b = a + strlen(a);
    while (b > a && strchr(ws, b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = malloc(n + 1);
    if (out) { memcpy(out, a, n); out[n] = '\0'; }
    return out ? out : strdup("");
}
static char *ce3_lower(const char *s) {
    char *out = strdup(s ? s : "");
    for (char *q = out; q && *q; q++)
        if (*q >= 'A' && *q <= 'Z') *q = (char)(*q + ('a' - 'A'));
    return out;
}
/* AttributeError spec for `<value>.<attr>` on a non-carrier (live message shape). */
static json_t *ce3_raise(const char *cls, const char *msg);
static json_t *ce3_attr_err(const json_t *v, const char *attr) {
    char *m = ce3_xsf("'%s' object has no attribute '%s'", ce3_type_name(v), attr);
    json_t *r = ce3_raise("AttributeError", m);
    free(m);
    return r;
}

/* ── universal seam ── */
typedef struct ce3_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} ce3_seams_t;

static json_t *ce3_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
/* HTTPException raise spec: msg == detail, plus status_code (live projection). */
static json_t *ce3_http_exc(int status, const char *detail) {
    json_t *o = ce3_raise("HTTPException", detail);
    json_set(o, "status_code", json_number((double)status));
    return o;
}
static bool ce3_is_raise(const json_t *r) {
    return ce3_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *ce3_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static const char *ce3_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}
/* Fail-closed dispatcher: no seam installed -> RuntimeError raise spec.
 * Ownership: ce3_call OWNS args (frees it after the call); the seam BORROWS args
 * (records a deep copy, must not free it). Results are NEW (caller frees). */
static json_t *ce3_call(const ce3_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        return ce3_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static void ce3_set_str(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
/* set a key to a copy of v, or an explicit JSON null when v is NULL/JSON_NULL.
 * (json_set(obj,key,NULL) is a no-op in libjson — never pass NULL.) */
static void ce3_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}
/* args builders: NULL/JSON_NULL values become explicit nulls; the harness trace
 * drops null-valued keys (the oracle seam does the same). */
static void ce3_arg_node(json_t *o, const char *k, const json_t *v) {
    ce3_set_node(o, k, v);
}

/* ── FULL builtin exception hierarchy (child -> parent) for isinstance-style
 *    catch folding. Catch sites in this port: except ValueError (_store_endpoint_key,
 *    _env_write_errors, _no_models_probe_message), except HTTPException then
 *    except Exception (_env_write_errors). The full table is carried so a scripted
 *    subclass name folds into its ancestor exactly as live isinstance does.
 *    ExceptionGroup's second base (Exception) is modelled by its primary
 *    BaseExceptionGroup parent; it is outside the fixture domain. HTTPException is
 *    the fastapi double (an Exception subclass) used on both sides. ── */
typedef struct ce3_exc_pair { const char *child; const char *parent; } ce3_exc_pair_t;
static const ce3_exc_pair_t CE3_EXC_TREE[] = {
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"Exception", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    /* the fastapi HTTPException double (Exception subclass, both sides) */
    {"HTTPException", "Exception"},
};
static const char *ce3_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(CE3_EXC_TREE) / sizeof(CE3_EXC_TREE[0]); i++)
        if (!strcmp(CE3_EXC_TREE[i].child, c)) return CE3_EXC_TREE[i].parent;
    return NULL;
}
/* isinstance(c, ancestor): walk child -> parent up to BaseException. */
static bool ce3_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = ce3_exc_parent(c);
    }
    return false;
}

/* ── domain constants (LIVE values; see header) ── */
static const char *CE3_ADDRESS_FIELDS[] = {"base_url", "url", "api", "baseUrl"};
static const char *CE3_DESKTOP_API_MODES[] = {
    "chat_completions", "codex_responses", "anthropic_messages"
};
#define CE3_REDACTED_DETAIL \
    "Refusing to save a redacted credential preview; re-enter the full secret to replace it."

/* _ENV_REF_RE.fullmatch: r"\${([^}]+)}" — "${" + >=1 non-"}" chars + "}", whole string. */
static bool ce3_env_ref_fullmatch(const char *s) {
    if (!s) return false;
    size_t n = strlen(s);
    if (n < 4) return false;                    /* "${x}" is the shortest match */
    if (s[0] != '$' || s[1] != '{' || s[n - 1] != '}') return false;
    for (size_t i = 2; i + 1 < n; i++)
        if (s[i] == '}') return false;
    return true;                                /* middle length n-3 >= 1 */
}

/* dict.pop(key, None) — in place; present-null is removed too. */
static void ce3_pop(json_t *o, const char *k) {
    if (ce3_is_obj(o)) json_obj_del(o, k);
}
/* list.pop(index) projection: a NEW array without element idx (order preserved). */
static json_t *ce3_arr_without(const json_t *arr, size_t idx) {
    json_t *out = json_array();
    size_t n = json_len(arr);
    for (size_t i = 0; i < n; i++)
        if (i != idx) json_append(out, json_copy(json_get(arr, i)));
    return out;
}
/* str.removeprefix("custom:") — removes the prefix once when present. */
static const char *ce3_remove_custom_prefix(const char *s) {
    return (strncmp(s, "custom:", 7) == 0) ? s + 7 : s;
}
/* str(x or "") — the `or ""` fallback every live str() site carries. */
static char *ce3_str_or_empty(const json_t *v) {
    return ce3_jtruth(v) ? ce3_str_of(v) : strdup("");
}
/* tuple-2 unpack guard (find_provider_entry / _api_key_display seam results).
 * Domain: 2-element arrays. Returns a raise spec on a bad shape, else NULL and
 * sets *a / *b to BORROWED element pointers. */
static json_t *ce3_unpack2(const json_t *r, const json_t **a, const json_t **b) {
    if (!ce3_is_arr(r)) {
        char *m = ce3_xsf("cannot unpack non-iterable %s object", ce3_type_name(r));
        json_t *e = ce3_raise("TypeError", m);
        free(m);
        return e;
    }
    size_t n = json_len(r);
    if (n != 2) {
        char *m = ce3_xsf("%s values to unpack (expected 2, got %zu)",
                          n > 2 ? "too many" : "not enough", n);
        json_t *e = ce3_raise("ValueError", m);
        free(m);
        return e;
    }
    *a = json_get(r, 0);
    *b = json_get(r, 1);
    return NULL;
}

/* ═══════════════════════ pure builders ═══════════════════════ */

/* PoP: ce3_provider_card @ hermes_cli/web_routers/config_env.py:_provider_card */
static json_t *ce3_provider_card(const json_t *d, const char *description,
                                 const json_t *url, bool is_password, bool advanced) {
    /* d is a catalog-entry double (SimpleNamespace on the live side): d.slug then
     * d.label, in dict-literal order. A missing attribute raises the module-qualified
     * SimpleNamespace AttributeError. */
    if (!ce3_is_obj(d) || !json_obj_get(d, "slug"))
        return ce3_raise("AttributeError",
                         "'types.SimpleNamespace' object has no attribute 'slug'");
    if (!json_obj_get(d, "label"))
        return ce3_raise("AttributeError",
                         "'types.SimpleNamespace' object has no attribute 'label'");
    json_t *o = json_object();
    ce3_set_str(o, "provider", json_get_str(d, "slug", ""));
    ce3_set_str(o, "provider_label", json_get_str(d, "label", ""));
    ce3_set_str(o, "description", description);
    ce3_set_node(o, "url", url);
    json_set(o, "is_password", json_bool(is_password));
    json_set(o, "advanced", json_bool(advanced));
    ce3_set_str(o, "category", "provider");
    return o;
}

/* PoP: ce3_endpoint_api_mode @ hermes_cli/web_routers/config_env.py:_endpoint_api_mode */
static json_t *ce3_endpoint_api_mode(const ce3_seams_t *s, const json_t *entry) {
    /* raw = str(entry.get("api_mode") or entry.get("transport") or "")
     * mode = _canonical_api_mode(raw).lower()
     * return mode if mode in _DESKTOP_API_MODES else "" */
    if (!ce3_is_obj(entry)) return ce3_attr_err(entry, "get");
    const json_t *am = ce3_get(entry, "api_mode");
    const json_t *tr = ce3_get(entry, "transport");
    const json_t *raw_node = ce3_jtruth(am) ? am : (ce3_jtruth(tr) ? tr : NULL);
    char *raw = ce3_str_or_empty(raw_node);
    json_t *args = json_object();
    ce3_set_str(args, "api_mode", raw);
    free(raw);
    json_t *r = ce3_call(s, "_canonical_api_mode", args);
    if (ce3_is_raise(r)) return r;
    if (!ce3_is_str(r)) {
        json_t *e = ce3_attr_err(r, "lower");
        json_free(r);
        return e;
    }
    char *mode = ce3_lower(r->str_val ? r->str_val : "");
    json_free(r);
    bool known = false;
    for (size_t i = 0; i < sizeof(CE3_DESKTOP_API_MODES) / sizeof(CE3_DESKTOP_API_MODES[0]); i++)
        if (!strcmp(mode, CE3_DESKTOP_API_MODES[i])) { known = true; break; }
    json_t *ret = json_string(known ? mode : "");
    free(mode);
    return ret;
}

/* PoP: ce3_endpoint_row @ hermes_cli/web_routers/config_env.py:_endpoint_row */
static json_t *ce3_endpoint_row(const ce3_seams_t *s, const char *endpoint_id,
                                const char *name, const char *base_url, const char *model,
                                const json_t *models, const json_t *context_length,
                                bool discover_models, const json_t *key_entry,
                                bool is_current, const char *source) {
    /* has_api_key, api_key_preview = _api_key_display(key_entry)  [CLAIMED -> seam]
     * then the row dict; api_mode composes REAL ce3_endpoint_api_mode. */
    json_t *args = json_object();
    ce3_arg_node(args, "entry", key_entry);
    json_t *r = ce3_call(s, "_api_key_display", args);
    if (ce3_is_raise(r)) return r;
    const json_t *has = NULL, *preview = NULL;
    json_t *bad = ce3_unpack2(r, &has, &preview);
    if (bad) { json_free(r); return bad; }
    json_t *api_mode = ce3_endpoint_api_mode(s, key_entry);
    if (ce3_is_raise(api_mode)) { json_free(r); return api_mode; }
    json_t *o = json_object();
    ce3_set_str(o, "id", endpoint_id);
    ce3_set_str(o, "name", name);
    ce3_set_str(o, "base_url", base_url);
    ce3_set_str(o, "model", model);
    ce3_set_node(o, "models", models);
    json_set(o, "api_mode", api_mode);            /* owned result, moved in */
    ce3_set_node(o, "context_length", context_length);
    json_set(o, "discover_models", json_bool(discover_models));
    ce3_set_node(o, "has_api_key", has);          /* verbatim (live stores the tuple slot) */
    ce3_set_node(o, "api_key_preview", preview);
    json_set(o, "is_current", json_bool(is_current));
    ce3_set_str(o, "source", source);
    json_free(r);
    return o;
}

/* PoP: ce3_model_names_provider @ hermes_cli/web_routers/config_env.py:_model_names_provider */
static json_t *ce3_model_names_provider(const ce3_seams_t *s, const json_t *model_cfg,
                                        const char *provider_key, const json_t *entry) {
    /* names = {coerce_provider_id(provider_key).lower()}
     * if isinstance(entry, dict) and coerce_provider_id(entry.get("name")):
     *     names.add(coerce_provider_id(entry.get("name")).lower())
     * current = str(model_cfg.get("provider") or "").strip().lower()
     * return current.removeprefix("custom:") in names
     * NOTE: entry.name is coerced TWICE when the guard passes (two seam events).
     * model_cfg is first touched at the `current = str(model_cfg.get(...))` line —
     * AFTER the coerce calls — so a non-dict model_cfg raises only once they fire. */
    char *names[2] = {NULL, NULL};
    size_t n_names = 0;
    json_t *args = json_object();
    ce3_set_str(args, "value", provider_key);
    json_t *r1 = ce3_call(s, "coerce_provider_id", args);
    if (ce3_is_raise(r1)) return r1;
    if (!ce3_is_str(r1)) {
        json_t *e = ce3_attr_err(r1, "lower");
        json_free(r1);
        return e;
    }
    names[n_names++] = ce3_lower(r1->str_val ? r1->str_val : "");
    json_free(r1);
    if (ce3_is_obj(entry)) {
        const json_t *name_val = ce3_get(entry, "name");
        args = json_object();
        ce3_arg_node(args, "value", name_val);
        json_t *r2 = ce3_call(s, "coerce_provider_id", args);
        if (ce3_is_raise(r2)) { free(names[0]); return r2; }
        if (ce3_jtruth(r2)) {
            json_free(r2);
            args = json_object();
            ce3_arg_node(args, "value", name_val);
            json_t *r3 = ce3_call(s, "coerce_provider_id", args);
            if (ce3_is_raise(r3)) { free(names[0]); return r3; }
            if (!ce3_is_str(r3)) {
                json_t *e = ce3_attr_err(r3, "lower");
                json_free(r3);
                free(names[0]);
                return e;
            }
            char *n2 = ce3_lower(r3->str_val ? r3->str_val : "");
            json_free(r3);
            if (strcmp(n2, names[0]) != 0) names[n_names++] = n2;
            else free(n2);
        } else {
            json_free(r2);
        }
    }
    if (!ce3_is_obj(model_cfg)) {
        free(names[0]);
        if (n_names > 1) free(names[1]);
        return ce3_attr_err(model_cfg, "get");
    }
    char *cur0 = ce3_str_or_empty(ce3_get(model_cfg, "provider"));
    char *cur1 = ce3_strip(cur0);
    char *cur = ce3_lower(cur1);
    free(cur0); free(cur1);
    const char *bare = ce3_remove_custom_prefix(cur);
    bool hit = false;
    for (size_t i = 0; i < n_names; i++)
        if (!strcmp(bare, names[i])) { hit = true; break; }
    free(names[0]);
    if (n_names > 1) free(names[1]);
    free(cur);
    return json_bool(hit);
}

/* ═══════════════════════ resolution / read flows ═══════════════════════ */

/* PoP: ce3_resolve_custom_endpoint_entry @ hermes_cli/web_routers/config_env.py:_resolve_custom_endpoint_entry */
static json_t *ce3_resolve_custom_endpoint_entry(const ce3_seams_t *s,
                                                 const json_t *providers,
                                                 const char *endpoint_id) {
    /* stored_key, entry = find_provider_entry(providers, endpoint_id)
     * if entry is not None: return stored_key, entry
     * normalized_key = _custom_endpoint_id(endpoint_id)   [CLAIMED -> seam]
     * if normalized_key == endpoint_id: return None, None
     * return find_provider_entry(providers, normalized_key) */
    json_t *args = json_object();
    ce3_arg_node(args, "providers", providers);
    ce3_set_str(args, "key", endpoint_id);
    json_t *r1 = ce3_call(s, "find_provider_entry", args);
    if (ce3_is_raise(r1)) return r1;
    const json_t *stored = NULL, *entry = NULL;
    json_t *bad = ce3_unpack2(r1, &stored, &entry);
    if (bad) { json_free(r1); return bad; }
    if (entry && entry->type != JSON_NULL) {      /* `is not None` — {} IS a hit */
        json_t *ret = json_array();
        json_append(ret, stored ? json_copy(stored) : json_null());
        json_append(ret, json_copy(entry));
        json_free(r1);
        return ret;
    }
    json_free(r1);
    args = json_object();
    ce3_set_str(args, "raw", endpoint_id);
    json_t *r2 = ce3_call(s, "_custom_endpoint_id", args);
    if (ce3_is_raise(r2)) return r2;
    if (ce3_is_str(r2) && !strcmp(r2->str_val ? r2->str_val : "", endpoint_id)) {
        json_free(r2);
        json_t *ret = json_array();
        json_append(ret, json_null());
        json_append(ret, json_null());
        return ret;
    }
    args = json_object();
    ce3_arg_node(args, "providers", providers);
    ce3_set_node(args, "key", r2);                /* normalized key verbatim */
    json_free(r2);
    json_t *r3 = ce3_call(s, "find_provider_entry", args);
    return r3;                                    /* [stored, entry] verbatim (or raise) */
}

/* PoP: ce3_raw_provider_api_key @ hermes_cli/web_routers/config_env.py:_raw_provider_api_key */
static json_t *ce3_raw_provider_api_key(const ce3_seams_t *s, const char *endpoint_id) {
    /* _stored, entry = find_provider_entry(read_raw_config().get("providers"), endpoint_id)
     * return entry.get("api_key") if isinstance(entry, dict) else None */
    json_t *r = ce3_call(s, "read_raw_config", json_object());
    if (ce3_is_raise(r)) return r;
    if (!ce3_is_obj(r)) {
        json_t *e = ce3_attr_err(r, "get");
        json_free(r);
        return e;
    }
    const json_t *providers = ce3_get(r, "providers");
    json_t *args = json_object();
    ce3_arg_node(args, "providers", providers);
    ce3_set_str(args, "key", endpoint_id);
    json_t *r2 = ce3_call(s, "find_provider_entry", args);
    json_free(r);
    if (ce3_is_raise(r2)) return r2;
    const json_t *stored = NULL, *entry = NULL;
    json_t *bad = ce3_unpack2(r2, &stored, &entry);
    if (bad) { json_free(r2); return bad; }
    (void)stored;
    json_t *ret = NULL;
    if (ce3_is_obj(entry)) {
        const json_t *ak = ce3_get(entry, "api_key");
        if (ak && ak->type != JSON_NULL) ret = json_copy(ak);
    }
    json_free(r2);
    return ret;                                   /* NULL == None */
}

/* ═══════════════════════ mutation flows ═══════════════════════ */

/* PoP: ce3_pop_legacy_custom_provider @ hermes_cli/web_routers/config_env.py:_pop_legacy_custom_provider */
static json_t *ce3_pop_legacy_custom_provider(const ce3_seams_t *s, json_t *cfg,
                                              const char *provider_key) {
    /* legacy = cfg.get("custom_providers"); if not isinstance(legacy, list): return None
     * for index, entry in enumerate(legacy):
     *     if isinstance(entry, dict) and _custom_endpoint_id(str(entry.get("name") or "")) == provider_key:
     *         return legacy.pop(index)
     * return None */
    if (!ce3_is_obj(cfg)) return ce3_attr_err(cfg, "get");
    const json_t *legacy = ce3_get(cfg, "custom_providers");
    if (!ce3_is_arr(legacy)) return NULL;
    size_t n = json_len(legacy);
    for (size_t i = 0; i < n; i++) {
        const json_t *entry = json_get(legacy, i);
        if (!ce3_is_obj(entry)) continue;
        char *raw = ce3_str_or_empty(ce3_get(entry, "name"));
        json_t *args = json_object();
        ce3_set_str(args, "raw", raw);
        free(raw);
        json_t *r = ce3_call(s, "_custom_endpoint_id", args);
        if (ce3_is_raise(r)) return r;
        bool match = ce3_is_str(r) && !strcmp(r->str_val ? r->str_val : "", provider_key);
        json_free(r);
        if (match) {
            json_t *popped = json_copy(entry);
            json_t *rest = ce3_arr_without(legacy, i);
            json_set(cfg, "custom_providers", rest);   /* replaces in place, keeps position */
            return popped;
        }
    }
    return NULL;
}

/* PoP: ce3_detach_main_model_from_provider @ hermes_cli/web_routers/config_env.py:_detach_main_model_from_provider */
static json_t *ce3_detach_main_model_from_provider(const ce3_seams_t *s, json_t *cfg,
                                                   const char *provider_key,
                                                   const json_t *entry) {
    /* model_cfg = cfg.get("model")
     * if not isinstance(model_cfg, dict) or not _model_names_provider(model_cfg, provider_key, entry):
     *     return
     * for field in ("provider", "base_url", "api_key", "key_env"): model_cfg.pop(field, None)
     * cfg["model"] = model_cfg   (same object — the reassignment is a no-op) */
    if (!ce3_is_obj(cfg)) return ce3_attr_err(cfg, "get");
    json_t *model_cfg = (json_t *)ce3_get(cfg, "model");
    if (!ce3_is_obj(model_cfg)) return NULL;
    json_t *named = ce3_model_names_provider(s, model_cfg, provider_key, entry);
    if (ce3_is_raise(named)) return named;
    bool hit = ce3_jtruth(named);
    json_free(named);
    if (!hit) return NULL;
    ce3_pop(model_cfg, "provider");
    ce3_pop(model_cfg, "base_url");
    ce3_pop(model_cfg, "api_key");
    ce3_pop(model_cfg, "key_env");
    return NULL;
}

/* PoP: ce3_drop_stale_addresses @ hermes_cli/web_routers/config_env.py:_drop_stale_addresses */
static json_t *ce3_drop_stale_addresses(json_t *entry) {
    /* for field in ENDPOINT_ADDRESS_FIELDS:
     *     if field != "base_url": entry.pop(field, None) */
    if (!ce3_is_obj(entry)) return ce3_attr_err(entry, "pop");
    for (size_t i = 0; i < sizeof(CE3_ADDRESS_FIELDS) / sizeof(CE3_ADDRESS_FIELDS[0]); i++)
        if (strcmp(CE3_ADDRESS_FIELDS[i], "base_url") != 0)
            ce3_pop(entry, CE3_ADDRESS_FIELDS[i]);
    return NULL;
}
/* PoP: ce3_store_endpoint_key @ hermes_cli/web_routers/config_env.py:_store_endpoint_key */
static json_t *ce3_store_endpoint_key(const ce3_seams_t *s, json_t *entry,
                                      const json_t *env_var, const char *key,
                                      const char *base_url, const json_t *own_provider) {
    /* try: store_secret(env_var, key, base_url, own_provider)
     * except ValueError as exc: raise HTTPException(status_code=400, detail=str(exc)) from exc
     * entry["key_env"] = env_var; entry.pop("api_key", None) */
    json_t *args = json_object();
    ce3_set_node(args, "env_name", env_var);
    ce3_set_str(args, "value", key);
    ce3_set_str(args, "base_url", base_url);
    ce3_arg_node(args, "own_provider", own_provider);
    json_t *r = ce3_call(s, "store_secret", args);
    if (ce3_is_raise(r)) {
        if (ce3_cls_is(ce3_raise_class(r), "ValueError")) {
            json_t *e = ce3_http_exc(400, ce3_raise_msg(r));
            json_free(r);
            return e;
        }
        return r;                                 /* everything else passes through */
    }
    json_free(r);
    if (!ce3_is_obj(entry)) {
        char *m = ce3_xsf("'%s' object does not support item assignment", ce3_type_name(entry));
        json_t *e = ce3_raise("TypeError", m);
        free(m);
        return e;
    }
    json_set(entry, "key_env", env_var ? json_copy(env_var) : json_null());
    ce3_pop(entry, "api_key");
    return NULL;
}

/* PoP: ce3_apply_endpoint_key @ hermes_cli/web_routers/config_env.py:_apply_endpoint_key */
static json_t *ce3_apply_endpoint_key(const ce3_seams_t *s, json_t *entry,
                                      const json_t *body, const char *endpoint_id,
                                      const char *base_url, const json_t *stored_key) {
    /* env_var = dashboard_endpoint_key_env(entry, endpoint_id)
     * submitted_key = body.api_key.strip() if body.api_key is not None else None
     * if submitted_key:
     *     if _ENV_REF_RE.fullmatch(submitted_key) or is_redacted_credential_preview(submitted_key):
     *         raise HTTPException(400, REDACTED_CREDENTIAL_WRITE_DETAIL)
     *     _store_endpoint_key(entry, env_var, submitted_key, base_url, stored_key)
     * elif submitted_key is not None:
     *     raw = read_raw_config()
     *     own = raw["providers"].get(stored_key) if isinstance(raw.get("providers"), dict) else None
     *     if not any(block is not own for block in env_reference_owners(raw, env_var)):
     *         remove_env_value(env_var)
     *     entry.pop("key_env", None); entry.pop("api_key", None)
     * elif str(entry.get("api_key") or "").strip() and not _config_api_key_is_env_ref(endpoint_id):
     *     _store_endpoint_key(entry, env_var, entry["api_key"].strip(), base_url, stored_key) */
    json_t *args = json_object();
    ce3_arg_node(args, "entry", entry);
    ce3_set_str(args, "endpoint_id", endpoint_id);
    json_t *env_var = ce3_call(s, "dashboard_endpoint_key_env", args);
    if (ce3_is_raise(env_var)) return env_var;

    /* body.api_key — SimpleNamespace double; str or None in the fixture domain. */
    if (!ce3_is_obj(body) || !json_obj_get(body, "api_key")) {
        json_free(env_var);
        return ce3_raise("AttributeError",
                         "'types.SimpleNamespace' object has no attribute 'api_key'");
    }
    const json_t *ak_body = ce3_get(body, "api_key");
    char *submitted = NULL;
    if (ak_body && ak_body->type != JSON_NULL) {
        if (!ce3_is_str(ak_body)) {
            json_t *e = ce3_attr_err(ak_body, "strip");
            json_free(env_var);
            return e;
        }
        submitted = ce3_strip(ak_body->str_val ? ak_body->str_val : "");
    }

    json_t *ret = NULL;
    if (submitted && submitted[0]) {
        /* truthy: a new key — reject display-only previews, else store it. */
        if (ce3_env_ref_fullmatch(submitted)) {
            ret = ce3_http_exc(400, CE3_REDACTED_DETAIL);
        } else {
            args = json_object();
            ce3_set_str(args, "submitted", submitted);
            json_t *r2 = ce3_call(s, "is_redacted_credential_preview", args);
            if (ce3_is_raise(r2)) {
                ret = r2;
            } else {
                bool redacted = ce3_jtruth(r2);
                json_free(r2);
                if (redacted) ret = ce3_http_exc(400, CE3_REDACTED_DETAIL);
                else ret = ce3_store_endpoint_key(s, entry, env_var, submitted,
                                                  base_url, stored_key);
            }
        }
    } else if (submitted) {
        /* blank field: clear the key unless another block still reads the variable. */
        json_t *raw = ce3_call(s, "read_raw_config", json_object());
        if (ce3_is_raise(raw)) {
            ret = raw;
        } else if (!ce3_is_obj(raw)) {
            ret = ce3_attr_err(raw, "get");
            json_free(raw);
        } else {
            const json_t *providers = ce3_get(raw, "providers");
            const json_t *own = NULL;
            if (ce3_is_obj(providers) && ce3_is_str(stored_key))
                own = ce3_get(providers, stored_key->str_val ? stored_key->str_val : "");
            bool own_is_none = !own || own->type == JSON_NULL;
            args = json_object();
            ce3_arg_node(args, "cfg", raw);
            ce3_set_node(args, "env_name", env_var);
            json_t *owners = ce3_call(s, "env_reference_owners", args);
            json_free(raw);
            if (ce3_is_raise(owners)) {
                ret = owners;
            } else if (!ce3_is_arr(owners)) {
                char *m = ce3_xsf("'%s' object is not iterable", ce3_type_name(owners));
                ret = ce3_raise("TypeError", m);
                free(m);
                json_free(owners);
            } else {
                /* any(block is not own ...) — identity: the {"__own__": true} marker
                 * denotes own itself; a JSON null block is the None singleton, identical
                 * to own when own is None; every other block is a distinct object. */
                bool any_distinct = false;
                size_t n = json_len(owners);
                for (size_t i = 0; i < n; i++) {
                    const json_t *block = json_get(owners, i);
                    bool identical = ce3_is_obj(block) &&
                                     json_get_bool(block, "__own__", false);
                    if (!identical && own_is_none && (!block || block->type == JSON_NULL))
                        identical = true;
                    if (!identical) { any_distinct = true; break; }
                }
                json_free(owners);
                if (!any_distinct) {
                    args = json_object();
                    ce3_set_node(args, "key", env_var);
                    json_t *r4 = ce3_call(s, "remove_env_value", args);
                    if (ce3_is_raise(r4)) ret = r4;
                    else json_free(r4);
                }
                if (!ret) {
                    if (!ce3_is_obj(entry)) ret = ce3_attr_err(entry, "pop");
                    else { ce3_pop(entry, "key_env"); ce3_pop(entry, "api_key"); }
                }
            }
        }
    } else {
        /* None: migrate a plaintext key an earlier release wrote (str + strip truthy,
         * and the on-disk key is not already a ${VAR} template). */
        if (!ce3_is_obj(entry)) {
            ret = ce3_attr_err(entry, "get");
        } else {
            const json_t *ak = ce3_get(entry, "api_key");
            char *s0 = ce3_str_or_empty(ak);
            char *st = ce3_strip(s0);
            free(s0);
            if (st[0]) {
                args = json_object();
                ce3_set_str(args, "endpoint_id", endpoint_id);
                json_t *r2 = ce3_call(s, "_config_api_key_is_env_ref", args);
                if (ce3_is_raise(r2)) {
                    ret = r2;
                } else {
                    bool is_ref = ce3_jtruth(r2);
                    json_free(r2);
                    if (!is_ref) {
                        if (!ce3_is_str(ak)) {
                            ret = ce3_attr_err(ak, "strip");
                        } else {
                            char *key = ce3_strip(ak->str_val ? ak->str_val : "");
                            ret = ce3_store_endpoint_key(s, entry, env_var, key,
                                                         base_url, stored_key);
                            free(key);
                        }
                    }
                }
            }
            free(st);
        }
    }
    free(submitted);
    json_free(env_var);
    return ret;
}

/* ═══════════════════════ credential / route-state flows ═══════════════════════ */

/* PoP: ce3_save_env_credential @ hermes_cli/web_routers/config_env.py:_save_env_credential */
static json_t *ce3_save_env_credential(const ce3_seams_t *s, const char *key,
                                       const char *value, bool provider_setup) {
    /* previous = load_env().get(key)
     * result = save_provider_env_credential(key, value)
     * record_api_key_saved(key, value, previous, web_setup_surface(), connecting=provider_setup)
     * return result
     * Kwarg evaluation: web_setup_surface() fires BEFORE record_api_key_saved. */
    json_t *r1 = ce3_call(s, "load_env", json_object());
    if (ce3_is_raise(r1)) return r1;
    if (!ce3_is_obj(r1)) {
        json_t *e = ce3_attr_err(r1, "get");
        json_free(r1);
        return e;
    }
    const json_t *prev = ce3_get(r1, key);
    json_t *previous = prev ? json_copy(prev) : NULL;
    json_free(r1);
    json_t *args = json_object();
    ce3_set_str(args, "env_var", key);
    ce3_set_str(args, "value", value);
    json_t *result = ce3_call(s, "save_provider_env_credential", args);
    if (ce3_is_raise(result)) { json_free(previous); return result; }
    json_t *surface = ce3_call(s, "web_setup_surface", json_object());
    if (ce3_is_raise(surface)) { json_free(previous); json_free(result); return surface; }
    args = json_object();
    ce3_set_str(args, "env_var", key);
    ce3_set_str(args, "value", value);
    ce3_arg_node(args, "previous", previous);
    ce3_set_node(args, "surface", surface);
    json_set(args, "connecting", json_bool(provider_setup));
    json_t *r4 = ce3_call(s, "record_api_key_saved", args);
    json_free(surface);
    json_free(previous);
    if (ce3_is_raise(r4)) { json_free(result); return r4; }
    json_free(r4);
    return result;                                /* verbatim (may be NULL == None) */
}

/* PoP: ce3_drop_carried_route_state @ hermes_cli/web_routers/config_env.py:_drop_carried_route_state */
static json_t *ce3_drop_carried_route_state(const ce3_seams_t *s, const json_t *existing,
                                            const json_t *merged) {
    /* drop_carried_route_state(existing.get("model"), merged.get("model"), merged)
     * deleg = merged.get("delegation")
     * if not (isinstance(deleg, dict) and deleg.get("base_url")
     *         and str(deleg.get("provider") or "").strip().lower() in ("", "custom", "auto")):
     *     drop_carried_route_state(existing.get("delegation"), deleg, merged)
     * old_aux, new_aux = existing.get("auxiliary"), merged.get("auxiliary")
     * if isinstance(old_aux, dict) and isinstance(new_aux, dict):
     *     for task, block in new_aux.items():
     *         drop_carried_route_state(old_aux.get(task), block, merged) */
    if (!ce3_is_obj(existing)) return ce3_attr_err(existing, "get");
    if (!ce3_is_obj(merged)) return ce3_attr_err(merged, "get");
    json_t *args = json_object();
    ce3_arg_node(args, "old", ce3_get(existing, "model"));
    ce3_arg_node(args, "new", ce3_get(merged, "model"));
    ce3_arg_node(args, "config", merged);
    json_t *r = ce3_call(s, "drop_carried_route_state", args);
    if (ce3_is_raise(r)) return r;
    json_free(r);

    const json_t *deleg = ce3_get(merged, "delegation");
    bool keyless_with_url = false;
    if (ce3_is_obj(deleg) && ce3_jtruth(ce3_get(deleg, "base_url"))) {
        char *p0 = ce3_str_or_empty(ce3_get(deleg, "provider"));
        char *p1 = ce3_strip(p0);
        char *pl = ce3_lower(p1);
        free(p0); free(p1);
        keyless_with_url = !strcmp(pl, "") || !strcmp(pl, "custom") || !strcmp(pl, "auto");
        free(pl);
    }
    if (!keyless_with_url) {
        args = json_object();
        ce3_arg_node(args, "old", ce3_get(existing, "delegation"));
        ce3_arg_node(args, "new", deleg);
        ce3_arg_node(args, "config", merged);
        r = ce3_call(s, "drop_carried_route_state", args);
        if (ce3_is_raise(r)) return r;
        json_free(r);
    }

    const json_t *old_aux = ce3_get(existing, "auxiliary");
    const json_t *new_aux = ce3_get(merged, "auxiliary");
    if (ce3_is_obj(old_aux) && ce3_is_obj(new_aux)) {
        for (size_t i = 0; i < new_aux->c.count; i++) {
            args = json_object();
            ce3_arg_node(args, "old", ce3_get(old_aux, new_aux->c.keys[i]));
            ce3_arg_node(args, "new", new_aux->c.items[i]);
            ce3_arg_node(args, "config", merged);
            r = ce3_call(s, "drop_carried_route_state", args);
            if (ce3_is_raise(r)) return r;
            json_free(r);
        }
    }
    return NULL;
}

/* ═══════════════════════ probe helpers ═══════════════════════ */

/* PoP: ce3_no_models_probe_message @ hermes_cli/web_routers/config_env.py:_no_models_probe_message */
static json_t *ce3_no_models_probe_message(const ce3_seams_t *s, const json_t *resp,
                                           const char *base_url) {
    /* url = base_url + "/models"
     * content_type = (resp.headers.get("content-type") or "").split(";")[0].strip()
     * try: payload = resp.json()
     * except ValueError: payload = None
     * if not isinstance(payload, (dict, list)):
     *     kind = content_type or "a non-JSON body"
     *     return f"Connected to {url}, but it returned {kind} instead of a JSON model list — ..."
     * data = payload.get("data") if isinstance(payload, dict) else payload
     * if not isinstance(data, list):
     *     return f"Connected to {url}, but its response is not an OpenAI-compatible model list."
     * return f"Connected to {url}, but it advertised no models. Start a model on that endpoint and try again." */
    if (!ce3_is_obj(resp)) return ce3_attr_err(resp, "headers");
    const json_t *headers = ce3_get(resp, "headers");
    if (!ce3_is_obj(headers)) return ce3_attr_err(headers, "get");
    const json_t *ct_raw = ce3_get(headers, "content-type");
    if (ce3_jtruth(ct_raw) && !ce3_is_str(ct_raw)) return ce3_attr_err(ct_raw, "split");
    char *ct0 = ce3_str_or_empty(ct_raw);
    char *semi = strchr(ct0, ';');
    if (semi) *semi = '\0';                        /* split(";")[0] */
    char *content_type = ce3_strip(ct0);
    free(ct0);
    char *url = ce3_xsf("%s/models", base_url ? base_url : "");

    json_t *payload = ce3_call(s, "resp_json", json_object());
    if (ce3_is_raise(payload)) {
        if (ce3_cls_is(ce3_raise_class(payload), "ValueError")) {
            json_free(payload);
            payload = NULL;                        /* except ValueError: payload = None */
        } else {
            free(content_type); free(url);
            return payload;                        /* anything else passes through */
        }
    }
    char *msg = NULL;
    if (!ce3_is_obj(payload) && !ce3_is_arr(payload)) {
        const char *kind = content_type[0] ? content_type : "a non-JSON body";
        msg = ce3_xsf("Connected to %s, but it returned %s instead of a JSON model list "
                      "\xe2\x80\x94 the URL may be an SPA catch-all or a wrong base URL, "
                      "not an OpenAI-compatible API.", url, kind);
    } else {
        const json_t *data = ce3_is_obj(payload) ? ce3_get(payload, "data") : payload;
        if (!ce3_is_arr(data)) {
            msg = ce3_xsf("Connected to %s, but its response is not an OpenAI-compatible "
                          "model list.", url);
        } else {
            msg = ce3_xsf("Connected to %s, but it advertised no models. "
                          "Start a model on that endpoint and try again.", url);
        }
    }
    json_t *ret = json_string(msg);
    free(msg);
    json_free(payload);
    free(content_type);
    free(url);
    return ret;
}

/* PoP: ce3_auto_api_mode @ hermes_cli/web_routers/config_env.py:_auto_api_mode */
static json_t *ce3_auto_api_mode(const ce3_seams_t *s, const char *base_url) {
    /* return _detect_api_mode_for_url(base_url) or "chat_completions" */
    json_t *args = json_object();
    ce3_set_str(args, "base_url", base_url);
    json_t *r = ce3_call(s, "_detect_api_mode_for_url", args);
    if (ce3_is_raise(r)) return r;
    if (ce3_jtruth(r)) return r;                  /* truthy operand returned verbatim */
    json_free(r);
    return json_string("chat_completions");
}

/* PoP: ce3_env_write_errors @ hermes_cli/web_routers/config_env.py:_env_write_errors */
static json_t *ce3_env_write_errors(const ce3_seams_t *s, const char *log_msg,
                                    const json_t *body_raise) {
    /* The @contextlib.contextmanager exit classification. The with-block body projects
     * to body_raise (a raise spec) or NULL (normal completion -> the CM returns None).
     *   except HTTPException: raise                      (pass through verbatim)
     *   except ValueError as exc: raise HTTPException(400, str(exc)) from exc
     *   except Exception: _log.exception(log_msg); raise HTTPException(500, "Internal server error")
     * BaseException-only kinds (SystemExit / KeyboardInterrupt / GeneratorExit /
     * BaseExceptionGroup) are NOT caught by except Exception and pass through.
     * Unknown scripted kinds are synthetic Exception subclasses on the live side,
     * so they take the Exception branch. */
    if (!ce3_is_raise(body_raise)) return NULL;
    const char *cls = ce3_raise_class(body_raise);
    if (ce3_cls_is(cls, "HTTPException")) return json_copy(body_raise);
    if (ce3_cls_is(cls, "ValueError")) return ce3_http_exc(400, ce3_raise_msg(body_raise));
    if (ce3_cls_is(cls, "Exception") || !ce3_cls_is(cls, "BaseException")) {
        json_t *args = json_object();
        ce3_set_str(args, "log_msg", log_msg);
        json_t *r = ce3_call(s, "_log.exception", args);
        if (ce3_is_raise(r)) return r;            /* a log-seam raise replaces the 500 */
        json_free(r);
        return ce3_http_exc(500, "Internal server error");
    }
    return json_copy(body_raise);                 /* BaseException passthrough */
}

/* PoP: ce3_record_custom_endpoint_setup @ hermes_cli/web_routers/config_env.py:_record_custom_endpoint_setup */
static json_t *ce3_record_custom_endpoint_setup(const ce3_seams_t *s, const json_t *home) {
    /* record_provider_setup_done(web_setup_surface(), "custom", hermes_home=home, background=True) */
    json_t *surface = ce3_call(s, "web_setup_surface", json_object());
    if (ce3_is_raise(surface)) return surface;
    json_t *args = json_object();
    ce3_set_node(args, "surface", surface);
    ce3_set_str(args, "provider", "custom");
    ce3_arg_node(args, "hermes_home", home);
    json_set(args, "background", json_bool(true));
    json_t *r = ce3_call(s, "record_provider_setup_done", args);
    json_free(surface);
    if (ce3_is_raise(r)) return r;
    json_free(r);
    return NULL;
}