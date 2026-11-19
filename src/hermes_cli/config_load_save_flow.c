/* config_load_save_flow.c — FLOW-core port of hermes_cli/config.py's
 * load / migration-adjacent / save pipeline.
 *
 * Ports the DECISION SEQUENCES around the config file lifecycle:
 *   worker-snapshot bypass -> lock-free cache fast path -> locked re-check ->
 *   parse -> corrupt/error fallback (FailedConfigRead) -> cache publication
 *   (_read_raw_config_impl), the merged-load cache signature + env-drift /
 *   read-error re-probe hit predicate (_load_config_cache_sig,
 *   _load_config_cache_hit), the last-known-good / backup fallback chain
 *   (_last_known_good_fallback), the managed-scope overlay merge
 *   (_merge_managed_overlay), the comment-preserving write guard chain
 *   (_write_config_state / atomic_config_replace), managed-key stripping
 *   (_strip_managed_keys_for_save), installer seeding (seed_config_file),
 *   env-ref resolution + recursive expansion (_env_ref_lookup,
 *   _expand_env_vars), the config-set key validation walk
 *   (_validate_config_key / _validate_schema_key / _is_wrong_prefix_suggestion)
 *   and the config-set value coercion chain (_coerce_config_set_value).
 *
 * All file / env / backend I/O is INJECTED through the cfgls_flow_io_t seam
 * dispatcher; the decision logic around each seam matches the live Python
 * exactly. Pure siblings (_deep_merge, _split_key_path, _env_ref_var_name,
 * _is_non_env_secret_ref, _looks_structured_value, _omitted_config_paths,
 * _strip_dotted_keys, cfg_get, _known_top_level_keys) are transcribed LIVE
 * (the older config_pure.c ports of _deep_merge / _normalize_max_turns drift
 * from the current upstream: None-over-dict merge guard, sparse agent:{}
 * growth) and stay static + untagged so the existing PoP claims keep credit.
 *
 * Seam model mirrors port_credential_pool_flow.c: every I/O call goes through
 * cfgls_seam(io, name, args); the dispatcher appends `name` to io->trace,
 * returns the scripted result, applies "state" effects, and models a raised
 * exception with {"__raise__": {type,message,...}}. An "import_raise" script
 * entry models a failed in-function import: the dispatcher returns
 * {"__raise__": spec, "__untrace__": true} and seam() retracts the trace token
 * (the live import statement records nothing before it raises).
 *
 * io->state is a json OBJECT mirroring the module globals the flows mutate:
 *   _RAW_CONFIG_CACHE / _LOAD_CONFIG_CACHE  path_key -> [*sig, data, ...]
 *   _LAST_EXPANDED_CONFIG_BY_PATH           path_key -> expanded dict
 *   _CONFIG_PARSE_FAILURES                  path_key -> failure record
 *   DEFAULT_CONFIG / _PLATFORM_CONTAINER_KEYS / _OPEN_SUBKEY_TOP_LEVEL_KEYS /
 *   _EXTRA_KNOWN_ROOT_KEYS / LEFT_CORE_TERMINAL_KEYS  (validation constants)
 * Cache entries are JSON ARRAYS (Python tuples); a FailedConfigRead rides as
 * {"__failed_read__": {"data": {...}, "error": {type,message,...}}}.
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state
 * / io->captured_stderr are harness-owned. cfgls_seam() frees its args. Every public
 * cfgls_* function returns a NEW reference (or a propagated raise sentinel).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── live module constants (byte-pinned to hermes_cli/config.py) ───────── */
#define CFGLS_FIX_PERMS "Fix the file permissions or move it aside first."
#define CFGLS_FIX_READ_RETRY "Try again; run `hermes config check` if it keeps failing."
#define CFGLS_FIX_YAML_PRE \
    "Fix it with `hermes config edit` and check with `hermes config check`, or copy the newest good file from "
#define CFGLS_FIX_YAML_POST " over config.yaml."
#define CFGLS_OMISSION_FIX \
    "Pass the complete current config, or use atomic_config_replace() only when deletion by omission is deliberate."

/* _SCALAR_WORDS (hermes_cli/config_set_scalars.py) — lowercased word -> value. */
typedef struct { const char *word; int kind; } cfgls_scalar_word_t; /* 0=false 1=true 2=null */
static const cfgls_scalar_word_t CFGLS_SCALAR_WORDS[] = {
    {"true", 1}, {"yes", 1}, {"on", 1},
    {"false", 0}, {"no", 0}, {"off", 0},
    {"null", 2}, {"none", 2}, {"~", 2},
};

/* ── seam dispatcher + trace + state ───────────────────────────────────── */
typedef struct cfgls_flow_io_t {
    json_t *(*call)(struct cfgls_flow_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* ARRAY of string tokens (harness-owned) */
    json_t *state;   /* OBJECT (harness-owned) */
    json_t *captured_stderr;  /* ARRAY of captured print/exit messages (harness-owned) */
    void   *ctx;
} cfgls_flow_io_t;

static void cfgls_trace(cfgls_flow_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}

static json_t *cfgls_call(cfgls_flow_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}

static bool cfgls_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

/* borrowed raise spec (the {type,message,...} object) or NULL */
static json_t *cfgls_raise_spec(const json_t *r) {
    if (!cfgls_raised(r)) return NULL;
    return json_obj_get(r, "__raise__");
}

static json_t *cfgls_make_raise(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    json_set(spec, "message", json_string(message ? message : ""));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}

static json_t *cfgls_make_raise_code(const char *type, double code) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    char buf[32];
    snprintf(buf, sizeof buf, "%.0f", code);
    json_set(spec, "message", json_string(buf));
    json_set(spec, "code", json_number(code));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}

/* Builtin exception class names (py3.14): make_exc's getattr(builtins, t)
 * succeeds only for these; anything else degrades to Exception. */
static bool cfgls_builtin_exc(const char *t) {
    static const char *names[] = {
        "BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit",
        "Exception", "StopIteration", "StopAsyncIteration", "ArithmeticError",
        "FloatingPointError", "OverflowError", "ZeroDivisionError", "AssertionError",
        "AttributeError", "BufferError", "EOFError", "ImportError", "ModuleNotFoundError",
        "LookupError", "IndexError", "KeyError", "MemoryError", "NameError",
        "UnboundLocalError", "OSError", "BlockingIOError", "ChildProcessError",
        "ConnectionError", "BrokenPipeError", "ConnectionAbortedError",
        "ConnectionRefusedError", "ConnectionResetError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", "ReferenceError",
        "RuntimeError", "NotImplementedError", "RecursionError", "SyntaxError",
        "IndentationError", "TabError", "SystemError", "TypeError", "ValueError",
        "UnicodeError", "UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError",
        "Warning", "DeprecationWarning", "PendingDeprecationWarning", "RuntimeWarning",
        "SyntaxWarning", "UserWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", "ExceptionGroup",
        "BaseExceptionGroup", NULL,
    };
    if (!t) return false;
    for (int i = 0; names[i]; i++) if (strcmp(t, names[i]) == 0) return true;
    return false;
}

/* Normalize a scripted raise spec the way the live seam stub + case wrapper
 * do: make_exc(spec) builds a REAL exception (unknown class -> Exception),
 * and exc_spec(e) re-projects it. So the propagated spec carries the
 * effective class name, str(e) as message, name ONLY for ModuleNotFoundError,
 * code ONLY for SystemExit, problem/problem_mark_line when set. Single-arg
 * constructors str() to the message (KeyError's repr-quoting is outside the
 * fixture/fuzz domain). Returns a NEW spec; caller frees. */
static json_t *cfgls_norm_exc_spec(const json_t *spec) {
    const char *t = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *out = json_object();
    if (!strcmp(t, "SystemExit")) {
        const json_t *code = json_obj_get(spec, "code");
        double cv = 1.0;
        char buf[64];
        const char *m;
        if (!code || code->type == JSON_NULL) { m = ""; }
        else if (code->type == JSON_NUMBER) {
            cv = code->num_val;
            if (cv == floor(cv) && fabs(cv) < 1e15) snprintf(buf, sizeof buf, "%.0f", cv);
            else snprintf(buf, sizeof buf, "%g", cv);
            m = buf;
        } else if (code->type == JSON_BOOL) { m = code->bool_val ? "True" : "False"; }
        else if (code->type == JSON_STRING) { m = code->str_val ? code->str_val : ""; }
        else { m = ""; }
        json_set(out, "type", json_string("SystemExit"));
        json_set(out, "message", json_string(m));
        json_set(out, "code", code ? json_copy(code) : json_number(1));
        (void)cv;
    } else {
        const char *eff = (cfgls_builtin_exc(t) || !strcmp(t, "YAMLError")) ? t : "Exception";
        json_set(out, "type", json_string(eff));
        json_set(out, "message", json_string(msg ? msg : ""));
        if (!strcmp(eff, "ModuleNotFoundError")) {
            const json_t *nm = json_obj_get(spec, "name");
            json_set(out, "name", nm ? json_copy(nm) : json_null());
        }
    }
    const json_t *problem = json_obj_get(spec, "problem");
    if (problem && problem->type != JSON_NULL) json_set(out, "problem", json_copy(problem));
    const json_t *pml = json_obj_get(spec, "problem_mark_line");
    if (pml && pml->type != JSON_NULL) json_set(out, "problem_mark_line", json_copy(pml));
    return out;
}

/* seam: trace the attempt, dispatch, free args. An "__untrace__" result (failed
 * in-function import) retracts the trace token so the live silent import matches. */
static json_t *cfgls_seam(cfgls_flow_io_t *io, const char *name, json_t *args) {
    cfgls_trace(io, name);
    json_t *r = cfgls_call(io, name, args);
    if (args) json_free(args);
    if (!r) return json_null();
    if (r->type == JSON_OBJECT && json_obj_get(r, "__untrace__")) {
        if (io && io->trace && io->trace->type == JSON_ARRAY && io->trace->c.count > 0) {
            size_t last = io->trace->c.count - 1;
            json_free(io->trace->c.items[last]);
            io->trace->c.count = last;
        }
        json_t *spec = json_obj_get(r, "__raise__");
        json_t *out = json_object();
        json_set(out, "__raise__", spec ? cfgls_norm_exc_spec(spec) : json_null());
        json_free(r);
        return out;
    }
    if (cfgls_raised(r)) {
        /* The live stub raises make_exc(spec): normalize at the seam boundary
         * so except-matching and the final projection see the real class. */
        json_t *norm = cfgls_norm_exc_spec(cfgls_raise_spec(r));
        json_free(r);
        json_t *out = json_object();
        json_set(out, "__raise__", norm);
        return out;
    }
    return r;
}

