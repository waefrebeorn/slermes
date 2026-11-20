/* models_dev_flow2.c — FLOW port of the UNCLAIMED remainder of
 * agent/models_dev.py (the 21 defs that carry NO PoP claim in
 * pop_claims_full.json / the src/agent models_dev* ports; those claimed
 * siblings are SEAMED here).
 *
 * Ported targets (static md2_*): _profile_models_dev_ids,
 * models_dev_provider_map, models_dev_id_for, _dict_or_empty,
 * _configured_catalog_provider, _models_dev_id, _cfg_get, _hermes_path,
 * _quietly, _serve_stale, _refresh_locked, _registry_provider,
 * _registry_models, _openrouter_catalog_lookup_base, _iter_model_entries,
 * _extract_limit, _builtin_model_metadata, _relay_vision_marker_metadata,
 * _provider_model_capabilities, _apply_overrides, _entry_supports_vision.
 *
 * Representation contract (shared with tests/oracle_models_dev_flow2.py):
 *   - tuple/set -> array (sets sorted by code point); None -> JSON null;
 *     integral floats collapse to ints on both sides; PurePosixPath -> str.
 *   - uncaught exception -> {"__raise__": cls}; an exception passed as a
 *     logger arg projects to {"__raise__": cls, "msg": str(e)}.
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion
 *     raises RuntimeError("script exhausted: <name>")); {"__raise__":cls,
 *     "msg":m} raises (trace ret is the WHOLE marker); any other value reused.
 *     None-valued seam arg keys dropped on BOTH sides ("" kept).
 *   - except Exception uses the FULL builtin hierarchy; the
 *     SystemExit/KeyboardInterrupt/GeneratorExit trio stays BaseException-only
 *     and propagates.  Custom _NotModified derives from Exception (so an
 *     `except Exception` still catches it, but the first `except _NotModified`
 *     wins).  Scripted raises are matched by class identity through the
 *     parent table.
 *   - in.env carries the module constants (provider_map, builtin_metadata,
 *     unknown_model_base, openrouter_providers, models_dev_url, cache,
 *     warned).  Module-global mutable state is single-call here (the harness
 *     drives one target per case with a FRESH warned set), so no state echo.
 *   - fixture domain: str()/strip() operands are ASCII; Path fixtures are
 *     simple absolute POSIX paths (no '.' components, no trailing slash);
 *     numeric coercion values are in int64 range; provider objects carry
 *     "name"/"models_dev_id"; profile objects carry "model_capabilities";
 *     builtin_metadata is keyed "provider|model" (lower-cased model);
 *     float()/int() tokens are plain ASCII.  These exclusions are deliberate
 *     (documented in the generator + oracle), not silent divergences.
 *
 * libjson-only.  Self-contained: the harness #includes "agent/models_dev_flow2.c".
 * The md2_ prefix is taken by cli/models_flow2.c, so this port uses md2_* in its
 * own translation unit (the harness never links both).
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct md2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted constants/flags */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} md2_ctx;

