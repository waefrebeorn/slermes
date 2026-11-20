/* ===========================================================================
 * account_usage_flow2.c -- FLOW port of the UNCLAIMED remainder of the
 * agent/account_usage*.py family. Sibling of usage_tracking_flow.c (ut_*),
 * which ported 16 agent/account_usage.py + 12 agent/account_usage_nous.py
 * defs. A strict PoP grep over src/** and tests/** (the scanner credits the
 * DEFINING module) leaves these 16 defs with NO PoP anywhere:
 *
 *   agent/account_usage.py        : _get_json, _plural, _data (the nested
 *                                   closure inside _fetch_openrouter_account_usage)
 *   agent/account_usage_nous.py   : _fetch_portal_account (thread-pool mechanics,
 *                                   deferred + SEAMED by usage_tracking_flow)
 *   agent/account_usage_cache.py  : _key, has_account_usage, _identity_id_for,
 *                                   remember_account_usage, cached_account_usage,
 *                                   snapshot_is_stale, _claim_refresh,
 *                                   _release_refresh, refresh_account_usage_async,
 *                                   _refresh_legacy, refresh_account_usage_entries_async,
 *                                   _refresh_entries  (the whole module: 0 claims)
 *
 * The 6 Nous defs whose only PoPs mis-credit the facade (_fmt_usd, _is_finite_num,
 * build_credits_view, build_nous_credits_snapshot, nous_credits_lines,
 * _snapshot_from_credits_state) are LEFT SEAMED -- a PoP grep finds them claimed,
 * so they are not re-claimed here. The cache module is included because the
 * >=12-unclaimed target cannot be met from the two facade modules alone (only 4
 * naively-unclaimed defs there); account_usage_cache.py is fully unclaimed and
 * is the cache half of the same /usage flow.
 *
 * Self-contained: libjson only. Every live dependency NOT ported here is injected
 * through ONE universal seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * args is a BORROWED object (the port owns it and frees it after the call); the
 * result is a NEW json_t owned by the caller (NULL == None; a
 * {"__raise__":kind,"message":msg,...} object == a raised exception). A scripted
 * JSON null result is delivered as a NULL pointer so truthiness reads falsy.
 * Fail-closed: no channel installed, or a missing/exhausted script, yields
 * {"__raise__":"RuntimeError","message":"no script for <name>"} /
 * "no seam installed for <name>", recorded by the dispatcher BEFORE propagation.
 *
 * ENTRIES. json_t *au2_<fn>(au2_io_t *io, const json_t *in, json_t *st)
 *   io  -- universal seam channel (NULL => fail-closed production default)
 *   in  -- borrowed case input object (never mutated)
 *   st  -- fresh object the entry fills with the final state projection.
 *          PURE entries (_get_json, _plural, _data, _fetch_portal_account, _key,
 *          has_account_usage, _identity_id_for, snapshot_is_stale) leave st={}.
 *          STATEFUL cache entries (remember/cached/claim/release/refresh_*) fill
 *          st with the FULL final projection of the four module dicts, on success
 *          AND on every raise path (partial mutation history). Caller owns ret.
 *          Tuples project to JSON arrays; None to NULL.
 *
 * Ported siblings compose REAL:
 *   remember_account_usage / cached_account_usage / refresh_account_usage_async /
 *   refresh_account_usage_entries_async / _refresh_entries -> _key;
 *   refresh_account_usage_async / refresh_account_usage_entries_async ->
 *   _claim_refresh; _refresh_legacy / _refresh_entries -> _release_refresh.
 * Claimed siblings are SEAMED (never re-claimed): fetch_account_usage,
 * _codex_principal_identity (via codex_principal_identity), hermes_home_key,
 * the Nous facade defs. threading.Lock (_lock) is INERT scaffolding (the harness
 * is single-threaded; a silent nullcontext, no seam event) per the cc2 doctrine.
 *
 * BaseException kinds (SystemExit, KeyboardInterrupt, GeneratorExit,
 * CancelledError, BaseException, BaseExceptionGroup) PASS THROUGH every
 * `except Exception` site. A FULL builtin exception-parent table (au2_exc_parent,
 * probed on quarry CPython 3.14.9) drives au2_is_exception. The
 * `except (AttributeError,TypeError,ValueError,OSError,OverflowError)` site in
 * snapshot_is_stale uses au2_caught_by (subclass-aware). _fetch_portal_account's
 * `except BaseException` catches EVERY kind (callback fires even for the trio)
 * then re-raises; its `finally: pool.shutdown(wait=False)` always runs.
 *
 * Seam name -> live call table:
 *   hermes_home_key            hermes_constants.hermes_home_key()
 *   time.monotonic             time.monotonic()
 *   time.time                  time.time()
 *   usage_fetchers             agent.account_usage._USAGE_FETCHERS (import read)
 *   get_provider_profile       providers.get_provider_profile(slug)
 *   codex_principal_identity   agent.credential_pool._codex_principal_identity(token)
 *   sha256_hexdigest           hashlib.sha256(token.encode()).hexdigest() (chain collapsed)
 *   fetch_account_usage        agent.account_usage.fetch_account_usage(provider,...)
 *   httpx.Client               httpx.Client(timeout=)            (ctor -> cm carrier)
 *   client.__enter__           cm.__enter__()                    (-> as-bound client)
 *   client.get                 client.get(url,headers=)          (-> response carrier)
 *   response.raise_for_status  response.raise_for_status()
 *   client.__exit__            cm.__exit__(exc_type,exc_val,exc_tb) (exc projected)
 *   response.json              response.json()
 *   contextvars.copy_context   contextvars.copy_context()        (-> context carrier)
 *   DaemonThreadPoolExecutor   tools.daemon_pool.DaemonThreadPoolExecutor(max_workers=1)
 *   partial                    functools.partial(get_nous_portal_account_info,force_fresh=True)
 *   pool.submit                pool.submit(context.run, read)    (-> future carrier)
 *   future.result              future.result(timeout=)
 *   future.add_done_callback   future.add_done_callback(lambda f: f.exception())
 *   pool.shutdown              pool.shutdown(wait=False)
 *   threading.Thread           threading.Thread(target=copy_context().run,args=...,name=,daemon=)
 *   thread.start               thread.start()
 *
 * has_account_usage's `type(profile).fetch_account_usage is not
 * ProviderProfile.fetch_account_usage` is REAL logic on the profile carrier
 * (hook=="custom" => overridden), NOT a seam -- mirrors ut's _call_plugin_usage_hook
 * mat_profile convention. The profile carrier is {"hook":"base"|"custom"} or null.
 *
 * State model (cache): in.state = {"snapshots":{keystr:snapshot},
 *   "started":{keystr:num}, "last_try":{keystr:num}, "inflight":[keystr,...]}.
 *   A slot key is the 4-tuple (home, provider, identity_id, base_url) encoded as
 *   a LENGTH-PREFIXED printable ASCII string: "".join(f"{len(s)}:{s}") -- e.g.
 *   "2:/h4:nous0:0:". Collision-free for ASCII-printable fields, no JSON control
 *   char escaping needed. _inflight is a SET live -> sorted array on BOTH sides.
 *   Snapshot carriers carry the FULL dataclass field set (generator emits them);
 *   C stores the carrier verbatim, the oracle materializes + projects it back.
 *
 * Datetime model: fetched_at travels as INTEGER epoch-MICROSECONDS (UTC);
 * snapshot_is_stale computes ts = us/1e6 (exact IEEE, probed == datetime.timestamp
 * over 20k random us). A fetched_at may also be a {"__raise__":kind,"message":msg}
 * spec modelling a .timestamp() that raises (caught iff kind is one of the 5).
 *
 * Domain restrictions (documented, enforced by the generator):
 *   - ASCII inputs; strip()/lower()/rstrip("/") operate on ASCII whitespace/letters.
 *   - carrier scripts (client/cm/response/context/pool/future/thread/profile/entry)
 *     are null or NON-EMPTY objects (an empty {} double is truthy live but falsy
 *     under json count>0). entry may be any JSON (getattr-with-default semantics).
 *   - hermes_home_key / sha256_hexdigest scripts are strings.
 *   - codex_principal_identity returns null or a 2-element array (str/int elems).
 *   - snapshot carriers: full AccountUsageSnapshot field set; fetched_at int-us,
 *     null, or a uniform {__raise__,message} spec.
 *   - providers: array of strings; requests: array of objects (provider/identity_id/
 *     api_key str-or-null, base_url optional). Integral floats excluded from
 *     str()-computed positions (libjson has no int/float identity).
 *   - _get_json: client.get raise + __exit__ suppression reaches the
 *     UnboundLocalError branch (modelled, py3.14 message). __exit__ truthy suppresses
 *     the pending exception; an __exit__ raise replaces it.
 *   - {body!r}-style repr not needed here; f-string id/principal elems str/int/bool/null.
 * =========================================================================== */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>