/* ── Python-faithful helpers ───────────────────────────────────────────── */
static bool cfgls_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static bool cfgls_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static bool cfgls_json_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!cfgls_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !cfgls_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

static int cfgls_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *cfgls_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && cfgls_is_ws((unsigned char)p[a])) a++;
    while (b > a && cfgls_is_ws((unsigned char)p[b - 1])) b--;
    char *out = malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, p + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *cfgls_lstrip(const char *s) {
    const char *p = s ? s : "";
    size_t a = 0, n = strlen(p);
    while (a < n && cfgls_is_ws((unsigned char)p[a])) a++;
    return strdup(p + a);
}

static char *cfgls_lower(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)p[i]);
    out[n] = '\0';
    return out;
}

static bool cfgls_startswith(const char *s, const char *pre) {
    if (!s || !pre) return false;
    return strncmp(s, pre, strlen(pre)) == 0;
}

static bool cfgls_endswith(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t ns = strlen(s), nf = strlen(suf);
    return ns >= nf && strcmp(s + ns - nf, suf) == 0;
}

static char *cfgls_join(const char *a, const char *b) {
    size_t na = strlen(a ? a : ""), nb = strlen(b ? b : "");
    char *out = malloc(na + nb + 1);
    if (!out) return NULL;
    if (na) memcpy(out, a, na);
    if (nb) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}

/* Path-style join with a single '/' separator (posix fixtures). */
static char *cfgls_path_join(const char *base, const char *name) {
    size_t nb = strlen(base ? base : "");
    bool need_sep = nb > 0 && base[nb - 1] != '/';
    size_t nn = strlen(name ? name : "");
    char *out = malloc(nb + nn + 2);
    if (!out) return NULL;
    if (nb) memcpy(out, base, nb);
    size_t k = nb;
    if (need_sep) out[k++] = '/';
    if (nn) memcpy(out + k, name, nn);
    out[k + nn] = '\0';
    return out;
}

/* Path.parent for a posix path string (PurePosixPath semantics, clean paths). */
static char *cfgls_path_parent(const char *path) {
    const char *p = path ? path : "";
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/') n--;   /* strip trailing slashes (not root) */
    const char *slash = NULL;
    for (size_t i = n; i > 0; i--) if (p[i - 1] == '/') { slash = p + i - 1; break; }
    if (!slash) return strdup(".");
    if (slash == p) return strdup("/");
    size_t plen = (size_t)(slash - p);
    char *out = malloc(plen + 1);
    if (!out) return NULL;
    memcpy(out, p, plen);
    out[plen] = '\0';
    return out;
}

static json_t *cfgls_state_get(cfgls_flow_io_t *io, const char *key) {
    if (!io || !io->state || io->state->type != JSON_OBJECT) return NULL;
    return json_obj_get(io->state, key);
}

static json_t *cfgls_state_obj(cfgls_flow_io_t *io, const char *key) {
    json_t *v = cfgls_state_get(io, key);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

/* OSError subclass family (Python isinstance(exc, OSError)). */
static bool cfgls_exc_oserror(const char *type) {
    static const char *fam[] = {
        "OSError", "PermissionError", "FileNotFoundError", "FileExistsError",
        "BlockingIOError", "ChildProcessError", "ConnectionError", "BrokenPipeError",
        "ConnectionAbortedError", "ConnectionRefusedError", "ConnectionResetError",
        "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "ProcessLookupError", "TimeoutError", NULL,
    };
    if (!type) return false;
    for (int i = 0; fam[i]; i++) if (strcmp(type, fam[i]) == 0) return true;
    return false;
}

static bool cfgls_raise_is(const json_t *r, const char *type) {
    json_t *spec = cfgls_raise_spec(r);
    return spec && strcmp(json_get_str(spec, "type", ""), type) == 0;
}

static bool cfgls_raise_oserror(const json_t *r) {
    json_t *spec = cfgls_raise_spec(r);
    return spec && cfgls_exc_oserror(json_get_str(spec, "type", ""));
}

/* Python type(x).__name__ for a JSON value (integral numbers read as int). */
static const char *cfgls_py_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* FakePath.__truediv__: base = s.rstrip("/") ("" when s == "/"), then
 * base + "/" + name — an EMPTY base still yields "/name". Caller frees. */
static char *cfgls_fakepath_join(const char *s, const char *name) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    if (n == 1 && p[0] == '/') n = 0;
    else while (n > 0 && p[n - 1] == '/') n--;
    size_t nn = strlen(name ? name : "");
    char *out = malloc(n + nn + 2);
    if (!out) return NULL;
    if (n) memcpy(out, p, n);
    out[n] = '/';
    if (nn) memcpy(out + n + 1, name, nn);
    out[n + 1 + nn] = '\0';
    return out;
}

/* (*v, ...) spread into arr: list items / str chars / dict keys; a
 * non-iterable returns a TypeError raise node (caller propagates). */
static json_t *cfgls_spread_into(json_t *arr, const json_t *v) {
    if (v && v->type == JSON_ARRAY) {
        for (size_t i = 0; i < v->c.count; i++) json_append(arr, json_copy(v->c.items[i]));
        return NULL;
    }
    if (v && v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        for (size_t i = 0; s[i]; i++) {
            char ch[2] = { s[i], '\0' };
            json_append(arr, json_string(ch));
        }
        return NULL;
    }
    if (v && v->type == JSON_OBJECT) {
        for (size_t i = 0; i < v->c.count; i++) json_append(arr, json_string(v->c.keys[i]));
        return NULL;
    }
    char m[128];
    snprintf(m, sizeof m, "Value after * must be an iterable, not %s", cfgls_py_type_name(v));
    return cfgls_make_raise("TypeError", m);
}

/* Python str(x) for the scalar JSON domain (f-string operands): None ->
 * "None", bool -> True/False, integral numbers -> decimal, strings verbatim.
 * Caller frees. */
static char *cfgls_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
            else snprintf(buf, sizeof buf, "%g", d);
            return strdup(buf);
        }
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default: return strdup("");
    }
}

/* FailedConfigRead marker: {"__failed_read__": {"data": {...}, "error": spec}} */
static bool cfgls_is_fcr(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__failed_read__") != NULL;
}

static json_t *cfgls_fcr(json_t *data, const json_t *error_spec) {
    json_t *inner = json_object();
    json_set(inner, "data", data ? data : json_object());
    json_set(inner, "error", error_spec ? json_copy(error_spec) : json_null());
    json_t *out = json_object();
    json_set(out, "__failed_read__", inner);
    return out;
}

static json_t *cfgls_fcr_error(const json_t *fcr) {
    json_t *inner = json_obj_get(fcr, "__failed_read__");
    return inner ? json_obj_get(inner, "error") : NULL;
}

/* array membership for a string */
static bool cfgls_arr_has(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *e = arr->c.items[i];
        if (e && e->type == JSON_STRING && strcmp(e->str_val ? e->str_val : "", s) == 0) return true;
    }
    return false;
}

/* ── live pure siblings (transcribed; static, untagged — claims stay put) ─ */

/* cfg_get (config.py:1644) — borrowed nested get, NULL on any miss. */
static json_t *cfgls_cfg_get(const json_t *cfg, const json_t *keys) {
    if (!cfg || cfg->type != JSON_OBJECT) return NULL;
    const json_t *node = cfg;
    if (!keys || keys->type != JSON_ARRAY) return NULL;
    for (size_t i = 0; i < keys->c.count; i++) {
        json_t *k = keys->c.items[i];
        if (!node || node->type != JSON_OBJECT || !k || k->type != JSON_STRING) return NULL;
        node = json_obj_get(node, k->str_val);
        if (!node) return NULL;
    }
    return (json_t *)node;
}

/* _split_key_path (config.py:650) — NEW array of segments; "\." escapes a dot. */
static json_t *cfgls_split_key_path(const char *key) {
    json_t *parts = json_array();
    if (!key) return parts;
    size_t n = strlen(key), i = 0;
    char *cur = malloc(n + 1);
    size_t cl = 0;
    if (!cur) return parts;
    while (i < n) {
        char ch = key[i];
        if (ch == '\\' && i + 1 < n && key[i + 1] == '.') {
            cur[cl++] = '.'; i += 2; continue;
        }
        if (ch == '.') {
            cur[cl] = '\0';
            json_append(parts, json_string(cur));
            cl = 0;
        } else {
            cur[cl++] = ch;
        }
        i++;
    }
    cur[cl] = '\0';
    json_append(parts, json_string(cur));
    free(cur);
    return parts;
}

/* join array elements[from:] with an arbitrary separator */
static char *cfgls_join_arr(const json_t *arr, size_t from, const char *sep) {
    size_t sl = strlen(sep ? sep : "");
    size_t total = 1;
    for (size_t i = from; i < json_len(arr); i++) {
        json_t *s = json_get(arr, i);
        total += (s && s->type == JSON_STRING ? strlen(s->str_val) : 0) + sl;
    }
    char *out = malloc(total);
    if (!out) return NULL;
    size_t k = 0;
    for (size_t i = from; i < json_len(arr); i++) {
        json_t *s = json_get(arr, i);
        const char *t = (s && s->type == JSON_STRING) ? s->str_val : "";
        if (i > from && sl) { memcpy(out + k, sep, sl); k += sl; }
        size_t tl = strlen(t);
        memcpy(out + k, t, tl);
        k += tl;
    }
    out[k] = '\0';
    return out;
}

/* ".".join(segments[from:]) */
static char *cfgls_join_segments(const json_t *segments, size_t from) {
    return cfgls_join_arr(segments, from, ".");
}

