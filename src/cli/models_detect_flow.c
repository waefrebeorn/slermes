/* models_detect_flow.c — hermes_cli/models_detect.py FLOW + leaves (C11).
 *
 * Decision-sequence port of the live-catalog / credential guards that
 * detect_provider_for_model consults:
 *   - current_provider_catalog_match  (live catalog outranks static guesses)
 *   - current_provider_owns_vendor    (single-vendor first-party ownership)
 *   - provider_has_credentials        (auto-detected target must be authed)
 *
 * Every file/network/config/credential dependency rides the injected universal
 * seam (mdet_call); the harness scripts results and records the ordered trace
 * the oracle diffs against LIVE models_detect.py driven under the same scripts.
 *
 * Self-contained, libjson-only. All symbols static mdet_*; the harness
 * #include "cli/models_detect_flow.c" into its own translation unit.
 *
 * Representation contract (shared with tests/oracle_models_detect_flow.py):
 *   - None -> NULL pointer (harness emits JSON null); bool -> json_bool.
 *   - uncaught exception -> {"__raise__": "<ClassName>"} (class only).
 *   - scripts (case "io", seam name -> script): absent key -> None;
 *     {"$seq":[...]} one element per call (exhaustion raises
 *     RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m}
 *     raises that class; any other JSON value reused (NULL == None).
 *   - seam args are owned by mdet_call (transferred into the trace event);
 *     None-valued arg keys are omitted on BOTH sides.
 *   - str()/strip()/lower() operand fields are ASCII strings or null in
 *     fixtures. str.strip() ASCII ws set: 0x09-0x0d, 0x1c-0x1f, 0x20.
 *   - `except Exception` swallows Exception subclasses (incl. synthetic unknown
 *     names) but passes BaseException-only kinds (SystemExit, KeyboardInterrupt,
 *     GeneratorExit, BaseException, BaseExceptionGroup) through. The FULL
 *     builtin hierarchy (py3.14) is tabulated below for that classification.
 *
 * Seam names -> live call:
 *   cached_provider_model_ids   models.cached_provider_model_ids(provider)
 *   get_env_value_prefer_dotenv config.get_env_value_prefer_dotenv(key)
 *   get_auth_status             auth.get_auth_status(provider_id)
 *   load_pool                   agent.credential_pool.load_pool(provider)
 *   has_credentials             pool.has_credentials()
 *   has_available               pool.has_available()
 *
 * Env tables (case "env"):
 *   aliases          _PROVIDER_ALIASES dict (normalize_provider)
 *   vendor_prefixes  _VENDOR_PREFIXES ordered dict (detect_vendor)
 *   aggregators      _AGGREGATOR_PROVIDERS membership list
 *   provider_models  _PROVIDER_MODELS dict provider -> [model ids]
 *   import_fail      list of module full names whose import raises
 *                    ModuleNotFoundError (sys.modules[full]=None gating).
 *                    Top-level imports of models_detect (hermes_cli.models /
 *                    model_normalize / auth / config) are OUTSIDE any try ->
 *                    uncaught ModuleNotFoundError; the in-try
 *                    agent.credential_pool import -> caught -> False.
 *
 * Pure leaves (normalize_provider, detect_vendor, has_usable_secret,
 * _is_placeholder_shape) are transcribed as INTERNAL helpers (no PoP claim;
 * claimed elsewhere) and stay REAL in the oracle via AST extraction. The
 * ported module's own _SKIP constant and auth's _PLACEHOLDER_SECRET_VALUES /
 * _PLACEHOLDER_KEY_PREFIXES are transcribed verbatim (oracle execs the real
 * Assign nodes).
 *
 * Fixture domains: catalog script is null or an array of strings (null ->
 * uncaught TypeError, iteration is outside the try); load_pool script is null
 * (-> AttributeError caught -> False) or an object (pool double); get_auth_status
 * script is an object or null (truthy non-object -> AttributeError caught).
 */
#include "hermes_json.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mdet_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted tables/constants */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} mdet_ctx;

