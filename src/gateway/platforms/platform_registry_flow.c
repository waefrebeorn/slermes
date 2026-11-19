/* platform_registry_flow.c -- FLOW port of gateway/platform_registry.py
 * (LIVE authority; the task's "gateway/platforms/registry.py" resolves to this
 * module -- the quarry has no gateway/platforms/registry.py, and every existing
 * PoP credits gateway/platform_registry.py).
 *
 * Ports 22 UNCLAIMED functions: the plugin-scope inference leaves
 * (_plugin_scope_from_callable, _caller_plugin_scope), core_ships_platform,
 * the registry construction (__init__), current_scope_key, the scope-map /
 * registration-state / prune accessors (_scope_maps, _registration_state,
 * _prune_scope), the activation-env record/read (note_activation_env,
 * activation_env), the snapshot/CAS-restore pair (snapshot_registration,
 * restore_registration), the pending-name walk (_pending_names, pending_names),
 * the configured/loaded entry materializers (configured_entries, configured_load,
 * loaded_entries), forget_failed_loads, _load_pending, required_env_names, the
 * off-loop deps CM (deps_resolved_off_loop) and ensure_deps. Claimed siblings
 * (get, _resolve_all) enter through the pr_io_t universal seam; ported siblings
 * compose for REAL (_scope_maps inside _registration_state, _registration_state
 * inside snapshot_registration/restore_registration, _prune_scope inside
 * restore_registration, current_scope_key inside pending_names/configured_load/
 * loaded_entries/required_env_names, configured_load inside configured_entries,
 * _pending_names inside pending_names, _plugin_scope_from_callable inside
 * _caller_plugin_scope).
 *
 * Decision sequences only: Python truthiness, `or`/`and` short-circuits,
 * `is None` identity vs truthy tests, operand/kwargs evaluation order, dict
 * insertion order (merge keeps the overridden key's position), set-union /
 * difference semantics, and except-Exception catch scopes are transcribed per
 * call site. A scripted BaseException (SystemExit / KeyboardInterrupt /
 * GeneratorExit / CancelledError) propagates past every except Exception guard
 * exactly as live; the (ImportError, AttributeError) catch in
 * _plugin_scope_from_callable swallows only that family.
 *
 * Value model (shared with tests/oracle_platform_registry_flow.py):
 *   - registry state travels flat in st: _entries / _scoped_entries (name ->
 *     entry record), _deferred / _scoped_deferred / _inflight_loaders /
 *     _consumed_loaders (name-or-key -> loader marker {"__loader__": id}),
 *     _inflight (encoded load key -> opaque event marker), _cancelled_inflight /
 *     _failed_loads / _loading (sorted arrays of encoded load keys),
 *     _activation_env (name -> sorted env-name array).
 *   - a load key tuple (scope, name) encodes as compact JSON [scope,name] with
 *     scope null for the global scope: [null,"irc"] / ["s1","irc"]. ASCII simple
 *     strings only (no quotes/backslash/control chars) so C and json.dumps agree.
 *   - entry records carry the fixture-declared fields (incl. "__id__" first);
 *     live PlatformEntry defaults are not observable through any ported flow, so
 *     the echo projects exactly the fixture key set. Callable fields are
 *     {"__callable__": "<seam>"} markers; ensure_deps fires the marker's seam.
 *   - identity (restore_registration CAS) is modeled by id: current/previous
 *     elements are null or {"__ref__": id}; C matches the stored entry __id__ /
 *     loader __loader__ against the ref id, the oracle interns objects per id.
 *   - raise specs {"__raise__": kind, "message": str(e)}; integral floats
 *     normalize to ints on both sides.
 *
 * Domain (ASCII scope, documented for the generator):
 *   - str()/strip()/lower() positions carry ASCII text + scalar numbers/bools;
 *     strip uses the ASCII whitespace set (fixtures stay ASCII). Container str()
 *     (core_ships_platform name, note_activation_env elements) is excluded.
 *   - configured_load's `any(wanted(n) for n in unresolved)` iterates a SET
 *     (hash-randomized order live); the trailing consecutive `wanted` seam events
 *     are sorted by name on BOTH sides (harness + oracle) -- they are the trace
 *     tail, so the normalization is order-faithful.
 *   - entry callable fields are always present markers (check_fn required);
 *     a null check_fn models the live None() TypeError (caught -> warning+False).
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The harness
 * #includes this .c via the area-qualified
 * "gateway/platforms/platform_registry_flow.c". Every symbol is static pr_*.
 *
 * Gate: bash tests/run_one_oracle.sh platform_registry_flow \
 *           oracle_platform_registry_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam -- */
typedef struct pr_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} pr_io_t;

/* -- truthiness / accessors -- */
static int pr_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int pr_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int pr_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int pr_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int pr_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int pr_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int pr_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *pr_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *pr_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static json_t *pr_copy_or_null(const json_t *v) { return v ? json_copy(v) : NULL; }

/* -- raise specs -- */
static int pr_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *pr_kind(const json_t *r)
{
    const char *k = pr_str(pr_get(r, "__raise__"));
    return k ? k : "";
}

static const char *pr_msg(const json_t *r)
{
    const char *m = pr_str(pr_get(r, "message"));
    return m ? m : "";
}

static json_t *pr_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static char *pr_xfmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* -- FULL builtin exception hierarchy (child -> parent). Unknown kinds model
 *    the oracle's synthetic type(kind,(Exception,),{}) -> parent Exception.
 *    The BaseException-only family (SystemExit / KeyboardInterrupt /
 *    GeneratorExit / CancelledError / BaseExceptionGroup) never reaches
 *    Exception, so it passes through every except Exception guard. -- */
typedef struct { const char *child, *parent; } pr_exc_rel;
static const pr_exc_rel PR_EXC_TABLE[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"OSError", "Exception"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"WindowsError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"UserWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"ResourceWarning", "Warning"},
};