/* _deep_merge (config.py:1353) — None over a dict section is IGNORED. */
static json_t *cfgls_deep_merge(const json_t *base, const json_t *override) {
    /* live: base.copy() / override.items() — non-dict operands raise
     * AttributeError (a scripted backup ret of 0/"" must not merge silently). */
    if (!base || base->type != JSON_OBJECT) {
        char m[128];
        snprintf(m, sizeof m, "'%s' object has no attribute 'copy'", cfgls_py_type_name(base));
        return cfgls_make_raise("AttributeError", m);
    }
    if (!override || override->type != JSON_OBJECT) {
        char m[128];
        snprintf(m, sizeof m, "'%s' object has no attribute 'items'", cfgls_py_type_name(override));
        return cfgls_make_raise("AttributeError", m);
    }
    json_t *result = json_copy(base);
    if (!result) return NULL;
    for (size_t i = 0; i < override->c.count; i++) {
        const char *key = override->c.keys[i];
        json_t *val = override->c.items[i];
        json_t *existing = json_obj_get(result, key);
        bool over_dict = existing && existing->type == JSON_OBJECT;
        if (over_dict && val && val->type == JSON_OBJECT) {
            json_set(result, key, cfgls_deep_merge(existing, val));
        } else if (!(over_dict && cfgls_is_null(val))) {
            json_set(result, key, json_copy(val));
        }
    }
    return result;
}

/* _is_non_env_secret_ref (config.py:1434) — ^[a-z][a-z0-9_-]*: */
static bool cfgls_is_non_env_secret_ref(const char *ref) {
    if (!ref || !strchr(ref, ':')) return false;
    const char *p = ref;
    if (!(*p >= 'a' && *p <= 'z')) return false;
    p++;
    while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-') p++;
    return *p == ':';
}

/* _env_ref_var_name (config.py:1439) — malloc'd name or NULL (None). */
static char *cfgls_env_ref_var_name(const char *ref) {
    char *s = cfgls_strip(ref);
    if (!s) return NULL;
    if (cfgls_startswith(s, "env:")) {
        char *tail = cfgls_strip(s + 4);
        free(s);
        if (tail && tail[0]) return tail;
        free(tail);
        return NULL;
    }
    if (cfgls_is_non_env_secret_ref(s)) { free(s); return NULL; }
    return s;   /* may be "" — an empty bare name is NOT None */
}

/* _looks_structured_value (config.py:2922) */
static bool cfgls_looks_structured_value(const char *value) {
    if (!value) return false;
    char *stripped = cfgls_lstrip(value);
    bool lead = stripped && (stripped[0] == '[' || stripped[0] == '{');
    free(stripped);
    if (lead) return true;
    if (!strchr(value, '\n')) return false;
    /* iterate value.splitlines() (\n, \r\n, \r) */
    const char *p = value;
    while (*p) {
        const char *e = p;
        while (*e && *e != '\n' && *e != '\r') e++;
        size_t len = (size_t)(e - p);
        char *line = malloc(len + 1);
        if (!line) return false;
        memcpy(line, p, len); line[len] = '\0';
        char *item = cfgls_strip(line);
        free(line);
        bool hit = false;
        if (item) {
            if (strcmp(item, "-") == 0 || cfgls_startswith(item, "- ")) hit = true;
            if (!hit) {
                char *sep = strstr(item, ": ");
                if (sep) {
                    size_t hl = (size_t)(sep - item);
                    bool head_ok = hl > 0 && item[0] != '#';
                    for (size_t i = 0; head_ok && i < hl; i++) if (item[i] == ' ') head_ok = false;
                    if (head_ok) hit = true;
                }
            }
            if (!hit && cfgls_endswith(item, ":")) {
                size_t il = strlen(item);
                bool tail_ok = il > 1;
                for (size_t i = 0; tail_ok && i + 1 < il; i++) if (item[i] == ' ') tail_ok = false;
                if (tail_ok) hit = true;
            }
        }
        free(item);
        if (hit) return true;
        if (*e == '\r' && e[1] == '\n') e += 2;
        else if (*e) e += 1;
        p = e;
    }
    return false;
}

/* _omitted_config_paths (config.py:1800) — NEW array of dotted paths. */
static void cfgls_omitted_rec(const json_t *existing, const json_t *proposed,
                              const json_t *prefix, json_t *omitted) {
    if (!existing || existing->type != JSON_OBJECT) return;
    for (size_t i = 0; i < existing->c.count; i++) {
        const char *key = existing->c.keys[i];
        json_t *old_value = existing->c.items[i];
        json_t *path = json_copy(prefix);
        json_append(path, json_string(key));
        json_t *new_value = proposed ? json_obj_get(proposed, key) : NULL;
        if (!new_value) {
            json_append(omitted, json_string(cfgls_join_segments(path, 0)));
            json_free(path);
            continue;
        }
        if (old_value && old_value->type == JSON_OBJECT) {
            if (new_value->type == JSON_OBJECT) {
                cfgls_omitted_rec(old_value, new_value, path, omitted);
            } else if (cfgls_truthy(old_value)) {
                json_append(omitted, json_string(cfgls_join_segments(path, 0)));
            }
        }
        json_free(path);
    }
}

static json_t *cfgls_omitted_config_paths(const json_t *existing, const json_t *proposed) {
    /* live: for key, old_value in existing.items() — a non-dict existing
     * (e.g. an unscripted None guard read) raises AttributeError. */
    if (!existing || existing->type != JSON_OBJECT) {
        char m[128];
        snprintf(m, sizeof m, "'%s' object has no attribute 'items'", cfgls_py_type_name(existing));
        return cfgls_make_raise("AttributeError", m);
    }
    json_t *omitted = json_array();
    cfgls_omitted_rec(existing, proposed, json_array(), omitted);
    return omitted;
}

/* _strip_dotted_keys (config.py:1371) — mutates cfg; *out_stripped NEW sorted array. */
static int cfgls_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void cfgls_strip_dotted_keys(json_t *cfg, const json_t *dotted_keys, json_t **out_stripped) {
    json_t *stripped = json_array();
    if (dotted_keys && dotted_keys->type == JSON_ARRAY) {
        for (size_t i = 0; i < dotted_keys->c.count; i++) {
            json_t *d = dotted_keys->c.items[i];
            if (!d || d->type != JSON_STRING) continue;
            const char *dotted = d->str_val;
            /* dotted.split(".") — plain split, last segment is the leaf */
            const char *last_dot = strrchr(dotted, '.');
            json_t *parents = json_array();
            json_t *node = cfg;
            if (last_dot) {
                char *par = malloc((size_t)(last_dot - dotted) + 1);
                memcpy(par, dotted, (size_t)(last_dot - dotted));
                par[(size_t)(last_dot - dotted)] = '\0';
                /* split parents on '.' */
                char *tok = par;
                for (char *q = par; ; q++) {
                    if (*q == '.' || *q == '\0') {
                        bool end = (*q == '\0');
                        *q = '\0';
                        json_append(parents, json_string(tok));
                        if (end) break;
                        tok = q + 1;
                    }
                }
                free(par);
                node = cfgls_cfg_get(cfg, parents);
            }
            const char *leaf = last_dot ? last_dot + 1 : dotted;
            if (node && node->type == JSON_OBJECT && json_obj_get(node, leaf)) {
                json_obj_del(node, leaf);
                json_append(stripped, json_string(dotted));
            }
            json_free(parents);
        }
    }
    /* sorted(set(stripped)) — keys are unique per input set */
    size_t n = json_len(stripped);
    if (n > 1) {
        const char **tmp = malloc(n * sizeof(char *));
        for (size_t i = 0; i < n; i++) tmp[i] = json_get(stripped, i)->str_val;
        qsort(tmp, n, sizeof(char *), cfgls_cmp_str);
        json_t *sorted = json_array();
        for (size_t i = 0; i < n; i++) json_append(sorted, json_string(tmp[i]));
        free(tmp);
        json_free(stripped);
        stripped = sorted;
    }
    *out_stripped = stripped;
}

/* _known_top_level_keys (config.py:2820) — set(DEFAULT_CONFIG) | EXTRA | OPEN_SUBKEY */
static json_t *cfgls_known_top_level_keys(cfgls_flow_io_t *io) {
    json_t *known = json_array();
    json_t *dc = cfgls_state_obj(io, "DEFAULT_CONFIG");
    if (dc) for (size_t i = 0; i < dc->c.count; i++) json_append(known, json_string(dc->c.keys[i]));
    json_t *extra = cfgls_state_get(io, "_EXTRA_KNOWN_ROOT_KEYS");
    if (extra && extra->type == JSON_ARRAY)
        for (size_t i = 0; i < extra->c.count; i++) json_append(known, json_copy(extra->c.items[i]));
    json_t *open = cfgls_state_get(io, "_OPEN_SUBKEY_TOP_LEVEL_KEYS");
    if (open && open->type == JSON_ARRAY)
        for (size_t i = 0; i < open->c.count; i++) json_append(known, json_copy(open->c.items[i]));
    return known;
}

/* ── exception-message builders (config_read_errors.py, transcribed) ───── */

/* " ".join(text.split()) — collapse whitespace runs to single spaces, trim. */
static char *cfgls_collapse_ws(const char *text) {
    const char *p = text ? text : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t k = 0;
    bool in_ws = true;   /* leading trim */
    for (size_t i = 0; i < n; i++) {
        if (cfgls_is_ws((unsigned char)p[i])) { in_ws = true; continue; }
        if (in_ws && k > 0) out[k++] = ' ';
        out[k++] = p[i];
        in_ws = false;
    }
    out[k] = '\0';
    return out;
}

/* _yaml_error_details(exc): problem if truthy else str(exc).strip(), collapsed. */
static char *cfgls_yaml_error_details(const json_t *spec) {
    const char *problem = spec ? json_get_str(spec, "problem", NULL) : NULL;
    const char *text;
    char *stripped = NULL;
    if (problem && problem[0]) {
        text = problem;
    } else {
        stripped = cfgls_strip(spec ? json_get_str(spec, "message", "") : "");
        text = stripped ? stripped : "";
    }
    char *out = cfgls_collapse_ws(text);
    free(stripped);
    return out;
}

/* _yaml_error_location(exc): "line N" from problem_mark.line (0-based) else "". */
static char *cfgls_yaml_error_location(const json_t *spec) {
    json_t *mark = spec ? json_obj_get(spec, "problem_mark_line") : NULL;
    if (mark && mark->type == JSON_NUMBER) {
        char buf[48];
        snprintf(buf, sizeof buf, "line %.0f", mark->num_val + 1);
        return strdup(buf);
    }
    return strdup("");
}