typedef struct au2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} au2_io_t;

/* ---- truthiness / accessors ---- */

static int au2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

/* `is not None` identity: present-non-null. */
static int au2_present(const json_t *v)
{
    return (v && v->type != JSON_NULL) ? 1 : 0;
}

static const char *au2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *au2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double au2_num(const json_t *v)
{
    if (v && v->type == JSON_NUMBER) return v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return 0.0;
}

static json_t *au2_copy_or_null(const json_t *v)
{
    return v ? json_copy(v) : NULL;
}

/* Python type-name of a JSON node (for AttributeError messages). */
static const char *au2_type_name(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        return (isfinite(d) && d == floor(d)) ? "int" : "float";
    }
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    return "dict";
}

/* ---- raise specs ---- */

static int au2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *au2_kind(const json_t *r)
{
    const char *k = au2_str(au2_get(r, "__raise__"));
    return k ? k : "";
}

static json_t *au2_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

/* FULL builtin exception-parent hierarchy (probed on quarry CPython 3.14.9).
 * ExceptionGroup has two bases (BaseExceptionGroup, Exception); it IS an
 * Exception, handled specially in au2_is_exception. CancelledError is the
 * asyncio extension (BaseException since 3.8). */
typedef struct { const char *name; const char *parent; } au2_exc_row;
static const au2_exc_row AU2_EXC_PARENT[] = {
    {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"ImportError", "Exception"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"OSError", "Exception"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"WindowsError", "OSError"},
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
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"NotImplementedError", "RuntimeError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {NULL, NULL}
};

static const char *au2_exc_parent(const char *kind)
{
    for (size_t i = 0; AU2_EXC_PARENT[i].name; i++)
        if (strcmp(AU2_EXC_PARENT[i].name, kind) == 0)
            return AU2_EXC_PARENT[i].parent;
    return NULL;
}

/* True iff the scripted kind is caught by `except Exception`. Unknown kinds are
 * synthetic Exception subclasses in the oracle, so they ARE exceptions. */
static int au2_is_exception(const char *kind)
{
    if (strcmp(kind, "ExceptionGroup") == 0) return 1;
    const char *cur = kind;
    for (int depth = 0; depth < 32 && cur; depth++) {
        if (strcmp(cur, "Exception") == 0) return 1;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return 0;
        cur = au2_exc_parent(cur);
        if (!cur) return 1; /* unknown -> synthetic Exception subclass */
    }
    return 1;
}

/* True iff kind is caught by `except (names...)` -- subclass-aware. Unknown
 * kinds (synthetic Exception subclasses) are NOT caught unless Exception is a
 * listed name. Stops at BaseException/object. */
static int au2_caught_by(const char *kind, const char *const *names, size_t n)
{
    const char *cur = kind;
    for (int depth = 0; depth < 32 && cur; depth++) {
        for (size_t i = 0; i < n; i++)
            if (strcmp(cur, names[i]) == 0) return 1;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return 0;
        cur = au2_exc_parent(cur);
        if (!cur) return 0; /* unknown -> synthetic Exception subclass; not listed */
    }
    return 0;
}

/* malloc'd printf-style string builder */
static char *au2_xfmt(const char *fmt, ...)
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

/* ---- universal seam call ----
 * args OWNED by the caller (built fresh), borrowed by the dispatcher, freed
 * here. Result caller-owned (NULL == None). Fail-closed with no channel. */
static json_t *au2_call(au2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = au2_xfmt("no seam installed for %s", name);
        r = au2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *au2_args(void)
{
    return json_object();
}

/* AttributeError: '<T>' object has no attribute '<attr>' */
static json_t *au2_attr_raise(const json_t *obj, const char *attr)
{
    char *m = au2_xfmt("'%s' object has no attribute '%s'", au2_type_name(obj), attr);
    json_t *r = au2_mkraise("AttributeError", m ? m : "attribute error");
    free(m);
    return r;
}

/* ---- string helpers (ASCII domain) ---- */

static int au2_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}