static const char *pr_exc_parent(const char *kind)
{
    if (!kind) return "Exception";
    if (strcmp(kind, "BaseException") == 0) return NULL;
    for (size_t i = 0; i < sizeof(PR_EXC_TABLE) / sizeof(PR_EXC_TABLE[0]); i++)
        if (strcmp(PR_EXC_TABLE[i].child, kind) == 0) return PR_EXC_TABLE[i].parent;
    return "Exception"; /* synthetic Exception subclass */
}

/* except Exception: true when the kind walks up to Exception. */
static int pr_is_exception(const char *kind)
{
    const char *cur = kind;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (strcmp(cur, "Exception") == 0) return 1;
        if (strcmp(cur, "BaseException") == 0) return 0;
        cur = pr_exc_parent(cur);
    }
    return 0;
}

static int pr_is_base(const char *kind) { return !pr_is_exception(kind); }

/* except ImportError: true when the kind walks up to ImportError. */
static int pr_is_import_error(const char *kind)
{
    const char *cur = kind;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (strcmp(cur, "ImportError") == 0) return 1;
        if (strcmp(cur, "Exception") == 0 || strcmp(cur, "BaseException") == 0) return 0;
        cur = pr_exc_parent(cur);
    }
    return 0;
}

static int pr_is_attribute_error(const char *kind)
{
    return kind && strcmp(kind, "AttributeError") == 0;
}

/* -- universal seam call: args OWNED by caller, borrowed by dispatcher, freed
 *    here. Result caller-owned (NULL == None). Fail-closed with no channel. -- */
static json_t *pr_call(pr_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = pr_xfmt("no seam installed for %s", name);
        r = pr_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *pr_args(void) { return json_object(); }

/* arg builders: None-valued keys dropped on both sides. */
static void pr_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void pr_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void pr_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* -- Python str() / type names (ASCII scalar domain) -- */

/* Shortest round-tripping decimal formatted like Python repr(float): fixed for
 * decimal exponents in [-4,16), scientific otherwise; integral keeps ".0". */
static void pr_float_repr(double v, char *out, size_t cap)
{
    if (isnan(v)) { snprintf(out, cap, "nan"); return; }
    if (isinf(v)) { snprintf(out, cap, v < 0 ? "-inf" : "inf"); return; }
    if (v == 0.0) { snprintf(out, cap, signbit(v) ? "-0.0" : "0.0"); return; }
    int neg = v < 0;
    double av = neg ? -v : v;
    char buf[64];
    int p;
    for (p = 1; p <= 17; p++) {
        snprintf(buf, sizeof(buf), "%.*e", p - 1, av);
        if (strtod(buf, NULL) == av) break;
    }
    if (p > 17) p = 17;
    char digits[32];
    size_t nd = 0;
    digits[nd++] = buf[0];
    for (int i = 2; buf[i] && buf[i] != 'e' && buf[i] != 'E'; i++) digits[nd++] = buf[i];
    digits[nd] = '\0';
    while (nd > 1 && digits[nd - 1] == '0') digits[--nd] = '\0';
    int exp = atoi(strchr(buf, 'e') + 1);
    char body[128];
    if (exp >= -4 && exp < 16) {
        size_t o = 0;
        if (exp >= 0) {
            size_t ip = (size_t)exp + 1;
            for (size_t i = 0; i < ip; i++) body[o++] = i < nd ? digits[i] : '0';
            body[o++] = '.';
            if (nd > ip) { for (size_t i = ip; i < nd; i++) body[o++] = digits[i]; }
            else body[o++] = '0';
        } else {
            body[o++] = '0'; body[o++] = '.';
            for (int i = 0; i < -exp - 1; i++) body[o++] = '0';
            for (size_t i = 0; i < nd; i++) body[o++] = digits[i];
        }
        body[o] = '\0';
    } else {
        char frac[32];
        size_t o = 0;
        frac[o++] = digits[0];
        if (nd > 1) { frac[o++] = '.'; for (size_t i = 1; i < nd; i++) frac[o++] = digits[i]; }
        frac[o] = '\0';
        snprintf(body, sizeof(body), "%se%c%02d", frac, exp < 0 ? '-' : '+', exp < 0 ? -exp : exp);
    }
    snprintf(out, cap, "%s%s", neg ? "-" : "", body);
}

/* malloc'd Python str() of a JSON scalar. Containers are outside the fixture
 * domain (live str() of a container is its repr); they read as "". */
static char *pr_py_str(const json_t *v)
{
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (isfinite(v->num_val) && v->num_val == floor(v->num_val) &&
                fabs(v->num_val) < 9.2233720368547758e18) {
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
                return strdup(buf);
            }
            pr_float_repr(v->num_val, buf, sizeof(buf));
            return strdup(buf);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default: return strdup("");
    }
}