/* ── string helpers (ASCII fixture domain; Python str semantics) ─────────── */

static char *mdet_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* str.strip() ASCII whitespace: 0x09-0x0d, 0x1c-0x1f, 0x20 (Python isspace). */
static bool mdet_is_ws(char c) {
    return (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f) || c == ' ';
}

static char *mdet_strip(const char *s) {
    while (*s && mdet_is_ws(*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && mdet_is_ws(s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

static char *mdet_lower(const char *s) {
    char *out = mdet_strdup(s);
    if (out) for (char *p = out; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}

static char *mdet_strip_lower(const char *s) {
    char *t = mdet_strip(s);
    char *out = mdet_lower(t ? t : "");
    free(t);
    return out;
}

static bool mdet_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

static bool mdet_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* ── JSON value helpers ──────────────────────────────────────────────────── */

static bool mdet_truthy(const json_t *v) {
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

static json_t *mdet_env_obj(const mdet_ctx *ctx, const char *key) {
    json_t *v = ctx->env ? json_obj_get(ctx->env, key) : NULL;
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static json_t *mdet_env_arr(const mdet_ctx *ctx, const char *key) {
    json_t *v = ctx->env ? json_obj_get(ctx->env, key) : NULL;
    return (v && v->type == JSON_ARRAY) ? v : NULL;
}

static bool mdet_arr_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && mdet_str_eq(it->str_val, s)) return true;
    }
    return false;
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

static bool mdet_exc_pending(const mdet_ctx *ctx) { return ctx && ctx->exc != NULL; }

static void mdet_exc_clear(mdet_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}

static void mdet_raise(mdet_ctx *ctx, const char *cls) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
}

/* FULL builtin exception hierarchy (CPython 3.14.9). {name, base1, base2}.
 * Aliases (EnvironmentError/IOError/WindowsError == OSError) report Exception
 * as their base, matching __bases__ on this build. */
typedef struct mdet_exc_row {
    const char *name;
    const char *base1;
    const char *base2;
} mdet_exc_row;

static const mdet_exc_row MDET_EXC_TABLE[] = {
    {"ArithmeticError", "Exception", NULL},
    {"AssertionError", "Exception", NULL},
    {"AttributeError", "Exception", NULL},
    {"BaseException", "object", NULL},
    {"BaseExceptionGroup", "BaseException", NULL},
    {"BlockingIOError", "OSError", NULL},
    {"BrokenPipeError", "ConnectionError", NULL},
    {"BufferError", "Exception", NULL},
    {"BytesWarning", "Warning", NULL},
    {"ChildProcessError", "OSError", NULL},
    {"ConnectionAbortedError", "ConnectionError", NULL},
    {"ConnectionError", "OSError", NULL},
    {"ConnectionRefusedError", "ConnectionError", NULL},
    {"ConnectionResetError", "ConnectionError", NULL},
    {"DeprecationWarning", "Warning", NULL},
    {"EOFError", "Exception", NULL},
    {"EncodingWarning", "Warning", NULL},
    {"EnvironmentError", "Exception", NULL},
    {"Exception", "BaseException", NULL},
    {"ExceptionGroup", "BaseExceptionGroup", "Exception"},
    {"FileExistsError", "OSError", NULL},
    {"FileNotFoundError", "OSError", NULL},
    {"FloatingPointError", "ArithmeticError", NULL},
    {"FutureWarning", "Warning", NULL},
    {"GeneratorExit", "BaseException", NULL},
    {"IOError", "Exception", NULL},
    {"ImportError", "Exception", NULL},
    {"ImportWarning", "Warning", NULL},
    {"IndentationError", "SyntaxError", NULL},
    {"IndexError", "LookupError", NULL},
    {"InterruptedError", "OSError", NULL},
    {"IsADirectoryError", "OSError", NULL},
    {"KeyError", "LookupError", NULL},
    {"KeyboardInterrupt", "BaseException", NULL},
    {"LookupError", "Exception", NULL},
    {"MemoryError", "Exception", NULL},
    {"ModuleNotFoundError", "ImportError", NULL},
    {"NameError", "Exception", NULL},
    {"NotADirectoryError", "OSError", NULL},
    {"NotImplementedError", "RuntimeError", NULL},
    {"OSError", "Exception", NULL},
    {"OverflowError", "ArithmeticError", NULL},
    {"PendingDeprecationWarning", "Warning", NULL},
    {"PermissionError", "OSError", NULL},
    {"ProcessLookupError", "OSError", NULL},
    {"PythonFinalizationError", "RuntimeError", NULL},
    {"RecursionError", "RuntimeError", NULL},
    {"ReferenceError", "Exception", NULL},
    {"ResourceWarning", "Warning", NULL},
    {"RuntimeError", "Exception", NULL},
    {"RuntimeWarning", "Warning", NULL},
    {"StopAsyncIteration", "Exception", NULL},
    {"StopIteration", "Exception", NULL},
    {"SyntaxError", "Exception", NULL},
    {"SyntaxWarning", "Warning", NULL},
    {"SystemError", "Exception", NULL},
    {"SystemExit", "BaseException", NULL},
    {"TabError", "IndentationError", NULL},
    {"TimeoutError", "OSError", NULL},
    {"TypeError", "Exception", NULL},
    {"UnboundLocalError", "NameError", NULL},
    {"UnicodeDecodeError", "UnicodeError", NULL},
    {"UnicodeEncodeError", "UnicodeError", NULL},
    {"UnicodeError", "ValueError", NULL},
    {"UnicodeTranslateError", "UnicodeError", NULL},
    {"UnicodeWarning", "Warning", NULL},
    {"UserWarning", "Warning", NULL},
    {"ValueError", "Exception", NULL},
    {"Warning", "Exception", NULL},
    {"WindowsError", "Exception", NULL},
    {"ZeroDivisionError", "ArithmeticError", NULL},
    {"_IncompleteInputError", "SyntaxError", NULL},
};

static const mdet_exc_row *mdet_exc_lookup(const char *name) {
    for (size_t i = 0; i < sizeof(MDET_EXC_TABLE) / sizeof(MDET_EXC_TABLE[0]); i++)
        if (strcmp(MDET_EXC_TABLE[i].name, name) == 0) return &MDET_EXC_TABLE[i];
    return NULL;
}

/* issubclass(cls, Exception): walk the builtin MRO. Unknown names are synthetic
 * Exception subclasses (the oracle's make_exc builds type(name,(Exception,{}))). */
static bool mdet_exc_is_exception(const char *cls) {
    if (!cls || !cls[0]) return true;
    const char *cur = cls;
    for (int depth = 0; cur && depth < 64; depth++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "object") == 0) return false;
        const mdet_exc_row *row = mdet_exc_lookup(cur);
        if (!row) return true; /* unknown -> synthetic Exception subclass */
        /* ExceptionGroup has two bases; Exception is base2 -> caught. For any
         * row whose base2 is "Exception" we are done; otherwise follow base1
         * (the only multi-base row is ExceptionGroup, handled here). */
        if (row->base2 && strcmp(row->base2, "Exception") == 0) return true;
        cur = row->base1;
    }
    return false;
}

/* Classify a pending seam raise for an `except Exception` site:
 *   0 = no pending exc; 1 = Exception subclass (cleared, caller handles);
 *   2 = BaseException-only kind (left pending, caller propagates). */
static int mdet_exc_classify(mdet_ctx *ctx) {
    if (!mdet_exc_pending(ctx)) return 0;
    const char *cls = json_get_str(ctx->exc, "__raise__", "");
    if (mdet_exc_is_exception(cls)) { mdet_exc_clear(ctx); return 1; }
    return 2;
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *mdet_script_result(mdet_ctx *ctx, const json_t *script) {
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

static json_t *mdet_call(mdet_ctx *ctx, const char *name, json_t *args) {
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
    if (chosen != ctx->exc) ret = mdet_script_result(ctx, chosen);
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

/* env import_fail gating: true when `full` is listed (sys.modules[full]=None). */
static bool mdet_import_fails(const mdet_ctx *ctx, const char *full) {
    return mdet_arr_has_str(mdet_env_arr(ctx, "import_fail"), full);
}

/* ── transcribed constants ───────────────────────────────────────────────── */

/* models_detect._SKIP = frozenset({"", "auto", "openrouter", "custom"}) */
static bool mdet_in_skip(const char *s) {
    return mdet_str_eq(s, "") || mdet_str_eq(s, "auto")
        || mdet_str_eq(s, "openrouter") || mdet_str_eq(s, "custom");
}

/* auth._PLACEHOLDER_SECRET_VALUES */
static bool mdet_in_placeholder_values(const char *lowered) {
    static const char *const vals[] = {
        "*", "**", "***", "changeme", "your_api_key", "your_api_key_here",
        "your-api-key", "placeholder", "example", "dummy", "null", "none"};
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++)
        if (strcmp(lowered, vals[i]) == 0) return true;
    return false;
}

/* ── internal pure leaves (claimed siblings; NO PoP claim here) ──────────── */

/* models.normalize_provider: (provider or "openrouter").strip().lower() ->
 * _PROVIDER_ALIASES.get(normalized, normalized). */
static char *mdet_normalize_provider(const mdet_ctx *ctx, const char *provider) {
    char *base;
    if (!provider || !provider[0]) base = mdet_strdup("openrouter");
    else base = mdet_strip_lower(provider);
    json_t *aliases = mdet_env_obj(ctx, "aliases");
    const char *mapped = aliases ? json_get_str(aliases, base, NULL) : NULL;
    char *out = mdet_strdup(mapped ? mapped : base);
    free(base);
    return out;
}

/* model_normalize.detect_vendor: vendor/ prefix, first hyphen token, or the
 * first _VENDOR_PREFIXES key the lowered name starts with. NULL == None. */
static char *mdet_detect_vendor(const mdet_ctx *ctx, const char *model_name) {
    char *name = mdet_strip(model_name ? model_name : "");
    if (!name[0]) { free(name); return NULL; }
    char *slash = strchr(name, '/');
    if (slash) {
        size_t plen = (size_t)(slash - name);
        char *prefix = (char *)malloc(plen + 1);
        memcpy(prefix, name, plen); prefix[plen] = '\0';
        char *low = mdet_lower(prefix);
        free(prefix); free(name);
        if (!low[0]) { free(low); return NULL; }
        return low;
    }
    char *name_lower = mdet_lower(name);
    free(name);
    json_t *vp = mdet_env_obj(ctx, "vendor_prefixes");
    /* first hyphen token */
    char *dash = strchr(name_lower, '-');
    size_t tlen = dash ? (size_t)(dash - name_lower) : strlen(name_lower);
    char *token = (char *)malloc(tlen + 1);
    memcpy(token, name_lower, tlen); token[tlen] = '\0';
    if (vp) {
        const char *mapped = json_get_str(vp, token, NULL);
        if (mapped) { free(token); free(name_lower); return mdet_strdup(mapped); }
    }
    free(token);
    /* startswith fallback in dict insertion order */
    if (vp) {
        for (size_t i = 0; i < vp->c.count; i++) {
            const char *prefix = vp->c.keys[i];
            json_t *val = vp->c.items[i];
            if (prefix && mdet_starts_with(name_lower, prefix)) {
                free(name_lower);
                return mdet_strdup(val && val->type == JSON_STRING ? val->str_val : "");
            }
        }
    }
    free(name_lower);
    return NULL;
}

/* auth._is_placeholder_shape */
static bool mdet_is_placeholder_shape(const char *value) {
    char *lowered = mdet_lower(value);
    bool result = false;
    size_t n = strlen(lowered);
    if (mdet_starts_with(lowered, "your_") && n >= 5 && strcmp(lowered + n - 5, "_here") == 0) {
        result = true; goto done;
    }
    static const char *const prefixes[] = {"sk-", "ghp_", "hf_"};
    for (size_t i = 0; i < 3; i++) {
        size_t plen = strlen(prefixes[i]);
        if (mdet_starts_with(lowered, prefixes[i])) {
            const char *tail = lowered + plen;
            if (tail[0]) {
                bool allx = true;
                for (const char *p = tail; *p; p++) if (*p != 'x') { allx = false; break; }
                if (allx) { result = true; goto done; }
            }
        }
    }
    {
        /* stripped = lowered.replace(" ","").replace("-","").replace("_","") */
        char *stripped = (char *)malloc(n + 1);
        size_t k = 0;
        for (size_t i = 0; i < n; i++) {
            char c = lowered[i];
            if (c != ' ' && c != '-' && c != '_') stripped[k++] = c;
        }
        stripped[k] = '\0';
        if (k > 0) {
            bool allx = true;
            for (size_t i = 0; i < k; i++) if (stripped[i] != 'x') { allx = false; break; }
            result = allx;
        }
        free(stripped);
    }
done:
    free(lowered);
    return result;
}

/* auth.has_usable_secret(value, min_length=4): isinstance str gate, strip,
 * length >= 4, not a placeholder value, not a placeholder shape. */
static bool mdet_has_usable_secret(const json_t *value) {
    if (!value || value->type != JSON_STRING) return false;
    char *cleaned = mdet_strip(value->str_val);
    bool ok = false;
    if (strlen(cleaned) >= 4) {
        char *low = mdet_lower(cleaned);
        if (!mdet_in_placeholder_values(low) && !mdet_is_placeholder_shape(cleaned)) ok = true;
        free(low);
    }
    free(cleaned);
    return ok;
}

/* ── ported flows ────────────────────────────────────────────────────────── */

/* PoP: mdet_current_provider_catalog_match @ hermes_cli/models_detect.py:current_provider_catalog_match */
static json_t *mdet_current_provider_catalog_match(mdet_ctx *ctx, const char *model_name,
                                                   const char *current_provider) {
    /* top-level in-function import (outside try): hermes_cli.models */
    if (mdet_import_fails(ctx, "hermes_cli.models")) { mdet_raise(ctx, "ModuleNotFoundError"); return NULL; }
    char *provider = mdet_strip_lower(current_provider ? current_provider : "");
    json_t *result = NULL;
    if (mdet_in_skip(provider) || mdet_starts_with(provider, "custom:")) goto done;
    {
        char *norm = mdet_normalize_provider(ctx, provider);
        bool skip = mdet_in_skip(norm);
        free(norm);
        if (skip) goto done;
    }
    char *wanted = mdet_strip_lower(model_name ? model_name : "");
    if (!wanted[0]) { free(wanted); goto done; }
    {
        json_t *args = json_object();
        json_set(args, "provider", json_string(provider));
        json_t *catalog = mdet_call(ctx, "cached_provider_model_ids", args);
        int ec = mdet_exc_classify(ctx);
        if (ec == 1) { json_free(catalog); free(wanted); goto done; } /* except Exception -> None */
        if (ec == 2) { json_free(catalog); free(wanted); goto done; } /* BaseException propagates */
        /* iteration is OUTSIDE the try: a non-list catalog raises uncaught. */
        if (!catalog || catalog->type != JSON_ARRAY) {
            mdet_raise(ctx, "TypeError");
            json_free(catalog); free(wanted); goto done;
        }
        /* pass 1: exact id (case-insensitive), first match wins */
        for (size_t i = 0; i < catalog->c.count; i++) {
            json_t *it = catalog->c.items[i];
            if (!it || it->type != JSON_STRING) {
                mdet_raise(ctx, "AttributeError"); /* mid.lower() */
                json_free(catalog); free(wanted); goto done;
            }
            char *low = mdet_lower(it->str_val);
            bool match = mdet_str_eq(low, wanted);
            free(low);
            if (match) { result = json_string(it->str_val); json_free(catalog); free(wanted); goto done; }
        }
        /* pass 2: bare part after vendor/ */
        for (size_t i = 0; i < catalog->c.count; i++) {
            json_t *it = catalog->c.items[i];
            const char *mid = it->str_val; /* strings guaranteed by pass 1 */
            const char *slash = strchr(mid, '/');
            if (!slash) continue;
            char *low = mdet_lower(slash + 1);
            bool match = mdet_str_eq(low, wanted);
            free(low);
            if (match) { result = json_string(mid); break; }
        }
        json_free(catalog);
    }
    free(wanted);
done:
    free(provider);
    return result;
}

/* PoP: mdet_current_provider_owns_vendor @ hermes_cli/models_detect.py:current_provider_owns_vendor */
static json_t *mdet_current_provider_owns_vendor(mdet_ctx *ctx, const char *model_name,
                                                 const char *current_provider) {
    /* top-level in-function imports (no try): model_normalize + models */
    if (mdet_import_fails(ctx, "hermes_cli.model_normalize")) { mdet_raise(ctx, "ModuleNotFoundError"); return NULL; }
    if (mdet_import_fails(ctx, "hermes_cli.models")) { mdet_raise(ctx, "ModuleNotFoundError"); return NULL; }
    char *provider = mdet_strip_lower(current_provider ? current_provider : "");
    json_t *result = json_bool(false);
    if (mdet_in_skip(provider) || mdet_starts_with(provider, "custom:")) goto done;
    char *normalized = mdet_normalize_provider(ctx, provider);
    if (mdet_in_skip(normalized) || mdet_arr_has_str(mdet_env_arr(ctx, "aggregators"), normalized)) {
        free(normalized); goto done;
    }
    char *vendor = mdet_detect_vendor(ctx, model_name ? model_name : "");
    if (!vendor || !vendor[0]) { free(vendor); free(normalized); goto done; }
    {
        json_t *pm = mdet_env_obj(ctx, "provider_models");
        json_t *mids = pm ? json_obj_get(pm, normalized) : NULL;
        /* _PROVIDER_MODELS.get(normalized, ()): absent -> empty -> native != {vendor} */
        if (mids && mids->type != JSON_ARRAY) {
            /* present non-list: live iteration raises (no try here) */
            mdet_raise(ctx, "TypeError");
            result = NULL;
            free(vendor); free(normalized); goto done;
        }
        bool owns = false;
        if (mids && mids->c.count > 0) {
            owns = true;
            for (size_t i = 0; i < mids->c.count; i++) {
                json_t *it = mids->c.items[i];
                if (!it || it->type != JSON_STRING) {
                    mdet_raise(ctx, "AttributeError"); /* detect_vendor: mid.strip() */
                    result = NULL; owns = false; break;
                }
                char *v = mdet_detect_vendor(ctx, it->str_val);
                bool same = v && mdet_str_eq(v, vendor);
                free(v);
                if (!same) { owns = false; break; }
            }
        }
        if (!mdet_exc_pending(ctx)) result = json_bool(owns);
    }
    free(vendor);
    free(normalized);
done:
    free(provider);
    return result;
}

/* PoP: mdet_provider_has_credentials @ hermes_cli/models_detect.py:provider_has_credentials */
static json_t *mdet_provider_has_credentials(mdet_ctx *ctx, const char *provider) {
    /* top-level in-function imports (outside try): auth then config */
    if (mdet_import_fails(ctx, "hermes_cli.auth")) { mdet_raise(ctx, "ModuleNotFoundError"); return NULL; }
    if (mdet_import_fails(ctx, "hermes_cli.config")) { mdet_raise(ctx, "ModuleNotFoundError"); return NULL; }
    char *pid = mdet_strip_lower(provider ? provider : "");
    json_t *result = json_bool(false);
    if (!pid[0]) goto done;
    if (mdet_str_eq(pid, "custom") || mdet_starts_with(pid, "custom:")) {
        result = json_bool(true); goto done;
    }
    /* ── try region ── */
    if (mdet_str_eq(pid, "openrouter")) {
        json_t *args = json_object();
        json_set(args, "key", json_string("OPENROUTER_API_KEY"));
        json_t *key = mdet_call(ctx, "get_env_value_prefer_dotenv", args);
        int ec = mdet_exc_classify(ctx);
        if (ec == 1) { json_free(key); goto done; }       /* except Exception -> False */
        if (ec == 2) { json_free(key); result = NULL; goto done; }
        bool usable = mdet_has_usable_secret(key);
        json_free(key);
        if (usable) { result = json_bool(true); goto done; }
    }
    {
        json_t *args = json_object();
        json_set(args, "provider_id", json_string(pid));
        json_t *status_r = mdet_call(ctx, "get_auth_status", args);
        int ec = mdet_exc_classify(ctx);
        if (ec == 1) { json_free(status_r); goto done; }
        if (ec == 2) { json_free(status_r); result = NULL; goto done; }
        /* status = get_auth_status(pid) or {} */
        if (mdet_truthy(status_r)) {
            if (status_r->type != JSON_OBJECT) {
                /* truthy non-dict: status.get raises AttributeError (caught) */
                json_free(status_r); goto done;
            }
            if (mdet_truthy(json_obj_get(status_r, "logged_in"))
                || mdet_truthy(json_obj_get(status_r, "configured"))) {
                json_free(status_r); result = json_bool(true); goto done;
            }
        }
        json_free(status_r);
    }
    /* from agent.credential_pool import load_pool (INSIDE try) */
    if (mdet_import_fails(ctx, "agent.credential_pool")) {
        mdet_raise(ctx, "ModuleNotFoundError");
        int ec = mdet_exc_classify(ctx);
        if (ec == 1) goto done;          /* ModuleNotFoundError caught -> False */
        result = NULL; goto done;        /* (ModuleNotFoundError is Exception; ec==2 unreachable) */
    }
    {
        json_t *args = json_object();
        json_set(args, "provider", json_string(pid));
        json_t *pool = mdet_call(ctx, "load_pool", args);
        int ec = mdet_exc_classify(ctx);
        if (ec == 1) { json_free(pool); goto done; }
        if (ec == 2) { json_free(pool); result = NULL; goto done; }
        /* pool.has_credentials(): non-object pool -> AttributeError (caught) */
        if (!pool || pool->type != JSON_OBJECT) {
            json_free(pool); goto done;
        }
        json_free(pool);
    }
    {
        json_t *hc = mdet_call(ctx, "has_credentials", json_object());
        int ec = mdet_exc_classify(ctx);
        if (ec == 1) { json_free(hc); goto done; }
        if (ec == 2) { json_free(hc); result = NULL; goto done; }
        bool has = mdet_truthy(hc);
        json_free(hc);
        if (!has) goto done; /* short-circuit: has_available not called */
    }
    {
        json_t *ha = mdet_call(ctx, "has_available", json_object());
        int ec = mdet_exc_classify(ctx);
        if (ec == 1) { json_free(ha); goto done; }
        if (ec == 2) { json_free(ha); result = NULL; goto done; }
        result = json_bool(mdet_truthy(ha));
        json_free(ha);
    }
done:
    free(pid);
    return result;
}

/* ── dispatch (harness entry) ────────────────────────────────────────────── */

static json_t *mdet_run(mdet_ctx *ctx, const char *func, json_t *in) {
    if (strcmp(func, "current_provider_catalog_match") == 0)
        return mdet_current_provider_catalog_match(ctx, json_get_str(in, "model_name", NULL),
                                                   json_get_str(in, "current_provider", NULL));
    if (strcmp(func, "current_provider_owns_vendor") == 0)
        return mdet_current_provider_owns_vendor(ctx, json_get_str(in, "model_name", NULL),
                                                 json_get_str(in, "current_provider", NULL));
    if (strcmp(func, "provider_has_credentials") == 0)
        return mdet_provider_has_credentials(ctx, json_get_str(in, "provider", NULL));
    return NULL;
}