static char *au2_strip(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && au2_is_ws((unsigned char)s[a])) a++;
    while (b > a && au2_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *au2_lower(const char *s)
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

static char *au2_rstrip_slash(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '/') n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* str(x) projection (ASCII domain). Caller frees. */
static char *au2_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return au2_xfmt("None");
    if (v->type == JSON_BOOL) return au2_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == 0.0) return au2_xfmt("0");
        if (isfinite(d) && d == floor(d) && fabs(d) < 1e16) return au2_xfmt("%.0f", d);
        return au2_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return au2_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize(v);
    return s ? s : au2_xfmt("");
}

/* str(x or "") projection: falsy -> "", else str(x). Caller frees. */
static char *au2_pystr_or_empty(const json_t *v)
{
    if (!au2_truth(v)) return au2_xfmt("");
    return au2_pystr(v);
}

/* normalized slug: str(provider or "").strip().lower(). Caller frees. */
static char *au2_slug(const json_t *provider)
{
    char *s = au2_pystr_or_empty(provider);
    char *t = au2_strip(s);
    free(s);
    char *l = au2_lower(t);
    free(t);
    return l;
}

/* ---- cache state model ---- */

typedef struct au2_state {
    json_t *snapshots;   /* keystr -> snapshot carrier */
    json_t *started;     /* keystr -> num */
    json_t *last_try;    /* keystr -> num */
    json_t *inflight;    /* sorted array of keystr */
} au2_state_t;

/* length-prefixed printable key encoding: "".join(f"{len(s)}:{s}") */
static char *au2_keystr(const json_t *keyarr)
{
    /* compute total length */
    size_t total = 1;
    char *parts[4];
    size_t lens[4];
    for (size_t i = 0; i < 4; i++) {
        json_t *e = (keyarr && keyarr->type == JSON_ARRAY) ? json_get(keyarr, i) : NULL;
        parts[i] = au2_pystr(e);
        lens[i] = strlen(parts[i]);
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%zu:", lens[i]);
        total += strlen(tmp) + lens[i];
    }
    char *out = (char *)malloc(total);
    if (!out) { for (int i = 0; i < 4; i++) free(parts[i]); return NULL; }
    size_t pos = 0;
    for (size_t i = 0; i < 4; i++) {
        pos += (size_t)sprintf(out + pos, "%zu:", lens[i]);
        memcpy(out + pos, parts[i], lens[i]);
        pos += lens[i];
        free(parts[i]);
    }
    out[pos] = '\0';
    return out;
}

static void au2_state_seed(au2_state_t *s, const json_t *in)
{
    const json_t *st = au2_get(in, "state");
    json_t *sn = au2_get(st, "snapshots");
    json_t *sta = au2_get(st, "started");
    json_t *lt = au2_get(st, "last_try");
    json_t *inf = au2_get(st, "inflight");
    s->snapshots = (sn && sn->type == JSON_OBJECT) ? json_copy(sn) : json_object();
    s->started = (sta && sta->type == JSON_OBJECT) ? json_copy(sta) : json_object();
    s->last_try = (lt && lt->type == JSON_OBJECT) ? json_copy(lt) : json_object();
    s->inflight = (inf && inf->type == JSON_ARRAY) ? json_copy(inf) : json_array();
}

static void au2_state_free(au2_state_t *s)
{
    json_free(s->snapshots);
    json_free(s->started);
    json_free(s->last_try);
    json_free(s->inflight);
}

/* fill st with the full projection (inflight kept sorted) */
static void au2_state_emit(json_t *st, au2_state_t *s)
{
    json_set(st, "snapshots", json_copy(s->snapshots));
    json_set(st, "started", json_copy(s->started));
    json_set(st, "last_try", json_copy(s->last_try));
    json_set(st, "inflight", json_copy(s->inflight));
}

static int au2_inflight_contains(au2_state_t *s, const char *ks)
{
    for (size_t i = 0; i < json_len(s->inflight); i++) {
        const char *e = au2_str(json_get(s->inflight, i));
        if (e && strcmp(e, ks) == 0) return 1;
    }
    return 0;
}

static void au2_inflight_add(au2_state_t *s, const char *ks)
{
    if (au2_inflight_contains(s, ks)) return;
    /* sorted insert */
    size_t n = json_len(s->inflight);
    size_t pos = n;
    for (size_t i = 0; i < n; i++) {
        const char *e = au2_str(json_get(s->inflight, i));
        if (e && strcmp(ks, e) < 0) { pos = i; break; }
    }
    /* rebuild array with insertion at pos (libjson has no insert-at) */
    json_t *na = json_array();
    for (size_t i = 0; i < n; i++) {
        if (i == pos) json_append(na, json_string(ks));
        json_append(na, json_copy(json_get(s->inflight, i)));
    }
    if (pos == n) json_append(na, json_string(ks));
    json_free(s->inflight);
    s->inflight = na;
}

static void au2_inflight_discard(au2_state_t *s, const char *ks)
{
    json_t *na = json_array();
    for (size_t i = 0; i < json_len(s->inflight); i++) {
        json_t *e = json_get(s->inflight, i);
        const char *es = au2_str(e);
        if (es && strcmp(es, ks) == 0) continue;
        json_append(na, json_copy(e));
    }
    json_free(s->inflight);
    s->inflight = na;
}

/* _claim_refresh core: 1 claimed, 0 refused. Mutates state on claim. */
static int au2_claim_core(au2_state_t *s, const char *ks, double now)
{
    if (au2_inflight_contains(s, ks)) return 0;
    json_t *last = json_obj_get(s->last_try, ks);
    double lastv = last ? au2_num(last) : -INFINITY;
    if (now - lastv < 120.0) return 0;
    au2_inflight_add(s, ks);
    json_set(s->last_try, ks, json_number(now));
    return 1;
}

static void au2_release_core(au2_state_t *s, const char *ks)
{
    au2_inflight_discard(s, ks);
}

/* _key core: fires hermes_home_key, returns a 4-array (or raise). */
static json_t *au2_key_core(au2_io_t *io, const json_t *provider,
                            const json_t *identity_id, const json_t *base_url)
{
    json_t *home = au2_call(io, "hermes_home_key", au2_args());
    if (au2_is_raise(home)) return home;
    char *slug = au2_slug(provider);
    char *is = au2_pystr_or_empty(identity_id);
    char *ident = au2_strip(is);
    free(is);
    char *bs = au2_pystr_or_empty(base_url);
    char *bst = au2_strip(bs);
    free(bs);
    char *base = au2_rstrip_slash(bst);
    free(bst);
    json_t *r = json_array();
    json_append(r, home ? home : json_null());
    json_append(r, json_string(slug ? slug : ""));
    json_append(r, json_string(ident ? ident : ""));
    json_append(r, json_string(base ? base : ""));
    free(slug); free(ident); free(base);
    return r;
}