/* _refuse_overwrite(config_path, reason, exc, fix) -> RuntimeError message. */
static char *cfgls_refuse_overwrite_msg(const char *config_path, const char *reason,
                                        const json_t *exc_spec, const char *fix) {
    char *where = cfgls_yaml_error_location(exc_spec);
    char *details = cfgls_yaml_error_details(exc_spec);
    size_t cap = strlen(config_path) + strlen(reason) + strlen(where) + strlen(fix) + strlen(details) + 96;
    char *out = malloc(cap);
    if (out) {
        if (where[0])
            snprintf(out, cap, "Your settings file (%s) %s (%s), so this change was not saved. %s Details: %s",
                     config_path, reason, where, fix, details);
        else
            snprintf(out, cap, "Your settings file (%s) %s, so this change was not saved. %s Details: %s",
                     config_path, reason, fix, details);
    }
    free(where);
    free(details);
    return out;
}

/* _FIX_YAML.format(backups=...) */
static char *cfgls_fix_yaml(const char *backups) {
    return cfgls_join(CFGLS_FIX_YAML_PRE, cfgls_join(backups, CFGLS_FIX_YAML_POST));
}

/* first line of a string (splitlines()[0]); "" when empty. */
static char *cfgls_first_line(const char *s) {
    const char *p = s ? s : "";
    size_t n = 0;
    while (p[n] && p[n] != '\n' && p[n] != '\r') n++;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

/* ── forward declarations (mutual recursion) ───────────────────────────── */
json_t *cfgls_env_ref_lookup(cfgls_flow_io_t *io, const char *name);
json_t *cfgls_expand_env_vars_flow(cfgls_flow_io_t *io, const json_t *obj);
json_t *cfgls_merge_managed_overlay_flow(cfgls_flow_io_t *io, const json_t *expanded);
json_t *cfgls_validate_config_key(cfgls_flow_io_t *io, const char *key);
json_t *cfgls_validate_schema_key_flow(cfgls_flow_io_t *io, const char *key);
json_t *cfgls_write_config_state_flow(cfgls_flow_io_t *io, const char *config_path,
                                      const json_t *data, bool allow_omissions,
                                      const char *extra_content_on_create);

/* ===================================================================
 * _worker_config_snapshot  (config.py:1674)
 * =================================================================== */
/* PoP: cfgls_worker_config_snapshot @ hermes_cli/config.py:_worker_config_snapshot */
json_t *cfgls_worker_config_snapshot(cfgls_flow_io_t *io) {
    json_t *r = cfgls_seam(io, "worker_config_snapshot", NULL);
    if (cfgls_raised(r)) {
        json_t *spec = cfgls_raise_spec(r);
        const char *type = json_get_str(spec, "type", "");
        if (strcmp(type, "ModuleNotFoundError") == 0) {
            const char *nm = json_get_str(spec, "name", "");
            /* if exc.name != "agent" and not "agent".startswith(f"{exc.name}."): raise */
            char *nm_dot = cfgls_join(nm, ".");
            bool rethrow = (strcmp(nm, "agent") != 0) && !cfgls_startswith("agent", nm_dot);
            free(nm_dot);
            if (rethrow) return r;
            json_free(r);
            return json_null();
        }
        return r;   /* only ModuleNotFoundError is caught */
    }
    return r;   /* snapshot dict or None */
}

/* ===================================================================
 * _raw_config_cache_hit  (config.py:1658)
 * =================================================================== */
/* PoP: cfgls_raw_config_cache_hit @ hermes_cli/config.py:_raw_config_cache_hit */
json_t *cfgls_raw_config_cache_hit(cfgls_flow_io_t *io, const char *path_key, const json_t *cache_key) {
    json_t *cache = cfgls_state_obj(io, "_RAW_CONFIG_CACHE");
    json_t *cached = cache ? json_obj_get(cache, path_key) : NULL;
    if (cached && cached->type != JSON_NULL) {
        /* live: cached[:len(cache_key)] — len() first (None/bool/number have
         * no len), then the slice (a non-list cached value is not subscriptable). */
        if (!cache_key || cache_key->type == JSON_NULL || cache_key->type == JSON_BOOL
                || cache_key->type == JSON_NUMBER) {
            char m[128];
            snprintf(m, sizeof m, "object of type '%s' has no len()", cfgls_py_type_name(cache_key));
            return cfgls_make_raise("TypeError", m);
        }
        if (cached->type != JSON_ARRAY) {
            char m[128];
            snprintf(m, sizeof m, "'%s' object is not subscriptable", cfgls_py_type_name(cached));
            return cfgls_make_raise("TypeError", m);
        }
        if (cache_key->type == JSON_ARRAY) {
            size_t n = json_len(cache_key);
            if (json_len(cached) >= n) {
                bool eq = true;
                for (size_t i = 0; i < n && eq; i++)
                    eq = cfgls_json_eq(json_get(cached, i), json_get(cache_key, i));
                if (eq) {
                    json_t *data = json_get(cached, n);
                    return data ? json_copy(data) : json_null();
                }
            }
        }
        /* str/dict cache_key: len() works but list == str/dict is False. */
    }
    return json_null();
}

/* ===================================================================
 * _raw_config_cache_store  (config.py:1668)
 * =================================================================== */
/* PoP: cfgls_raw_config_cache_store @ hermes_cli/config.py:_raw_config_cache_store */
json_t *cfgls_raw_config_cache_store(cfgls_flow_io_t *io, const char *path_key,
                                     const json_t *cache_key, const json_t *data) {
    /* live: (*cache_key, data) — a non-iterable signature raises TypeError;
     * str spreads characters, dict spreads keys. */
    if (!cache_key || cache_key->type == JSON_NULL || cache_key->type == JSON_BOOL
            || cache_key->type == JSON_NUMBER) {
        char m[128];
        snprintf(m, sizeof m, "Value after * must be an iterable, not %s", cfgls_py_type_name(cache_key));
        return cfgls_make_raise("TypeError", m);
    }
    json_t *entry = json_array();
    if (cache_key->type == JSON_ARRAY) {
        for (size_t i = 0; i < cache_key->c.count; i++) json_append(entry, json_copy(cache_key->c.items[i]));
    } else if (cache_key->type == JSON_STRING) {
        const char *s = cache_key->str_val ? cache_key->str_val : "";
        for (size_t i = 0; s[i]; i++) {
            char ch[2] = { s[i], '\0' };
            json_append(entry, json_string(ch));
        }
    } else if (cache_key->type == JSON_OBJECT) {
        for (size_t i = 0; i < cache_key->c.count; i++) json_append(entry, json_string(cache_key->c.keys[i]));
    }
    json_append(entry, data ? json_copy(data) : json_null());
    json_t *cache = cfgls_state_obj(io, "_RAW_CONFIG_CACHE");
    if (cache) json_set(cache, path_key, entry);
    else json_free(entry);
    return json_null();
}

/* ===================================================================
 * _read_raw_config_impl  (config.py:1687)  — FLOW
 * =================================================================== */
/* PoP: cfgls_read_raw_config_impl_flow @ hermes_cli/config.py:_read_raw_config_impl */
json_t *cfgls_read_raw_config_impl_flow(cfgls_flow_io_t *io, bool want_deepcopy) {
    (void)want_deepcopy;   /* deepcopy vs alias is value-identical in the JSON model */
    json_t *snapshot = cfgls_worker_config_snapshot(io);
    if (cfgls_raised(snapshot)) return snapshot;
    if (!cfgls_is_null(snapshot)) return snapshot;
    json_free(snapshot);

    /* lock-free fast path; any exception falls through to the locked re-check */
    json_t *cp = cfgls_seam(io, "get_config_path", NULL);
    if (!cfgls_raised(cp)) {
        json_t *args = json_object();
        json_set(args, "path", json_copy(cp));
        json_t *ck = cfgls_seam(io, "config_cache_version", args);
        if (!cfgls_raised(ck)) {
            char *pk = cfgls_py_str(cp); /* live: str(config_path) */
            json_t *hit = cfgls_raw_config_cache_hit(io, pk, ck);
            free(pk);
            if (cfgls_raised(hit)) {
                json_free(hit); /* fast path: except Exception: pass */
            } else if (!cfgls_is_null(hit)) {
                json_free(ck);
                json_free(cp);
                return hit;
            } else {
                json_free(hit);
            }
        }
        json_free(ck);
    }
    json_free(cp);

    /* locked section */
    cfgls_trace(io, "lock.acquire");
    json_t *cp2 = cfgls_seam(io, "get_config_path", NULL);
    if (cfgls_raised(cp2)) { cfgls_trace(io, "lock.release"); return cp2; }
    char *path_key = cfgls_py_str(cp2); /* live: str(config_path) */
    json_t *args2 = json_object();
    json_set(args2, "path", json_copy(cp2));
    json_t *ck2 = cfgls_seam(io, "config_cache_version", args2);
    if (cfgls_raised(ck2)) {
        cfgls_trace(io, "lock.release");
        if (cfgls_raise_is(ck2, "FileNotFoundError")) { json_free(ck2); json_free(cp2); return json_object(); }
        if (cfgls_raise_oserror(ck2)) {
            json_t *spec = json_copy(cfgls_raise_spec(ck2));
            json_free(ck2); json_free(cp2);
            return cfgls_fcr(json_object(), spec);
        }
        json_free(cp2);
        return ck2;
    }
    json_t *hit2 = cfgls_raw_config_cache_hit(io, path_key, ck2);
    if (cfgls_raised(hit2)) {
        cfgls_trace(io, "lock.release");
        json_free(ck2); json_free(cp2); free(path_key);
        return hit2;
    }
    if (!cfgls_is_null(hit2)) {
        cfgls_trace(io, "lock.release");
        json_free(ck2); json_free(cp2); free(path_key);
        return hit2;
    }
    json_free(hit2);

    json_t *args3 = json_object();
    json_set(args3, "path", json_copy(cp2));
    json_t *doc = cfgls_seam(io, "read_config_doc", args3);
    if (cfgls_raised(doc)) {
        json_t *spec = json_copy(cfgls_raise_spec(doc));
        json_t *wargs = json_object();
        json_set(wargs, "path", json_string(path_key));
        json_set(wargs, "error", json_copy(spec));
        json_set(wargs, "fallback", json_string("defaults"));
        json_t *w = cfgls_seam(io, "_warn_config_parse_failure", wargs);
        json_free(w);
        cfgls_trace(io, "lock.release");
        json_free(doc); json_free(ck2); json_free(cp2);
        return cfgls_fcr(json_object(), spec);
    }
    json_t *data = cfgls_truthy(doc) ? doc : json_object();
    if (cfgls_truthy(doc) == false) json_free(doc);
    if (data->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "top-level YAML must be a mapping, got %s", cfgls_py_type_name(data));
        json_t *spec = json_object();
        json_set(spec, "type", json_string("TypeError"));
        json_set(spec, "message", json_string(msg));
        cfgls_trace(io, "lock.release");
        json_free(data); json_free(ck2); json_free(cp2);
        return cfgls_fcr(json_object(), spec);
    }
    /* the file reads now — drop any transient failure record */
    json_t *pf = cfgls_state_obj(io, "_CONFIG_PARSE_FAILURES");
    if (pf) json_obj_del(pf, path_key);
    json_t *cached_copy = json_copy(data);
    json_t *stored = cfgls_raw_config_cache_store(io, path_key, ck2, cached_copy);
    json_free(cached_copy);
    if (cfgls_raised(stored)) {
        cfgls_trace(io, "lock.release");
        json_free(data); json_free(ck2); json_free(cp2); free(path_key);
        return stored;
    }
    json_free(stored);
    cfgls_trace(io, "lock.release");
    json_free(ck2); json_free(cp2); free(path_key);
    return data;   /* want_deepcopy ? data : cached_copy — value-equal */
}

