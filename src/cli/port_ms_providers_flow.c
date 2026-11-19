/* port_ms_providers_flow.c — hermes_cli/model_switch_providers.py FLOW.
 *
 * Decision-sequence port: credential discovery ladders, curated/live model-id
 * resolution ordering, the Nous free-tier / policy / usage branches, custom
 * endpoint probe/cache decisions and the row finalization post-passes. All
 * config/credential/network I/O rides the injected universal seam (msp_call);
 * the harness scripts results and records the ordered trace the oracle diffs
 * against LIVE hermes_cli/model_switch_providers.py under the same scripts.
 *
 * Representation contract: see include/port_ms_providers_flow.h.
 * libjson-only (default oracle deps suffice).
 */
#include "port_ms_providers_flow.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── string helpers (ASCII fixture domain; Python str semantics) ─────────── */

static char *msp_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static bool msp_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.strip() — both ends, ASCII whitespace fixture domain. */
static char *msp_strip(const char *s) {
    while (*s && msp_is_ws(*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && msp_is_ws(s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

/* str.lower() — ASCII fixture domain. */
static char *msp_lower(const char *s) {
    char *out = msp_strdup(s);
    if (out) for (char *p = out; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}

static char *msp_strip_lower(const char *s) {
    char *t = msp_strip(s ? s : "");
    char *out = msp_lower(t ? t : "");
    free(t);
    return out;
}

/* str.rstrip(chars) — trailing occurrences of an explicit character set. */
static char *msp_rstrip_chars(const char *s, const char *set) {
    size_t n = strlen(s);
    while (n > 0 && strchr(set, s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

static bool msp_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

/* ── JSON value helpers ──────────────────────────────────────────────────── */

static bool msp_is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

/* Python truthiness for a JSON node (NULL pointer == None == falsy). */
static bool msp_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

/* `v is True` — identity with the True singleton (JSON true only). */
static bool msp_is_true(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}

/* String value of a node ("" for non-strings; fixture domain). */
static const char *msp_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static bool msp_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool msp_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

/* str(x or "") for string/null fixture fields. */
static char *msp_str_or_empty(const json_t *v) {
    if (msp_truthy(v) && v->type == JSON_STRING) return msp_strdup(v->str_val);
    return msp_strdup("");
}

static bool msp_arr_has_str(const json_t *arr, const char *s) {
    if (!msp_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && msp_str_eq(it->str_val, s)) return true;
    }
    return false;
}

/* Deep Python-style equality; NULL pointer and JSON_NULL both read None.
 * Object key order is irrelevant (dict ==). */
static bool msp_json_eq(const json_t *a, const json_t *b) {
    if (msp_is_none(a) && msp_is_none(b)) return true;
    if (msp_is_none(a) || msp_is_none(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return msp_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!msp_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !msp_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* list(dict) == model_ids — ordered key/list comparison (insertion order). */
static bool msp_keys_eq_list(const json_t *obj, const json_t *list) {
    if (!msp_is_arr(list)) return false;
    if (obj->c.count != list->c.count) return false;
    for (size_t i = 0; i < obj->c.count; i++) {
        json_t *it = list->c.items[i];
        if (!it || it->type != JSON_STRING || !msp_str_eq(obj->c.keys[i], it->str_val))
            return false;
    }
    return true;
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

bool msp_exc_pending(const msp_ctx *ctx) { return ctx && ctx->exc != NULL; }

void msp_exc_clear(msp_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}

void msp_raise(msp_ctx *ctx, const char *cls) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
}

bool msp_exc_is(const msp_ctx *ctx, const char *cls) {
    return ctx && ctx->exc && msp_str_eq(json_get_str(ctx->exc, "__raise__", ""), cls);
}

/* Builtin exception subclass chain (parent lookup). Custom scripted classes
 * have no entry and read as Exception subclasses (make_exc fallback). */
static const char *msp_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"ModuleNotFoundError", "ImportError"},
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"TimeoutError", "OSError"}, {"socket.timeout", "OSError"},
        {"NotImplementedError", "RuntimeError"},
        {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"FloatingPointError", "ArithmeticError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"IncompleteRead", "HTTPException"}, {"BadStatusLine", "HTTPException"},
        {"LineTooLong", "HTTPException"}, {"InvalidURL", "HTTPException"},
        {"UnknownProtocol", "HTTPException"}, {"UnknownTransferEncoding", "HTTPException"},
        {"UnimplementedFileMode", "HTTPException"}, {"ImproperConnectionState", "HTTPException"},
        {"CannotSendRequest", "ImproperConnectionState"}, {"CannotSendHeader", "ImproperConnectionState"},
        {"ResponseNotReady", "ImproperConnectionState"}, {"NotConnected", "ImproperConnectionState"},
        {"RemoteDisconnected", "ConnectionResetError"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (msp_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}

bool msp_exc_is_exception(const msp_ctx *ctx) {
    if (!msp_exc_pending(ctx)) return false;
    const char *cls = json_get_str(ctx->exc, "__raise__", "");
    for (int depth = 0; depth < 32; depth++) {
        if (msp_str_eq(cls, "Exception")) return true;
        if (msp_str_eq(cls, "SystemExit") || msp_str_eq(cls, "KeyboardInterrupt")
            || msp_str_eq(cls, "GeneratorExit") || msp_str_eq(cls, "BaseException"))
            return false;
        const char *p = msp_exc_parent(cls);
        if (!p) return true; /* custom Exception subclass */
        cls = p;
    }
    return false;
}

bool msp_exc_matches(const msp_ctx *ctx, const char *const *classes, size_t n) {
    if (!msp_exc_pending(ctx)) return false;
    const char *cls = json_get_str(ctx->exc, "__raise__", "");
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (msp_str_eq(cls, classes[i])) return true;
        const char *p = msp_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}

/* Tri-state exception disposition after a seam/internal call:
 *   0 = no exception pending; 1 = caught by `except Exception` and cleared;
 *   2 = not catchable here (caller propagates by returning NULL). */
static int msp_exc_tri(msp_ctx *ctx) {
    if (!msp_exc_pending(ctx)) return 0;
    if (msp_exc_is_exception(ctx)) { msp_exc_clear(ctx); return 1; }
    return 2;
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *msp_script_result(msp_ctx *ctx, const json_t *script) {
    /* Returns a NEW value (NULL == None); sets ctx->exc for a scripted raise. */
    if (!script || script->type == JSON_NULL) return NULL;
    if (script->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(script, "__raise__");
        if (raise && raise->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(script);
            return NULL;
        }
    }
    return json_copy(script);
}

json_t *msp_call(msp_ctx *ctx, const char *name, json_t *args) {
    /* args ownership is transferred (NULL -> empty object). Result is NEW. */
    if (!args) args = json_object();
    json_t *ret = NULL;
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *chosen = script;
    if (script && script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                if (ctx->exc) json_free(ctx->exc);
                ctx->exc = json_object();
                json_set(ctx->exc, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof(msg), "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = msp_script_result(ctx, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (ctx->exc && chosen == ctx->exc) json_set(ev, "ret", json_copy(ctx->exc));
    else if (ctx->exc && chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))
        json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    return ret;
}

/* Arg builders: None-valued keys are dropped on BOTH sides ("" is kept). */
static void msp_arg_json(json_t *args, const char *key, json_t *val) {
    /* adopts val */
    if (msp_is_none(val)) { json_free(val); return; }
    json_set(args, key, val);
}

static void msp_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}

static void msp_arg_copy(json_t *args, const char *key, const json_t *val) {
    if (msp_is_none(val)) return;
    json_set(args, key, json_copy(val));
}

static void msp_arg_bool(json_t *args, const char *key, bool val) {
    json_set(args, key, json_bool(val));
}

/* read_env seam: os.environ.get(name, "") — None result reads as "". Returns
 * a NEW string (NULL only when an exception is pending). */
static char *msp_read_env(msp_ctx *ctx, const char *name) {
    json_t *args = json_object();
    msp_arg_str(args, "name", name);
    json_t *r = msp_call(ctx, "read_env", args);
    if (msp_exc_pending(ctx)) { json_free(r); return NULL; }
    char *out = msp_strdup(msp_jstr(r));
    json_free(r);
    return out;
}

/* ── module constants (live Assigns; fixture / source order) ─────────────── */

/* _UNCAPPED_PICKER_PROVIDERS frozenset. */
static const char *const MSP_UNCAPPED[] = {"opencode-zen", "opencode-go", "nous", "openrouter"};
#define MSP_UNCAPPED_N (sizeof(MSP_UNCAPPED) / sizeof(MSP_UNCAPPED[0]))

/* hermes_cli.anon_auth FREE_TIER_LABEL / GUEST_MODEL (live Assigns). */
#define MSP_FREE_TIER_LABEL "Nous \xc2\xb7 free tier"
#define MSP_GUEST_MODEL "nous/welcome"

/* hermes_cli.config_providers ENDPOINT_ADDRESS_FIELDS (live Assign). */
static const char *const MSP_ENDPOINT_ADDRESS_FIELDS[] = {"base_url", "url", "api", "baseUrl"};
#define MSP_ENDPOINT_ADDRESS_FIELDS_N \
    (sizeof(MSP_ENDPOINT_ADDRESS_FIELDS) / sizeof(MSP_ENDPOINT_ADDRESS_FIELDS[0]))

/* env table helpers (borrowed). */
static const json_t *msp_env_get(const msp_ctx *ctx, const char *key) {
    return ctx->env ? json_obj_get(ctx->env, key) : NULL;
}

static const json_t *msp_registry_get(const msp_ctx *ctx, const char *key) {
    const json_t *reg = msp_env_get(ctx, "provider_registry");
    return msp_is_obj(reg) ? json_obj_get(reg, key) : NULL;
}

/* ── internal pure helpers (no PoP claim; stay REAL in the oracle) ───────── */

/* _first_curated — first non-empty curated list under keys; the last key's
 * (possibly empty/None) value otherwise. Returns a NEW value (NULL == None). */
static json_t *msp_first_curated(const json_t *curated, const json_t *keys) {
    const json_t *last = NULL;
    bool have = false;
    if (msp_is_arr(keys)) {
        for (size_t i = 0; i < keys->c.count; i++) {
            const char *key = msp_jstr(keys->c.items[i]);
            const json_t *v = msp_is_obj(curated) ? json_obj_get(curated, key) : NULL;
            last = v; /* curated.get(key, []) — missing reads as the [] default */
            have = true;
            if (msp_truthy(v)) break;
        }
    }
    if (!have) return json_array(); /* keys empty -> initial [] */
    return last ? json_copy(last) : json_array();
}

/* fallback_keys or (slug,) — a NEW keys array (owned). */
static json_t *msp_fallback_keys(const char *slug, const json_t *fallback_keys) {
    if (msp_is_arr(fallback_keys) && fallback_keys->c.count > 0)
        return json_copy(fallback_keys);
    json_t *k = json_array();
    json_append(k, json_string(slug ? slug : ""));
    return k;
}

/* ── ported flows/leaves ─────────────────────────────────────────────────── */

/* PoP: _discovered_catalog_stale @ hermes_cli/model_switch_providers.py:_discovered_catalog_stale */
json_t *msp_discovered_catalog_stale(msp_ctx *ctx, const json_t *entry, const json_t *model_ids) {
    (void)ctx;
    const json_t *existing = msp_is_obj(entry) ? json_obj_get(entry, "models") : NULL;
    bool legacy = msp_is_obj(existing)
        && msp_is_true(json_obj_get(existing, "__discovered_model_catalog__"));
    bool entry_discovered = msp_is_true(msp_is_obj(entry) ? json_obj_get(entry, "models_discovered") : NULL)
        || legacy;
    if (msp_is_obj(existing))
        return json_bool(entry_discovered && (legacy || !msp_keys_eq_list(existing, model_ids)));
    if (msp_is_arr(existing)) {
        bool any_dict = false;
        for (size_t i = 0; i < existing->c.count; i++)
            if (msp_is_obj(existing->c.items[i])) { any_dict = true; break; }
        return json_bool(!any_dict && !msp_json_eq(existing, model_ids));
    }
    return json_bool(true);
}

/* PoP: _any_env @ hermes_cli/model_switch_providers.py:_any_env */
json_t *msp_any_env(msp_ctx *ctx, const json_t *env_vars) {
    if (!msp_is_arr(env_vars)) return json_bool(false);
    for (size_t i = 0; i < env_vars->c.count; i++) {
        json_t *args = json_object();
        msp_arg_str(args, "name", msp_jstr(env_vars->c.items[i]));
        json_t *r = msp_call(ctx, "read_env", args);
        if (msp_exc_pending(ctx)) { json_free(r); return NULL; }
        bool hit = msp_truthy(r);
        json_free(r);
        if (hit) return json_bool(true); /* any() short-circuits */
    }
    return json_bool(false);
}

/* PoP: _skip @ hermes_cli/model_switch_providers.py:_skip */
json_t *msp_skip(msp_ctx *ctx, const json_t *seen, const json_t *excluded, const json_t *keys) {
    (void)ctx;
    if (!msp_is_arr(keys)) return json_bool(false);
    /* lowered = [k.lower() for k in keys]; any(seen) or any(excluded). */
    for (size_t i = 0; i < keys->c.count; i++) {
        char *lk = msp_lower(msp_jstr(keys->c.items[i]));
        bool hit = msp_arr_has_str(seen, lk);
        free(lk);
        if (hit) return json_bool(true);
    }
    for (size_t i = 0; i < keys->c.count; i++) {
        char *lk = msp_lower(msp_jstr(keys->c.items[i]));
        bool hit = msp_arr_has_str(excluded, lk);
        free(lk);
        if (hit) return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: _auth_store_has_provider @ hermes_cli/model_switch_providers.py:_auth_store_has_provider */
json_t *msp_auth_store_has_provider(msp_ctx *ctx, const json_t *keys) {
    json_t *store = msp_call(ctx, "load_auth_store", NULL);
    int t = msp_exc_tri(ctx);
    if (t == 2) { json_free(store); return NULL; }
    if (t == 1) { json_free(store); return json_bool(false); }
    /* store.get("providers", {}) — None store raises AttributeError (caught). */
    if (msp_is_none(store)) { json_free(store); return json_bool(false); }
    bool result = false;
    if (msp_truthy(store)) {
        const json_t *prov = json_obj_get(store, "providers");
        if (!prov) {
            result = false; /* default {} -> any() over empty */
        } else if (msp_is_none(prov)) {
            result = false; /* `k in None` TypeError (caught) */
        } else if (msp_is_obj(prov) && msp_is_arr(keys)) {
            for (size_t i = 0; i < keys->c.count; i++) {
                if (json_obj_get(prov, msp_jstr(keys->c.items[i]))) { result = true; break; }
            }
        }
    }
    json_free(store);
    return json_bool(result);
}

/* PoP: _raw_pool_usable @ hermes_cli/model_switch_providers.py:_raw_pool_usable */
json_t *msp_raw_pool_usable(msp_ctx *ctx, const char *hermes_id, bool for_picker) {
    json_t *store = msp_call(ctx, "load_auth_store", NULL);
    int t = msp_exc_tri(ctx);
    if (t == 2) { json_free(store); return NULL; }
    if (t == 1) { json_free(store); return json_bool(false); }
    /* if store and store.get("credential_pool", {}).get(hermes_id): */
    if (msp_truthy(store)) {
        const json_t *cp = json_obj_get(store, "credential_pool");
        if (!msp_is_none(cp)) {
            const json_t *entry = json_obj_get(cp, hermes_id ? hermes_id : "");
            if (msp_truthy(entry)) {
                json_t *args = json_object();
                msp_arg_str(args, "provider", hermes_id);
                msp_arg_bool(args, "raw_pool_present", true);
                msp_arg_bool(args, "for_picker", for_picker);
                json_t *r = msp_call(ctx, "credential_pool_is_usable", args);
                int t2 = msp_exc_tri(ctx);
                if (t2 == 2) { json_free(r); json_free(store); return NULL; }
                if (t2 == 1) { json_free(r); json_free(store); return json_bool(false); }
                json_free(store);
                return r; /* live returns the seam value directly (NULL == None) */
            }
        }
    }
    json_free(store);
    return json_bool(false);
}

/* PoP: _pool_usable @ hermes_cli/model_switch_providers.py:_pool_usable */
json_t *msp_pool_usable(msp_ctx *ctx, const char *slug, bool for_picker) {
    json_t *args = json_object();
    msp_arg_str(args, "provider", slug);
    msp_arg_bool(args, "for_picker", for_picker);
    json_t *r = msp_call(ctx, "credential_pool_is_usable", args);
    int t = msp_exc_tri(ctx);
    if (t == 2) { json_free(r); return NULL; }
    if (t == 1) { json_free(r); return json_bool(false); }
    return r; /* live returns the seam value directly (NULL == None) */
}

/* PoP: _azure_entra_configured @ hermes_cli/model_switch_providers.py:_azure_entra_configured */
json_t *msp_azure_entra_configured(msp_ctx *ctx) {
    json_t *cfg = msp_call(ctx, "get_model_config_dict", NULL);
    if (msp_exc_pending(ctx)) { json_free(cfg); return NULL; } /* no try: propagates */
    if (msp_is_none(cfg)) { json_free(cfg); msp_raise(ctx, "AttributeError"); return NULL; }
    char *prov = msp_str_or_empty(json_obj_get(cfg, "provider"));
    char *prov_n = msp_strip_lower(prov);
    free(prov);
    char *mode = msp_str_or_empty(json_obj_get(cfg, "auth_mode"));
    char *mode_n = msp_strip_lower(mode);
    free(mode);
    bool ok = msp_str_eq(prov_n, "azure-foundry") && msp_str_eq(mode_n, "entra_id");
    free(prov_n);
    free(mode_n);
    if (!ok) { json_free(cfg); return json_bool(false); }
    char *base = msp_str_or_empty(json_obj_get(cfg, "base_url"));
    char *base_s = msp_strip(base);
    free(base);
    bool have_base = base_s && base_s[0] != '\0';
    free(base_s);
    json_free(cfg);
    if (have_base) return json_bool(true);
    char *env = msp_read_env(ctx, "AZURE_FOUNDRY_BASE_URL");
    if (!env) return NULL; /* read_env raise propagates */
    bool hit = env[0] != '\0';
    free(env);
    return json_bool(hit);
}

/* PoP: _overlay_has_env_creds @ hermes_cli/model_switch_providers.py:_overlay_has_env_creds */
json_t *msp_overlay_has_env_creds(msp_ctx *ctx, const char *pid, const char *hermes_slug,
                                  const json_t *overlay) {
    const char *auth_type = msp_jstr(msp_is_obj(overlay) ? json_obj_get(overlay, "auth_type") : NULL);
    const json_t *extra = msp_is_obj(overlay) ? json_obj_get(overlay, "extra_env_vars") : NULL;
    bool has_creds = false;
    if (msp_str_eq(auth_type, "vertex")) {
        json_t *r = msp_call(ctx, "has_vertex_credentials", NULL);
        int t = msp_exc_tri(ctx);
        if (t == 2) { json_free(r); return NULL; }
        if (t == 0) has_creds = msp_truthy(r);
        json_free(r);
    } else if (msp_truthy(extra)) {
        json_t *r = msp_any_env(ctx, extra);
        if (msp_exc_pending(ctx)) { json_free(r); return NULL; }
        has_creds = msp_truthy(r);
        json_free(r);
    }
    if (!has_creds && msp_str_eq(auth_type, "api_key")) {
        const char *keys[2] = {pid, hermes_slug};
        for (int i = 0; i < 2; i++) {
            const json_t *pcfg = msp_registry_get(ctx, keys[i] ? keys[i] : "");
            const json_t *vars = msp_is_obj(pcfg) ? json_obj_get(pcfg, "api_key_env_vars") : NULL;
            if (msp_truthy(pcfg) && msp_truthy(vars)) {
                json_t *r = msp_any_env(ctx, vars);
                if (msp_exc_pending(ctx)) { json_free(r); return NULL; }
                bool hit = msp_truthy(r);
                json_free(r);
                if (hit) return json_bool(true);
            }
        }
    }
    if (!has_creds && msp_str_eq(hermes_slug, "azure-foundry")) {
        json_t *r = msp_azure_entra_configured(ctx);
        if (msp_exc_pending(ctx)) { json_free(r); return NULL; }
        has_creds = msp_truthy(r);
        json_free(r);
    }
    return json_bool(has_creds);
}

/* _set(name) = bool(os.environ.get(name, "").strip()); -1 on pending exc. */
static int msp_aws_set(msp_ctx *ctx, const char *name) {
    char *v = msp_read_env(ctx, name);
    if (!v) return -1;
    char *s = msp_strip(v);
    free(v);
    bool hit = s && s[0] != '\0';
    free(s);
    return hit ? 1 : 0;
}

/* PoP: _has_fast_aws_sdk_signal @ hermes_cli/model_switch_providers.py:_has_fast_aws_sdk_signal */
json_t *msp_has_fast_aws_sdk_signal(msp_ctx *ctx) {
    int a = msp_aws_set(ctx, "AWS_BEARER_TOKEN_BEDROCK");
    if (a < 0) return NULL;
    if (a) return json_bool(true);
    int k = msp_aws_set(ctx, "AWS_ACCESS_KEY_ID");
    if (k < 0) return NULL;
    if (k) {
        int s = msp_aws_set(ctx, "AWS_SECRET_ACCESS_KEY");
        if (s < 0) return NULL;
        if (s) return json_bool(true);
    }
    static const char *const names[] = {
        "AWS_PROFILE", "AWS_CONTAINER_CREDENTIALS_RELATIVE_URI",
        "AWS_CONTAINER_CREDENTIALS_FULL_URI", "AWS_WEB_IDENTITY_TOKEN_FILE",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        int v = msp_aws_set(ctx, names[i]);
        if (v < 0) return NULL;
        if (v) return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: _has_aws_sdk_creds_for_listing @ hermes_cli/model_switch_providers.py:_has_aws_sdk_creds_for_listing */
json_t *msp_has_aws_sdk_creds_for_listing(msp_ctx *ctx, const char *slug, const char *current_provider) {
    json_t *fast = msp_has_fast_aws_sdk_signal(ctx);
    if (msp_exc_pending(ctx)) { json_free(fast); return NULL; }
    bool hit = msp_truthy(fast);
    json_free(fast);
    if (hit) return json_bool(true);
    char *s = msp_strip_lower(slug);
    char *c = msp_strip_lower(current_provider);
    bool same = msp_str_eq(s, c);
    free(s);
    free(c);
    if (!same) return json_bool(false);
    json_t *r = msp_call(ctx, "has_aws_credentials", NULL);
    int t = msp_exc_tri(ctx);
    if (t == 2) { json_free(r); return NULL; }
    bool result = (t == 0) && msp_truthy(r);
    json_free(r);
    return json_bool(result);
}

/* PoP: _live_or_curated_ids @ hermes_cli/model_switch_providers.py:_live_or_curated_ids */
json_t *msp_live_or_curated_ids(msp_ctx *ctx, const char *slug, const json_t *curated,
                                const json_t *fallback_keys, bool merge_models_dev, bool non_blocking) {
    json_t *args = json_object();
    msp_arg_str(args, "slug", slug);
    msp_arg_bool(args, "non_blocking", non_blocking);
    json_t *ids = msp_call(ctx, "cached_provider_model_ids", args);
    if (msp_exc_pending(ctx)) { json_free(ids); return NULL; }
    if (!msp_truthy(ids)) {
        json_free(ids);
        json_t *keys = msp_fallback_keys(slug, fallback_keys);
        ids = msp_first_curated(curated, keys);
        json_free(keys);
        const json_t *pref = msp_env_get(ctx, "models_dev_preferred");
        if (merge_models_dev && msp_arr_has_str(pref, slug ? slug : "")) {
            json_t *ma = json_object();
            msp_arg_str(ma, "slug", slug);
            msp_arg_json(ma, "model_ids", ids); /* adopts ids */
            ids = msp_call(ctx, "merge_with_models_dev", ma);
            if (msp_exc_pending(ctx)) { json_free(ids); return NULL; }
        }
    }
    json_t *oa = json_object();
    msp_arg_json(oa, "model_ids", ids); /* adopts ids (dropped when None) */
    json_t *out = msp_call(ctx, "without_generation_models", oa);
    if (msp_exc_pending(ctx)) { json_free(out); return NULL; }
    return out;
}

/* PoP: _aws_live_or_curated_ids @ hermes_cli/model_switch_providers.py:_aws_live_or_curated_ids */
json_t *msp_aws_live_or_curated_ids(msp_ctx *ctx, const char *slug, const json_t *curated,
                                    const json_t *fallback_keys, bool non_blocking) {
    json_t *r = msp_live_or_curated_ids(ctx, slug, curated, fallback_keys, false, non_blocking);
    int t = msp_exc_tri(ctx);
    if (t == 2) { json_free(r); return NULL; }
    if (t == 1) {
        json_free(r);
        json_t *keys = msp_fallback_keys(slug, fallback_keys);
        json_t *fc = msp_first_curated(curated, keys);
        json_free(keys);
        if (msp_truthy(fc)) return fc;
        json_free(fc);
        return json_array();
    }
    if (msp_truthy(r)) return r;
    json_free(r);
    return json_array();
}

/* PoP: _nous_picker_model_ids @ hermes_cli/model_switch_providers.py:_nous_picker_model_ids */
json_t *msp_nous_picker_model_ids(msp_ctx *ctx, const json_t *curated, bool force_fresh_nous_tier) {
    const json_t *nous = msp_is_obj(curated) ? json_obj_get(curated, "nous") : NULL;
    json_t *model_ids = nous ? json_copy(nous) : json_array();
    json_t *pricing = NULL;
    char *portal = NULL;

    /* Block 1: pricing / portal / tier unions (except Exception: pass). */
    bool block1_ok = true;
    json_t *pa = json_object();
    msp_arg_str(pa, "provider", "nous");
    msp_arg_bool(pa, "cached_only", true);
    pricing = msp_call(ctx, "get_pricing_for_provider", pa);
    int t = msp_exc_tri(ctx);
    if (t == 2) { json_free(pricing); json_free(model_ids); return NULL; }
    if (t == 1) { block1_ok = false; json_free(pricing); pricing = NULL; }
    if (block1_ok) {
        if (!msp_truthy(pricing)) { json_free(pricing); pricing = json_object(); }
        /* inner try: portal = (get_provider_auth_state("nous") or {}).get(...) or "" */
        portal = msp_strdup("");
        json_t *aa = json_object();
        msp_arg_str(aa, "provider", "nous");
        json_t *auth = msp_call(ctx, "get_provider_auth_state", aa);
        int tp = msp_exc_tri(ctx);
        if (tp == 2) { json_free(auth); json_free(pricing); free(portal); json_free(model_ids); return NULL; }
        if (tp == 0) {
            const json_t *pb = msp_truthy(auth) ? json_obj_get(auth, "portal_base_url") : NULL;
            if (msp_truthy(pb) && pb->type == JSON_STRING) {
                free(portal);
                portal = msp_strdup(pb->str_val);
            }
        }
        json_free(auth);
        json_t *ca = json_object();
        msp_arg_bool(ca, "force_fresh", force_fresh_nous_tier);
        json_t *tier = msp_call(ctx, "check_nous_free_tier", ca);
        int tt = msp_exc_tri(ctx);
        if (tt == 2) { json_free(tier); json_free(pricing); free(portal); json_free(model_ids); return NULL; }
        if (tt == 1) { block1_ok = false; }
        else {
            bool free_tier = msp_truthy(tier);
            json_free(tier);
            const char *useam = free_tier ? "union_with_portal_free_recommendations"
                                          : "union_with_portal_paid_recommendations";
            json_t *ua = json_object();
            msp_arg_copy(ua, "model_ids", model_ids);
            msp_arg_copy(ua, "pricing", pricing);
            msp_arg_str(ua, "portal", portal);
            json_t *tup = msp_call(ctx, useam, ua);
            int tu = msp_exc_tri(ctx);
            if (tu == 2) { json_free(tup); json_free(pricing); free(portal); json_free(model_ids); return NULL; }
            if (tu == 0) {
                json_t *nid = (msp_is_arr(tup) && tup->c.count > 0) ? json_copy(tup->c.items[0]) : json_array();
                json_free(model_ids);
                model_ids = nid;
            }
            json_free(tup);
            if (tu == 0 && !free_tier) {
                json_t *sa = json_object();
                msp_arg_copy(sa, "model_ids", model_ids);
                msp_arg_copy(sa, "pricing", pricing);
                json_t *sale = msp_call(ctx, "union_with_nous_on_sale_models", sa);
                int ts = msp_exc_tri(ctx);
                if (ts == 2) { json_free(sale); json_free(pricing); free(portal); json_free(model_ids); return NULL; }
                if (ts == 0) { json_free(model_ids); model_ids = sale; }
                else json_free(sale);
            }
        }
    }
    json_free(pricing);
    free(portal);

    /* Block 2: policy restriction (except Exception: debug). */
    json_t *allowed = msp_call(ctx, "nous_policy_allowed_ids", NULL);
    int ta = msp_exc_tri(ctx);
    if (ta == 2) { json_free(allowed); json_free(model_ids); return NULL; }
    if (ta == 0) {
        json_t *ra = json_object();
        msp_arg_copy(ra, "model_ids", model_ids);
        msp_arg_json(ra, "allowed_ids", allowed); /* adopts allowed */
        msp_arg_bool(ra, "rescue_empty", true);
        json_t *restricted = msp_call(ctx, "restrict_to_nous_policy", ra);
        int tr = msp_exc_tri(ctx);
        if (tr == 2) { json_free(restricted); json_free(model_ids); return NULL; }
        if (tr == 0) { json_free(model_ids); model_ids = restricted; }
        else json_free(restricted);
    } else {
        json_free(allowed);
    }

    /* Block 3: usage featured-first (except Exception: pass). */
    json_t *ua = json_object();
    msp_arg_bool(ua, "any_age", true);
    json_t *usage = msp_call(ctx, "nous_picker_usage", ua);
    int tus = msp_exc_tri(ctx);
    if (tus == 2) { json_free(usage); json_free(model_ids); return NULL; }
    if (tus == 0 && !msp_is_none(usage)) {
        const json_t *featured = msp_is_obj(usage) ? json_obj_get(usage, "featured_model_ids") : NULL;
        json_t *fa = json_object();
        msp_arg_copy(fa, "model_ids", model_ids);
        msp_arg_copy(fa, "featured_model_ids", featured);
        json_t *ff = msp_call(ctx, "featured_first", fa);
        int tf = msp_exc_tri(ctx);
        if (tf == 2) { json_free(ff); json_free(usage); json_free(model_ids); return NULL; }
        if (tf == 0) { json_free(model_ids); model_ids = ff; }
        else json_free(ff);
    }
    json_free(usage);
    return model_ids;
}

/* PoP: _free_tier_nous_row @ hermes_cli/model_switch_providers.py:_free_tier_nous_row */
json_t *msp_free_tier_nous_row(msp_ctx *ctx, const json_t *row) {
    json_t *g = msp_call(ctx, "has_guest", NULL);
    if (msp_exc_pending(ctx)) { json_free(g); return NULL; }
    bool guest = msp_truthy(g);
    json_free(g);
    if (!guest) return json_copy(row);
    json_t *ga = msp_call(ctx, "guest_allowed", NULL);
    if (msp_exc_pending(ctx)) { json_free(ga); return NULL; }
    bool allowed = msp_truthy(ga);
    json_free(ga);
    if (!allowed) return NULL; /* None */
    json_t *fi = msp_call(ctx, "free_inference_active", NULL);
    if (msp_exc_pending(ctx)) { json_free(fi); return NULL; }
    bool active = msp_truthy(fi);
    json_free(fi);
    if (!active) return json_copy(row);
    json_t *out = json_copy(row);
    json_set(out, "name", json_string(MSP_FREE_TIER_LABEL));
    json_t *models = json_array();
    json_append(models, json_string(MSP_GUEST_MODEL));
    json_set(out, "models", models);
    json_set(out, "total_models", json_number(1));
    json_set(out, "free_tier_row", json_bool(true));
    return out;
}

/* PoP: _cap_models @ hermes_cli/model_switch_providers.py:_cap_models */
json_t *msp_cap_models(msp_ctx *ctx, const json_t *model_ids, const json_t *max_models, const char *slug) {
    (void)ctx;
    if (msp_is_none(max_models)) return json_copy(model_ids);
    if (msp_truthy(max_models)) {
        for (size_t i = 0; i < MSP_UNCAPPED_N; i++)
            if (msp_str_eq(MSP_UNCAPPED[i], slug ? slug : "")) return json_copy(model_ids);
    }
    size_t n = 0;
    if (max_models->type == JSON_NUMBER) {
        double d = max_models->num_val;
        n = d > 0 ? (size_t)d : 0;
    }
    json_t *out = json_array();
    if (msp_is_arr(model_ids)) {
        size_t count = model_ids->c.count;
        if (n > count) n = count;
        for (size_t i = 0; i < n; i++) json_append(out, json_copy(model_ids->c.items[i]));
    }
    return out;
}

/* _NativePickerModelList script marker: {"__native__": [...]} -> native list.
 * Returns the items array (borrowed) and sets *native. */
static const json_t *msp_native_items(const json_t *v, bool *native) {
    *native = false;
    if (msp_is_obj(v)) {
        const json_t *items = json_obj_get(v, "__native__");
        if (msp_is_arr(items)) { *native = true; return items; }
    }
    return v;
}

static json_t *msp_tuple2(json_t *models, bool native_empty) {
    json_t *t = json_array();
    json_append(t, models ? models : json_null());
    json_append(t, json_bool(native_empty));
    return t;
}

/* PoP: _discover_endpoint_models @ hermes_cli/model_switch_providers.py:_discover_endpoint_models */
json_t *msp_discover_endpoint_models(msp_ctx *ctx, const json_t *api_key, const char *api_url,
                                     const char *native_catalog_provider, bool has_explicit_models,
                                     const json_t *headers, const json_t *api_mode, bool probe_live,
                                     bool discovery_allowed, bool fast_custom_probe) {
    double timeout = fast_custom_probe ? 1.5 : 5.0;
    if (probe_live) {
        json_t *a = json_object();
        msp_arg_copy(a, "api_key", api_key);
        msp_arg_str(a, "api_url", api_url);
        msp_arg_str(a, "native_catalog_provider", native_catalog_provider);
        msp_arg_bool(a, "preserve_native_models", has_explicit_models);
        msp_arg_copy(a, "headers", headers);
        json_set(a, "timeout", json_number(timeout));
        msp_arg_copy(a, "api_mode", api_mode);
        json_t *live = msp_call(ctx, "fetch_picker_live_models", a);
        int t = msp_exc_tri(ctx);
        if (t == 2) { json_free(live); return NULL; }
        if (t == 0) {
            bool native = false;
            const json_t *items = msp_native_items(live, &native);
            if (native && has_explicit_models) { json_free(live); return msp_tuple2(NULL, false); }
            if (!msp_is_none(live) && (msp_truthy(items) || !has_explicit_models || native)) {
                json_t *models = native ? json_copy(items) : live; /* transfer ownership */
                bool ne = native && !msp_truthy(items);
                if (native) json_free(live);
                return msp_tuple2(models, ne);
            }
        }
        json_free(live);
    } else if (discovery_allowed) {
        static const char *const catch_cls[] = {
            "ImportError", "OSError", "RuntimeError", "TimeoutError",
            "TypeError", "ValueError", "HTTPException",
        };
        json_t *a = json_object();
        msp_arg_copy(a, "api_key", api_key);
        msp_arg_str(a, "api_url", api_url);
        msp_arg_bool(a, "cache_only", true);
        json_set(a, "timeout", json_number(timeout));
        msp_arg_copy(a, "headers", headers);
        msp_arg_copy(a, "api_mode", api_mode);
        json_t *cached = msp_call(ctx, "cached_fetch_api_models", a);
        if (msp_exc_pending(ctx)) {
            if (msp_exc_matches(ctx, catch_cls, sizeof(catch_cls) / sizeof(catch_cls[0]))) {
                msp_exc_clear(ctx);
                json_free(cached);
            } else {
                json_free(cached);
                return NULL; /* e.g. KeyError / SystemExit propagates */
            }
        } else {
            bool native = false;
            const json_t *items = msp_native_items(cached, &native);
            if (has_explicit_models && native) { json_free(cached); return msp_tuple2(NULL, false); }
            if (msp_truthy(items) || native) {
                json_t *models = native ? json_copy(items) : cached;
                bool ne = native && !msp_truthy(items);
                if (native) json_free(cached);
                return msp_tuple2(models, ne);
            }
            json_free(cached);
        }
    }
    return msp_tuple2(NULL, false);
}

/* PoP: _finalize_picker_rows @ hermes_cli/model_switch_providers.py:_finalize_picker_rows */
json_t *msp_finalize_picker_rows(msp_ctx *ctx, json_t *results, const json_t *user_providers,
                                 const char *current_model) {
    size_t count = msp_is_arr(results) ? results->c.count : 0;
    size_t *kept = (size_t *)malloc((count ? count : 1) * sizeof(size_t));
    size_t nkept = 0;
    for (size_t i = 0; i < count; i++) kept[nkept++] = i;
    /* live rebinds `results` to a filtered list ONLY when disabled is non-empty;
     * otherwise the final sort mutates the ORIGINAL argument list in place. */
    bool rebound = false;

    /* Step 1 (try/except Exception): enabled post-filter. A caught raise aborts
     * the filter (no rebind) but injection + sort still run; only a non-Exception
     * (e.g. SystemExit) propagates. */
    if (msp_is_obj(user_providers)) {
        char **disabled = (char **)malloc((user_providers->c.count ? user_providers->c.count : 1) * sizeof(char *));
        size_t ndis = 0;
        bool aborted = false;
        for (size_t i = 0; i < user_providers->c.count; i++) {
            const json_t *cfg = user_providers->c.items[i];
            if (!msp_is_obj(cfg)) continue;
            json_t *a = json_object();
            msp_arg_copy(a, "cfg", cfg);
            json_t *r = msp_call(ctx, "is_provider_enabled", a);
            int t = msp_exc_tri(ctx);
            if (t == 2) {
                json_free(r);
                for (size_t d = 0; d < ndis; d++) free(disabled[d]);
                free(disabled);
                free(kept);
                return NULL;
            }
            if (t == 1) { json_free(r); aborted = true; break; }
            bool enabled = msp_truthy(r);
            json_free(r);
            if (!enabled) disabled[ndis++] = msp_strip_lower(user_providers->c.keys[i]);
        }
        if (!aborted && ndis > 0) {
            size_t w = 0;
            for (size_t i = 0; i < nkept; i++) {
                const json_t *row = results->c.items[kept[i]];
                char *pid = msp_strip_lower(msp_jstr(json_obj_get(row, "provider_id")));
                char *slug = msp_strip_lower(msp_jstr(json_obj_get(row, "slug")));
                bool drop = false;
                for (size_t d = 0; d < ndis; d++)
                    if (msp_str_eq(pid, disabled[d]) || msp_str_eq(slug, disabled[d])) { drop = true; break; }
                free(pid);
                free(slug);
                if (!drop) kept[w++] = kept[i];
            }
            nkept = w;
            rebound = true;
        }
        for (size_t d = 0; d < ndis; d++) free(disabled[d]);
        free(disabled);
    }

    /* Step 2: inject the current model into the first eligible current row. */
    if (current_model && current_model[0] != '\0') {
        for (size_t i = 0; i < nkept; i++) {
            json_t *row = results->c.items[kept[i]];
            if (!msp_truthy(json_obj_get(row, "is_current"))
                || msp_truthy(json_obj_get(row, "native_catalog_empty")))
                continue;
            const json_t *models = json_obj_get(row, "models");
            if (!msp_truthy(models)) models = NULL; /* row.get("models") or [] */
            if (!msp_arr_has_str(models, current_model)) {
                json_t *a = json_object();
                msp_arg_copy(a, "slug", json_obj_get(row, "slug"));
                msp_arg_str(a, "model", current_model);
                json_t *r = msp_call(ctx, "model_requires_account_discovery", a);
                if (msp_exc_pending(ctx)) { json_free(r); free(kept); return NULL; }
                bool needs = msp_truthy(r);
                json_free(r);
                if (needs) break;
                json_t *nm = json_array();
                json_append(nm, json_string(current_model));
                if (msp_is_arr(models))
                    for (size_t m = 0; m < models->c.count; m++)
                        json_append(nm, json_copy(models->c.items[m]));
                size_t base = msp_is_arr(models) ? models->c.count : 0;
                const json_t *total = json_obj_get(row, "total_models");
                if (total && total->type == JSON_NUMBER) base = (size_t)total->num_val;
                json_set(row, "models", nm);
                json_set(row, "total_models", json_number((double)(base + 1)));
            }
            break;
        }
    }

    /* Step 3: stable sort by (not is_current, -total_models). */
    for (size_t i = 1; i < nkept; i++) {
        size_t key = kept[i];
        const json_t *rk = results->c.items[key];
        bool cur_k = msp_truthy(json_obj_get(rk, "is_current"));
        double tot_k = json_get_num(rk, "total_models", 0);
        size_t j = i;
        while (j > 0) {
            const json_t *rj = results->c.items[kept[j - 1]];
            bool cur_j = msp_truthy(json_obj_get(rj, "is_current"));
            double tot_j = json_get_num(rj, "total_models", 0);
            bool after = (!cur_j && cur_k) || (cur_j == cur_k && tot_j < tot_k);
            if (!after) break;
            kept[j] = kept[j - 1];
            j--;
        }
        kept[j] = key;
    }

    /* Step 4: build the returned list. Without a rebind the original argument
     * list is sorted in place (shared row objects); with a rebind it is left
     * untouched and the return is a fresh filtered+sorted list. */
    json_t *out = json_array();
    if (!rebound && count > 0) {
        json_t **ni = (json_t **)malloc(count * sizeof(json_t *));
        for (size_t i = 0; i < count; i++) ni[i] = results->c.items[kept[i]];
        memcpy(results->c.items, ni, count * sizeof(json_t *));
        free(ni);
        for (size_t i = 0; i < count; i++) json_append(out, json_copy(results->c.items[i]));
    } else {
        for (size_t i = 0; i < nkept; i++) json_append(out, json_copy(results->c.items[kept[i]]));
    }
    free(kept);
    return out;
}

/* PoP: _saved_entry_on_other_origin @ hermes_cli/model_switch_providers.py:_saved_entry_on_other_origin */
json_t *msp_saved_entry_on_other_origin(msp_ctx *ctx, const char *name, const char *base_url) {
    json_t *cfg = msp_call(ctx, "load_config", NULL);
    if (msp_exc_pending(ctx)) { json_free(cfg); return NULL; } /* no try: propagates */
    const json_t *providers = msp_is_obj(cfg) ? json_obj_get(cfg, "providers") : NULL;
    const json_t *saved = msp_truthy(providers) ? json_obj_get(providers, name ? name : "") : NULL;
    if (!msp_is_obj(saved)) { json_free(cfg); return json_bool(false); }
    for (size_t i = 0; i < MSP_ENDPOINT_ADDRESS_FIELDS_N; i++) {
        char *url = msp_str_or_empty(json_obj_get(saved, MSP_ENDPOINT_ADDRESS_FIELDS[i]));
        char *url_s = msp_strip(url);
        free(url);
        if (url_s && url_s[0] != '\0') {
            json_t *a = json_object();
            msp_arg_str(a, "url", url_s);
            msp_arg_str(a, "base_url", base_url);
            json_t *r = msp_call(ctx, "same_base_url_origin", a);
            if (msp_exc_pending(ctx)) { json_free(r); free(url_s); json_free(cfg); return NULL; }
            bool same = msp_truthy(r);
            json_free(r);
            if (!same) { free(url_s); json_free(cfg); return json_bool(true); }
        }
        free(url_s);
    }
    json_free(cfg);
    return json_bool(false);
}