/* ===========================================================================
 * agent/account_usage.py
 * =========================================================================== */

/* PoP: au2_plural @ agent/account_usage.py:_plural */
json_t *au2_plural(au2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *count = au2_get(in, "count");
    /* count != 1: number/bool 1 equals 1; everything else differs. */
    int eq1 = 0;
    if (count && count->type == JSON_NUMBER) eq1 = (count->num_val == 1.0);
    else if (count && count->type == JSON_BOOL) eq1 = (count->bool_val ? 1 : 0);
    return json_string(eq1 ? "" : "s");
}

/* _get_json core. cm = ctor carrier; client = as-bound (__enter__ result). */
/* PoP: au2_get_json @ agent/account_usage.py:_get_json */
json_t *au2_get_json(au2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *url = au2_get(in, "url");
    const json_t *headers = au2_get(in, "headers");
    double timeout = au2_num(au2_get(in, "timeout"));

    json_t *ca = au2_args();
    json_set(ca, "timeout", json_number(timeout));
    json_t *cm = au2_call(io, "httpx.Client", ca);
    if (au2_is_raise(cm)) return cm;
    if (!au2_present(cm)) return au2_attr_raise(NULL, "__enter__");
    if (cm->type != JSON_OBJECT) { json_t *r = au2_attr_raise(cm, "__enter__"); json_free(cm); return r; }

    json_t *ea = au2_args();
    json_set(ea, "client", json_copy(cm));
    json_t *client = au2_call(io, "client.__enter__", ea);
    if (au2_is_raise(client)) { json_free(cm); return client; }
    /* client may be NULL (None) or a carrier; body uses it */

    json_t *pending = NULL;       /* raised exception inside the with body */
    json_t *response = NULL;      /* bound only if client.get succeeded */
    int response_bound = 0;

    /* body: response = client.get(url, headers=headers) */
    if (!au2_present(client)) {
        pending = au2_attr_raise(NULL, "get");
    } else if (client->type != JSON_OBJECT) {
        pending = au2_attr_raise(client, "get");
    } else {
        json_t *ga = au2_args();
        json_set(ga, "client", json_copy(client));
        json_set(ga, "url", au2_copy_or_null(url));
        json_set(ga, "headers", au2_copy_or_null(headers));
        response = au2_call(io, "client.get", ga);
        if (au2_is_raise(response)) { pending = response; response = NULL; }
        else response_bound = 1;
    }
    /* body: response.raise_for_status() */
    if (!pending) {
        if (!au2_present(response)) {
            pending = au2_attr_raise(NULL, "raise_for_status");
        } else if (response->type != JSON_OBJECT) {
            pending = au2_attr_raise(response, "raise_for_status");
        } else {
            json_t *ra = au2_args();
            json_set(ra, "response", json_copy(response));
            json_t *rr = au2_call(io, "response.raise_for_status", ra);
            if (au2_is_raise(rr)) pending = rr;
            else json_free(rr);
        }
    }

    /* __exit__ on the ORIGINAL cm, with the pending exc projected */
    json_t *xa = au2_args();
    json_set(xa, "client", json_copy(cm));
    json_set(xa, "exc", pending ? json_copy(pending) : json_null());
    json_t *xr = au2_call(io, "client.__exit__", xa);
    if (au2_is_raise(xr)) {
        /* exit raise replaces pending; response.json never runs */
        json_free(pending); json_free(response); json_free(client); json_free(cm);
        return xr;
    }
    int suppress = au2_truth(xr);
    json_free(xr);
    if (pending && !suppress) {
        json_free(response); json_free(client); json_free(cm);
        return pending;
    }
    json_free(pending); pending = NULL;
    json_free(cm); json_free(client);

    /* after the with: return response.json() or {} */
    if (!response_bound) {
        json_free(response);
        return au2_mkraise("UnboundLocalError",
            "cannot access local variable 'response' where it is not associated with a value");
    }
    json_t *ja = au2_args();
    json_set(ja, "response", json_copy(response));
    json_free(response);
    json_t *j = au2_call(io, "response.json", ja);
    if (au2_is_raise(j)) return j;
    if (au2_truth(j)) return j;
    json_free(j);
    return json_object();   /* {} */
}

/* _data: the nested closure inside _fetch_openrouter_account_usage. Free
 * variables (client, normalized, headers) become explicit inputs. */
/* PoP: au2_data @ agent/account_usage.py:_data */
json_t *au2_data(au2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *client = au2_get(in, "client");
    const char *normalized = au2_str(au2_get(in, "normalized"));
    const char *path = au2_str(au2_get(in, "path"));
    const json_t *headers = au2_get(in, "headers");
    if (!normalized) normalized = "";
    if (!path) path = "";
    char *url = au2_xfmt("%s/%s", normalized, path);

    if (!au2_present(client)) { free(url); return au2_attr_raise(NULL, "get"); }
    if (client->type != JSON_OBJECT) { json_t *r = au2_attr_raise(client, "get"); free(url); return r; }

    json_t *ga = au2_args();
    json_set(ga, "client", json_copy(client));
    json_set(ga, "url", json_string(url ? url : ""));
    json_set(ga, "headers", au2_copy_or_null(headers));
    free(url);
    json_t *resp = au2_call(io, "client.get", ga);
    if (au2_is_raise(resp)) return resp;
    if (!au2_present(resp)) return au2_attr_raise(NULL, "raise_for_status");
    if (resp->type != JSON_OBJECT) { json_t *r = au2_attr_raise(resp, "raise_for_status"); json_free(resp); return r; }

    json_t *ra = au2_args();
    json_set(ra, "response", json_copy(resp));
    json_t *rr = au2_call(io, "response.raise_for_status", ra);
    if (au2_is_raise(rr)) { json_free(resp); return rr; }
    json_free(rr);

    json_t *ja = au2_args();
    json_set(ja, "response", json_copy(resp));
    json_free(resp);
    json_t *j = au2_call(io, "response.json", ja);
    if (au2_is_raise(j)) return j;

    /* (resp.json() or {}).get("data") or {} */
    if (!au2_truth(j)) { json_free(j); return json_object(); }
    if (j->type != JSON_OBJECT) { json_t *r = au2_attr_raise(j, "get"); json_free(j); return r; }
    json_t *data = json_obj_get(j, "data");
    if (!au2_truth(data)) { json_free(j); return json_object(); }
    json_t *out = json_copy(data);
    json_free(j);
    return out;
}