/* ===================================================================
 * _load_config_cache_sig  (config.py:2007)
 * =================================================================== */
static json_t *cfgls_zero_sig(void) {
    json_t *z = json_array();
    json_append(z, json_number(0));
    json_append(z, json_number(0));
    json_append(z, json_number(0));
    json_append(z, json_number(0));
    json_append(z, json_string(""));
    return z;
}

/* PoP: cfgls_load_config_cache_sig @ hermes_cli/config.py:_load_config_cache_sig */
json_t *cfgls_load_config_cache_sig(cfgls_flow_io_t *io, const char *config_path) {
    json_t *user_sig = NULL;
    json_t *args = json_object();
    json_set(args, "path", json_string(config_path));
    json_t *ck = cfgls_seam(io, "config_cache_version", args);
    if (cfgls_raised(ck)) {
        if (cfgls_raise_is(ck, "FileNotFoundError")) { json_free(ck); user_sig = json_null(); }
        else return ck;
    } else {
        user_sig = ck;
    }

    json_t *md = cfgls_seam(io, "get_managed_dir", NULL);
    if (cfgls_raised(md)) { json_free(user_sig); return md; }
    json_t *managed_sig = NULL;
    /* get_managed_dir returns FakePath(str): ANY string (even "") is a truthy
     * object; only a falsy non-str (None/0/""-less) skips the managed probe. */
    if (!cfgls_is_null(md) && (cfgls_truthy(md) || md->type == JSON_STRING)) {
        if (md->type != JSON_STRING) {
            char m[128];
            snprintf(m, sizeof m, "unsupported operand type(s) for /: '%s' and 'str'",
                     cfgls_py_type_name(md));
            json_free(user_sig); json_free(md);
            return cfgls_make_raise("TypeError", m);
        }
        char *mpath = cfgls_fakepath_join(md->str_val, "config.yaml");
        json_t *fargs = json_object();
        json_set(fargs, "path", json_string(mpath));
        free(mpath);
        json_t *fv = cfgls_seam(io, "file_cache_version", fargs);
        if (cfgls_raised(fv)) {
            if (cfgls_raise_oserror(fv)) { json_free(fv); managed_sig = cfgls_zero_sig(); }
            else { json_free(user_sig); json_free(md); return fv; }
        } else {
            managed_sig = fv;
        }
    } else {
        managed_sig = cfgls_zero_sig();
    }
    json_free(md);

    json_t *zero = cfgls_zero_sig();
    if (cfgls_is_null(user_sig) && cfgls_json_eq(managed_sig, zero)) {
        json_free(zero); json_free(user_sig); json_free(managed_sig);
        json_t *out = json_array();
        json_append(out, json_null());
        json_append(out, json_null());
        return out;
    }
    json_free(zero);
    /* cache_sig = (*(user_sig or zero), *managed_sig) — a non-iterable
     * managed_sig (e.g. an unscripted None) raises TypeError HERE. */
    json_t *base = (!cfgls_is_null(user_sig) && cfgls_truthy(user_sig)) ? user_sig : cfgls_zero_sig();
    json_t *cache_sig = json_array();
    json_t *spr = cfgls_spread_into(cache_sig, base);
    if (base != user_sig) json_free(base);
    if (!spr) spr = cfgls_spread_into(cache_sig, managed_sig);
    if (spr) {
        json_free(cache_sig); json_free(user_sig); json_free(managed_sig);
        return spr;
    }
    json_t *out = json_array();
    json_append(out, user_sig ? user_sig : json_null());
    json_append(out, cache_sig);
    json_free(managed_sig);
    return out;
}

/* ===================================================================
 * _env_ref_lookup  (config.py:1388)
 * =================================================================== */
/* PoP: cfgls_env_ref_lookup @ hermes_cli/config.py:_env_ref_lookup */
json_t *cfgls_env_ref_lookup(cfgls_flow_io_t *io, const char *name) {
    json_t *scope = cfgls_seam(io, "current_secret_scope", NULL);
    if (cfgls_raised(scope)) {
        /* except Exception: the import failed -> plain os.environ */
        json_free(scope);
        json_t *args = json_object();
        json_set(args, "name", json_string(name));
        return cfgls_seam(io, "os.environ.get", args);
    }
    if (cfgls_is_null(scope)) {
        json_free(scope);
        json_t *args = json_object();
        json_set(args, "name", json_string(name));
        return cfgls_seam(io, "os.environ.get", args);
    }
    json_free(scope);
    json_t *args = json_object();
    json_set(args, "name", json_string(name));
    return cfgls_seam(io, "get_secret", args);
}

/* ===================================================================
 * _load_config_cache_hit  (config.py:2084)  — FLOW
 * =================================================================== */
/* PoP: cfgls_load_config_cache_hit_flow @ hermes_cli/config.py:_load_config_cache_hit */
json_t *cfgls_load_config_cache_hit_flow(cfgls_flow_io_t *io, const char *path_key, const json_t *cache_sig) {
    json_t *cache = cfgls_state_obj(io, "_LOAD_CONFIG_CACHE");
    json_t *cached = cache ? json_obj_get(cache, path_key) : NULL;
    if (!cached || cfgls_is_null(cache_sig)) return json_null();
    if (cached->type != JSON_ARRAY || cache_sig->type != JSON_ARRAY) return json_null();
    size_t n = json_len(cache_sig);
    if (json_len(cached) < n) return json_null();
    for (size_t i = 0; i < n; i++)
        if (!cfgls_json_eq(json_get(cached, i), json_get(cache_sig, i))) return json_null();
    json_t *hit = json_get(cached, n);
    if (cfgls_is_fcr(hit)) {
        json_t *err = cfgls_fcr_error(hit);
        const char *etype = err ? json_get_str(err, "type", "") : "";
        if (cfgls_exc_oserror(etype)) {
            json_t *args = json_object();
            json_set(args, "path", json_string(path_key));
            json_t *pr = cfgls_seam(io, "probe_config_readable", args);
            if (cfgls_raised(pr)) {
                if (cfgls_raise_oserror(pr)) { json_free(pr); return json_copy(hit); }
                return pr;   /* non-OSError propagates */
            }
            json_free(pr);
            return json_null();
        }
    }
    json_t *snap = (json_len(cached) > n + 1) ? json_get(cached, n + 1) : NULL;
    if (snap && snap->type == JSON_OBJECT) {
        for (size_t i = 0; i < snap->c.count; i++) {
            const char *k = snap->c.keys[i];
            json_t *v = snap->c.items[i];
            json_t *cur = cfgls_env_ref_lookup(io, k);
            if (cfgls_raised(cur)) return cur;
            bool eq = cfgls_json_eq(cur, v);
            json_free(cur);
            if (!eq) return json_null();
        }
    }
    return hit ? json_copy(hit) : json_null();
}

/* ===================================================================
 * _expand_env_vars  (config.py:1449)  — FLOW
 * =================================================================== */
/* expand one ${...} match; returns malloc'd replacement, or NULL with *err set. */
static char *cfgls_expand_match(cfgls_flow_io_t *io, const char *raw, const char *inner_raw, json_t **err) {
    char *inner = cfgls_strip(inner_raw);
    char *name = cfgls_env_ref_var_name(inner);
    if (!name) {
        if (!cfgls_startswith(inner, "env:") && cfgls_is_non_env_secret_ref(inner)) {
            json_t *w = cfgls_seam(io, "logger.warning", NULL);
            json_free(w);
        }
        free(inner);
        return strdup(raw);
    }
    json_t *val = cfgls_env_ref_lookup(io, name);
    if (cfgls_raised(val)) { *err = val; free(inner); free(name); return NULL; }
    if (!cfgls_is_null(val)) {
        const char *vs = (val->type == JSON_STRING) ? val->str_val : "";
        char *out = strdup(vs);
        json_free(val); free(inner); free(name);
        return out;
    }
    json_free(val);
    if (cfgls_startswith(inner, "env:")) {
        json_t *w = cfgls_seam(io, "logger.warning", NULL);
        json_free(w);
    }
    free(inner); free(name);
    return strdup(raw);
}

/* recursive expansion; returns a NEW value or a propagated raise sentinel. */
static json_t *cfgls_expand_rec(cfgls_flow_io_t *io, const json_t *obj) {
    if (!obj) return json_null();
    if (obj->type == JSON_STRING) {
        const char *s = obj->str_val ? obj->str_val : "";
        size_t n = strlen(s);
        char *out = malloc(n + 1);
        size_t k = 0;
        size_t i = 0;
        while (i < n) {
            if (s[i] == '$' && i + 1 < n && s[i + 1] == '{') {
                size_t j = i + 2;
                while (j < n && s[j] != '}') j++;
                if (j < n && j > i + 2) {   /* [^}]+ — at least one inner char */
                    size_t raw_len = j - i + 1;
                    char *raw = malloc(raw_len + 1);
                    memcpy(raw, s + i, raw_len); raw[raw_len] = '\0';
                    size_t inner_len = j - (i + 2);
                    char *inner = malloc(inner_len + 1);
                    memcpy(inner, s + i + 2, inner_len); inner[inner_len] = '\0';
                    json_t *err = NULL;
                    char *rep = cfgls_expand_match(io, raw, inner, &err);
                    free(raw); free(inner);
                    if (!rep) {
                        free(out);
                        return err ? err : json_null();
                    }
                    size_t rl = strlen(rep);
                    /* keep room for the ENTIRE remaining literal tail, not just
                     * the replacement — the old k+rl+1 size overflowed on the
                     * next out[k++] = s[i++] write (multi-${} templates). */
                    size_t remaining = n - (j + 1);
                    char *grown = realloc(out, k + rl + remaining + 1);
                    if (!grown) { free(rep); free(out); return json_null(); }
                    out = grown;
                    memcpy(out + k, rep, rl); k += rl;
                    free(rep);
                    i = j + 1;
                    continue;
                }
            }
            out[k++] = s[i++];
        }
        out[k] = '\0';
        json_t *res = json_string(out);
        free(out);
        return res;
    }
    if (obj->type == JSON_OBJECT) {
        json_t *res = json_object();
        for (size_t i = 0; i < obj->c.count; i++) {
            json_t *v = cfgls_expand_rec(io, obj->c.items[i]);
            if (cfgls_raised(v)) { json_free(res); return v; }
            json_set(res, obj->c.keys[i], v);
        }
        return res;
    }
    if (obj->type == JSON_ARRAY) {
        json_t *res = json_array();
        for (size_t i = 0; i < obj->c.count; i++) {
            json_t *v = cfgls_expand_rec(io, obj->c.items[i]);
            if (cfgls_raised(v)) { json_free(res); return v; }
            json_append(res, v);
        }
        return res;
    }
    return json_copy(obj);
}