static const char *pr_typename(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (isfinite(d) && d == floor(d) && fabs(d) < 9.2233720368547758e18) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* -- string helpers (ASCII whitespace domain) -- */
static int pr_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *pr_strip(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && pr_is_ws((unsigned char)s[a])) a++;
    while (b > a && pr_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *pr_lower(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    out[n] = '\0';
    return out;
}

/* -- load-key encoding: (scope, name) -> compact JSON [scope,name] -- */
static char *pr_key(const char *scope, const char *name)
{
    char *sq = scope ? json_escape_string(scope) : NULL;
    char *nq = json_escape_string(name ? name : "");
    char *out = pr_xfmt("[%s,%s]", sq ? sq : "null", nq ? nq : "\"\"");
    free(sq);
    free(nq);
    return out;
}

/* decoded scope / name (malloc'd; NULL scope == global). Caller frees both. */
static void pr_key_decode(const char *key, char **scope_out, char **name_out)
{
    *scope_out = NULL;
    *name_out = NULL;
    json_t *arr = key ? json_parse(key, NULL) : NULL;
    if (arr && arr->type == JSON_ARRAY && json_len(arr) == 2) {
        json_t *s = json_get(arr, 0);
        json_t *n = json_get(arr, 1);
        if (pr_is_str(s)) *scope_out = strdup(s->str_val);
        if (pr_is_str(n)) *name_out = strdup(n->str_val);
    }
    json_free(arr);
}

/* -- state helpers -- */

/* borrowed (entries, deferred) maps for a scope, mirroring _scope_maps; create
 * auto-vivifies the scoped maps. scope is a JSON node (null == global). */
static void pr_scope_maps_ref(json_t *st, const json_t *scope, int create,
                              json_t **entries_out, json_t **deferred_out)
{
    if (pr_is_null(scope)) {
        *entries_out = pr_get(st, "_entries");
        *deferred_out = pr_get(st, "_deferred");
        return;
    }
    const char *sk = pr_str(scope);
    json_t *se = pr_get(st, "_scoped_entries");
    json_t *sd = pr_get(st, "_scoped_deferred");
    json_t *e = pr_get(se, sk);
    json_t *d = pr_get(sd, sk);
    if (create) {
        if (!e) { e = json_object(); json_set(se, sk, e); }
        if (!d) { d = json_object(); json_set(sd, sk, d); }
    }
    *entries_out = e;
    *deferred_out = d;
}

/* string-set arrays (sorted at echo; membership/add here). */
static int pr_set_has(const json_t *arr, const char *id)
{
    if (!pr_is_arr(arr)) return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *it = arr->c.items[i];
        if (pr_is_str(it) && strcmp(it->str_val, id) == 0) return 1;
    }
    return 0;
}

static void pr_set_add(json_t *arr, const char *id)
{
    if (!pr_is_arr(arr) || pr_set_has(arr, id)) return;
    json_append(arr, json_string(id));
}

static int pr_str_cmp(const void *a, const void *b)
{
    const json_t *x = *(const json_t *const *)a;
    const json_t *y = *(const json_t *const *)b;
    return strcmp(x->str_val ? x->str_val : "", y->str_val ? y->str_val : "");
}

static json_t *pr_sorted_strs(const json_t *arr)
{
    json_t *out = json_array();
    if (pr_is_arr(arr)) {
        for (size_t i = 0; i < arr->c.count; i++)
            if (pr_is_str(arr->c.items[i])) json_append(out, json_copy(arr->c.items[i]));
    }
    if (out->c.count > 1) qsort(out->c.items, out->c.count, sizeof(json_t *), pr_str_cmp);
    return out;
}

/* entry / loader identity markers. */
static const char *pr_entry_id(const json_t *e) { return pr_str(pr_get(e, "__id__")); }
static const char *pr_loader_id(const json_t *l) { return pr_str(pr_get(l, "__loader__")); }
static const char *pr_ref_id(const json_t *r) { return pr_str(pr_get(r, "__ref__")); }
static const char *pr_callable_seam(const json_t *m) { return pr_str(pr_get(m, "__callable__")); }

/* identity match: stored node (entry dict / loader marker / NULL) vs a current
 * element (null or {"__ref__": id}). */
static int pr_ident_match(const json_t *stored, const json_t *cur, const char *(*idfn)(const json_t *))
{
    if (pr_is_null(stored) && pr_is_null(cur)) return 1;
    if (pr_is_null(stored) || pr_is_null(cur)) return 0;
    const char *sid = idfn(stored);
    const char *rid = pr_ref_id(cur);
    return sid && rid && strcmp(sid, rid) == 0;
}

/* scan every entry map for an entry with __id__ == id (borrowed). */
static json_t *pr_find_entry(json_t *st, const char *id)
{
    if (!id) return NULL;
    json_t *g = pr_get(st, "_entries");
    if (pr_is_obj(g))
        for (size_t i = 0; i < g->c.count; i++) {
            const char *eid = pr_entry_id(g->c.items[i]);
            if (eid && strcmp(eid, id) == 0) return g->c.items[i];
        }
    json_t *se = pr_get(st, "_scoped_entries");
    if (pr_is_obj(se))
        for (size_t s = 0; s < se->c.count; s++) {
            json_t *inner = se->c.items[s];
            if (!pr_is_obj(inner)) continue;
            for (size_t i = 0; i < inner->c.count; i++) {
                const char *eid = pr_entry_id(inner->c.items[i]);
                if (eid && strcmp(eid, id) == 0) return inner->c.items[i];
            }
        }
    return NULL;
}

/* scan every loader map for a loader marker with __loader__ == id (borrowed). */
static json_t *pr_find_loader(json_t *st, const char *id)
{
    if (!id) return NULL;
    const char *keys[] = { "_deferred", "_inflight_loaders", "_consumed_loaders" };
    for (int k = 0; k < 3; k++) {
        json_t *map = pr_get(st, keys[k]);
        if (!pr_is_obj(map)) continue;
        for (size_t i = 0; i < map->c.count; i++) {
            const char *lid = pr_loader_id(map->c.items[i]);
            if (lid && strcmp(lid, id) == 0) return map->c.items[i];
        }
    }
    json_t *sd = pr_get(st, "_scoped_deferred");
    if (pr_is_obj(sd))
        for (size_t s = 0; s < sd->c.count; s++) {
            json_t *inner = sd->c.items[s];
            if (!pr_is_obj(inner)) continue;
            for (size_t i = 0; i < inner->c.count; i++) {
                const char *lid = pr_loader_id(inner->c.items[i]);
                if (lid && strcmp(lid, id) == 0) return inner->c.items[i];
            }
        }
    return NULL;
}

/* resolve a previous-state element (null / {"__ref__":id} / literal record) to
 * the borrowed node to write, or NULL for None. */
static json_t *pr_resolve_prev(json_t *st, const json_t *prev, int is_entry)
{
    if (pr_is_null(prev)) return NULL;
    const char *rid = pr_ref_id(prev);
    if (rid) return is_entry ? pr_find_entry(st, rid) : pr_find_loader(st, rid);
    return (json_t *)prev; /* literal entry dict / loader marker */
}

/* {**_entries, **_scoped_entries.get(scope, {})} -- merge keeps the overridden
 * key's position; returns a NEW object. */
static json_t *pr_merge_entries(json_t *st, const char *scope)
{
    json_t *merged = json_object();
    json_t *g = pr_get(st, "_entries");
    if (pr_is_obj(g))
        for (size_t i = 0; i < g->c.count; i++)
            json_set(merged, g->c.keys[i], json_copy(g->c.items[i]));
    json_t *se = pr_get(st, "_scoped_entries");
    json_t *sc = pr_get(se, scope);
    if (pr_is_obj(sc))
        for (size_t i = 0; i < sc->c.count; i++)
            json_set(merged, sc->c.keys[i], json_copy(sc->c.items[i]));
    return merged;
}

/* values of an object as a NEW array (insertion order). */
static json_t *pr_obj_values(const json_t *o)
{
    json_t *a = json_array();
    if (pr_is_obj(o))
        for (size_t i = 0; i < o->c.count; i++) json_append(a, json_copy(o->c.items[i]));
    return a;
}

/* -- ported function forward declarations (ported siblings compose) -- */
static json_t *pr_plugin_scope_from_callable(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_caller_plugin_scope(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_core_ships_platform(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_current_scope_key(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_scope_maps(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_registration_state(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_prune_scope(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_note_activation_env(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_activation_env(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_snapshot_registration(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_restore_registration(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_u_pending_names(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_pending_names(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_configured_load(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_configured_entries(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_loaded_entries(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_forget_failed_loads(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_load_pending(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_required_env_names(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_deps_resolved_off_loop(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_ensure_deps(pr_io_t *io, const json_t *in, json_t *st);
static json_t *pr_init(pr_io_t *io, const json_t *in, json_t *st);

/* -- _plugin_scope_from_callable --
 * try: from tools.registry import registry; return
 * registry.plugin_scope_for_callable(callback) except (ImportError,
 * AttributeError): return None. Import failure / missing attribute are modeled
 * by in flags (no seam event); a scripted call raise of that family is swallowed
 * WITH its trace event, any other kind propagates. */
/* PoP: pr_plugin_scope_from_callable @ gateway/platform_registry.py:_plugin_scope_from_callable */
static json_t *pr_plugin_scope_from_callable(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    if (pr_truth(pr_get(in, "import_fails")) || pr_truth(pr_get(in, "attr_missing")))
        return json_null();
    const json_t *cb = pr_get(in, "callback");
    const char *module = pr_str(pr_get(pr_get(cb, "__attr__"), "__module__"));
    json_t *a = pr_args();
    pr_arg_s(a, "module", module);
    json_t *r = pr_call(io, "plugin_scope_for_callable", a);
    if (pr_is_raise(r)) {
        const char *k = pr_kind(r);
        if (pr_is_import_error(k) || pr_is_attribute_error(k)) {
            json_free(r);
            return json_null();
        }
    }
    return r;
}

/* -- _caller_plugin_scope --
 * try: module_name = sys._getframe(2).f_globals.get("__name__","") or ""
 * except Exception: return None; then compose _plugin_scope_from_callable on a
 * synthetic _Caller class carrying __module__ = module_name. */
/* PoP: pr_caller_plugin_scope @ gateway/platform_registry.py:_caller_plugin_scope */
static json_t *pr_caller_plugin_scope(pr_io_t *io, const json_t *in, json_t *st)
{
    json_t *a = pr_args();
    pr_arg_n(a, "depth", 2);
    json_t *frame = pr_call(io, "sys._getframe", a);
    if (pr_is_raise(frame)) {
        if (pr_is_exception(pr_kind(frame))) { json_free(frame); return json_null(); }
        return frame; /* BaseException propagates */
    }
    const json_t *fg = pr_get(frame, "f_globals");
    if (!pr_is_obj(frame) || !pr_is_obj(fg)) {
        /* live AttributeError on .f_globals / .get swallowed by except Exception */
        json_free(frame);
        return json_null();
    }
    const json_t *nm = pr_get(fg, "__name__");
    char *module = pr_truth(nm) ? pr_py_str(nm) : strdup("");
    json_free(frame);
    /* compose the ported sibling with a _Caller attribute double. */
    json_t *tmp = json_object();
    json_t *cb = json_object();
    json_t *attr = json_object();
    json_set(attr, "__module__", json_string(module ? module : ""));
    json_set(cb, "__attr__", attr);
    json_set(tmp, "callback", cb);
    pr_arg_j(tmp, "import_fails", pr_get(in, "import_fails"));
    pr_arg_j(tmp, "attr_missing", pr_get(in, "attr_missing"));
    json_t *r = pr_plugin_scope_from_callable(io, tmp, st);
    json_free(tmp);
    free(module);
    return r;
}

/* -- core_ships_platform --
 * from gateway.config import Platform; value = str(name or "").strip().lower();
 * return value in {Platform[m].value for m in Platform._member_names_} or
 * (value in Platform._scan_bundled_plugin_platforms()[0]). The enum lookup is
 * fixture data (in.platform.values / member_names); the bundled scan is a seam
 * whose result models the tuple's [0] collection. */
/* PoP: pr_core_ships_platform @ gateway/platform_registry.py:core_ships_platform */
static json_t *pr_core_ships_platform(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    if (pr_truth(pr_get(in, "platform_import_fails")))
        return pr_mkraise("ModuleNotFoundError",
                          "import of gateway.config halted; None in sys.modules");
    const json_t *name = pr_get(in, "name");
    char *s = pr_truth(name) ? pr_py_str(name) : strdup("");
    char *t = pr_strip(s);
    char *value = pr_lower(t);
    free(s);
    free(t);
    const json_t *plat = pr_get(in, "platform");
    const json_t *members = pr_get(plat, "member_names");
    const json_t *values = pr_get(plat, "values");
    int found = 0;
    json_t *err = NULL;
    if (pr_is_arr(members)) {
        for (size_t i = 0; i < members->c.count; i++) {
            const char *m = pr_str(members->c.items[i]);
            if (!m) continue;
            const json_t *v = pr_get(values, m);
            if (!v) {
                /* Platform[member] KeyError; str(KeyError(k)) == repr(k). */
                char *q = pr_xfmt("'%s'", m);
                err = pr_mkraise("KeyError", q ? q : m);
                free(q);
                break;
            }
            const char *vs = pr_str(v);
            if (vs && strcmp(vs, value) == 0) { found = 1; break; }
        }
    }
    if (err) { free(value); return err; }
    if (found) { free(value); return json_bool(true); }
    /* short-circuit: the bundled scan seam fires only when the enum miss. */
    json_t *bundled = pr_call(io, "platform_scan_bundled", pr_args());
    if (pr_is_raise(bundled)) { free(value); return bundled; }
    if (!pr_is_arr(bundled)) {
        char *m = pr_xfmt("argument of type '%s' is not a container or iterable",
                          pr_typename(bundled));
        json_t *te = pr_mkraise("TypeError", m ? m : "argument of type");
        free(m);
        json_free(bundled);
        free(value);
        return te;
    }
    int in_bundled = pr_set_has(bundled, value);
    json_free(bundled);
    free(value);
    return json_bool(in_bundled ? true : false);
}

/* -- current_scope_key (staticmethod): return hermes_home_key() -- */
/* PoP: pr_current_scope_key @ gateway/platform_registry.py:current_scope_key */
static json_t *pr_current_scope_key(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    return pr_call(io, "hermes_home_key", pr_args());
}

/* -- _scope_maps: (entries, deferred) for a scope; create auto-vivifies -- */
/* PoP: pr_scope_maps @ gateway/platform_registry.py:_scope_maps */
static json_t *pr_scope_maps(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *scope = pr_get(in, "scope");
    int create = pr_truth(pr_get(in, "create"));
    json_t *e = NULL, *d = NULL;
    pr_scope_maps_ref(st, scope, create, &e, &d);
    json_t *ret = json_array();
    json_append(ret, e ? json_copy(e) : json_object());
    json_append(ret, d ? json_copy(d) : json_object());
    return ret;
}

/* -- _registration_state: (entry, loader); loader falls back to in-flight then
 *    consumed. Composes _scope_maps for real. -- */
/* PoP: pr_registration_state @ gateway/platform_registry.py:_registration_state */
static json_t *pr_registration_state(pr_io_t *io, const json_t *in, json_t *st)
{
    const json_t *scope = pr_get(in, "scope");
    const char *name = pr_str(pr_get(in, "name"));
    json_t *tmp = json_object();
    pr_arg_j(tmp, "scope", scope);
    pr_arg_j(tmp, "create", pr_get(in, "create"));
    json_t *maps = pr_scope_maps(io, tmp, st);
    json_free(tmp);
    json_t *entries = json_get(maps, 0);
    json_t *deferred = json_get(maps, 1);
    const json_t *entry = pr_get(entries, name);
    const json_t *loader = pr_get(deferred, name);
    json_t *fallback = NULL;
    if (pr_is_null(entry) && pr_is_null(loader)) {
        char *key = pr_key(pr_str(scope), name);
        json_t *il = pr_get(st, "_inflight_loaders");
        json_t *cl = pr_get(st, "_consumed_loaders");
        const json_t *l1 = pr_get(il, key);
        fallback = pr_truth(l1) ? json_copy(l1) : pr_copy_or_null(pr_get(cl, key));
        free(key);
    }
    json_t *ret = json_array();
    /* None entries/loaders must survive as JSON null: json_append(arr, NULL)
     * silently drops the item (Python returns a 2-tuple either way). */
    json_t *ec = pr_copy_or_null(entry);
    json_append(ret, ec ? ec : json_null());
    json_t *lc = fallback ? fallback : pr_copy_or_null(loader);
    json_append(ret, lc ? lc : json_null());
    json_free(maps);
    return ret;
}

/* -- _prune_scope: drop empty/absent scoped maps (scope None is a no-op) -- */
/* PoP: pr_prune_scope @ gateway/platform_registry.py:_prune_scope */
static json_t *pr_prune_scope(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *scope = pr_get(in, "scope");
    if (pr_is_null(scope)) return json_null();
    const char *sk = pr_str(scope);
    const char *keys[] = { "_scoped_entries", "_scoped_deferred" };
    for (int i = 0; i < 2; i++) {
        json_t *maps = pr_get(st, keys[i]);
        if (!pr_is_obj(maps)) continue;
        if (!pr_truth(pr_get(maps, sk))) json_obj_del(maps, sk);
    }
    return json_null();
}

/* -- note_activation_env: union the stripped str(n) names into _activation_env -- */
/* PoP: pr_note_activation_env @ gateway/platform_registry.py:note_activation_env */
static json_t *pr_note_activation_env(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const char *name = pr_str(pr_get(in, "name"));
    const json_t *env_names = pr_get(in, "env_names");
    json_t *names = json_array();
    if (pr_truth(env_names) && pr_is_arr(env_names)) {
        for (size_t i = 0; i < env_names->c.count; i++) {
            char *s = pr_py_str(env_names->c.items[i]);
            char *t = pr_strip(s);
            free(s);
            if (t && t[0] && !pr_set_has(names, t)) json_append(names, json_string(t));
            free(t);
        }
    }
    json_t *ae = pr_get(st, "_activation_env");
    const json_t *cur = pr_get(ae, name);
    json_t *uni = json_array();
    if (pr_is_arr(cur))
        for (size_t i = 0; i < cur->c.count; i++)
            if (pr_is_str(cur->c.items[i]) && !pr_set_has(uni, cur->c.items[i]->str_val))
                json_append(uni, json_copy(cur->c.items[i]));
    for (size_t i = 0; i < names->c.count; i++)
        if (!pr_set_has(uni, names->c.items[i]->str_val))
            json_append(uni, json_copy(names->c.items[i]));
    json_set(ae, name, uni);
    json_free(names);
    return json_null();
}

/* -- activation_env: frozenset -> sorted list -- */
/* PoP: pr_activation_env @ gateway/platform_registry.py:activation_env */
static json_t *pr_activation_env(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const char *name = pr_str(pr_get(in, "name"));
    return pr_sorted_strs(pr_get(pr_get(st, "_activation_env"), name));
}

/* -- snapshot_registration: _registration_state under the (silent) lock -- */
/* PoP: pr_snapshot_registration @ gateway/platform_registry.py:snapshot_registration */
static json_t *pr_snapshot_registration(pr_io_t *io, const json_t *in, json_t *st)
{
    json_t *tmp = json_object();
    pr_arg_j(tmp, "scope", pr_get(in, "scope"));
    pr_arg_j(tmp, "name", pr_get(in, "name"));
    json_set(tmp, "create", json_bool(false));
    json_t *r = pr_registration_state(io, tmp, st);
    json_free(tmp);
    return r;
}

/* -- restore_registration: CAS restore of a displaced registration -- */
/* PoP: pr_restore_registration @ gateway/platform_registry.py:restore_registration */
static json_t *pr_restore_registration(pr_io_t *io, const json_t *in, json_t *st)
{
    const json_t *scope = pr_get(in, "scope");
    const char *name = pr_str(pr_get(in, "name"));
    const json_t *current = pr_get(in, "current");
    const json_t *previous = pr_get(in, "previous");
    json_t *tmp = json_object();
    pr_arg_j(tmp, "scope", scope);
    pr_arg_s(tmp, "name", name);
    json_set(tmp, "create", json_bool(true));
    json_t *rs = pr_registration_state(io, tmp, st);
    json_free(tmp);
    const json_t *entry = json_get(rs, 0);
    const json_t *loader = json_get(rs, 1);
    const json_t *cur0 = json_get(current, 0);
    const json_t *cur1 = json_get(current, 1);
    if (!pr_ident_match(entry, cur0, pr_entry_id) ||
        !pr_ident_match(loader, cur1, pr_loader_id)) {
        json_free(rs);
        return json_bool(false);
    }
    json_t *prev0 = pr_resolve_prev(st, json_get(previous, 0), 1);
    json_t *prev1 = pr_resolve_prev(st, json_get(previous, 1), 0);
    json_t *em = NULL, *dm = NULL;
    pr_scope_maps_ref(st, scope, 0, &em, &dm);
    if (pr_is_obj(em)) {
        if (!prev0) json_obj_del(em, name);
        else json_set(em, name, json_copy(prev0));
    }
    if (pr_is_obj(dm)) {
        if (!prev1) json_obj_del(dm, name);
        else json_set(dm, name, json_copy(prev1));
    }
    char *key = pr_key(pr_str(scope), name);
    if (pr_get(pr_get(st, "_inflight"), key))
        pr_set_add(pr_get(st, "_cancelled_inflight"), key);
    json_obj_del(pr_get(st, "_consumed_loaders"), key);
    free(key);
    json_t *ptmp = json_object();
    pr_arg_j(ptmp, "scope", scope);
    json_free(pr_prune_scope(io, ptmp, st));
    json_free(ptmp);
    json_free(rs);
    return json_bool(true);
}

/* -- _pending_names: scoped-first deferred/in-flight names, sorted -- */
/* PoP: pr_u_pending_names @ gateway/platform_registry.py:_pending_names */
static json_t *pr_u_pending_names(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const char *active = pr_str(pr_get(in, "active_scope"));
    json_t *scnode = json_string(active ? active : "");
    json_t *e = NULL, *sd = NULL;
    pr_scope_maps_ref(st, scnode, 0, &e, &sd);
    json_free(scnode);
    json_t *scoped = json_array();
    json_t *global = json_array();
    if (pr_is_obj(sd))
        for (size_t i = 0; i < sd->c.count; i++) pr_set_add(scoped, sd->c.keys[i]);
    json_t *gd = pr_get(st, "_deferred");
    if (pr_is_obj(gd))
        for (size_t i = 0; i < gd->c.count; i++) pr_set_add(global, gd->c.keys[i]);
    json_t *inflight = pr_get(st, "_inflight");
    if (pr_is_obj(inflight)) {
        for (size_t i = 0; i < inflight->c.count; i++) {
            char *owner = NULL, *nm = NULL;
            pr_key_decode(inflight->c.keys[i], &owner, &nm);
            if (nm) {
                if (owner && active && strcmp(owner, active) == 0) pr_set_add(scoped, nm);
                else if (!owner) pr_set_add(global, nm);
            }
            free(owner);
            free(nm);
        }
    }
    json_t *ret = pr_sorted_strs(scoped);
    json_t *gs = pr_sorted_strs(global);
    for (size_t i = 0; i < gs->c.count; i++)
        if (!pr_set_has(scoped, gs->c.items[i]->str_val))
            json_append(ret, json_copy(gs->c.items[i]));
    json_free(gs);
    json_free(scoped);
    json_free(global);
    return ret;
}

/* -- pending_names: _pending_names(current_scope_key()) -- */
/* PoP: pr_pending_names @ gateway/platform_registry.py:pending_names */
static json_t *pr_pending_names(pr_io_t *io, const json_t *in, json_t *st)
{
    json_t *sk = pr_current_scope_key(io, in, st);
    if (pr_is_raise(sk)) return sk;
    json_t *tmp = json_object();
    json_set(tmp, "active_scope", json_string(pr_str(sk) ? pr_str(sk) : ""));
    json_t *r = pr_u_pending_names(io, tmp, st);
    json_free(tmp);
    json_free(sk);
    return r;
}

/* -- configured_load: resolve wanted, merge entries, report wanted failures --
 * The trailing `any(wanted(n) for n in unresolved)` iterates a set live
 * (hash-randomized); the harness/oracle sort the trailing wanted events. */
/* PoP: pr_configured_load @ gateway/platform_registry.py:configured_load */
static json_t *pr_configured_load(pr_io_t *io, const json_t *in, json_t *st)
{
    const json_t *wanted = pr_get(in, "wanted");
    const char *wseam = pr_callable_seam(wanted);
    json_t *a = pr_args();
    pr_arg_j(a, "wanted", wanted);
    json_t *rr = pr_call(io, "self._resolve_all", a);
    if (pr_is_raise(rr)) return rr;
    json_free(rr);
    json_t *sk = pr_current_scope_key(io, in, st);
    if (pr_is_raise(sk)) return sk;
    const char *scope = pr_str(sk);
    json_t *merged = pr_merge_entries(st, scope);
    json_t *entries = pr_obj_values(merged);
    json_free(merged);
    /* unresolved = consumed + failed names owned by None or scope, minus loaded */
    json_t *unresolved = json_array();
    json_t *cl = pr_get(st, "_consumed_loaders");
    if (pr_is_obj(cl))
        for (size_t i = 0; i < cl->c.count; i++) {
            char *owner = NULL, *nm = NULL;
            pr_key_decode(cl->c.keys[i], &owner, &nm);
            if (nm && (!owner || (scope && strcmp(owner, scope) == 0))) pr_set_add(unresolved, nm);
            free(owner);
            free(nm);
        }
    json_t *fl = pr_get(st, "_failed_loads");
    if (pr_is_arr(fl))
        for (size_t i = 0; i < fl->c.count; i++) {
            char *owner = NULL, *nm = NULL;
            pr_key_decode(pr_str(fl->c.items[i]), &owner, &nm);
            if (nm && (!owner || (scope && strcmp(owner, scope) == 0))) pr_set_add(unresolved, nm);
            free(owner);
            free(nm);
        }
    json_t *loaded_names = json_array();
    for (size_t i = 0; i < entries->c.count; i++) {
        const char *en = pr_str(pr_get(entries->c.items[i], "name"));
        if (en) pr_set_add(loaded_names, en);
    }
    json_t *todo = json_array();
    for (size_t i = 0; i < unresolved->c.count; i++)
        if (!pr_set_has(loaded_names, unresolved->c.items[i]->str_val))
            json_append(todo, json_copy(unresolved->c.items[i]));
    json_free(unresolved);
    json_free(loaded_names);
    int any = 0;
    json_t *prop = NULL;
    for (size_t i = 0; i < todo->c.count; i++) {
        json_t *wa = pr_args();
        pr_arg_s(wa, "name", todo->c.items[i]->str_val);
        json_t *wr = pr_call(io, wseam ? wseam : "wanted", wa);
        if (pr_is_raise(wr)) { prop = wr; break; }
        if (pr_truth(wr)) { any = 1; json_free(wr); break; }
        json_free(wr);
    }
    json_free(todo);
    json_free(sk);
    if (prop) { json_free(entries); return prop; }
    json_t *ret = json_array();
    json_append(ret, entries);
    json_append(ret, json_bool(any ? true : false));
    return ret;
}

/* -- configured_entries: configured_load(wanted)[0] -- */
/* PoP: pr_configured_entries @ gateway/platform_registry.py:configured_entries */
static json_t *pr_configured_entries(pr_io_t *io, const json_t *in, json_t *st)
{
    json_t *r = pr_configured_load(io, in, st);
    if (pr_is_raise(r)) return r;
    json_t *entries = json_copy(json_get(r, 0));
    json_free(r);
    return entries;
}

/* -- loaded_entries: merged values, never imports a deferred adapter -- */
/* PoP: pr_loaded_entries @ gateway/platform_registry.py:loaded_entries */
static json_t *pr_loaded_entries(pr_io_t *io, const json_t *in, json_t *st)
{
    json_t *sk = pr_current_scope_key(io, in, st);
    if (pr_is_raise(sk)) return sk;
    json_t *merged = pr_merge_entries(st, pr_str(sk));
    json_t *ret = pr_obj_values(merged);
    json_free(merged);
    json_free(sk);
    return ret;
}

/* -- forget_failed_loads: drop scope's failed-load records -- */
/* PoP: pr_forget_failed_loads @ gateway/platform_registry.py:forget_failed_loads */
static json_t *pr_forget_failed_loads(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const char *scope = pr_str(pr_get(in, "scope"));
    json_t *fl = pr_get(st, "_failed_loads");
    json_t *keep = json_array();
    if (pr_is_arr(fl))
        for (size_t i = 0; i < fl->c.count; i++) {
            const char *key = pr_str(fl->c.items[i]);
            char *owner = NULL, *nm = NULL;
            pr_key_decode(key, &owner, &nm);
            int same = (!owner && !scope) || (owner && scope && strcmp(owner, scope) == 0);
            if (!same && key) json_append(keep, json_string(key));
            free(owner);
            free(nm);
        }
    json_set(st, "_failed_loads", keep);
    return json_null();
}

/* -- _load_pending: must a lookup run/await a deferred loader? -- */
/* PoP: pr_load_pending @ gateway/platform_registry.py:_load_pending */
static json_t *pr_load_pending(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *scope = pr_get(in, "scope");
    const char *name = pr_str(pr_get(in, "name"));
    json_t *e = NULL, *d = NULL;
    pr_scope_maps_ref(st, scope, 0, &e, &d);
    int pending = 0;
    if (pr_get(d, name)) pending = 1;
    else if (!pr_get(pr_get(st, "_entries"), name) && pr_get(pr_get(st, "_deferred"), name)) pending = 1;
    else {
        char *k1 = pr_key(pr_str(scope), name);
        char *k2 = pr_key(NULL, name);
        json_t *inflight = pr_get(st, "_inflight");
        if (pr_get(inflight, k1) || pr_get(inflight, k2)) pending = 1;
        free(k1);
        free(k2);
    }
    return json_bool(pending ? true : false);
}

/* -- required_env_names: str required_env of global + scoped loaded entries -- */
/* PoP: pr_required_env_names @ gateway/platform_registry.py:required_env_names */
static json_t *pr_required_env_names(pr_io_t *io, const json_t *in, json_t *st)
{
    json_t *sk = pr_current_scope_key(io, in, st);
    if (pr_is_raise(sk)) return sk;
    json_t *scnode = json_string(pr_str(sk) ? pr_str(sk) : "");
    json_t *entries = NULL, *d = NULL;
    pr_scope_maps_ref(st, scnode, 0, &entries, &d);
    json_free(scnode);
    json_free(sk);
    json_t *names = json_array();
    json_t *maps[2] = { pr_get(st, "_entries"), entries };
    for (int m = 0; m < 2; m++) {
        json_t *map = maps[m];
        if (!pr_is_obj(map)) continue;
        for (size_t i = 0; i < map->c.count; i++) {
            const json_t *re = pr_get(map->c.items[i], "required_env");
            if (!pr_is_arr(re)) continue;
            for (size_t j = 0; j < re->c.count; j++)
                if (pr_is_str(re->c.items[j])) pr_set_add(names, re->c.items[j]->str_val);
        }
    }
    json_t *ret = pr_sorted_strs(names);
    json_free(names);
    return ret;
}

/* -- deps_resolved_off_loop: ContextVar set/reset CM (modeled as state) -- */
/* PoP: pr_deps_resolved_off_loop @ gateway/platform_registry.py:deps_resolved_off_loop */
static json_t *pr_deps_resolved_off_loop(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *oldv = pr_get(st, "off_loop_deps");
    json_t *old = oldv ? json_copy(oldv) : json_null();
    const json_t *deps_ok = pr_get(in, "deps_ok");
    json_set(st, "off_loop_deps", deps_ok ? json_copy(deps_ok) : json_null());
    json_t *inside = deps_ok ? json_copy(deps_ok) : json_null();
    const json_t *body_raise = pr_get(in, "body_raise");
    /* finally: reset(token) restores the previous value even on a body raise */
    json_set(st, "off_loop_deps", old);
    if (pr_is_raise(body_raise)) {
        json_free(inside);
        return json_copy(body_raise);
    }
    json_t *ret = json_object();
    json_set(ret, "inside", inside);
    json_set(ret, "after", json_copy(pr_get(st, "off_loop_deps")));
    return ret;
}

/* -- ensure_deps probe helper: bool(fn()) under except Exception -> warning+False.
 *    Returns a propagate-raise (BaseException or a logger raise) or NULL. -- */
static json_t *pr_probe(pr_io_t *io, const json_t *entry, const char *fn_field,
                        const char *failure_msg, int *ok_out)
{
    const char *label = pr_str(pr_get(entry, "label"));
    const char *seam = pr_callable_seam(pr_get(entry, fn_field));
    json_t *r;
    if (!seam) r = pr_mkraise("TypeError", "'NoneType' object is not callable");
    else r = pr_call(io, seam, pr_args());
    if (pr_is_raise(r)) {
        if (pr_is_exception(pr_kind(r))) {
            json_t *lg = pr_args();
            pr_arg_s(lg, "msg", failure_msg);
            json_t *ar = json_array();
            json_append(ar, json_string(label ? label : ""));
            json_append(ar, json_string(pr_msg(r)));
            json_set(lg, "args", ar);
            json_t *lr = pr_call(io, "logger.warning", lg);
            json_free(r);
            if (pr_is_raise(lr)) return lr;
            json_free(lr);
            *ok_out = 0;
            return NULL;
        }
        return r; /* BaseException passes through except Exception */
    }
    *ok_out = pr_truth(r);
    json_free(r);
    return NULL;
}

/* -- ensure_deps: passive probe then active installer; off-loop short-circuit -- */
/* PoP: pr_ensure_deps @ gateway/platform_registry.py:ensure_deps */
static json_t *pr_ensure_deps(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *a = pr_args();
    pr_arg_j(a, "name", pr_get(in, "name"));
    json_t *entry = pr_call(io, "self.get", a);
    if (pr_is_raise(entry)) return entry;
    if (pr_is_null(entry)) { json_free(entry); return json_null(); }
    const json_t *off_loop = pr_get(in, "off_loop_deps");
    if (!pr_is_null(off_loop)) {
        json_t *r = json_copy(off_loop);
        json_free(entry);
        return r;
    }
    const char *label = pr_str(pr_get(entry, "label"));
    int deps_ok = 0;
    json_t *prop = pr_probe(io, entry, "check_fn", "Platform '%s' check_fn raised: %s", &deps_ok);
    if (prop) { json_free(entry); return prop; }
    if (!deps_ok && !pr_is_null(pr_get(entry, "ensure_deps_fn"))) {
        json_t *lg = pr_args();
        pr_arg_s(lg, "msg", "Platform '%s' dependencies missing \xe2\x80\x94 attempting install...");
        json_t *ar = json_array();
        json_append(ar, json_string(label ? label : ""));
        json_set(lg, "args", ar);
        json_t *lr = pr_call(io, "logger.info", lg);
        if (pr_is_raise(lr)) { json_free(entry); return lr; }
        json_free(lr);
        prop = pr_probe(io, entry, "ensure_deps_fn",
                        "Platform '%s' dependency install raised: %s", &deps_ok);
        if (prop) { json_free(entry); return prop; }
    }
    if (!deps_ok) {
        const json_t *ih = pr_get(entry, "install_hint");
        char *hint = strdup("");
        if (pr_truth(ih)) {
            char *hs = pr_py_str(ih);
            free(hint);
            hint = pr_xfmt(" (%s)", hs ? hs : "");
            free(hs);
        }
        json_t *lg = pr_args();
        pr_arg_s(lg, "msg", "Platform '%s' requirements not met%s");
        json_t *ar = json_array();
        json_append(ar, json_string(label ? label : ""));
        json_append(ar, json_string(hint ? hint : ""));
        json_set(lg, "args", ar);
        json_t *lr = pr_call(io, "logger.warning", lg);
        free(hint);
        if (pr_is_raise(lr)) { json_free(entry); return lr; }
        json_free(lr);
    }
    json_free(entry);
    return json_bool(deps_ok ? true : false);
}

/* -- __init__: fresh registry state (lock / ContextVar scaffolding excluded) -- */
/* PoP: pr_init @ gateway/platform_registry.py:__init__ */
static json_t *pr_init(pr_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    while (st->c.count > 0) json_obj_del(st, st->c.keys[0]);
    json_set(st, "_entries", json_object());
    json_set(st, "_scoped_entries", json_object());
    json_set(st, "_deferred", json_object());
    json_set(st, "_scoped_deferred", json_object());
    json_set(st, "_inflight", json_object());
    json_set(st, "_inflight_loaders", json_object());
    json_set(st, "_cancelled_inflight", json_array());
    json_set(st, "_consumed_loaders", json_object());
    json_set(st, "_failed_loads", json_array());
    json_set(st, "_activation_env", json_object());
    json_set(st, "_loading", json_array());
    return json_null();
}