/* ===========================================================================
 * agent/account_usage_nous.py
 * =========================================================================== */

/* PoP: au2_fetch_portal_account @ agent/account_usage_nous.py:_fetch_portal_account */
json_t *au2_fetch_portal_account(au2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    double timeout = au2_num(au2_get(in, "timeout"));
    int stored_token_only = au2_truth(au2_get(in, "stored_token_only"));

    json_t *ctx = au2_call(io, "contextvars.copy_context", au2_args());
    if (au2_is_raise(ctx)) return ctx;

    json_t *pa = au2_args();
    json_set(pa, "max_workers", json_number(1));
    json_t *pool = au2_call(io, "DaemonThreadPoolExecutor", pa);
    if (au2_is_raise(pool)) { json_free(ctx); return pool; }

    json_t *read;
    if (stored_token_only) {
        read = json_object();
        json_set(read, "fn", json_string("stored_token_account_info"));
    } else {
        json_t *fa = au2_args();
        json_set(fa, "fn", json_string("get_nous_portal_account_info"));
        json_set(fa, "force_fresh", json_bool(true));
        read = au2_call(io, "partial", fa);
        if (au2_is_raise(read)) { json_free(ctx); json_free(pool); return read; }
    }

    /* future = pool.submit(context.run, read) -- BEFORE the try */
    if (!au2_present(ctx)) { json_free(read); json_free(pool); return au2_attr_raise(NULL, "run"); }
    if (ctx->type != JSON_OBJECT) { json_t *r = au2_attr_raise(ctx, "run"); json_free(read); json_free(pool); json_free(ctx); return r; }
    if (!au2_present(pool)) { json_free(read); json_free(ctx); return au2_attr_raise(NULL, "submit"); }
    if (pool->type != JSON_OBJECT) { json_t *r = au2_attr_raise(pool, "submit"); json_free(read); json_free(ctx); json_free(pool); return r; }
    json_t *sa = au2_args();
    json_set(sa, "context", json_copy(ctx));
    json_set(sa, "read", au2_copy_or_null(read));
    json_free(read);
    json_t *future = au2_call(io, "pool.submit", sa);
    if (au2_is_raise(future)) { json_free(ctx); json_free(pool); return future; }

    json_t *pending = NULL, *ret = NULL;
    /* try: return future.result(timeout=timeout) */
    if (!au2_present(future)) {
        pending = au2_attr_raise(NULL, "result");
    } else if (future->type != JSON_OBJECT) {
        pending = au2_attr_raise(future, "result");
    } else {
        json_t *ra = au2_args();
        json_set(ra, "timeout", json_number(timeout));
        ret = au2_call(io, "future.result", ra);
        if (au2_is_raise(ret)) { pending = ret; ret = NULL; }
    }
    /* except BaseException: future.add_done_callback(...); raise */
    if (pending) {
        json_t *cb;
        if (!au2_present(future)) cb = au2_attr_raise(NULL, "add_done_callback");
        else if (future->type != JSON_OBJECT) cb = au2_attr_raise(future, "add_done_callback");
        else {
            json_t *cba = au2_args();
            json_t *marker = json_object();
            json_set(marker, "__callable__", json_string("future.exception_drain"));
            json_set(cba, "callback", marker);
            cb = au2_call(io, "future.add_done_callback", cba);
        }
        if (au2_is_raise(cb)) { json_free(pending); pending = cb; }
        else json_free(cb);
    }
    /* finally: pool.shutdown(wait=False) -- pool is present-object here */
    json_t *sda = au2_args();
    json_set(sda, "wait", json_bool(false));
    json_t *sd = au2_call(io, "pool.shutdown", sda);
    if (au2_is_raise(sd)) {
        json_free(pending); json_free(ret); json_free(future); json_free(ctx); json_free(pool);
        return sd;
    }
    json_free(sd);
    json_free(future); json_free(ctx); json_free(pool);
    if (pending) { json_free(ret); return pending; }
    return ret;
}

/* ===========================================================================
 * agent/account_usage_cache.py
 * =========================================================================== */

/* PoP: au2_key @ agent/account_usage_cache.py:_key */
json_t *au2_key(au2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return au2_key_core(io, au2_get(in, "provider"), au2_get(in, "identity_id"),
                        au2_get(in, "base_url"));
}

/* PoP: au2_has_account_usage @ agent/account_usage_cache.py:has_account_usage */
json_t *au2_has_account_usage(au2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    /* imports execute first: _USAGE_FETCHERS read fires the usage_fetchers seam */
    json_t *fetchers = au2_call(io, "usage_fetchers", au2_args());
    if (au2_is_raise(fetchers)) return fetchers;
    char *slug = au2_slug(au2_get(in, "provider"));
    if (!au2_present(fetchers)) {
        free(slug);
        return au2_mkraise("TypeError", "argument of type 'NoneType' is not a container or iterable");
    }
    /* slug in fetchers (dict key membership; array/string also supported) */
    int member = 0;
    if (fetchers->type == JSON_OBJECT) {
        member = json_obj_get(fetchers, slug ? slug : "") != NULL;
    } else if (fetchers->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(fetchers); i++) {
            const char *e = au2_str(json_get(fetchers, i));
            if (e && slug && strcmp(e, slug) == 0) { member = 1; break; }
        }
    } else if (fetchers->type == JSON_STRING) {
        member = (slug && fetchers->str_val && strstr(fetchers->str_val, slug)) ? 1 : 0;
    }
    json_free(fetchers);
    if (member) { free(slug); return json_bool(true); }
    if (!slug || !slug[0]) { free(slug); return json_bool(false); }
    json_t *ga = au2_args();
    json_set(ga, "slug", json_string(slug));
    free(slug);
    json_t *profile = au2_call(io, "get_provider_profile", ga);
    if (au2_is_raise(profile)) return profile;
    if (!au2_present(profile)) { json_free(profile); return json_bool(false); }
    /* type(profile).fetch_account_usage is not ProviderProfile.fetch_account_usage
     * -> hook=="custom" means overridden (mat_profile convention). */
    const char *hook = au2_str(au2_get(profile, "hook"));
    int overridden = (hook && strcmp(hook, "custom") == 0) ? 1 : 0;
    json_free(profile);
    return json_bool(overridden);
}