/* PoP: cfgls_expand_env_vars_flow @ hermes_cli/config.py:_expand_env_vars */
json_t *cfgls_expand_env_vars_flow(cfgls_flow_io_t *io, const json_t *obj) {
    return cfgls_expand_rec(io, obj);
}

/* ===================================================================
 * _merge_managed_overlay  (config.py:2066)  — FLOW
 * =================================================================== */
/* PoP: cfgls_merge_managed_overlay_flow @ hermes_cli/config.py:_merge_managed_overlay */
json_t *cfgls_merge_managed_overlay_flow(cfgls_flow_io_t *io, const json_t *expanded) {
    json_t *mc = cfgls_seam(io, "load_managed_config", NULL);
    if (cfgls_raised(mc)) return mc;
    if (!cfgls_truthy(mc)) {
        json_t *out = json_array();
        json_append(out, expanded ? json_copy(expanded) : json_null());
        json_append(out, mc);
        return out;
    }
    json_t *nargs = json_object();
    json_set(nargs, "config", json_copy(mc));
    json_t *mn = cfgls_seam(io, "_normalize_root_model_keys", nargs);
    if (cfgls_raised(mn)) { json_free(mc); return mn; }
    if (!mn || mn->type != JSON_OBJECT) { json_free(mn); mn = json_object(); }
    json_t *model = json_obj_get(mn, "model");
    if (model && model->type == JSON_STRING) {
        json_t *copy_mn = json_copy(mn);
        json_t *nm = json_object();
        json_set(nm, "default", json_copy(model));
        json_set(copy_mn, "model", nm);
        json_free(mn);
        mn = copy_mn;
    }
    json_t *exp_mn = cfgls_expand_env_vars_flow(io, mn);
    if (cfgls_raised(exp_mn)) { json_free(mn); json_free(mc); return exp_mn; }
    json_t *merged = cfgls_deep_merge(expanded, exp_mn);
    json_free(exp_mn);
    json_free(mn);
    if (cfgls_raised(merged)) { json_free(mc); return merged; }
    json_t *out = json_array();
    json_append(out, merged);
    json_append(out, mc);
    return out;
}

/* ===================================================================
 * _last_known_good_fallback  (config.py:2025)  — FLOW
 * =================================================================== */
/* PoP: cfgls_last_known_good_fallback_flow @ hermes_cli/config.py:_last_known_good_fallback */
json_t *cfgls_last_known_good_fallback_flow(cfgls_flow_io_t *io, const char *config_path,
                                            const char *path_key, const json_t *cache_sig,
                                            const json_t *exc_spec) {
    /* drive passes make_exc(inp["exc"]): a REAL exception (unknown class ->
     * Exception; absent exc -> RuntimeError("")). Normalize once. */
    json_t *nexc = cfgls_norm_exc_spec(exc_spec);
    json_t *lkg_src = cfgls_state_obj(io, "_LAST_EXPANDED_CONFIG_BY_PATH");
    json_t *lkg = lkg_src ? json_obj_get(lkg_src, path_key) : NULL;
    lkg = lkg ? json_copy(lkg) : NULL;
    const char *fallback = "last-known-good";

    if (cfgls_is_null(lkg)) {
        json_free(lkg); lkg = NULL;
        json_t *bargs = json_object();
        json_set(bargs, "path", json_string(config_path));
        json_t *raw_good = cfgls_seam(io, "load_newest_good_backup", bargs);
        if (cfgls_raised(raw_good)) return raw_good;
        if (!cfgls_is_null(raw_good)) {
            json_t *dc = cfgls_state_obj(io, "DEFAULT_CONFIG");
            json_t *merged_good = cfgls_deep_merge(dc, raw_good);
            if (cfgls_raised(merged_good)) { json_free(raw_good); return merged_good; }
            /* apply_user_moa_presets(merged_good, raw_good) — scripted in-place mutation */
            json_t *pargs = json_object();
            json_set(pargs, "merged", json_copy(merged_good));
            json_set(pargs, "raw", json_copy(raw_good));
            json_t *pr = cfgls_seam(io, "apply_user_moa_presets", pargs);
            if (cfgls_raised(pr)) { json_free(merged_good); json_free(raw_good); return pr; }
            if (pr && pr->type == JSON_OBJECT) { json_free(merged_good); merged_good = pr; }
            else json_free(pr);
            json_t *cargs = json_object();
            json_set(cargs, "config", json_copy(merged_good));
            json_t *normalized = cfgls_seam(io, "_canonicalize_config", cargs);
            if (cfgls_raised(normalized)) { json_free(merged_good); json_free(raw_good); return normalized; }
            if (!normalized || normalized->type != JSON_OBJECT) { json_free(normalized); normalized = json_object(); }
            json_t *expanded_good = cfgls_expand_env_vars_flow(io, normalized);
            if (cfgls_raised(expanded_good)) { json_free(normalized); json_free(merged_good); json_free(raw_good); return expanded_good; }
            json_t *ov = cfgls_merge_managed_overlay_flow(io, expanded_good);
            if (cfgls_raised(ov)) { json_free(expanded_good); json_free(normalized); json_free(merged_good); json_free(raw_good); return ov; }
            json_t *new_lkg = json_get(ov, 0);
            lkg = new_lkg ? json_copy(new_lkg) : NULL;
            json_free(ov);
            json_free(expanded_good);
            json_free(normalized);
            json_free(merged_good);
            fallback = "last-known-good-backup";
        }
        json_free(raw_good);
    }

    const char *fb = !cfgls_is_null(lkg) ? fallback : "defaults";
    json_t *wargs = json_object();
    json_set(wargs, "path", json_string(config_path));
    json_set(wargs, "error", json_copy(nexc));
    json_set(wargs, "fallback", json_string(fb));
    json_t *w = cfgls_seam(io, "_warn_config_parse_failure", wargs);
    json_free(w);

    if (cfgls_is_null(lkg)) { json_free(lkg); json_free(nexc); return json_null(); }

    json_t *lkg_exp = cfgls_expand_env_vars_flow(io, lkg);
    if (cfgls_raised(lkg_exp)) { json_free(lkg); json_free(nexc); return lkg_exp; }
    json_t *lkg_copy = cfgls_fcr(lkg_exp, nexc);
    json_free(nexc);
    if (!cfgls_is_null(cache_sig) && cache_sig->type == JSON_ARRAY) {
        json_t *entry = json_array();
        for (size_t i = 0; i < cache_sig->c.count; i++) json_append(entry, json_copy(cache_sig->c.items[i]));
        json_append(entry, json_copy(lkg_copy));
        json_append(entry, json_object());
        json_t *cache = cfgls_state_obj(io, "_LOAD_CONFIG_CACHE");
        if (cache) json_set(cache, path_key, entry);
        else json_free(entry);
    }
    json_free(lkg);
    return lkg_copy;
}

/* ===================================================================
 * _write_config_state  (config.py:1825)  — FLOW
 * =================================================================== */
/* _refuse_failed_read(config_path, data) — NULL when ok, raise sentinel otherwise. */
static json_t *cfgls_refuse_failed_read(cfgls_flow_io_t *io, const char *config_path, const json_t *data) {
    if (!cfgls_is_fcr(data)) return NULL;
    json_t *spec = cfgls_fcr_error(data);
    const char *type = spec ? json_get_str(spec, "type", "") : "";
    if (cfgls_exc_oserror(type)) {
        const char *fix = (strcmp(type, "PermissionError") == 0) ? CFGLS_FIX_PERMS : CFGLS_FIX_READ_RETRY;
        char *msg = cfgls_refuse_overwrite_msg(config_path, "could not be read", spec, fix);
        json_t *r = cfgls_make_raise("RuntimeError", msg);
        free(msg);
        return r;
    }
    if (strcmp(type, "SystemExit") == 0) {
        json_t *code = spec ? json_obj_get(spec, "code") : NULL;
        char codebuf[64];
        if (code && code->type == JSON_NUMBER) snprintf(codebuf, sizeof codebuf, "%.0f", code->num_val);
        else if (code && code->type == JSON_STRING) snprintf(codebuf, sizeof codebuf, "%s", code->str_val);
        else snprintf(codebuf, sizeof codebuf, "None");
        char *msg = cfgls_join("The config could not be read, so this change was not saved: ", codebuf);
        json_t *r = cfgls_make_raise("RuntimeError", msg);
        free(msg);
        return r;
    }
    json_t *dh = cfgls_seam(io, "display_hermes_home", NULL);
    if (cfgls_raised(dh)) return dh;
    /* f"{display_hermes_home()}/backups/config/" — str(None) is "None". */
    char *dhs = cfgls_py_str(dh);
    char *backups = cfgls_join(dhs, "/backups/config/");
    free(dhs);
    json_free(dh);
    char *fix = cfgls_fix_yaml(backups);
    free(backups);
    char *msg = cfgls_refuse_overwrite_msg(config_path, "has a formatting error", spec, fix);
    free(fix);
    json_t *r = cfgls_make_raise("RuntimeError", msg);
    free(msg);
    return r;
}