static bool md2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool md2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool md2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool md2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool md2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool md2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool md2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool md2_truthy(const json_t *v) {
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
static const char *md2_jstr(const json_t *v) {
    return (md2_is_str(v) && v->str_val) ? v->str_val : "";
}
static double md2_num(const json_t *v) { return md2_is_num(v) ? v->num_val : 0.0; }

static json_t *md2_get(const json_t *obj, const char *key) {
    return md2_is_obj(obj) ? json_obj_get(obj, key) : NULL;
}
static json_t *md2_get_def(const json_t *obj, const char *key, json_t *def) {
    if (md2_is_obj(obj)) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}
static json_t *md2_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}
static json_t *md2_obj_copy(const json_t *v) {
    json_t *out = json_object();
    if (md2_is_obj(v))
        for (size_t i = 0; i < v->c.count; i++)
            json_set(out, v->c.keys[i], json_copy(v->c.items[i]));
    return out;
}
static json_t *md2_arr_copy(const json_t *v) {
    json_t *out = json_array();
    if (md2_is_arr(v))
        for (size_t i = 0; i < v->c.count; i++)
            json_append(out, json_copy(v->c.items[i]));
    return out;
}
static bool md2_arr_has_str(const json_t *arr, const char *s) {
    if (!md2_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (md2_is_str(it) && md2_str_eq(it->str_val, s)) return true;
    }
    return false;
}
/* does any VALUE of the object equal the string s? */
static bool md2_obj_has_str(const json_t *obj, const char *s) {
    if (!md2_is_obj(obj)) return false;
    for (size_t i = 0; i < obj->c.count; i++)
        if (md2_is_str(obj->c.items[i]) && md2_str_eq(obj->c.items[i]->str_val, s)) return true;
    return false;
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static bool md2_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *md2_strip(const char *s) {
    if (!s) return strdup("");
    const char *b = s;
    while (*b && md2_is_space(*b)) b++;
    const char *e = b + strlen(b);
    while (e > b && md2_is_space(e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, b, n); r[n] = '\0';
    return r;
}
static char *md2_lower(const char *s) {
    char *r = strdup(s ? s : "");
    if (!r) return NULL;
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}
static bool md2_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* py str(v) for scalars (null -> "None"). Caller frees. */
static char *md2_pystr(const json_t *v) {
    if (md2_is_none(v)) return strdup("None");
    if (md2_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (md2_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    if (md2_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

/* PurePosixPath join (verbatim component, simple-path domain). Caller frees. */
static char *md2_path_join(const char *base, const char *name) {
    size_t nb = strlen(base ? base : ""), nn = strlen(name ? name : "");
    bool slash = nb > 0 && base[nb - 1] == '/';
    char *r = (char *)malloc(nb + nn + 2);
    if (!r) return NULL;
    memcpy(r, base ? base : "", nb);
    if (!slash) r[nb++] = '/';
    memcpy(r + nb, name ? name : "", nn);
    r[nb + nn] = '\0';
    return r;
}

/* int(value) when isinstance(value,(int,float)) and value>0; else false.
 * JSON bool is admitted (isinstance(True,int)); strings/null are not. */
static bool md2_pos_int(const json_t *v, long long *out) {
    if (!v) return false;
    if (v->type == JSON_BOOL) {
        if (!v->bool_val) return false;
        *out = 1; return true;
    }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!(d > 0.0)) return false;
        *out = (long long)d;
        return true;
    }
    return false;
}

/* ── exception plumbing (FULL builtin hierarchy) ────────────────────────── */

static bool md2_exc_pending(const md2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void md2_exc_clear(md2_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void md2_raise(md2_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *md2_exc_class(const md2_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
/* Full builtin parent table.  BaseException trio maps to BaseException ONLY,
 * never Exception.  _NotModified is a custom Exception subclass. */
static const char *md2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        /* BaseException family */
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"Exception", "BaseException"},
        /* Exception direct/near children */
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"ValueError", "Exception"}, {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        {"JSONDecodeError", "ValueError"},
        {"TypeError", "Exception"}, {"AttributeError", "Exception"},
        {"NameError", "Exception"}, {"UnboundLocalError", "NameError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        {"ArithmeticError", "Exception"}, {"ZeroDivisionError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"FloatingPointError", "ArithmeticError"},
        {"OSError", "Exception"}, {"FileNotFoundError", "OSError"},
        {"PermissionError", "OSError"}, {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"}, {"FileExistsError", "OSError"},
        {"InterruptedError", "OSError"}, {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"}, {"ProcessLookupError", "OSError"},
        {"ConnectionError", "OSError"}, {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"},
        {"ConnectionResetError", "ConnectionError"}, {"TimeoutError", "OSError"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"AssertionError", "Exception"}, {"SyntaxError", "Exception"},
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"SystemError", "Exception"}, {"ReferenceError", "Exception"},
        {"MemoryError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ImportError", "Exception"},
        {"ModuleNotFoundError", "ImportError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"FutureWarning", "Warning"},
        {"RuntimeWarning", "Warning"}, {"SyntaxWarning", "Warning"},
        {"ResourceWarning", "Warning"},
        /* custom: agent/models_dev.py::_NotModified */
        {"_NotModified", "Exception"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (md2_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool md2_exc_matches(const md2_ctx *ctx, const char *const *classes, size_t n) {
    if (!md2_exc_pending(ctx)) return false;
    const char *cls = md2_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (md2_str_eq(cls, classes[i])) return true;
        const char *p = md2_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
static bool md2_exc_is_exception(const md2_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return md2_exc_matches(ctx, E, 1);
}
static bool md2_exc_is_not_modified(const md2_ctx *ctx) {
    static const char *E[] = {"_NotModified"};
    return md2_exc_matches(ctx, E, 1);
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *md2_script_result(md2_ctx *ctx, const json_t *script) {
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

/* args ownership is TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *md2_call(md2_ctx *ctx, const char *name, json_t *args) {
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
                snprintf(msg, sizeof msg, "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = md2_script_result(ctx, chosen);
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

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). */
static void md2_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void md2_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void md2_arg_bool(json_t *args, const char *key, bool val) {
    json_set(args, key, json_bool(val));
}
/* logger seam: {"msg","args"}. args_arr ownership transferred. */
static void md2_log(md2_ctx *ctx, const char *level, const char *msg, json_t *args_arr) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = md2_call(ctx, name, a);
    json_free(r);
}
static json_t *md2_exc_arg(const md2_ctx *ctx) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(md2_exc_class(ctx)));
    json_set(e, "msg", json_string(json_get_str(ctx->exc, "msg", "")));
    return e;
}

/* pair result (tuple -> array) */
static json_t *md2_pair(json_t *a, json_t *b) {
    json_t *r = json_array();
    json_append(r, a ? a : json_null());
    json_append(r, b ? b : json_null());
    return r;
}

/* ── env table accessors ─────────────────────────────────────────────────── */

static json_t *md2_env_obj(const md2_ctx *ctx, const char *key) {
    json_t *v = ctx->env ? json_obj_get(ctx->env, key) : NULL;
    return md2_is_obj(v) ? v : NULL;
}
static json_t *md2_env_arr(const md2_ctx *ctx, const char *key) {
    json_t *v = ctx->env ? json_obj_get(ctx->env, key) : NULL;
    return md2_is_arr(v) ? v : NULL;
}
static const char *md2_env_str(const md2_ctx *ctx, const char *key, const char *def) {
    return ctx->env ? json_get_str(ctx->env, key, def) : def;
}

/* ── forward decls for internal composition ─────────────────────────────── */

static json_t *md2_models_dev_id_for(md2_ctx *ctx, json_t *in);
static json_t *md2_cfg_get(md2_ctx *ctx, json_t *in);
static json_t *md2_profile_models_dev_ids(md2_ctx *ctx, json_t *in);
static json_t *md2_models_dev_provider_map(md2_ctx *ctx, json_t *in);
static json_t *md2_configured_catalog_provider(md2_ctx *ctx, json_t *in);
static json_t *md2_models_dev_id(md2_ctx *ctx, json_t *in);
static json_t *md2_hermes_path(md2_ctx *ctx, json_t *in);
static json_t *md2_quietly(md2_ctx *ctx, json_t *in);
static json_t *md2_openrouter_catalog_lookup_base(md2_ctx *ctx, json_t *in);
static json_t *md2_registry_provider(md2_ctx *ctx, json_t *in);
static json_t *md2_builtin_model_metadata(md2_ctx *ctx, json_t *in);
static json_t *md2_relay_vision_marker_metadata(md2_ctx *ctx, json_t *in);
static json_t *md2_provider_model_capabilities(md2_ctx *ctx, json_t *in);

/* ── ported functions ────────────────────────────────────────────────────── */

/* PoP: md2_profile_models_dev_ids @ agent/models_dev.py:_profile_models_dev_ids */
static json_t *md2_profile_models_dev_ids(md2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *provs = md2_call(ctx, "list_providers", json_object());
    if (md2_exc_pending(ctx)) { json_free(provs); return NULL; }
    json_t *out = json_object();
    json_t *pmap = md2_env_obj(ctx, "provider_map");
    if (md2_is_arr(provs)) {
        for (size_t i = 0; i < provs->c.count; i++) {
            json_t *p = provs->c.items[i];
            const char *name = json_get_str(p, "name", "");
            json_t *mdev = md2_get(p, "models_dev_id");
            if (md2_truthy(mdev) && !(pmap && json_obj_get(pmap, name)))
                json_set(out, name, json_copy(mdev));
        }
    }
    json_free(provs);
    return out;
}

/* PoP: md2_models_dev_provider_map @ agent/models_dev.py:models_dev_provider_map */
static json_t *md2_models_dev_provider_map(md2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *out = md2_obj_copy(md2_env_obj(ctx, "provider_map"));
    json_t *extra = md2_profile_models_dev_ids(ctx, NULL);  /* internal composition (both sides) */
    if (md2_exc_pending(ctx)) { json_free(extra); json_free(out); return NULL; }
    if (md2_is_obj(extra))
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(out, extra->c.keys[i], json_copy(extra->c.items[i]));
    json_free(extra);
    return out;
}

/* PoP: md2_models_dev_id_for @ agent/models_dev.py:models_dev_id_for */
static json_t *md2_models_dev_id_for(md2_ctx *ctx, json_t *in) {
    char *key = md2_strip(json_get_str(in, "provider", ""));
    json_t *pmap = md2_env_obj(ctx, "provider_map");
    bool inmap = pmap && json_obj_get(pmap, key) != NULL;
    if (!key[0] || inmap) {
        json_t *ret = md2_copy_or_null(inmap ? json_obj_get(pmap, key) : NULL);
        free(key);
        return ret;
    }
    json_t *prof = md2_profile_models_dev_ids(ctx, NULL);  /* internal composition */
    if (md2_exc_pending(ctx)) { free(key); json_free(prof); return NULL; }
    json_t *r = md2_copy_or_null(md2_get(prof, key));
    free(key);
    json_free(prof);
    return r;
}

/* PoP: md2_dict_or_empty @ agent/models_dev.py:_dict_or_empty */
static json_t *md2_dict_or_empty(md2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *v = md2_get(in, "value");
    return md2_is_obj(v) ? json_copy(v) : json_object();
}

/* PoP: md2_cfg_get @ agent/models_dev.py:_cfg_get */
static json_t *md2_cfg_get(md2_ctx *ctx, json_t *in) {
    json_t *keys = md2_get(in, "keys");
    json_t *def = md2_get(in, "default");
    json_t *config = md2_get(in, "config");
    json_t *cfg = NULL;
    if (config && config->type != JSON_NULL) {
        cfg = json_copy(config);
    } else {
        cfg = md2_call(ctx, "load_config_readonly", json_object());
        if (md2_exc_pending(ctx)) {
            json_free(cfg);
            if (!md2_exc_is_exception(ctx)) return NULL;  /* BaseException propagates */
            md2_exc_clear(ctx);            /* except Exception: return default */
            return md2_copy_or_null(def);
        }
    }
    json_t *a = json_object();
    md2_arg(a, "cfg", cfg);
    if (md2_is_arr(keys)) json_set(a, "keys", json_copy(keys));
    md2_arg(a, "default", def);
    json_free(cfg);
    json_t *ret = md2_call(ctx, "cfg_get", a);
    if (md2_exc_pending(ctx)) {
        json_free(ret);
        if (!md2_exc_is_exception(ctx)) return NULL;  /* BaseException propagates */
        md2_exc_clear(ctx);
        return md2_copy_or_null(def);
    }
    return ret;
}

/* PoP: md2_configured_catalog_provider @ agent/models_dev.py:_configured_catalog_provider */
static json_t *md2_configured_catalog_provider(md2_ctx *ctx, json_t *in) {
    char *name = md2_strip(json_get_str(in, "provider", ""));
    if (md2_starts_with(name, "custom:")) {
        char *n2 = strdup(name + 7);
        free(name);
        name = n2;
    }
    json_t *config = md2_get(in, "config");
    if (!name[0]) { free(name); return json_null(); }
    /* models_dev_id_for(name) is not None -> return None */
    json_t *tmp = json_object();
    json_set(tmp, "provider", json_string(name));
    json_t *mid = md2_models_dev_id_for(ctx, tmp);
    json_free(tmp);
    if (md2_exc_pending(ctx)) { free(name); json_free(mid); return NULL; }
    if (!md2_is_none(mid)) { free(name); json_free(mid); return json_null(); }
    json_free(mid);

    /* provider_config = _cfg_get("providers", name, default=None, config=config) */
    json_t *ci = json_object();
    json_t *karr = json_array();
    json_append(karr, json_string("providers"));
    json_append(karr, json_string(name));
    json_set(ci, "keys", karr);
    if (config && config->type != JSON_NULL) json_set(ci, "config", json_copy(config));
    json_t *pcfg = md2_cfg_get(ctx, ci);
    json_free(ci);
    if (md2_exc_pending(ctx)) { free(name); json_free(pcfg); return NULL; }
    json_t *alias = md2_get(pcfg, "catalog_provider");
    json_t *alias_copy = alias ? json_copy(alias) : NULL;
    json_free(pcfg);
    if (!md2_truthy(alias_copy)) {
        json_free(alias_copy);
        json_t *li = json_object();
        json_t *lk = json_array();
        json_append(lk, json_string("custom_providers"));
        json_set(li, "keys", lk);
        if (config && config->type != JSON_NULL) json_set(li, "config", json_copy(config));
        json_t *legacy = md2_cfg_get(ctx, li);
        json_free(li);
        if (md2_exc_pending(ctx)) { free(name); json_free(legacy); return NULL; }
        alias_copy = NULL;
        if (md2_is_arr(legacy)) {
            for (size_t i = 0; i < legacy->c.count; i++) {
                json_t *e = legacy->c.items[i];
                if (!md2_is_obj(e)) continue;
                char *en = md2_strip(json_get_str(e, "name", ""));
                if (md2_str_eq(en, name)) {
                    json_t *a = md2_get(e, "catalog_provider");
                    if (a) { json_free(alias_copy); alias_copy = json_copy(a); }
                    free(en);
                    break;
                }
                free(en);
            }
        }
        json_free(legacy);
    }
    free(name);
    char *astr = md2_strip(md2_jstr(alias_copy));
    json_free(alias_copy);
    json_t *ret = json_string(astr);
    free(astr);
    /* str(alias or "").strip(); return alias or None -> null when empty */
    if (!md2_truthy(ret)) { json_free(ret); ret = json_null(); }
    return ret;
}

/* PoP: md2_models_dev_id @ agent/models_dev.py:_models_dev_id */
static json_t *md2_models_dev_id(md2_ctx *ctx, json_t *in) {
    char *key = md2_strip(json_get_str(in, "provider", ""));
    json_t *config = md2_get(in, "config");
    json_t *ti = json_object();
    json_set(ti, "provider", json_string(key));
    json_t *mdev = md2_models_dev_id_for(ctx, ti);
    json_free(ti);
    if (md2_exc_pending(ctx)) { free(key); json_free(mdev); return NULL; }
    if (!md2_is_none(mdev)) { free(key); return mdev; }
    json_free(mdev);

    json_t *ai = json_object();
    json_set(ai, "provider", json_string(key));
    if (config && config->type != JSON_NULL) json_set(ai, "config", json_copy(config));
    json_t *alias_v = md2_configured_catalog_provider(ctx, ai);
    json_free(ai);
    if (md2_exc_pending(ctx)) { free(key); json_free(alias_v); return NULL; }
    char *alias = md2_strip(md2_jstr(alias_v));
    json_free(alias_v);
    if (!alias[0]) { free(key); free(alias); return json_null(); }

    ti = json_object();
    json_set(ti, "provider", json_string(alias));
    json_t *am = md2_models_dev_id_for(ctx, ti);
    json_free(ti);
    if (md2_exc_pending(ctx)) { free(key); free(alias); json_free(am); return NULL; }
    json_t *mdev2;
    if (!md2_is_none(am)) {
        mdev2 = am;
    } else {
        json_free(am);
        mdev2 = json_string(alias);
    }

    /* validate: `mdev_id not in models_dev_provider_map().values() and
       mdev_id not in fetch_models_dev(allow_network=False)` — the AND
       SHORT-CIRCUITS: fetch fires ONLY when the provider_map value misses. */
    json_t *pmap = md2_models_dev_provider_map(ctx, NULL);
    if (md2_exc_pending(ctx)) { free(key); free(alias); json_free(mdev2); json_free(pmap); return NULL; }
    const char *mdev_s = md2_jstr(mdev2);
    bool known = md2_obj_has_str(pmap, mdev_s);
    json_t *reg = NULL;
    if (!known) {
        json_t *fa = json_object();
        json_set(fa, "allow_network", json_bool(false));
        reg = md2_call(ctx, "fetch_models_dev", fa);
        if (md2_exc_pending(ctx)) { free(key); free(alias); json_free(mdev2); json_free(pmap); json_free(reg); return NULL; }
        known = (md2_is_obj(reg) && json_obj_get(reg, mdev_s) != NULL);
    }
    if (!known) {
        /* warn once (per-case fresh warned set) */
        json_t *warned = md2_env_arr(ctx, "warned");
        bool seen = false;
        if (md2_is_arr(warned))
            for (size_t i = 0; i < warned->c.count; i++) {
                json_t *pair = warned->c.items[i];
                if (md2_is_arr(pair) && pair->c.count >= 2 &&
                    md2_str_eq(md2_jstr(pair->c.items[0]), key) &&
                    md2_str_eq(md2_jstr(pair->c.items[1]), alias)) { seen = true; break; }
            }
        if (!seen) {
            /* the oracle records the FORMAT string + args; C emits those verbatim */
            json_t *args = json_array();
            json_append(args, json_string(key));
            json_append(args, json_string(alias));
            md2_log(ctx, "warning",
                    "providers.%s: catalog_provider %r is neither a Hermes provider id nor a models.dev id; ignoring",
                    args);
            if (md2_exc_pending(ctx)) { free(key); free(alias); json_free(mdev2); json_free(pmap); json_free(reg); return NULL; }
        }
        json_free(mdev2);
        mdev2 = json_null();
    }
    free(key); free(alias);
    json_free(pmap); json_free(reg);
    return mdev2;
}

/* PoP: md2_hermes_path @ agent/models_dev.py:_hermes_path */
static json_t *md2_hermes_path(md2_ctx *ctx, json_t *in) {
    const char *name = json_get_str(in, "name", "");
    json_t *home = md2_call(ctx, "get_hermes_home", json_object());
    if (md2_exc_pending(ctx)) { json_free(home); return NULL; }
    char *joined = (!name[0]) ? strdup(md2_jstr(home))
                              : md2_path_join(md2_jstr(home), name);
    json_free(home);
    json_t *ret = json_string(joined ? joined : "");
    free(joined);
    return ret;
}

/* PoP: md2_quietly @ agent/models_dev.py:_quietly */
static json_t *md2_quietly(md2_ctx *ctx, json_t *in) {
    const char *what = json_get_str(in, "what", "");
    json_t *def = md2_get(in, "default");
    json_t *a = json_object();
    md2_arg_str(a, "what", what);
    json_t *ret = md2_call(ctx, "quietly_fn", a);
    if (md2_exc_pending(ctx)) {
        json_free(ret);
        if (!md2_exc_is_exception(ctx)) return NULL;   /* BaseException propagates */
        json_t *excarg = md2_exc_arg(ctx);
        md2_exc_clear(ctx);                            /* handler body runs with no pending exc */
        json_t *args = json_array();
        json_append(args, json_string(what));
        json_append(args, excarg);
        md2_log(ctx, "debug", "Failed to %s: %s", args);
        if (md2_exc_pending(ctx)) return NULL;
        return md2_copy_or_null(def);
    }
    return ret;
}

/* PoP: md2_serve_stale @ agent/models_dev.py:_serve_stale */
static json_t *md2_serve_stale(md2_ctx *ctx, json_t *in) {
    json_t *r1 = md2_call(ctx, "_mark_stale_cache_grace", json_object());
    if (md2_exc_pending(ctx)) { json_free(r1); return NULL; }
    json_free(r1);
    json_t *r2 = md2_call(ctx, "_start_background_refresh_models_dev", json_object());
    if (md2_exc_pending(ctx)) { json_free(r2); return NULL; }
    json_free(r2);
    const char *msg = json_get_str(in, "msg", "");
    json_t *pargs = md2_get(in, "args");
    json_t *args = md2_is_arr(pargs) ? md2_arr_copy(pargs) : json_array();
    md2_log(ctx, "debug", msg, args);
    if (md2_exc_pending(ctx)) return NULL;
    json_t *cache = md2_env_obj(ctx, "cache");
    return cache ? json_copy(cache) : json_object();  /* absent -> {} (oracle: env.get(...) or {}) */
}

/* shared exception tail for _refresh_locked: mirrors the live except ladder. */
static json_t *md2_refresh_exc_tail(md2_ctx *ctx, const char *where) {
    if (md2_exc_is_not_modified(ctx)) {
        md2_exc_clear(ctx);
        json_t *ca = json_object();
        json_set(ca, "where", json_string(where));
        json_t *r = md2_call(ctx, "_confirm_cache_not_modified", ca);
        if (md2_exc_pending(ctx)) { json_free(r); return NULL; }
        json_free(r);
        json_t *cache = md2_env_obj(ctx, "cache");
        return cache ? json_copy(cache) : json_object();
    }
    if (md2_exc_is_exception(ctx)) {
        json_t *e = md2_exc_arg(ctx);
        md2_exc_clear(ctx);
        json_t *na = json_object();
        json_set(na, "exc", e);
        json_set(na, "where", json_string(where));
        json_t *r = md2_call(ctx, "_note_refresh_failure", na);
        if (md2_exc_pending(ctx)) { json_free(r); return NULL; }
        json_free(r);
        return json_null();
    }
    return NULL;  /* BaseException propagates */
}

/* PoP: md2_refresh_locked @ agent/models_dev.py:_refresh_locked */
static json_t *md2_refresh_locked(md2_ctx *ctx, json_t *in) {
    const char *where = json_get_str(in, "where", "");
    bool conditional = md2_truthy(md2_env_obj(ctx, "cache"));
    json_t *a = json_object();
    json_set(a, "conditional", json_bool(conditional));
    json_t *pair = md2_call(ctx, "_fetch_models_dev_from_network", a);
    if (md2_exc_pending(ctx)) {
        json_free(pair);
        return md2_refresh_exc_tail(ctx, where);
    }
    json_t *data = json_get(pair, 0);
    json_t *etag = json_get(pair, 1);
    /* snapshot data BEFORE the seam: _commit_registry may pop/own pair, and
       pair is freed below — a post-free json_copy(data) is use-after-free. */
    json_t *data_copy = json_copy(data);
    json_t *ca = json_object();
    json_set(ca, "data", json_copy(data));
    json_set(ca, "etag", md2_copy_or_null(etag));
    json_set(ca, "where", json_string(where));
    json_t *r = md2_call(ctx, "_commit_registry", ca);
    json_free(pair);
    if (md2_exc_pending(ctx)) {
        json_free(r);
        json_free(data_copy);
        return md2_refresh_exc_tail(ctx, where);
    }
    json_free(r);
    return data_copy;
}

/* PoP: md2_registry_provider @ agent/models_dev.py:_registry_provider */
static json_t *md2_registry_provider(md2_ctx *ctx, json_t *in) {
    const char *mdev = json_get_str(in, "mdev_id", "");
    bool allow = md2_truthy(md2_get(in, "allow_network"));
    json_t *a = json_object();
    if (!allow) json_set(a, "allow_network", json_bool(false));
    json_t *reg = md2_call(ctx, "fetch_models_dev", a);
    if (md2_exc_pending(ctx)) { json_free(reg); return NULL; }
    json_t *pd = md2_get(reg, mdev);
    json_t *ret = md2_is_obj(pd) ? json_copy(pd) : json_null();
    json_free(reg);
    return ret;
}

/* PoP: md2_registry_models @ agent/models_dev.py:_registry_models */
static json_t *md2_registry_models(md2_ctx *ctx, json_t *in) {
    json_t *ti = json_object();
    json_set(ti, "mdev_id", json_copy(md2_get(in, "mdev_id")));
    json_set(ti, "allow_network", json_copy(md2_get(in, "allow_network")));
    json_t *pd = md2_registry_provider(ctx, ti);
    json_free(ti);
    if (md2_exc_pending(ctx)) { json_free(pd); return NULL; }
    if (md2_is_none(pd)) { json_free(pd); return json_null(); }
    json_t *models = md2_get(pd, "models");
    json_t *ret = models ? (md2_is_obj(models) ? json_copy(models) : json_null())
                         : json_object();   /* .get("models", {}) */
    json_free(pd);
    return ret;
}

/* PoP: md2_openrouter_catalog_lookup_base @ agent/models_dev.py:_openrouter_catalog_lookup_base */
static json_t *md2_openrouter_catalog_lookup_base(md2_ctx *ctx, json_t *in) {
    const char *provider = json_get_str(in, "provider", "");
    const char *model = json_get_str(in, "model", "");
    if (!md2_arr_has_str(md2_env_arr(ctx, "openrouter_providers"), provider))
        return json_null();
    json_t *a = json_object();
    md2_arg_str(a, "model", model);
    json_t *ret = md2_call(ctx, "openrouter_variant_base", a);
    return ret;
}

/* PoP: md2_iter_model_entries @ agent/models_dev.py:_iter_model_entries */
static void md2_iter_collect(md2_ctx *ctx, json_t *out, json_t *models,
                             const char *model, bool suffix_fallback,
                             const char *provider, int depth) {
    const char *suffixes[2] = {":cloud", "-cloud"};
    int nnames = suffix_fallback ? 3 : 1;
    for (int ni = 0; ni < nnames; ni++) {
        char *name = (ni == 0) ? strdup(model) : NULL;
        if (ni > 0) {
            size_t ln = strlen(model) + strlen(suffixes[ni - 1]) + 1;
            name = (char *)malloc(ln);
            if (name) snprintf(name, ln, "%s%s", model, suffixes[ni - 1]);
        }
        if (!name) break;
        json_t *entry = md2_get(models, name);
        if (md2_is_obj(entry))
            json_append(out, md2_pair(json_string(name), json_copy(entry)));
        char *nl = md2_lower(name);
        if (md2_is_obj(models)) {
            for (size_t k = 0; k < models->c.count; k++) {
                json_t *mdata = models->c.items[k];
                char *midl = md2_lower(models->c.keys[k]);
                if (md2_str_eq(midl, nl) && md2_is_obj(mdata))
                    json_append(out, md2_pair(json_string(models->c.keys[k]), json_copy(mdata)));
                free(midl);
            }
        }
        free(nl);
        free(name);
    }
    if (md2_exc_pending(ctx)) return;
    json_t *ti = json_object();
    json_set(ti, "provider", json_string(provider ? provider : ""));
    json_set(ti, "model", json_string(model));
    json_t *routed = md2_openrouter_catalog_lookup_base(ctx, ti);
    json_free(ti);
    if (md2_exc_pending(ctx)) { json_free(routed); return; }
    if (!md2_is_none(routed) && depth < 8) {
        json_t *ri = json_object();
        json_set(ri, "provider", json_string(""));
        json_set(ri, "model", json_copy(routed));
        /* recursion inline (collect into the same out array) */
        md2_iter_collect(ctx, out, models, md2_jstr(routed), suffix_fallback, "", depth + 1);
        json_free(ri);
    }
    json_free(routed);
}
static json_t *md2_iter_model_entries(md2_ctx *ctx, json_t *in) {
    json_t *out = json_array();
    md2_iter_collect(ctx, out, md2_get(in, "models"),
                     json_get_str(in, "model", ""),
                     md2_truthy(md2_get_def(in, "suffix_fallback", json_bool(true))),
                     json_get_str(in, "provider", ""), 0);
    if (md2_exc_pending(ctx)) { json_free(out); return NULL; }
    return out;
}

/* PoP: md2_extract_limit @ agent/models_dev.py:_extract_limit */
static json_t *md2_extract_limit(md2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *entry = md2_get(in, "entry");
    const char *key = json_get_str(in, "key", "");
    json_t *lim = md2_get(entry, "limit");
    json_t *value = md2_get(lim, key);
    long long iv = 0;
    if (md2_pos_int(value, &iv)) return json_number((double)iv);
    return json_null();
}

/* PoP: md2_builtin_model_metadata @ agent/models_dev.py:_builtin_model_metadata */
static json_t *md2_builtin_model_metadata(md2_ctx *ctx, json_t *in) {
    const char *provider = json_get_str(in, "provider", "");
    const char *model = json_get_str(in, "model", "");
    json_t *config = md2_get(in, "config");
    json_t *ti = json_object();
    json_set(ti, "provider", json_string(provider));
    if (config && config->type != JSON_NULL) json_set(ti, "config", json_copy(config));
    json_t *pk = md2_models_dev_id(ctx, ti);
    json_free(ti);
    if (md2_exc_pending(ctx)) { json_free(pk); return NULL; }
    const char *pkey = md2_is_none(pk) ? provider : md2_jstr(pk);
    char *mkey = md2_lower(md2_strip(model));
    char *comp = (char *)malloc(strlen(pkey) + strlen(mkey) + 2);
    if (!comp) { json_free(pk); free(mkey); return NULL; }
    snprintf(comp, strlen(pkey) + strlen(mkey) + 2, "%s|%s", pkey, mkey);
    json_t *meta = md2_env_obj(ctx, "builtin_metadata");
    json_t *ret = md2_copy_or_null(md2_get(meta, comp));
    free(comp); free(mkey); json_free(pk);
    return ret;
}

/* PoP: md2_relay_vision_marker_metadata @ agent/models_dev.py:_relay_vision_marker_metadata */
static json_t *md2_relay_vision_marker_metadata(md2_ctx *ctx, json_t *in) {
    const char *provider = json_get_str(in, "provider", "");
    char *ml = md2_lower(md2_strip(json_get_str(in, "model", "")));
    if (!strstr(ml, "-vision")) { free(ml); return json_null(); }
    free(ml);
    json_t *a = json_object();
    md2_arg_str(a, "provider", provider);
    json_t *fam = md2_call(ctx, "opencode_provider_family", a);
    if (md2_exc_pending(ctx)) { json_free(fam); return NULL; }
    if (md2_is_none(fam)) { json_free(fam); return json_null(); }
    json_free(fam);
    json_t *out = md2_obj_copy(md2_env_obj(ctx, "unknown_model_base"));
    json_t *mods = json_object();
    json_t *inp = json_array();
    json_append(inp, json_string("text"));
    json_append(inp, json_string("image"));
    json_t *outp = json_array();
    json_append(outp, json_string("text"));
    json_set(mods, "input", inp);
    json_set(mods, "output", outp);
    json_set(out, "modalities", mods);
    return out;
}

/* PoP: md2_provider_model_capabilities @ agent/models_dev.py:_provider_model_capabilities */
static json_t *md2_provider_model_capabilities(md2_ctx *ctx, json_t *in) {
    const char *provider = json_get_str(in, "provider", "");
    const char *model = json_get_str(in, "model", "");
    json_t *a = json_object();
    md2_arg_str(a, "provider", provider);
    json_t *prof = md2_call(ctx, "get_provider_profile", a);
    if (md2_exc_pending(ctx)) { json_free(prof); return NULL; }
    if (md2_is_none(prof)) { json_free(prof); return json_object(); }
    json_t *mc = md2_get(prof, "model_capabilities");
    json_t *ret = md2_copy_or_null(md2_get(mc, model));
    if (md2_is_none(ret)) { json_free(ret); ret = json_object(); }
    json_free(prof);
    return ret;
}

/* claimed sibling _merge_catalog_entry_with_override is SEAMED, never ported here */
/* PoP: md2_apply_overrides @ agent/models_dev.py:_apply_overrides */
static json_t *md2_apply_overrides(md2_ctx *ctx, json_t *in) {
    const char *provider = json_get_str(in, "provider", "");
    const char *model = json_get_str(in, "model", "");
    json_t *entry = md2_get(in, "entry");
    json_t *config = md2_get(in, "config");

    /* builtin = _builtin_model_metadata(provider, model, config) */
    json_t *bi = json_object();
    json_set(bi, "provider", json_string(provider));
    json_set(bi, "model", json_string(model));
    if (config && config->type != JSON_NULL) json_set(bi, "config", json_copy(config));
    json_t *builtin = md2_builtin_model_metadata(ctx, bi);
    json_free(bi);
    if (md2_exc_pending(ctx)) { json_free(builtin); return NULL; }

    json_t *base = !md2_is_none(entry) ? json_copy(entry)
                                       : (md2_is_none(builtin) ? NULL : builtin);
    if (md2_is_none(builtin) && base != builtin) json_free(builtin);

    /* declared = _provider_model_capabilities(provider, model) */
    json_t *di = json_object();
    json_set(di, "provider", json_string(provider));
    json_set(di, "model", json_string(model));
    json_t *declared = md2_provider_model_capabilities(ctx, di);
    json_free(di);
    if (md2_exc_pending(ctx)) { json_free(declared); json_free(base); return NULL; }
    if (md2_truthy(declared)) {
        json_t *src = base ? base : md2_obj_copy(md2_env_obj(ctx, "unknown_model_base"));
        json_t *ma = json_object();
        json_set(ma, "raw", json_copy(src));
        json_set(ma, "override", json_copy(declared));
        json_t *merged = md2_call(ctx, "_merge_catalog_entry_with_override", ma);
        if (md2_exc_pending(ctx)) { json_free(merged); json_free(declared); json_free(src); json_free(base); return NULL; }
        if (src != base) json_free(src);
        json_free(base);
        base = merged;
    }
    json_free(declared);

    /* override = _override_for(..., catalog_hit = base is not None) */
    json_t *oa = json_object();
    json_set(oa, "provider", json_string(provider));
    json_set(oa, "model", json_string(model));
    json_set(oa, "catalog_hit", json_bool(base != NULL));
    if (config && config->type != JSON_NULL) json_set(oa, "config", json_copy(config));
    json_t *override = md2_call(ctx, "_override_for", oa);
    if (md2_exc_pending(ctx)) { json_free(override); json_free(base); return NULL; }
    if (md2_is_none(base)) {
        json_free(base);
        json_t *ri = json_object();
        json_set(ri, "provider", json_string(provider));
        json_set(ri, "model", json_string(model));
        base = md2_relay_vision_marker_metadata(ctx, ri);
        json_free(ri);
        if (md2_exc_pending(ctx)) { json_free(override); json_free(base); return NULL; }
    }
    if (md2_is_none(override)) { json_free(override); return base; }
    json_t *src = base ? base : md2_obj_copy(md2_env_obj(ctx, "unknown_model_base"));
    json_t *ma = json_object();
    json_set(ma, "raw", json_copy(src));
    json_set(ma, "override", json_copy(override));
    json_t *res = md2_call(ctx, "_merge_catalog_entry_with_override", ma);
    if (md2_exc_pending(ctx)) {
        json_free(res); json_free(override);
        if (src != base) json_free(src);
        json_free(base);
        return NULL;
    }
    if (src != base) json_free(src);
    json_free(base);
    json_free(override);
    return res;
}

/* PoP: md2_entry_supports_vision @ agent/models_dev.py:_entry_supports_vision */
static json_t *md2_entry_supports_vision(md2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *entry = md2_get(in, "entry");
    json_t *mods = md2_get_def(entry, "modalities", NULL);
    json_t *inputv = md2_get(mods, "input");
    bool r;
    if (md2_is_arr(inputv))
        r = md2_arr_has_str(inputv, "image");
    else
        r = md2_truthy(md2_get_def(entry, "attachment", json_bool(false)));
    return json_bool(r);
}