/* PoP: au2_identity_id_for @ agent/account_usage_cache.py:_identity_id_for */
json_t *au2_identity_id_for(au2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *provider = au2_get(in, "provider");
    const json_t *entry = au2_get(in, "entry");
    /* token = str(getattr(entry,"runtime_api_key","") or "").strip() */
    char *ts = au2_pystr_or_empty(au2_get(entry, "runtime_api_key"));
    char *token = au2_strip(ts);
    free(ts);
    if (!token || !token[0]) {
        free(token);
        /* f"entry:{getattr(entry,'id','')}" -- missing -> "", present-null -> "None" */
        const json_t *idn = (entry && entry->type == JSON_OBJECT) ? json_obj_get(entry, "id") : NULL;
        char *ids = idn ? au2_pystr(idn) : au2_xfmt("");
        char *out = au2_xfmt("entry:%s", ids ? ids : "");
        free(ids);
        json_t *r = json_string(out ? out : "");
        free(out);
        return r;
    }
    const char *prov = au2_str(provider);
    if (prov && strcmp(prov, "openai-codex") == 0) {
        json_t *pa = au2_args();
        json_set(pa, "token", json_string(token));
        json_t *principal = au2_call(io, "codex_principal_identity", pa);
        if (au2_is_raise(principal)) { free(token); return principal; }
        if (au2_truth(principal) && principal->type == JSON_ARRAY) {
            if (json_len(principal) < 2) {
                json_free(principal); free(token);
                return au2_mkraise("IndexError", "list index out of range");
            }
            char *p0 = au2_pystr(json_get(principal, 0));
            char *p1 = au2_pystr(json_get(principal, 1));
            char *out = au2_xfmt("codex:%s:%s", p0 ? p0 : "", p1 ? p1 : "");
            free(p0); free(p1);
            json_free(principal); free(token);
            json_t *r = json_string(out ? out : "");
            free(out);
            return r;
        }
        json_free(principal);
    }
    /* fp: hashlib.sha256(token.encode()).hexdigest()[:16] */
    json_t *ha = au2_args();
    json_set(ha, "text", json_string(token));
    free(token);
    json_t *hex = au2_call(io, "sha256_hexdigest", ha);
    if (au2_is_raise(hex)) return hex;
    const char *hs = au2_str(hex);
    if (!hs) hs = "";
    size_t n = strlen(hs);
    if (n > 16) n = 16;
    char *out = (char *)malloc(n + 4);
    if (out) { memcpy(out, "fp:", 3); memcpy(out + 3, hs, n); out[n + 3] = '\0'; }
    json_free(hex);
    json_t *r = json_string(out ? out : "fp:");
    free(out);
    return r;
}

/* PoP: au2_remember_account_usage @ agent/account_usage_cache.py:remember_account_usage */
json_t *au2_remember_account_usage(au2_io_t *io, const json_t *in, json_t *st)
{
    au2_state_t state;
    au2_state_seed(&state, in);
    const json_t *provider = au2_get(in, "provider");
    const json_t *snapshot = au2_get(in, "snapshot");
    const json_t *identity_id = au2_get(in, "identity_id");
    const json_t *base_url = au2_get(in, "base_url");
    const json_t *started_monotonic = au2_get(in, "started_monotonic");

    if (!au2_truth(provider) || !au2_present(snapshot) || !au2_truth(au2_get(snapshot, "windows"))) {
        au2_state_emit(st, &state);
        au2_state_free(&state);
        return NULL;
    }
    double started;
    if (au2_present(started_monotonic)) {
        started = au2_num(started_monotonic);
    } else {
        json_t *sm = au2_call(io, "time.monotonic", au2_args());
        if (au2_is_raise(sm)) { au2_state_emit(st, &state); au2_state_free(&state); return sm; }
        started = au2_num(sm);
        json_free(sm);
    }
    const json_t *snapshot_identity = au2_get(snapshot, "identity");
    /* build target keystrs */
    char *targets[2];
    int ntargets = 0;
    json_t *err = NULL;
    if (!au2_present(identity_id)) {
        json_t *k1 = au2_key_core(io, provider, NULL, base_url);
        if (au2_is_raise(k1)) err = k1;
        else { targets[ntargets++] = au2_keystr(k1); json_free(k1); }
        if (!err && au2_truth(snapshot_identity)) {
            json_t *k2 = au2_key_core(io, provider, snapshot_identity, NULL);
            if (au2_is_raise(k2)) err = k2;
            else { targets[ntargets++] = au2_keystr(k2); json_free(k2); }
        }
    } else {
        const json_t *id_used = au2_truth(snapshot_identity) ? snapshot_identity : identity_id;
        json_t *k1 = au2_key_core(io, provider, id_used, NULL);
        if (au2_is_raise(k1)) err = k1;
        else { targets[ntargets++] = au2_keystr(k1); json_free(k1); }
    }
    if (err) {
        for (int i = 0; i < ntargets; i++) free(targets[i]);
        au2_state_emit(st, &state);
        au2_state_free(&state);
        return err;
    }
    for (int i = 0; i < ntargets; i++) {
        json_t *prev = json_obj_get(state.started, targets[i]);
        if (prev && au2_num(prev) > started) { free(targets[i]); continue; }
        json_set(state.snapshots, targets[i], json_copy(snapshot));
        json_set(state.started, targets[i], json_number(started));
        free(targets[i]);
    }
    au2_state_emit(st, &state);
    au2_state_free(&state);
    return NULL;
}

/* PoP: au2_cached_account_usage @ agent/account_usage_cache.py:cached_account_usage */
json_t *au2_cached_account_usage(au2_io_t *io, const json_t *in, json_t *st)
{
    au2_state_t state;
    au2_state_seed(&state, in);
    json_t *k = au2_key_core(io, au2_get(in, "provider"), au2_get(in, "identity_id"),
                             au2_get(in, "base_url"));
    if (au2_is_raise(k)) { au2_state_emit(st, &state); au2_state_free(&state); return k; }
    char *ks = au2_keystr(k);
    json_free(k);
    json_t *found = json_obj_get(state.snapshots, ks ? ks : "");
    json_t *ret = found ? json_copy(found) : NULL;
    free(ks);
    au2_state_emit(st, &state);
    au2_state_free(&state);
    return ret;
}