/* PoP: cfgls_write_config_state_flow @ hermes_cli/config.py:_write_config_state */
json_t *cfgls_write_config_state_flow(cfgls_flow_io_t *io, const char *config_path,
                                      const json_t *data, bool allow_omissions,
                                      const char *extra_content_on_create) {
    json_t *refusal = cfgls_refuse_failed_read(io, config_path, data);
    if (refusal) return refusal;

    json_t *rargs = json_object();
    json_set(rargs, "data", data ? json_copy(data) : json_null());
    json_t *rr = cfgls_seam(io, "refuse_redacted_config_credentials", rargs);
    if (cfgls_raised(rr)) return rr;
    json_free(rr);

    if (!allow_omissions) {
        /* existing = keeping_read_base(lambda: require_readable_config_before_write(path)) */
        json_t *kr = cfgls_seam(io, "keeping_read_base", NULL);
        if (cfgls_raised(kr)) return kr;
        json_free(kr);
        json_t *gargs = json_object();
        json_set(gargs, "path", json_string(config_path));
        json_t *existing = cfgls_seam(io, "require_readable_config_before_write", gargs);
        if (cfgls_raised(existing)) return existing;
        json_t *omitted = cfgls_omitted_config_paths(existing, data);
        if (cfgls_raised(omitted)) { json_free(existing); return omitted; }
        size_t no = json_len(omitted);
        if (no > 0) {
            size_t shown_n = no < 12 ? no : 12;
            json_t *shown_arr = json_array();
            for (size_t i = 0; i < shown_n; i++) json_append(shown_arr, json_copy(json_get(omitted, i)));
            char *shown = cfgls_join_arr(shown_arr, 0, ", ");
            json_free(shown_arr);
            char *shown_full = shown;
            if (no > 12) {
                char tail[48];
                snprintf(tail, sizeof tail, ", +%zu more", no - 12);
                shown_full = cfgls_join(shown, tail);
                free(shown);
            }
            char *exc_msg = cfgls_join("omitted config paths: ", shown_full);
            free(shown_full);
            json_t *vspec = json_object();
            json_set(vspec, "type", json_string("ValueError"));
            json_set(vspec, "message", json_string(exc_msg));
            free(exc_msg);
            char *msg = cfgls_refuse_overwrite_msg(config_path, "would lose settings omitted by this write",
                                                   vspec, CFGLS_OMISSION_FIX);
            json_free(vspec);
            json_free(omitted);
            json_free(existing);
            json_t *r = cfgls_make_raise("RuntimeError", msg);
            free(msg);
            return r;
        }
        json_free(omitted);
        json_free(existing);
    }

    json_t *wargs = json_object();
    json_set(wargs, "path", json_string(config_path));
    json_set(wargs, "data", data ? json_copy(data) : json_null());
    json_set(wargs, "extra_content_on_create",
             extra_content_on_create ? json_string(extra_content_on_create) : json_null());
    json_t *wr = cfgls_seam(io, "write_config_document", wargs);
    if (cfgls_raised(wr)) return wr;
    json_free(wr);
    return json_null();
}

/* ===================================================================
 * atomic_config_replace  (config.py:1866)
 * =================================================================== */
/* PoP: cfgls_atomic_config_replace @ hermes_cli/config.py:atomic_config_replace */
json_t *cfgls_atomic_config_replace(cfgls_flow_io_t *io, const char *config_path,
                                    const json_t *data, const char *extra_content_on_create) {
    return cfgls_write_config_state_flow(io, config_path, data, true, extra_content_on_create);
}

/* ===================================================================
 * _strip_managed_keys_for_save  (config.py:2236)
 * =================================================================== */
/* PoP: cfgls_strip_managed_keys_for_save @ hermes_cli/config.py:_strip_managed_keys_for_save */
json_t *cfgls_strip_managed_keys_for_save(cfgls_flow_io_t *io, const json_t *config) {
    json_t *mk = cfgls_seam(io, "managed_config_keys", NULL);
    if (cfgls_raised(mk)) return mk;
    if (!cfgls_truthy(mk)) {
        json_free(mk);
        return config ? json_copy(config) : json_null();
    }
    json_t *cfgcopy = config ? json_copy(config) : json_object();
    json_t *stripped = NULL;
    cfgls_strip_dotted_keys(cfgcopy, mk, &stripped);
    size_t ns = json_len(stripped);
    if (ns > 0) {
        char *joined = cfgls_join_arr(stripped, 0, ", ");
        char head[96];
        snprintf(head, sizeof head, "Note: %zu managed setting(s) were not saved (managed by your administrator): ", ns);
        char *msg = cfgls_join(head, joined);
        free(joined);
        json_t *pargs = json_object();
        json_set(pargs, "message", json_string(msg));
        free(msg);
        json_t *pr = cfgls_seam(io, "print_stderr", pargs);
        json_free(pr);
    }
    json_free(stripped);
    json_free(mk);
    return cfgcopy;
}

/* ===================================================================
 * seed_config_file  (config.py:513)
 * =================================================================== */
/* PoP: cfgls_seed_config_file @ hermes_cli/config.py:seed_config_file */
json_t *cfgls_seed_config_file(cfgls_flow_io_t *io, const char *config_path, const char *template) {
    char *tmpl = NULL;
    if (template && template[0]) { /* live: template or get_project_root()/... */
        tmpl = strdup(template);
    } else {
        json_t *pr = cfgls_seam(io, "get_project_root", NULL);
        if (cfgls_raised(pr)) return pr;
        if (!pr || pr->type != JSON_STRING) {
            /* None / non-str root: None / "str" raises TypeError in live. */
            char m[128];
            snprintf(m, sizeof m, "unsupported operand type(s) for /: '%s' and 'str'",
                     cfgls_py_type_name(pr));
            json_free(pr);
            return cfgls_make_raise("TypeError", m);
        }
        tmpl = cfgls_fakepath_join(pr->str_val, "cli-config.yaml.example");
        json_free(pr);
    }
    char *parent = cfgls_path_parent(config_path);
    json_t *margs = json_object();
    json_set(margs, "path", json_string(parent));
    json_t *mr = cfgls_seam(io, "mkdir_parents", margs);
    if (cfgls_raised(mr)) { free(tmpl); free(parent); return mr; }
    json_free(mr);

    json_t *eargs = json_object();
    json_set(eargs, "path", json_string(tmpl));
    json_t *ex = cfgls_seam(io, "path_exists", eargs);
    if (cfgls_raised(ex)) { free(tmpl); free(parent); return ex; }
    bool exists = cfgls_truthy(ex);
    json_free(ex);

    if (exists) {
        json_t *cargs = json_object();
        json_set(cargs, "src", json_string(tmpl));
        json_set(cargs, "dst", json_string(config_path));
        json_t *cr = cfgls_seam(io, "copy2", cargs);
        if (cfgls_raised(cr)) { free(tmpl); free(parent); return cr; }
        json_free(cr);
        json_t *sargs = json_object();
        json_set(sargs, "path", json_string(config_path));
        json_t *sf = cfgls_seam(io, "_secure_file", sargs);
        if (cfgls_raised(sf)) { free(tmpl); free(parent); return sf; }
        json_free(sf);
        free(tmpl); free(parent);
        return json_bool(true);
    }
    json_t *scargs = json_object();
    json_t *dc = cfgls_state_obj(io, "DEFAULT_CONFIG");
    json_set(scargs, "config", dc ? json_copy(dc) : json_null());
    json_t *sc = cfgls_seam(io, "save_config", scargs);
    if (cfgls_raised(sc)) { free(tmpl); free(parent); return sc; }
    json_free(sc);
    free(tmpl); free(parent);
    return json_bool(false);
}

/* ===================================================================
 * _validate_config_key  (config.py:2835)
 * =================================================================== */
/* PoP: cfgls_validate_config_key @ hermes_cli/config.py:_validate_config_key */
json_t *cfgls_validate_config_key(cfgls_flow_io_t *io, const char *key) {
    json_t *r = cfgls_validate_schema_key_flow(io, key);
    if (cfgls_raised(r)) return r;
    json_t *known_v = json_get(r, 0);
    bool known = known_v && known_v->type == JSON_BOOL && known_v->bool_val;
    if (known || !key || !key[0]) return r;
    json_t *parts = cfgls_split_key_path(key);
    json_t *iargs = json_object();
    json_set(iargs, "parts", parts);
    json_t *rr = cfgls_seam(io, "is_runtime_read_key", iargs);
    if (cfgls_raised(rr)) { json_free(r); return rr; }
    bool is_rt = cfgls_truthy(rr);
    json_free(rr);
    json_t *out = json_array();
    if (is_rt) {
        json_append(out, json_bool(true));
        json_append(out, json_null());
    } else {
        json_append(out, json_bool(false));
        json_t *sug = json_get(r, 1);
        json_append(out, sug ? json_copy(sug) : json_null());
    }
    json_free(r);
    return out;
}

/* ===================================================================
 * _validate_schema_key  (config.py:2845)  — FLOW
 * =================================================================== */
static json_t *cfgls_valid_pair(bool known, const char *suggestion) {
    json_t *out = json_array();
    json_append(out, json_bool(known));
    json_append(out, suggestion ? json_string(suggestion) : json_null());
    return out;
}