/* PoP: au2_snapshot_is_stale @ agent/account_usage_cache.py:snapshot_is_stale */
json_t *au2_snapshot_is_stale(au2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *snapshot = au2_get(in, "snapshot");
    if (!snapshot || snapshot->type != JSON_OBJECT) return json_bool(true);
    json_t *now = au2_call(io, "time.time", au2_args());
    if (au2_is_raise(now)) return now;
    double nowv = au2_num(now);
    json_free(now);
    const json_t *fa = au2_get(snapshot, "fetched_at");
    double ts;
    if (fa && fa->type == JSON_NUMBER) {
        ts = fa->num_val / 1e6;
    } else if (fa && au2_is_raise(fa)) {
        static const char *const caught[] = {
            "AttributeError", "TypeError", "ValueError", "OSError", "OverflowError"
        };
        if (au2_caught_by(au2_kind(fa), caught, 5)) return json_bool(true);
        return json_copy(fa);   /* propagate (e.g. RuntimeError / BaseException trio) */
    } else {
        /* null/missing/other -> None.timestamp() AttributeError -> caught */
        return json_bool(true);
    }
    double age = nowv - ts;
    return json_bool(age < 0.0 || age > 900.0);
}

/* PoP: au2_claim_refresh @ agent/account_usage_cache.py:_claim_refresh */
json_t *au2_claim_refresh(au2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    au2_state_t state;
    au2_state_seed(&state, in);
    char *ks = au2_keystr(au2_get(in, "key"));
    double now = au2_num(au2_get(in, "now"));
    int claimed = au2_claim_core(&state, ks ? ks : "", now);
    free(ks);
    au2_state_emit(st, &state);
    au2_state_free(&state);
    return json_bool(claimed);
}

/* PoP: au2_release_refresh @ agent/account_usage_cache.py:_release_refresh */
json_t *au2_release_refresh(au2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    au2_state_t state;
    au2_state_seed(&state, in);
    char *ks = au2_keystr(au2_get(in, "key"));
    au2_release_core(&state, ks ? ks : "");
    free(ks);
    au2_state_emit(st, &state);
    au2_state_free(&state);
    return NULL;
}

/* thread construction projection for refresh_account_usage_async */
static json_t *au2_legacy_thread_proj(const char *provider, const json_t *keyarr)
{
    json_t *p = json_object();
    json_set(p, "fn", json_string("_refresh_legacy"));
    json_set(p, "provider", json_string(provider ? provider : ""));
    json_set(p, "key", json_copy(keyarr));
    return p;
}

/* PoP: au2_refresh_account_usage_async @ agent/account_usage_cache.py:refresh_account_usage_async */
json_t *au2_refresh_account_usage_async(au2_io_t *io, const json_t *in, json_t *st)
{
    au2_state_t state;
    au2_state_seed(&state, in);
    json_t *now = au2_call(io, "time.monotonic", au2_args());
    if (au2_is_raise(now)) { au2_state_emit(st, &state); au2_state_free(&state); return now; }
    double nowv = au2_num(now);
    json_free(now);
    const json_t *providers = au2_get(in, "providers");
    json_t *started = json_array();
    /* dict.fromkeys(providers): dedupe preserving first occurrence */
    json_t *seen = json_object();
    for (size_t i = 0; providers && providers->type == JSON_ARRAY && i < json_len(providers); i++) {
        json_t *pn = json_get(providers, i);
        const char *ps = au2_str(pn);
        if (!ps) continue;
        if (json_obj_get(seen, ps)) continue;
        json_set(seen, ps, json_bool(true));
        json_t *key = au2_key_core(io, pn, NULL, NULL);
        if (au2_is_raise(key)) {
            json_free(seen); json_free(started);
            au2_state_emit(st, &state); au2_state_free(&state);
            return key;
        }
        char *ks = au2_keystr(key);
        int claimed = au2_claim_core(&state, ks ? ks : "", nowv);
        free(ks);
        if (!claimed) { json_free(key); continue; }
        json_t *ctx = au2_call(io, "contextvars.copy_context", au2_args());
        if (au2_is_raise(ctx)) {
            json_free(key); json_free(seen); json_free(started);
            au2_state_emit(st, &state); au2_state_free(&state);
            return ctx;
        }
        if (!au2_present(ctx)) {
            json_free(ctx); json_free(key); json_free(seen); json_free(started);
            au2_state_emit(st, &state); au2_state_free(&state);
            return au2_attr_raise(NULL, "run");
        }
        json_t *ta = au2_args();
        json_set(ta, "target", json_string("context.run"));
        json_set(ta, "fn", json_string("_refresh_legacy"));
        json_set(ta, "provider", json_string(ps));
        json_set(ta, "key", json_copy(key));
        json_set(ta, "name", json_string("hermes-account-usage-refresh"));
        json_set(ta, "daemon", json_bool(true));
        json_t *thread = au2_call(io, "threading.Thread", ta);
        if (au2_is_raise(thread)) {
            json_free(ctx); json_free(key); json_free(seen); json_free(started);
            au2_state_emit(st, &state); au2_state_free(&state);
            return thread;
        }
        if (!au2_present(thread)) {
            json_free(thread); json_free(ctx); json_free(key); json_free(seen); json_free(started);
            au2_state_emit(st, &state); au2_state_free(&state);
            return au2_attr_raise(NULL, "start");
        }
        json_t *sra = au2_args();
        json_set(sra, "thread", json_copy(thread));
        json_t *sr = au2_call(io, "thread.start", sra);
        if (au2_is_raise(sr)) {
            json_free(thread); json_free(ctx); json_free(key); json_free(seen); json_free(started);
            au2_state_emit(st, &state); au2_state_free(&state);
            return sr;
        }
        json_free(sr);
        json_append(started, au2_legacy_thread_proj(ps, key));
        json_free(thread); json_free(ctx); json_free(key);
    }
    json_free(seen);
    au2_state_emit(st, &state);
    au2_state_free(&state);
    return started;
}

/* PoP: au2_refresh_legacy @ agent/account_usage_cache.py:_refresh_legacy */
json_t *au2_refresh_legacy(au2_io_t *io, const json_t *in, json_t *st)
{
    au2_state_t state;
    au2_state_seed(&state, in);
    const json_t *provider = au2_get(in, "provider");
    char *ks = au2_keystr(au2_get(in, "key"));
    json_t *fa = au2_args();
    json_set(fa, "provider", au2_copy_or_null(provider));
    json_t *r = au2_call(io, "fetch_account_usage", fa);
    json_t *pending = au2_is_raise(r) ? r : NULL;
    if (!pending) json_free(r);
    au2_release_core(&state, ks ? ks : "");   /* finally */
    free(ks);
    au2_state_emit(st, &state);
    au2_state_free(&state);
    return pending;   /* NULL on success */
}

/* PoP: au2_refresh_account_usage_entries_async @ agent/account_usage_cache.py:refresh_account_usage_entries_async */
json_t *au2_refresh_account_usage_entries_async(au2_io_t *io, const json_t *in, json_t *st)
{
    au2_state_t state;
    au2_state_seed(&state, in);
    json_t *now = au2_call(io, "time.monotonic", au2_args());
    if (au2_is_raise(now)) { au2_state_emit(st, &state); au2_state_free(&state); return now; }
    double nowv = au2_num(now);
    json_free(now);
    const json_t *requests = au2_get(in, "requests");
    json_t *due = json_array();
    for (size_t i = 0; requests && requests->type == JSON_ARRAY && i < json_len(requests); i++) {
        json_t *req = json_get(requests, i);
        char *ps = au2_slug(au2_get(req, "provider"));
        char *is = au2_pystr_or_empty(au2_get(req, "identity_id"));
        char *ident = au2_strip(is); free(is);
        char *as = au2_pystr_or_empty(au2_get(req, "api_key"));
        char *apikey = au2_strip(as); free(as);
        int valid = (ps && ps[0] && ident && ident[0] && apikey && apikey[0]);
        free(apikey);
        if (!valid) { free(ps); free(ident); continue; }
        json_t *key = au2_key_core(io, json_string(ps), json_string(ident), NULL);
        free(ps); free(ident);
        if (au2_is_raise(key)) {
            json_free(due);
            au2_state_emit(st, &state); au2_state_free(&state);
            return key;
        }
        char *ks = au2_keystr(key);
        json_free(key);
        int claimed = au2_claim_core(&state, ks ? ks : "", nowv);
        free(ks);
        if (!claimed) continue;
        json_append(due, json_copy(req));
    }
    if (json_len(due) == 0) {
        json_free(due);
        au2_state_emit(st, &state); au2_state_free(&state);
        return json_array();
    }
    json_t *ctx = au2_call(io, "contextvars.copy_context", au2_args());
    if (au2_is_raise(ctx)) {
        json_free(due);
        au2_state_emit(st, &state); au2_state_free(&state);
        return ctx;
    }
    if (!au2_present(ctx)) {
        json_free(ctx); json_free(due);
        au2_state_emit(st, &state); au2_state_free(&state);
        return au2_attr_raise(NULL, "run");
    }
    json_t *ta = au2_args();
    json_set(ta, "target", json_string("context.run"));
    json_set(ta, "fn", json_string("_refresh_entries"));
    json_set(ta, "due", json_copy(due));
    json_set(ta, "name", json_string("hermes-account-usage-refresh"));
    json_set(ta, "daemon", json_bool(true));
    json_t *thread = au2_call(io, "threading.Thread", ta);
    if (au2_is_raise(thread)) {
        json_free(ctx); json_free(due);
        au2_state_emit(st, &state); au2_state_free(&state);
        return thread;
    }
    if (!au2_present(thread)) {
        json_free(thread); json_free(ctx); json_free(due);
        au2_state_emit(st, &state); au2_state_free(&state);
        return au2_attr_raise(NULL, "start");
    }
    json_t *sra = au2_args();
    json_set(sra, "thread", json_copy(thread));
    json_t *sr = au2_call(io, "thread.start", sra);
    if (au2_is_raise(sr)) {
        json_free(thread); json_free(ctx); json_free(due);
        au2_state_emit(st, &state); au2_state_free(&state);
        return sr;
    }
    json_free(sr);
    json_t *construction = json_object();
    json_set(construction, "fn", json_string("_refresh_entries"));
    json_set(construction, "due", json_copy(due));
    json_t *ret = json_array();
    json_append(ret, construction);
    json_free(thread); json_free(ctx); json_free(due);
    au2_state_emit(st, &state);
    au2_state_free(&state);
    return ret;
}

/* PoP: au2_refresh_entries @ agent/account_usage_cache.py:_refresh_entries */
json_t *au2_refresh_entries(au2_io_t *io, const json_t *in, json_t *st)
{
    au2_state_t state;
    au2_state_seed(&state, in);
    const json_t *requests = au2_get(in, "requests");
    for (size_t i = 0; requests && requests->type == JSON_ARRAY && i < json_len(requests); i++) {
        json_t *req = json_get(requests, i);
        /* provider = request["provider"] -- KeyError if MISSING (present-null ok) */
        json_t *prov = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "provider") : NULL;
        if (!prov) {
            au2_state_emit(st, &state); au2_state_free(&state);
            return au2_mkraise("KeyError", "'provider'");
        }
        json_t *ident = json_obj_get(req, "identity_id");
        if (!ident) {
            au2_state_emit(st, &state); au2_state_free(&state);
            return au2_mkraise("KeyError", "'identity_id'");
        }
        json_t *key = au2_key_core(io, prov, ident, NULL);
        if (au2_is_raise(key)) {
            au2_state_emit(st, &state); au2_state_free(&state);
            return key;
        }
        char *ks = au2_keystr(key);
        json_free(key);
        /* try: fetch_account_usage(provider, base_url=, api_key=, read_only=True, identity_id=) */
        json_t *fa = au2_args();
        json_set(fa, "provider", json_copy(prov));
        json_set(fa, "base_url", au2_copy_or_null(au2_get(req, "base_url")));
        json_set(fa, "api_key", au2_copy_or_null(au2_get(req, "api_key")));
        json_set(fa, "read_only", json_bool(true));
        json_set(fa, "identity_id", json_copy(ident));
        json_t *r = au2_call(io, "fetch_account_usage", fa);
        json_t *pending = au2_is_raise(r) ? r : NULL;
        if (!pending) json_free(r);
        au2_release_core(&state, ks ? ks : "");   /* finally */
        free(ks);
        if (pending) {
            au2_state_emit(st, &state); au2_state_free(&state);
            return pending;   /* propagate out of the loop */
        }
    }
    au2_state_emit(st, &state);
    au2_state_free(&state);
    return NULL;
}