/* PoP: cfgls_validate_schema_key_flow @ hermes_cli/config.py:_validate_schema_key */
json_t *cfgls_validate_schema_key_flow(cfgls_flow_io_t *io, const char *key) {
    if (!key || !key[0]) return cfgls_valid_pair(false, NULL);
    json_t *segments = cfgls_split_key_path(key);
    json_t *top_v = json_get(segments, 0);
    const char *top = (top_v && top_v->type == JSON_STRING) ? top_v->str_val : "";
    json_t *pck = cfgls_state_get(io, "_PLATFORM_CONTAINER_KEYS");
    json_t *lct = cfgls_state_get(io, "LEFT_CORE_TERMINAL_KEYS");
    if (top[0] == '_' || cfgls_arr_has(pck, top) || cfgls_arr_has(lct, key)) {
        json_free(segments);
        return cfgls_valid_pair(true, NULL);
    }
    json_t *known = cfgls_known_top_level_keys(io);
    if (!cfgls_arr_has(known, top)) {
        json_t *sargs = json_object();
        json_set(sargs, "key", json_string(top));
        json_set(sargs, "candidates", known);
        json_t *sug = cfgls_seam(io, "_suggest_closest_key", sargs);
        if (cfgls_raised(sug)) { json_free(segments); return sug; }
        if (cfgls_is_null(sug)) {
            json_free(sug); json_free(segments);
            return cfgls_valid_pair(false, NULL);
        }
        const char *sugs = (sug->type == JSON_STRING) ? sug->str_val : "";
        char *rest = cfgls_join_segments(segments, 1);
        json_t *out;
        if (rest && rest[0]) {
            char *sv = cfgls_py_str(sug); /* f"{suggestion}.{rest}" */
            char *full = cfgls_join(sv, ".");
            char *full2 = cfgls_join(full, rest);
            free(sv);
            free(full);
            out = cfgls_valid_pair(false, full2);
            free(full2);
        } else {
            /* live returns the RAW suggestion operand (may be non-str). */
            out = json_array();
            json_append(out, json_bool(false));
            json_append(out, json_copy(sug));
        }
        (void)sugs;
        free(rest);
        json_free(sug); json_free(segments);
        return out;
    }
    json_free(known);
    json_t *open = cfgls_state_get(io, "_OPEN_SUBKEY_TOP_LEVEL_KEYS");
    if (cfgls_arr_has(open, top)) {
        json_free(segments);
        return cfgls_valid_pair(true, NULL);
    }
    json_t *dc = cfgls_state_obj(io, "DEFAULT_CONFIG");
    json_t *node = dc ? json_obj_get(dc, top) : NULL;
    json_t *consumed = json_array();
    json_append(consumed, json_string(top));
    size_t nseg = json_len(segments);
    for (size_t i = 1; i < nseg; i++) {
        json_t *seg_v = json_get(segments, i);
        const char *seg = (seg_v && seg_v->type == JSON_STRING) ? seg_v->str_val : "";
        if (cfgls_arr_has(pck, seg) || !node || node->type != JSON_OBJECT || node->c.count == 0) {
            json_free(consumed); json_free(segments);
            return cfgls_valid_pair(true, NULL);
        }
        if (!json_obj_get(node, seg)) {
            char *rest = cfgls_join_segments(segments, json_len(consumed));
            json_t *rest_parts = cfgls_split_key_path(rest);
            json_t *first_v = json_get(rest_parts, 0);
            const char *first = (first_v && first_v->type == JSON_STRING) ? first_v->str_val : "";
            bool first_known = (dc && json_obj_get(dc, first)) || cfgls_arr_has(open, first);
            json_free(rest_parts);
            if (first_known) {
                json_t *vr = cfgls_validate_config_key(io, rest);
                if (cfgls_raised(vr)) { free(rest); json_free(consumed); json_free(segments); return vr; }
                json_t *vk = json_get(vr, 0);
                bool vknown = vk && vk->type == JSON_BOOL && vk->bool_val;
                json_free(vr);
                if (vknown) {
                    json_t *out = cfgls_valid_pair(false, rest);
                    free(rest); json_free(consumed); json_free(segments);
                    return out;
                }
            }
            free(rest);
            json_t *cand = json_array();
            for (size_t k = 0; k < node->c.count; k++) json_append(cand, json_string(node->c.keys[k]));
            json_t *sargs = json_object();
            json_set(sargs, "key", json_string(seg));
            json_set(sargs, "candidates", cand);
            json_t *sibling = cfgls_seam(io, "_suggest_closest_key", sargs);
            if (cfgls_raised(sibling)) { json_free(consumed); json_free(segments); return sibling; }
            if (!cfgls_is_null(sibling)) {
                if (sibling->type != JSON_STRING) {
                    /* ".".join(consumed + [sibling]) demands str items. */
                    char m[128];
                    snprintf(m, sizeof m, "sequence item %zu: expected str instance, %s found",
                             json_len(consumed), cfgls_py_type_name(sibling));
                    json_free(sibling); json_free(consumed); json_free(segments);
                    return cfgls_make_raise("TypeError", m);
                }
                const char *sibs = (sibling->type == JSON_STRING) ? sibling->str_val : "";
                json_append(consumed, json_string(sibs));
                char *full = cfgls_join_segments(consumed, 0);
                json_t *out = cfgls_valid_pair(false, full);
                free(full);
                json_free(sibling); json_free(consumed); json_free(segments);
                return out;
            }
            json_free(sibling); json_free(consumed); json_free(segments);
            return cfgls_valid_pair(false, NULL);
        }
        json_append(consumed, json_string(seg));
        node = json_obj_get(node, seg);
    }
    json_free(consumed); json_free(segments);
    return cfgls_valid_pair(true, NULL);
}

/* ===================================================================
 * _is_wrong_prefix_suggestion  (config.py:2902)
 * =================================================================== */
/* PoP: cfgls_is_wrong_prefix_suggestion @ hermes_cli/config.py:_is_wrong_prefix_suggestion */
json_t *cfgls_is_wrong_prefix_suggestion(cfgls_flow_io_t *io, const char *key, const char *suggestion) {
    if (!suggestion || !suggestion[0]) return json_bool(false);
    json_t *ks = cfgls_split_key_path(key);
    json_t *ss = cfgls_split_key_path(suggestion);
    size_t nk = json_len(ks), ns = json_len(ss);
    bool ok = false;
    if (ns < nk) {
        bool suffix_eq = true;
        for (size_t i = 0; i < ns && suffix_eq; i++)
            suffix_eq = cfgls_json_eq(json_get(ks, nk - ns + i), json_get(ss, i));
        if (suffix_eq) {
            json_t *vr = cfgls_validate_config_key(io, suggestion);
            if (cfgls_raised(vr)) { json_free(ks); json_free(ss); return vr; }
            json_t *vk = json_get(vr, 0);
            ok = vk && vk->type == JSON_BOOL && vk->bool_val;
            json_free(vr);
        }
    }
    json_free(ks); json_free(ss);
    return json_bool(ok);
}

/* ===================================================================
 * _coerce_config_set_value  (config.py:2943)  — FLOW
 * =================================================================== */
/* PoP: cfgls_coerce_config_set_value_flow @ hermes_cli/config.py:_coerce_config_set_value */
json_t *cfgls_coerce_config_set_value_flow(cfgls_flow_io_t *io, const char *key, const char *value) {
    json_t *dargs = json_object();
    json_set(dargs, "key", json_string(key));
    json_t *dv1 = cfgls_seam(io, "_default_value_for_key", dargs);
    if (cfgls_raised(dv1)) return dv1;
    bool str_default = dv1 && dv1->type == JSON_STRING;
    json_free(dv1);
    bool model_structured = (strcmp(key, "model") == 0) && cfgls_looks_structured_value(value);
    if (str_default && !model_structured) return json_string(value);

    char *stripped = cfgls_strip(value);
    char *lower = cfgls_lower(stripped);
    for (size_t i = 0; i < sizeof(CFGLS_SCALAR_WORDS) / sizeof(CFGLS_SCALAR_WORDS[0]); i++) {
        if (strcmp(lower, CFGLS_SCALAR_WORDS[i].word) == 0) {
            int kind = CFGLS_SCALAR_WORDS[i].kind;
            free(stripped); free(lower);
            if (kind == 0) return json_bool(false);
            if (kind == 1) return json_bool(true);
            return json_null();
        }
    }
    json_t *dargs2 = json_object();
    json_set(dargs2, "key", json_string(key));
    json_t *dv2 = cfgls_seam(io, "_default_value_for_key", dargs2);
    if (cfgls_raised(dv2)) { free(stripped); free(lower); return dv2; }
    bool bool_default = dv2 && dv2->type == JSON_BOOL;
    json_free(dv2);
    if (bool_default) {
        json_t *qargs = json_object();
        json_set(qargs, "stripped", json_string(stripped));
        json_t *q = cfgls_seam(io, "_quoted_bool", qargs);
        if (cfgls_raised(q)) { free(stripped); free(lower); return q; }
        if (!cfgls_is_null(q)) { free(stripped); free(lower); return q; }
        json_free(q);
    }
    json_t *iargs = json_object();
    json_set(iargs, "stripped", json_string(stripped));
    json_t *ci = cfgls_seam(io, "_coerce_int", iargs);
    if (cfgls_raised(ci)) { free(stripped); free(lower); return ci; }
    if (!cfgls_is_null(ci)) { free(stripped); free(lower); return ci; }
    json_free(ci);
    json_t *fargs = json_object();
    json_set(fargs, "stripped", json_string(stripped));
    json_t *cf = cfgls_seam(io, "_coerce_float", fargs);
    if (cfgls_raised(cf)) { free(stripped); free(lower); return cf; }
    if (!cfgls_is_null(cf)) { free(stripped); free(lower); return cf; }
    json_free(cf);
    free(lower);
    if (!cfgls_looks_structured_value(value)) {
        json_t *out = json_string(value);
        free(stripped);
        return out;
    }
    free(stripped);
    json_t *yargs = json_object();
    json_set(yargs, "value", json_string(value));
    json_t *parsed = cfgls_seam(io, "yaml.safe_load", yargs);
    if (cfgls_raised(parsed)) {
        json_t *spec = cfgls_raise_spec(parsed);
        const char *type = spec ? json_get_str(spec, "type", "") : "";
        if (strcmp(type, "YAMLError") == 0) {
            const char *problem = spec ? json_get_str(spec, "problem", NULL) : NULL;
            const char *detail_src = (problem && problem[0]) ? problem : json_get_str(spec, "message", "");
            char *detail = cfgls_first_line(detail_src);
            /* ✗ Value for '{key}' ... ({detail}) — nothing was written.\n  Fix ... */
            const char *pre = "\xe2\x9c\x97 Value for '";
            const char *mid = "' looks like a list/mapping but is not valid YAML/JSON (";
            const char *post = ") \xe2\x80\x94 nothing was written.\n  Fix the literal, or quote it (e.g. \"'[text'\") to store a plain string.";
            char *m1 = cfgls_join(pre, key);
            char *m2 = cfgls_join(m1, mid);
            char *m3 = cfgls_join(m2, detail);
            char *msg = cfgls_join(m3, post);
            free(m1); free(m2); free(m3); free(detail);
            json_t *eargs = json_object();
            json_set(eargs, "message", json_string(msg));
            free(msg);
            json_t *er = cfgls_seam(io, "exit_invalid", eargs);
            json_free(er);
            json_free(parsed);
            return cfgls_make_raise_code("SystemExit", 1);
        }
        return parsed;
    }
    if (parsed && (parsed->type == JSON_ARRAY || parsed->type == JSON_OBJECT)) return parsed;
    json_free(parsed);
    return json_string(value);
}
