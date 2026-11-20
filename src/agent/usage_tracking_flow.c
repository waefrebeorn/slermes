/* ===========================================================================
 * usage_tracking_flow.c -- FLOW + leaf port of the /usage account-usage
 * tracking cluster. The task named agent/usage_tracking.py, which does NOT
 * exist in the quarry; it resolves to the REAL defining modules:
 *   agent/account_usage.py        (the /usage facade: snapshot/window types,
 *                                  Codex quota fetch + reset redemption,
 *                                  provider plan-limit lines, plugin hook)
 *   agent/account_usage_nous.py   (the Nous credits / usage-window views)
 * Each PoP credits the module that DEFINES the function (the facade late-
 * imports the Nous sibling; the scanner credits the defining module).
 *
 * Self-contained: libjson only. Every live dependency that is NOT ported here
 * is injected through ONE universal seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * args is a BORROWED object (the port owns it and frees it after the call);
 * the result is a NEW json_t owned by the caller (NULL == None; a
 * {"__raise__":kind,"message":msg,...} object == a raised exception). A
 * scripted JSON null result is delivered as a NULL pointer so truthiness
 * reads falsy. Fail-closed: no channel installed, or a missing/exhausted
 * script, yields {"__raise__":"RuntimeError","message":"no script for <name>"}
 * / "no seam installed for <name>", recorded by the dispatcher BEFORE the
 * port propagates it.
 *
 * ENTRIES. json_t *ut_<fn>(ut_io_t *io, const json_t *in, json_t *st)
 *   io  -- universal seam channel (NULL => fail-closed production default)
 *   in  -- borrowed case input object (never mutated)
 *   st  -- fresh object the entry fills with the final state projection.
 *          EVERY ported function here is pure (no carrier mutation), so st
 *          stays {} on success AND on every raise path. Caller owns the
 *          returned json_t. Tuples project to JSON arrays; None to NULL.
 *
 * Ported siblings compose REAL:
 *   _nous_snapshot -> _snapshot; _usage_window_rows -> _bp_window +
 *   _window_top_tier_line; apply_header_gauge -> _header_gauge_percent +
 *   _window_top_tier_line; _fetch_codex_account_usage_impl -> _codex_headers +
 *   _usage_windows + _codex_window_labels + _codex_banked_resets + _is_num +
 *   _snapshot + _codex_snapshot_identity; _fetch_codex_account_usage_read_only
 *   -> _fetch_codex_account_usage_impl; _codex_reset_outcome -> _unavailable +
 *   _is_num; _codex_reset_guard -> _is_num; nous_credits_view /
 *   nous_credits_usage_fields / nous_usage_view -> _nous_account_view;
 *   _nous_account_view -> _nous_logged_in + apply_header_gauge;
 *   _render_usage_window_lines -> _reset_phrase.
 * Claimed siblings are SEAMED (never re-claimed): _utc_now, _parse_dt,
 * _format_reset, _title_case_slug, _plural, _get_json, _codex_backend_urls,
 * _resolve_codex_usage_credentials, render_account_usage_lines,
 * fetch_account_usage, _snapshot_from_credits_state,
 * build_nous_credits_snapshot, nous_credits_lines, _dev_fixture_usage_model,
 * usage_model_from_account. The UNCLAIMED _fetch_portal_account (thread-pool
 * mechanics) is deferred and SEAMED as a collaborator of _nous_account_view.
 *
 * BaseException kinds (SystemExit, KeyboardInterrupt, GeneratorExit,
 * CancelledError, BaseException, BaseExceptionGroup) PASS THROUGH every
 * `except Exception` site. A FULL builtin exception-parent table (ut_exc_
 * parent, probed on the quarry CPython 3.14.9) drives ut_is_exception so a
 * scripted RuntimeError/ValueError/... is swallowed while the trio propagates.
 * The one `except httpx.HTTPStatusError` site matches exactly that kind.
 *
 * Seam name -> live call table:
 *   utc_now                          agent.account_usage._utc_now()
 *   parse_dt                         agent.account_usage._parse_dt(value)
 *   format_reset                     agent.account_usage._format_reset(dt)
 *   title_case_slug                  agent.account_usage._title_case_slug(value)
 *   plural                           agent.account_usage._plural(count)
 *   get_json                         agent.account_usage._get_json(url,headers,timeout=)
 *   codex_backend_urls               agent.account_usage._codex_backend_urls(base_url)
 *   resolve_codex_usage_credentials  agent.account_usage._resolve_codex_usage_credentials(base_url,api_key,force_refresh=)
 *   render_account_usage_lines       agent.account_usage.render_account_usage_lines(snapshot,markdown=)
 *   fetch_account_usage              agent.account_usage.fetch_account_usage(provider,base_url=,api_key=)
 *   snapshot_from_credits_state      agent.account_usage_nous._snapshot_from_credits_state(state)
 *   build_nous_credits_snapshot      agent.account_usage_nous.build_nous_credits_snapshot(account,cta=)
 *   nous_credits_lines               agent.account_usage_nous.nous_credits_lines(markdown=,credits_state=,nous_route=)
 *   fetch_portal_account             agent.account_usage_nous._fetch_portal_account(timeout,stored_token_only=)
 *   dev_fixture_usage_model          agent.billing_usage._dev_fixture_usage_model()
 *   usage_model_from_account         agent.billing_usage.usage_model_from_account(account)
 *   codex_account_headers            agent.codex_headers.codex_account_headers(token)
 *   codex_principal_identity         agent.credential_pool._codex_principal_identity(token)
 *   clear_codex_pool_quota_cooldowns hermes_cli.auth.clear_codex_pool_quota_cooldowns()
 *   get_provider_auth_state          hermes_cli.auth.get_provider_auth_state(provider)
 *   format_resets_in                 hermes_cli.model_picker_usage.format_resets_in(ms)
 *   render_bar                       hermes_cli.model_picker_usage.render_bar(used)
 *   format_percent_used              hermes_cli.model_picker_usage.format_percent_used(used)
 *   run_bounded_sync                 agent.deadline.run_bounded_sync(fn,timeout,label=)
 *   parse_gauges                     agent.usage_gauges.parse_gauges(gauges)
 *   all_scope                        agent.usage_gauges.all_scope(gauges)
 *   gauge_title                      agent.usage_gauges.gauge_title(gauge)
 *   paused_for_payment               agent.usage_gauges.paused_for_payment(reason)
 *   dev_fixture_credits_state        agent.credits_tracker.dev_fixture_credits_state()
 *   time.time                        time.time()
 *   logger.debug                     logger.debug(fmt,*args,exc_info=)
 *
 * Datetime model: a datetime travels as an INTEGER epoch-MICROSECONDS (UTC).
 * The oracle materializes it to a real datetime (exact integer arithmetic) and
 * projects it back via calendar.timegm+microsecond; the C reads the integer.
 * _bp_window computes reset_at = resets_at_ms*1000 (exact). _window_top_tier_
 * line renders the LOCAL calendar date via localtime() (same host tz the live
 * .astimezone() uses); %b is the English month abbreviation. _reset_phrase
 * reproduces int(dt.timestamp()*1000) as the IEEE double path (us/1e6*1e3 then
 * truncate). Domain: 0 <= epoch-us <= ~year 2100 (Windows localtime/gmtime
 * reject negative time_t; fromtimestamp float rounding stays < 0.5us).
 *
 * State model: all entries pure; st = {} always (harness + oracle agree).
 *
 * Domain restrictions (documented, enforced by the generator):
 *   - ASCII inputs; strip()/lower() operate on ASCII whitespace/letters.
 *   - used_bp / resets_at_ms / available_count / seconds are INT-typed JSON
 *     literals (no integral-float ".0" form: libjson has no int/float identity,
 *     so an integral float is outside the domain). Non-integral floats ARE
 *     admitted to exercise the isinstance-int rejection branches.
 *   - bool is rejected by the isinstance-int gates (used_bp/resets_at_ms) but
 *     admitted by _is_num (isinstance(v,(int,float)) admits bool).
 *   - snapshot / window / gauge / credits_state scripts are null or NON-EMPTY
 *     carriers (an empty {} double is truthy live but falsy under json count>0).
 *   - _get_json / payload scripts are dict-or-raise (a truthy non-dict .get
 *     raises AttributeError, modeled); null payload raises NoneType.get.
 *   - HTTPStatusError raises carry a status_code field (exc.response.status_code).
 *   - resolve_codex_usage_credentials returns a 3-element array (or raise).
 *   - codex_principal_identity returns null or a 2-element array.
 *   - {body!r} repr domain: ASCII str/int/bool/null/nested dict/list (no floats).
 *   - float .2f/.0f positions use exact round-half-even (ut_fmt_fixed), probed
 *     against Python format() on a randomized corpus.
 * =========================================================================== */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

typedef struct ut_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} ut_io_t;

/* ---- truthiness / accessors ---- */

static int ut_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

/* `is not None` identity: present-non-null (an object double is always truthy
 * live; empty {} stays present here). */
static int ut_present(const json_t *v)
{
    return (v && v->type != JSON_NULL) ? 1 : 0;
}

static const char *ut_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *ut_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double ut_num(const json_t *v)
{
    if (v && v->type == JSON_NUMBER) return v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return 0.0;
}

/* isinstance(v,(int,float)) -- admits bool (bool is a subclass of int). */
static int ut_is_num(const json_t *v)
{
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}

/* isinstance(v,int) and not isinstance(v,bool): an integral, finite, non-bool
 * number. libjson has no int/float identity, so integral == int here; the
 * generator keeps used_bp/resets_at_ms INT-typed (non-integral floats exercise
 * the rejection branch). */
static int ut_is_int(const json_t *v)
{
    if (!v || v->type != JSON_NUMBER) return 0;
    double d = v->num_val;
    if (!isfinite(d)) return 0;
    return d == floor(d) ? 1 : 0;
}

static int ut_is_bool(const json_t *v)
{
    return v && v->type == JSON_BOOL;
}

static json_t *ut_copy_or_null(const json_t *v)
{
    return v ? json_copy(v) : NULL;
}

/* ---- raise specs ---- */

static int ut_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *ut_kind(const json_t *r)
{
    const char *k = ut_str(ut_get(r, "__raise__"));
    return k ? k : "";
}

static json_t *ut_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

/* FULL builtin exception-parent hierarchy (probed on quarry CPython 3.14.9).
 * ExceptionGroup has two bases (BaseExceptionGroup, Exception); it IS an
 * Exception, handled specially in ut_is_exception. CancelledError is the
 * asyncio extension (BaseException since 3.8). */
typedef struct { const char *name; const char *parent; } ut_exc_row;
static const ut_exc_row UT_EXC_PARENT[] = {
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
    {"EnvironmentError", "Exception"},
    {"IOError", "Exception"},
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
    {"WindowsError", "Exception"},
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

static const char *ut_exc_parent(const char *kind)
{
    for (size_t i = 0; UT_EXC_PARENT[i].name; i++)
        if (strcmp(UT_EXC_PARENT[i].name, kind) == 0)
            return UT_EXC_PARENT[i].parent;
    return NULL;
}

/* True iff the scripted kind is caught by `except Exception`. Unknown kinds
 * are synthetic Exception subclasses in the oracle (type(kind,(Exception,))),
 * so they ARE exceptions. ExceptionGroup is an Exception via its second base. */
static int ut_is_exception(const char *kind)
{
    if (strcmp(kind, "ExceptionGroup") == 0) return 1;
    const char *cur = kind;
    for (int depth = 0; depth < 32 && cur; depth++) {
        if (strcmp(cur, "Exception") == 0) return 1;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return 0;
        cur = ut_exc_parent(cur);
        if (!cur) return 1; /* unknown -> synthetic Exception subclass */
    }
    return 1;
}

/* malloc'd printf-style string builder */
static char *ut_xfmt(const char *fmt, ...)
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
static json_t *ut_call(ut_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = ut_xfmt("no seam installed for %s", name);
        r = ut_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *ut_args(void)
{
    return json_object();
}

/* logger.debug(fmt, exc_info=True): no positional args, exc_info kwarg. */
static json_t *ut_log_exc(ut_io_t *io, const char *fmt)
{
    json_t *a = ut_args();
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", json_array());
    json_set(a, "exc_info", json_bool(true));
    return ut_call(io, "logger.debug", a);
}
/* ---- string helpers (ASCII domain) ---- */

static int ut_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}

static char *ut_strip(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && ut_is_ws((unsigned char)s[a])) a++;
    while (b > a && ut_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *ut_lower(const char *s)
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

static int ut_startswith(const char *s, const char *prefix)
{
    if (!s || !prefix) return 0;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* str(x or "") projection: falsy -> "", else str(x). Caller frees. */
static char *ut_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return ut_xfmt("None");
    if (v->type == JSON_BOOL) return ut_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == 0.0) return ut_xfmt("0");
        if (isfinite(d) && d == floor(d) && fabs(d) < 1e16) return ut_xfmt("%.0f", d);
        return ut_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return ut_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize(v);
    return s ? s : ut_xfmt("");
}

static char *ut_pystr_or_empty(const json_t *v)
{
    if (!ut_truth(v)) return ut_xfmt("");
    return ut_pystr(v);
}

/* ---- Python repr (ASCII domain: str/int/bool/null/nested dict/list) ---- */

static char *ut_repr_str(const char *s)
{
    if (!s) s = "";
    int has_sq = 0, has_dq = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '\'') has_sq = 1;
        if (*p == '"') has_dq = 1;
    }
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3, n = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[n++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\' ) { out[n++] = '\\'; out[n++] = '\\'; }
        else if (c == '\n') { out[n++] = '\\'; out[n++] = 'n'; }
        else if (c == '\r') { out[n++] = '\\'; out[n++] = 'r'; }
        else if (c == '\t') { out[n++] = '\\'; out[n++] = 't'; }
        else if (c == (unsigned char)q) { out[n++] = '\\'; out[n++] = (char)c; }
        else if (c < 0x20) { n += (size_t)sprintf(out + n, "\\x%02x", c); }
        else out[n++] = (char)c;
    }
    out[n++] = q;
    out[n] = '\0';
    return out;
}

static char *ut_repr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return ut_xfmt("None");
    if (v->type == JSON_BOOL) return ut_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isfinite(d) && d == floor(d) && fabs(d) < 1e16) return ut_xfmt("%.0f", d);
        return ut_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return ut_repr_str(v->str_val ? v->str_val : "");
    if (v->type == JSON_ARRAY) {
        size_t cap = 64, n = 0;
        char *out = (char *)malloc(cap);
        if (!out) return NULL;
        out[n++] = '[';
        for (size_t i = 0; i < v->c.count; i++) {
            char *item = ut_repr(v->c.items[i]);
            size_t need = n + (item ? strlen(item) : 4) + 4;
            if (need >= cap) { while (need >= cap) cap *= 2; out = (char *)realloc(out, cap); }
            if (i) { out[n++] = ','; out[n++] = ' '; }
            if (item) { memcpy(out + n, item, strlen(item)); n += strlen(item); free(item); }
            else { memcpy(out + n, "None", 4); n += 4; }
        }
        out[n++] = ']'; out[n] = '\0';
        return out;
    }
    if (v->type == JSON_OBJECT) {
        size_t cap = 64, n = 0;
        char *out = (char *)malloc(cap);
        if (!out) return NULL;
        out[n++] = '{';
        for (size_t i = 0; i < v->c.count; i++) {
            char *k = ut_repr_str(v->c.keys[i]);
            char *val = ut_repr(v->c.items[i]);
            size_t need = n + (k ? strlen(k) : 2) + (val ? strlen(val) : 4) + 8;
            if (need >= cap) { while (need >= cap) cap *= 2; out = (char *)realloc(out, cap); }
            if (i) { out[n++] = ','; out[n++] = ' '; }
            if (k) { memcpy(out + n, k, strlen(k)); n += strlen(k); free(k); }
            out[n++] = ':'; out[n++] = ' ';
            if (val) { memcpy(out + n, val, strlen(val)); n += strlen(val); free(val); }
            else { memcpy(out + n, "None", 4); n += 4; }
        }
        out[n++] = '}'; out[n] = '\0';
        return out;
    }
    return ut_xfmt("None");
}

/* ---- exact Python format(v, ".Nf"), N in {0,1,2}: round-half-to-even on the
 * EXACT binary value via integer mantissa/exponent arithmetic (no libc printf
 * rounding, which differs). Domain: finite |v| < 1e15. Probed against Python
 * format() on a randomized corpus. ---- */
static char *ut_fmt_fixed(double v, int prec)
{
    if (isnan(v)) return ut_xfmt("nan");
    if (isinf(v)) return ut_xfmt(v < 0 ? "-inf" : "inf");
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    int neg = (bits >> 63) ? 1 : 0;
    uint64_t mant = bits & ((1ULL << 52) - 1);
    int expf = (int)((bits >> 52) & 0x7FF);
    int e;
    if (expf == 0) { e = -1074; }
    else { mant |= (1ULL << 52); e = expf - 1023 - 52; }
    unsigned __int128 scale = (prec == 2) ? 100u : (prec == 1 ? 10u : 1u);
    unsigned __int128 M = (unsigned __int128)mant * scale;
    unsigned __int128 Q;
    if (e >= 0) {
        Q = M << e;
    } else {
        int shift = -e;
        if (shift >= 61) {
            Q = 0; /* M < 2^60 -> M/2^shift < 0.5, 2M < 2^shift -> rounds to 0 */
        } else {
            unsigned __int128 base = (unsigned __int128)1 << shift;
            Q = M >> shift;
            unsigned __int128 rem = M & (base - 1);
            unsigned __int128 twice = rem << 1;
            if (twice > base || (twice == base && (Q & 1))) Q++;
        }
    }
    unsigned __int128 ip = Q / scale;
    unsigned long long fp = (unsigned long long)(Q % scale);
    if (prec == 0) return ut_xfmt("%s%llu", neg ? "-" : "", (unsigned long long)ip);
    return ut_xfmt("%s%llu.%0*llu", neg ? "-" : "", (unsigned long long)ip, prec, fp);
}

/* ---- datetime model (epoch-microseconds, UTC) ---- */

static const char *UT_MONTHS[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* LOCAL calendar date of an epoch-us stamp (host tz, same as .astimezone()).
 * Returns 0 on failure (negative time_t outside the domain). */
static int ut_local_date(long long epoch_us, int *mon, int *mday)
{
    time_t secs = (time_t)(epoch_us / 1000000);
    struct tm tmv;
#ifdef _WIN32
    if (localtime_s(&tmv, &secs) != 0) return 0;
#else
    if (!localtime_r(&secs, &tmv)) return 0;
#endif
    *mon = tmv.tm_mon;
    *mday = tmv.tm_mday;
    return 1;
}

/* int(dt.timestamp() * 1000): the IEEE double path CPython uses
 * (total_us/1e6 then *1e3 then truncate toward zero). Domain epoch-us >= 0. */
static long long ut_dt_ms(long long epoch_us)
{
    double ts = (double)epoch_us / 1000000.0;
    double ms = ts * 1000.0;
    return (long long)ms;
}

/* read an epoch-us datetime carrier (a number) as long long; present flag. */
static int ut_dt_present(const json_t *v)
{
    return v && v->type == JSON_NUMBER;
}
static long long ut_dt_us(const json_t *v)
{
    return (long long)ut_num(v);
}
/* ---- Python type-name projection (for AttributeError/TypeError messages) ---- */
static const char *ut_typename(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
    case JSON_BOOL: return "bool";
    case JSON_NUMBER: return "int";
    case JSON_STRING: return "str";
    case JSON_ARRAY: return "list";
    case JSON_OBJECT: return "dict";
    default: return "object";
    }
}

/* ---- live message constants (byte-exact, hex-escaped non-ASCII) ---- */
#define UT_NO_CREDITS_BANKED "No banked reset credits on this account \xe2\x80\x94 nothing to redeem."
#define UT_GUARD_P1 "\xe2\x9a\xa0\xef\xb8\x8f Not redeeming: "
#define UT_GUARD_P2 ". A banked reset restores your FULL 5h + weekly limits, so spending it now would waste most of it. You have "
#define UT_GUARD_P3 " reset"
#define UT_GUARD_P4 " banked. Use `/usage reset --force` to redeem anyway."
#define UT_NOTE_BUSY "your busiest window is only "
#define UT_NOTE_BUSY_SUF "% used"
#define UT_NOTE_UNCONF "your current usage could not be confirmed as exhausted"
#define UT_RESET_P1 "\xe2\x9c\x85 Reset redeemed \xe2\x80\x94 your usage limits have been reset. "
#define UT_RESET_P2 " banked reset"
#define UT_RESET_P3 " remaining."
#define UT_NOTHING_TO_RESET "Backend reports nothing to reset \xe2\x80\x94 your limits aren't exhausted. The credit was NOT spent."
#define UT_NO_CREDIT "Backend reports no available reset credit on this account."
#define UT_ALREADY_REDEEMED "This redemption was already processed \xe2\x80\x94 no additional credit was spent."
#define UT_UNAVAIL_PREFIX "Unexpected response from the Codex backend: "
#define UT_SEP_DOT "  \xc2\xb7  "
#define UT_LOG_PORTAL "credits \xe2\x96\xb8 /usage portal fetch/render failed (fail-open)"
#define UT_LOG_FIELDS "credits \xe2\x96\xb8 /usage fields unavailable"
#define UT_LOG_RESET_FAIL "Failed to clear Codex pool cooldowns after reset redemption"
#define UT_LOG_RESET_PHRASE "Reset phrase formatting failed"
#define UT_LOG_BAR "Usage bar rendering failed"
#define UT_LOG_FIXTURE "Dev credits fixture unavailable"
#define UT_TOP_TIER_USED_UP "Monthly large-model usage is used up."
#define UT_RESETS_PREFIX " Resets "
#define UT_RESETS_DOT "."
#define UT_TAIL_KEEP_FALSE " Your other included usage is still available."
#define UT_TAIL_KEEP_OTHER " Past this, large models use your credits."
#define UT_RESETS_SOON "resets soon"
#define UT_RESETS_FB_PREFIX "resets "
#define UT_PAUSED_LINE "Status: included usage paused. Your subscription payment failed. Update your card to restore it. Until then, Hermes uses your credits."
#define UT_PAUSED_OTHER "Status: included usage paused. See Billing in the Portal. Until then, Hermes uses your credits."
#define UT_KEEP_OFF_LINE "Keep going on credits: off. Hermes waits for the reset (turn on in /topup)."
#define UT_KEEP_OFF_STATUS "Keep going on credits: off. Nous sessions wait for the reset."
#define UT_KEEP_ON_LINE "Keep going on credits: on. Uses your credits when included usage runs out."
#define UT_NOUS_HEADING "Nous Portal balance (a separate account: this session runs on another provider)"
#define UT_BANKED_PREFIX "You have "
#define UT_BANKED_SUFFIX " banked - use /usage reset to activate"
#define UT_CREDITS_BALANCE_PREFIX "Credits balance: $"
#define UT_CREDITS_UNLIMITED "Credits balance: unlimited"

/* CodexResetRedeemResult carrier (all four dataclass fields). */
static json_t *ut_redeem_result(const char *status, const char *message,
                                long long available_count, long long windows_reset)
{
    json_t *o = json_object();
    json_set(o, "status", json_string(status));
    json_set(o, "message", json_string(message ? message : ""));
    json_set(o, "available_count", json_number((double)available_count));
    json_set(o, "windows_reset", json_number((double)windows_reset));
    return o;
}

/* _plural seam (claimed sibling): returns "s"/"" for a count. */
static json_t *ut_plural(ut_io_t *io, long long count)
{
    json_t *a = ut_args();
    json_set(a, "count", json_number((double)count));
    return ut_call(io, "plural", a);
}

/* ================= agent/account_usage.py leaves ================= */

/* PoP: ut_is_num_entry @ agent/account_usage.py:_is_num */
json_t *ut_is_num_entry(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return json_bool(ut_is_num(ut_get(in, "v")));
}

static json_t *ut_codex_banked_resets_core(const json_t *payload)
{
    json_t *rlrc = ut_get(payload, "rate_limit_reset_credits");
    json_t *raw = NULL;
    if (ut_truth(rlrc)) {
        if (rlrc->type != JSON_OBJECT) {
            char *m = ut_xfmt("'%s' object has no attribute 'get'", ut_typename(rlrc));
            json_t *r = ut_mkraise("AttributeError", m); free(m); return r;
        }
        raw = ut_get(rlrc, "available_count");
    }
    if (ut_is_num(raw)) return json_number((double)(long long)ut_num(raw));
    return json_number(0);
}

/* PoP: ut_codex_banked_resets @ agent/account_usage.py:_codex_banked_resets */
json_t *ut_codex_banked_resets(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return ut_codex_banked_resets_core(ut_get(in, "payload"));
}

static json_t *ut_codex_window_labels_core(const json_t *rate_limit)
{
    static const char *keys[2] = { "primary_window", "secondary_window" };
    static const char *fallbacks[2] = { "Session", "Weekly" };
    json_t *out = json_array();
    for (int i = 0; i < 2; i++) {
        json_t *window = ut_get(rate_limit, keys[i]);
        json_t *seconds = NULL;
        if (ut_truth(window) && window->type == JSON_OBJECT)
            seconds = ut_get(window, "limit_window_seconds");
        const char *label = fallbacks[i];
        if (ut_is_num(seconds)) {
            long long s = (long long)ut_num(seconds);
            if (s == 18000) label = "Session";
            else if (s == 604800) label = "Weekly";
        }
        json_t *pair = json_array();
        json_append(pair, json_string(keys[i]));
        json_append(pair, json_string(label));
        json_append(out, pair);
    }
    return out;
}

/* PoP: ut_codex_window_labels @ agent/account_usage.py:_codex_window_labels */
json_t *ut_codex_window_labels(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return ut_codex_window_labels_core(ut_get(in, "rate_limit"));
}

static json_t *ut_codex_headers_core(ut_io_t *io, const char *token, const json_t *account_id)
{
    if (!token) token = "";
    json_t *h = json_object();
    char *auth = ut_xfmt("Bearer %s", token);
    json_set(h, "Authorization", json_string(auth)); free(auth);
    json_set(h, "Accept", json_string("application/json"));
    json_set(h, "User-Agent", json_string("codex-cli"));
    json_t *a = ut_args();
    json_set(a, "token", json_string(token));
    json_t *ch = ut_call(io, "codex_account_headers", a);
    if (ut_is_raise(ch)) { json_free(h); return ch; }
    if (!ch || ch->type != JSON_OBJECT) {
        char *m = ut_xfmt("'%s' object is not a mapping", ut_typename(ch));
        json_t *r = ut_mkraise("TypeError", m); free(m);
        json_free(ch); json_free(h); return r;
    }
    for (size_t i = 0; i < ch->c.count; i++)
        json_set(h, ch->c.keys[i], json_copy(ch->c.items[i]));
    json_free(ch);
    if (ut_truth(account_id))
        json_set(h, "ChatGPT-Account-ID", json_copy(account_id));
    return h;
}

/* PoP: ut_codex_headers @ agent/account_usage.py:_codex_headers */
json_t *ut_codex_headers(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ut_codex_headers_core(io, ut_str(ut_get(in, "token")), ut_get(in, "account_id"));
}

static json_t *ut_codex_snapshot_identity_core(ut_io_t *io, const char *token)
{
    if (!token) token = "";
    json_t *a = ut_args();
    json_set(a, "token", json_string(token));
    json_t *p = ut_call(io, "codex_principal_identity", a);
    if (ut_is_raise(p)) return p;
    if (!ut_truth(p)) { json_free(p); return NULL; }
    if (p->type != JSON_ARRAY || json_len(p) < 2) {
        json_t *r = ut_mkraise("IndexError", "list index out of range");
        json_free(p); return r;
    }
    char *p0 = ut_pystr(json_get(p, 0));
    char *p1 = ut_pystr(json_get(p, 1));
    char *s = ut_xfmt("codex:%s:%s", p0 ? p0 : "", p1 ? p1 : "");
    free(p0); free(p1); json_free(p);
    json_t *r = json_string(s); free(s); return r;
}

/* PoP: ut_codex_snapshot_identity @ agent/account_usage.py:_codex_snapshot_identity */
json_t *ut_codex_snapshot_identity(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ut_codex_snapshot_identity_core(io, ut_str(ut_get(in, "token")));
}

static json_t *ut_unavailable_core(const char *msg)
{
    return ut_redeem_result("unavailable", msg ? msg : "", 0, 0);
}

/* PoP: ut_unavailable @ agent/account_usage.py:_unavailable */
json_t *ut_unavailable(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return ut_unavailable_core(ut_str(ut_get(in, "message")));
}

/* _codex_reset_guard core (also composed by nothing here; standalone). */
static json_t *ut_codex_reset_guard_core(ut_io_t *io, const json_t *payload,
                                         long long available, int force)
{
    if (available <= 0)
        return ut_redeem_result("no_credits_banked", UT_NO_CREDITS_BANKED, 0, 0);
    json_t *rate_limit = ut_get(payload, "rate_limit");
    if (ut_truth(rate_limit) && rate_limit->type != JSON_OBJECT) {
        char *m = ut_xfmt("'%s' object has no attribute 'get'", ut_typename(rate_limit));
        json_t *r = ut_mkraise("AttributeError", m); free(m); return r;
    }
    static const char *keys[2] = { "primary_window", "secondary_window" };
    double pcts[2]; int np = 0;
    for (int i = 0; i < 2; i++) {
        json_t *w = ut_get(rate_limit, keys[i]);
        json_t *u = NULL;
        if (ut_truth(w)) {
            if (w->type != JSON_OBJECT) {
                char *m = ut_xfmt("'%s' object has no attribute 'get'", ut_typename(w));
                json_t *r = ut_mkraise("AttributeError", m); free(m); return r;
            }
            u = ut_get(w, "used_percent");
        }
        if (ut_is_num(u)) pcts[np++] = ut_num(u);
    }
    int have_worst = np > 0;
    double worst = 0.0;
    for (int i = 0; i < np; i++) if (pcts[i] > worst) worst = pcts[i];
    if (force || (have_worst && worst >= 100.0)) return NULL;
    char *note;
    if (have_worst) {
        char *w = ut_fmt_fixed(worst, 0);
        note = ut_xfmt("%s%s%s", UT_NOTE_BUSY, w, UT_NOTE_BUSY_SUF);
        free(w);
    } else {
        note = ut_xfmt("%s", UT_NOTE_UNCONF);
    }
    json_t *pl = ut_plural(io, available);
    if (ut_is_raise(pl)) { free(note); return pl; }
    char *pls = ut_pystr(pl); json_free(pl);
    char *msg = ut_xfmt("%s%s%s%lld%s%s%s", UT_GUARD_P1, note, UT_GUARD_P2,
                        available, UT_GUARD_P3, pls ? pls : "", UT_GUARD_P4);
    free(note); free(pls);
    json_t *r = ut_redeem_result("not_exhausted", msg, available, 0);
    free(msg); return r;
}

/* PoP: ut_codex_reset_guard @ agent/account_usage.py:_codex_reset_guard */
json_t *ut_codex_reset_guard(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *payload = ut_get(in, "payload");
    long long available = (long long)ut_num(ut_get(in, "available"));
    int force = ut_truth(ut_get(in, "force"));
    return ut_codex_reset_guard_core(io, payload, available, force);
}

/* PoP: ut_codex_reset_outcome @ agent/account_usage.py:_codex_reset_outcome */
json_t *ut_codex_reset_outcome(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *body = ut_get(in, "body");
    long long available = (long long)ut_num(ut_get(in, "available"));
    json_t *cv = ut_get(body, "code");
    char *s = ut_pystr_or_empty(cv);
    char *stripped = ut_strip(s); free(s);
    char *code = ut_lower(stripped); free(stripped);
    long long remaining = available - 1; if (remaining < 0) remaining = 0;
    /* the outcomes dict literal is built EAGERLY: the "reset" f-string calls
     * _plural(remaining) BEFORE the membership check, on every code. */
    json_t *pl = ut_plural(io, remaining);
    if (ut_is_raise(pl)) { free(code); return pl; }
    char *pls = ut_pystr(pl); json_free(pl);
    char *reset_msg = ut_xfmt("%s%lld%s%s%s", UT_RESET_P1, remaining,
                              UT_RESET_P2, pls ? pls : "", UT_RESET_P3);
    free(pls);
    int known = strcmp(code, "reset") == 0 || strcmp(code, "nothing_to_reset") == 0
             || strcmp(code, "no_credit") == 0 || strcmp(code, "already_redeemed") == 0;
    if (!known) {
        char *rep = ut_repr(body);
        char *msg = ut_xfmt("%s%s", UT_UNAVAIL_PREFIX, rep ? rep : "");
        free(rep); free(code); free(reset_msg);
        json_t *r = ut_unavailable_core(msg);
        free(msg); return r;
    }
    long long windows_reset = 0;
    if (strcmp(code, "reset") == 0) {
        json_t *r = ut_call(io, "clear_codex_pool_quota_cooldowns", ut_args());
        if (ut_is_raise(r)) {
            if (ut_is_exception(ut_kind(r))) {
                json_free(r);
                json_t *lg = ut_log_exc(io, UT_LOG_RESET_FAIL);
                if (ut_is_raise(lg)) { free(code); free(reset_msg); return lg; }
                json_free(lg);
            } else { free(code); free(reset_msg); return r; }
        } else json_free(r);
        json_t *raw = ut_get(body, "windows_reset");
        windows_reset = ut_is_num(raw) ? (long long)ut_num(raw) : 0;
    }
    const char *msg; long long count;
    if (strcmp(code, "reset") == 0) { msg = reset_msg; count = remaining; }
    else if (strcmp(code, "nothing_to_reset") == 0) { msg = UT_NOTHING_TO_RESET; count = available; }
    else if (strcmp(code, "no_credit") == 0) { msg = UT_NO_CREDIT; count = 0; }
    else { msg = UT_ALREADY_REDEEMED; count = remaining; }
    json_t *r = ut_redeem_result(code, msg, count, windows_reset);
    free(code); free(reset_msg); return r;
}
/* ---- _snapshot core: AccountUsageSnapshot carrier (fetched_at via utc_now seam) ---- */
static json_t *ut_snapshot_core(ut_io_t *io, const char *provider, const char *source,
                                const json_t *windows, const json_t *details, const json_t *kw)
{
    json_t *fa = ut_call(io, "utc_now", ut_args());
    if (ut_is_raise(fa)) return fa;
    json_t *o = json_object();
    json_set(o, "provider", json_string(provider ? provider : ""));
    json_set(o, "source", json_string(source ? source : ""));
    json_set(o, "fetched_at", fa ? fa : json_null());
    json_set(o, "title", (kw && json_has(kw, "title")) ? json_copy(ut_get(kw, "title"))
                                                       : json_string("Account limits"));
    json_set(o, "plan", (kw && json_has(kw, "plan")) ? json_copy(ut_get(kw, "plan")) : json_null());
    json_set(o, "windows", windows ? json_copy(windows) : json_array());
    json_set(o, "details", details ? json_copy(details) : json_array());
    json_set(o, "unavailable_reason", (kw && json_has(kw, "unavailable_reason"))
                                          ? json_copy(ut_get(kw, "unavailable_reason")) : json_null());
    json_set(o, "identity", (kw && json_has(kw, "identity")) ? json_copy(ut_get(kw, "identity")) : json_null());
    json_set(o, "raw", (kw && json_has(kw, "raw")) ? json_copy(ut_get(kw, "raw")) : json_null());
    json_set(o, "usage_window", (kw && json_has(kw, "usage_window"))
                                    ? json_copy(ut_get(kw, "usage_window")) : json_bool(false));
    return o;
}

/* PoP: ut_snapshot @ agent/account_usage.py:_snapshot */
json_t *ut_snapshot(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ut_snapshot_core(io, ut_str(ut_get(in, "provider")), ut_str(ut_get(in, "source")),
                            ut_get(in, "windows"), ut_get(in, "details"), ut_get(in, "kw"));
}

/* ---- _usage_windows core ---- */
static int ut_in_strset(const json_t *arr, const char *key)
{
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        const char *s = ut_str(arr->c.items[i]);
        if (s && strcmp(s, key) == 0) return 1;
    }
    return 0;
}

static json_t *ut_usage_windows_core(ut_io_t *io, const json_t *source, const json_t *mapping,
                                     const char *used_key, const char *reset_key,
                                     int fraction, const json_t *model_scoped)
{
    json_t *out = json_array();
    if (!mapping || mapping->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < mapping->c.count; i++) {
        json_t *pair = mapping->c.items[i];
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
        const char *key = ut_str(json_get(pair, 0));
        const char *label = ut_str(json_get(pair, 1));
        if (!key) continue;
        json_t *w = ut_get(source, key);
        json_t *used = NULL;
        if (ut_truth(w)) {
            if (w->type != JSON_OBJECT) {
                char *m = ut_xfmt("'%s' object has no attribute 'get'", ut_typename(w));
                json_t *r = ut_mkraise("AttributeError", m); free(m);
                json_free(out); return r;
            }
            used = ut_get(w, used_key);
        }
        if (!used || used->type == JSON_NULL) continue;
        double u = ut_num(used);
        if (fraction && u <= 1) u *= 100;
        json_t *a = ut_args();
        json_set(a, "value", w ? ut_copy_or_null(ut_get(w, reset_key)) : json_null());
        json_t *reset_at = ut_call(io, "parse_dt", a);
        if (ut_is_raise(reset_at)) { json_free(out); return reset_at; }
        json_t *win = json_object();
        json_set(win, "label", json_string(label ? label : ""));
        json_set(win, "used_percent", json_number(u));
        json_set(win, "reset_at", reset_at ? reset_at : json_null());
        json_set(win, "detail", json_null());
        json_set(win, "scope", json_string(ut_in_strset(model_scoped, key) ? "model" : "account"));
        json_append(out, win);
    }
    return out;
}

/* PoP: ut_usage_windows @ agent/account_usage.py:_usage_windows */
json_t *ut_usage_windows(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ut_usage_windows_core(io, ut_get(in, "source"), ut_get(in, "mapping"),
                                 ut_str(ut_get(in, "used_key")), ut_str(ut_get(in, "reset_key")),
                                 ut_truth(ut_get(in, "fraction")), ut_get(in, "model_scoped"));
}

/* PoP: ut_account_limit_lines @ agent/account_usage.py:account_limit_lines */
json_t *ut_account_limit_lines(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *provider = ut_get(in, "provider");
    if (!ut_truth(provider)) return json_array();
    json_t *a = ut_args();
    json_set(a, "provider", json_copy(provider));
    json_set(a, "base_url", ut_copy_or_null(ut_get(in, "base_url")));
    json_set(a, "api_key", ut_copy_or_null(ut_get(in, "api_key")));
    json_t *snapshot = ut_call(io, "fetch_account_usage", a);
    if (ut_is_raise(snapshot)) return snapshot;
    json_t *b = ut_args();
    json_set(b, "snapshot", snapshot ? snapshot : json_null());
    json_set(b, "markdown", json_bool(ut_truth(ut_get(in, "markdown"))));
    json_t *lines = ut_call(io, "render_account_usage_lines", b);
    if (ut_is_raise(lines)) return lines;
    return lines ? lines : json_null();
}

/* ---- _reset_phrase core (dt is an epoch-us node or NULL) ---- */
static json_t *ut_reset_phrase_core(ut_io_t *io, const json_t *dt)
{
    if (!ut_dt_present(dt)) return json_string(UT_RESETS_SOON);
    long long ms = ut_dt_ms(ut_dt_us(dt));
    json_t *a = ut_args();
    json_set(a, "ms", json_number((double)ms));
    json_t *r = ut_call(io, "format_resets_in", a);
    if (ut_is_raise(r)) {
        if (!ut_is_exception(ut_kind(r))) return r;
        json_free(r);
        json_t *lg = ut_log_exc(io, UT_LOG_RESET_PHRASE);
        if (ut_is_raise(lg)) return lg;
        json_free(lg);
        json_t *b = ut_args();
        json_set(b, "dt", json_copy(dt));
        json_t *fb = ut_call(io, "format_reset", b);
        if (ut_is_raise(fb)) return fb;
        char *fbs = ut_pystr(fb); json_free(fb);
        char *s = ut_xfmt("%s%s", UT_RESETS_FB_PREFIX, fbs ? fbs : "");
        free(fbs);
        json_t *res = json_string(s); free(s); return res;
    }
    return r;
}

/* PoP: ut_reset_phrase @ agent/account_usage.py:_reset_phrase */
json_t *ut_reset_phrase(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ut_reset_phrase_core(io, ut_get(in, "dt"));
}

/* ---- _render_usage_window_lines ---- */
static json_t *ut_render_usage_window_lines_core(ut_io_t *io, const json_t *snapshot)
{
    json_t *lines = json_array();
    const json_t *plan = ut_get(snapshot, "plan");
    if (ut_truth(plan)) {
        json_append(lines, json_copy(plan));
    } else {
        json_append(lines, ut_copy_or_null(ut_get(snapshot, "title")));
        char *prov = ut_xfmt("Provider: %s", ut_str(ut_get(snapshot, "provider")) ? ut_str(ut_get(snapshot, "provider")) : "");
        json_append(lines, json_string(prov)); free(prov);
    }
    const json_t *windows = ut_get(snapshot, "windows");
    size_t nw = (windows && windows->type == JSON_ARRAY) ? windows->c.count : 0;
    size_t label_w = 0;
    for (size_t i = 0; i < nw; i++) {
        const char *lab = ut_str(ut_get(windows->c.items[i], "label"));
        size_t l = lab ? strlen(lab) : 0;
        if (l > label_w) label_w = l;
    }
    for (size_t i = 0; i < nw; i++) {
        const json_t *win = windows->c.items[i];
        const char *lab = ut_str(ut_get(win, "label"));
        if (!lab) lab = "";
        char *label = label_w ? ut_xfmt("%-*s", (int)label_w, lab) : ut_xfmt("%s", lab);
        const json_t *up = ut_get(win, "used_percent");
        char *base = NULL;
        if (!up || up->type == JSON_NULL) {
            base = ut_xfmt("%s  unavailable", label);
        } else {
            double used = ut_num(up);
            if (used < 0.0) used = 0.0;
            if (used > 100.0) used = 100.0;
            used = used / 100.0;
            char *bar = NULL, *pct = NULL;
            int fallback = 0;
            json_t *a1 = ut_args(); json_set(a1, "used", json_number(used));
            json_t *rb = ut_call(io, "render_bar", a1);
            if (ut_is_raise(rb)) {
                if (!ut_is_exception(ut_kind(rb))) { free(label); json_free(lines); return rb; }
                json_free(rb); fallback = 1;
            } else {
                json_t *a2 = ut_args(); json_set(a2, "used", json_number(used));
                json_t *fp = ut_call(io, "format_percent_used", a2);
                if (ut_is_raise(fp)) {
                    if (!ut_is_exception(ut_kind(fp))) { json_free(rb); free(label); json_free(lines); return fp; }
                    json_free(fp); json_free(rb); fallback = 1;
                } else {
                    bar = ut_pystr(rb); pct = ut_pystr(fp);
                    json_free(rb); json_free(fp);
                }
            }
            if (fallback) {
                json_t *lg = ut_log_exc(io, UT_LOG_BAR);
                if (ut_is_raise(lg)) { free(label); json_free(lines); return lg; }
                json_free(lg);
                bar = ut_xfmt("");
                pct = ut_xfmt("%lld%%", (long long)(used * 100.0));
            }
            if (bar && bar[0])
                base = ut_xfmt("%s  %s  %s used", label, bar, pct ? pct : "");
            else
                base = ut_xfmt("%s  %s used", label, pct ? pct : "");
            free(bar); free(pct);
        }
        const json_t *reset_at = ut_get(win, "reset_at");
        const json_t *detail = ut_get(win, "detail");
        if (ut_present(reset_at)) {
            json_t *rp = ut_reset_phrase_core(io, reset_at);
            if (ut_is_raise(rp)) { free(base); free(label); json_free(lines); return rp; }
            char *rps = ut_pystr(rp); json_free(rp);
            char *nb = ut_xfmt("%s%s%s", base, UT_SEP_DOT, rps ? rps : "");
            free(base); free(rps); base = nb;
        } else if (ut_truth(detail)) {
            char *ds = ut_pystr(detail);
            char *nb = ut_xfmt("%s%s%s", base, UT_SEP_DOT, ds ? ds : "");
            free(base); free(ds); base = nb;
        }
        json_append(lines, json_string(base));
        free(base); free(label);
    }
    const json_t *details = ut_get(snapshot, "details");
    if (details && details->type == JSON_ARRAY)
        for (size_t i = 0; i < details->c.count; i++)
            json_append(lines, json_copy(details->c.items[i]));
    const json_t *ur = ut_get(snapshot, "unavailable_reason");
    if (ut_truth(ur)) {
        char *urs = ut_pystr(ur);
        char *line = ut_xfmt("Unavailable: %s", urs ? urs : "");
        free(urs); json_append(lines, json_string(line)); free(line);
    }
    return lines;
}

/* PoP: ut_render_usage_window_lines @ agent/account_usage.py:_render_usage_window_lines */
json_t *ut_render_usage_window_lines(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ut_render_usage_window_lines_core(io, ut_get(in, "snapshot"));
}

/* PoP: ut_call_plugin_usage_hook @ agent/account_usage.py:_call_plugin_usage_hook */
json_t *ut_call_plugin_usage_hook(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *profile = ut_get(in, "profile");
    if (!ut_present(profile))
        return ut_mkraise("AttributeError", "type object 'NoneType' has no attribute 'fetch_account_usage'");
    const char *hook = ut_str(ut_get(profile, "hook"));
    if (hook && strcmp(hook, "base") == 0) return NULL;
    json_t *a = ut_args();
    json_t *fn = json_object();
    json_set(fn, "__callable__", json_string("profile.fetch_account_usage"));
    json_set(a, "fn", fn);
    json_set(a, "timeout", json_number(10.0));
    json_set(a, "label", json_string("plugin-account-usage"));
    json_t *bounded = ut_call(io, "run_bounded_sync", a);
    if (ut_is_raise(bounded)) return bounded;
    if (!ut_present(bounded) || bounded->type != JSON_OBJECT) {
        char *m = ut_xfmt("'%s' object has no attribute 'timed_out'", ut_typename(bounded));
        json_t *r = ut_mkraise("AttributeError", m); free(m);
        json_free(bounded); return r;
    }
    if (ut_truth(ut_get(bounded, "timed_out"))) { json_free(bounded); return NULL; }
    json_t *val = ut_copy_or_null(ut_get(bounded, "value"));
    json_free(bounded);
    return val;
}

/* ---- Codex fetch flow ---- */
static json_t *ut_resolve_creds(ut_io_t *io, const json_t *base_url, const json_t *api_key,
                                int force_refresh)
{
    json_t *a = ut_args();
    json_set(a, "base_url", ut_copy_or_null(base_url));
    json_set(a, "api_key", ut_copy_or_null(api_key));
    if (force_refresh) json_set(a, "force_refresh", json_bool(true));
    return ut_call(io, "resolve_codex_usage_credentials", a);
}

static json_t *ut_codex_get_json(ut_io_t *io, const char *resolved_base_url,
                                 const char *token, const json_t *account_id)
{
    json_t *a = ut_args();
    json_set(a, "base_url", json_string(resolved_base_url ? resolved_base_url : ""));
    json_t *urls = ut_call(io, "codex_backend_urls", a);
    if (ut_is_raise(urls)) return urls;
    const char *url = ut_str(json_get(urls, 0));
    json_t *headers = ut_codex_headers_core(io, token, account_id);
    if (ut_is_raise(headers)) { json_free(urls); return headers; }
    json_t *b = ut_args();
    json_set(b, "url", json_string(url ? url : ""));
    json_set(b, "headers", headers);
    json_set(b, "timeout", json_number(15.0));
    json_t *payload = ut_call(io, "get_json", b);
    json_free(urls);
    return payload;
}

static json_t *ut_fetch_codex_impl_core(ut_io_t *io, const json_t *base_url,
                                        const json_t *api_key, int read_only)
{
    json_t *creds = ut_resolve_creds(io, base_url, api_key, 0);
    if (ut_is_raise(creds)) return creds;
    if (!creds || creds->type != JSON_ARRAY || json_len(creds) < 3) {
        json_free(creds);
        return ut_mkraise("ValueError", "not enough values to unpack (expected 3, got 0)");
    }
    char *token = ut_pystr(json_get(creds, 0));
    char *rbase = ut_pystr(json_get(creds, 1));
    json_t *account_id = json_copy(json_get(creds, 2));
    json_free(creds);
    json_t *payload = ut_codex_get_json(io, rbase, token, account_id);
    if (ut_is_raise(payload)) {
        if (strcmp(ut_kind(payload), "HTTPStatusError") == 0) {
            long long sc = (long long)ut_num(ut_get(payload, "status_code"));
            if (sc != 401 || read_only) { free(token); free(rbase); json_free(account_id); return payload; }
            json_free(payload);
            json_t *creds2 = ut_resolve_creds(io, base_url, api_key, 1);
            if (ut_is_raise(creds2)) { free(token); free(rbase); json_free(account_id); return creds2; }
            if (!creds2 || creds2->type != JSON_ARRAY || json_len(creds2) < 3) {
                json_free(creds2); free(token); free(rbase); json_free(account_id);
                return ut_mkraise("ValueError", "not enough values to unpack (expected 3, got 0)");
            }
            free(token); free(rbase); json_free(account_id);
            token = ut_pystr(json_get(creds2, 0));
            rbase = ut_pystr(json_get(creds2, 1));
            account_id = json_copy(json_get(creds2, 2));
            json_free(creds2);
            payload = ut_codex_get_json(io, rbase, token, account_id);
            if (ut_is_raise(payload)) { free(token); free(rbase); json_free(account_id); return payload; }
        } else { free(token); free(rbase); json_free(account_id); return payload; }
    }
    free(rbase);
    /* payload.get("rate_limit") or {} */
    if (!ut_present(payload) || payload->type != JSON_OBJECT) {
        char *m = ut_xfmt("'%s' object has no attribute 'get'", ut_typename(payload));
        json_t *r = ut_mkraise("AttributeError", m); free(m);
        free(token); json_free(account_id); json_free(payload); return r;
    }
    json_t *rate_limit = ut_get(payload, "rate_limit");
    if (ut_truth(rate_limit) && rate_limit->type != JSON_OBJECT) {
        char *m = ut_xfmt("'%s' object has no attribute 'get'", ut_typename(rate_limit));
        json_t *r = ut_mkraise("AttributeError", m); free(m);
        free(token); json_free(account_id); json_free(payload); return r;
    }
    json_t *labels = ut_codex_window_labels_core(rate_limit);
    json_t *windows = ut_usage_windows_core(io, rate_limit, labels, "used_percent", "reset_at", 0, NULL);
    json_free(labels);
    if (ut_is_raise(windows)) { free(token); json_free(account_id); json_free(payload); return windows; }
    json_t *details = json_array();
    json_t *count = ut_codex_banked_resets_core(payload);
    if (ut_is_raise(count)) { json_free(windows); json_free(details); free(token); json_free(account_id); json_free(payload); return count; }
    long long cnt = (long long)ut_num(count); json_free(count);
    if (cnt > 0) {
        json_t *pl = ut_plural(io, cnt);
        if (ut_is_raise(pl)) { json_free(windows); json_free(details); free(token); json_free(account_id); json_free(payload); return pl; }
        char *pls = ut_pystr(pl); json_free(pl);
        char *line = ut_xfmt("%s%lld%s%s%s", UT_BANKED_PREFIX, cnt, " reset", pls ? pls : "", UT_BANKED_SUFFIX);
        free(pls); json_append(details, json_string(line)); free(line);
    }
    json_t *credits = ut_get(payload, "credits");
    if (ut_truth(credits) && credits->type != JSON_OBJECT) {
        char *m = ut_xfmt("'%s' object has no attribute 'get'", ut_typename(credits));
        json_t *r = ut_mkraise("AttributeError", m); free(m);
        json_free(windows); json_free(details); free(token); json_free(account_id); json_free(payload); return r;
    }
    json_t *balance = ut_get(credits, "balance");
    if (ut_truth(ut_get(credits, "has_credits")) && ut_is_num(balance)) {
        char *bs = ut_fmt_fixed(ut_num(balance), 2);
        char *line = ut_xfmt("%s%s", UT_CREDITS_BALANCE_PREFIX, bs);
        free(bs); json_append(details, json_string(line)); free(line);
    } else if (ut_truth(ut_get(credits, "has_credits")) && ut_truth(ut_get(credits, "unlimited"))) {
        json_append(details, json_string(UT_CREDITS_UNLIMITED));
    }
    /* _snapshot(..., plan=_title_case_slug(payload.get("plan_type")), identity=..., raw=payload) */
    json_t *ta = ut_args();
    json_set(ta, "value", ut_copy_or_null(ut_get(payload, "plan_type")));
    json_t *plan = ut_call(io, "title_case_slug", ta);
    if (ut_is_raise(plan)) { json_free(windows); json_free(details); free(token); json_free(account_id); json_free(payload); return plan; }
    json_t *identity = ut_codex_snapshot_identity_core(io, token);
    if (ut_is_raise(identity)) { json_free(plan); json_free(windows); json_free(details); free(token); json_free(account_id); json_free(payload); return identity; }
    json_t *kw = json_object();
    json_set(kw, "plan", plan ? plan : json_null());
    json_set(kw, "identity", identity ? identity : json_null());
    json_set(kw, "raw", json_copy(payload));
    json_t *snap = ut_snapshot_core(io, "openai-codex", "usage_api", windows, details, kw);
    json_free(kw); json_free(windows); json_free(details);
    free(token); json_free(account_id); json_free(payload);
    return snap;
}

/* PoP: ut_fetch_codex_account_usage_impl @ agent/account_usage.py:_fetch_codex_account_usage_impl */
json_t *ut_fetch_codex_account_usage_impl(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ut_fetch_codex_impl_core(io, ut_get(in, "base_url"), ut_get(in, "api_key"),
                                    ut_truth(ut_get(in, "read_only")));
}

/* PoP: ut_fetch_codex_account_usage_read_only @ agent/account_usage.py:_fetch_codex_account_usage_read_only */
json_t *ut_fetch_codex_account_usage_read_only(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ut_fetch_codex_impl_core(io, ut_get(in, "base_url"), ut_get(in, "api_key"), 1);
}
/* ================= agent/account_usage_nous.py ================= */

static json_t *ut_header_gauge_percent_core(const json_t *bp)
{
    if (ut_is_bool(bp) || !ut_is_int(bp)) return NULL;
    double p = ut_num(bp) / 10000.0 * 100.0;
    if (p < 0.0) p = 0.0;
    if (p > 100.0) p = 100.0;
    return json_number(p);
}

/* PoP: ut_header_gauge_percent @ agent/account_usage_nous.py:_header_gauge_percent */
json_t *ut_header_gauge_percent(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return ut_header_gauge_percent_core(ut_get(in, "bp"));
}

static json_t *ut_bp_window_core(const char *label, const json_t *used_bp, const json_t *resets_at_ms)
{
    if (ut_is_bool(used_bp) || !ut_is_int(used_bp)) return NULL;
    int known = ut_is_int(resets_at_ms);
    json_t *reset_at = known ? json_number((double)((long long)ut_num(resets_at_ms) * 1000)) : json_null();
    double p = ut_num(used_bp) / 10000.0 * 100.0;
    if (p < 0.0) p = 0.0;
    if (p > 100.0) p = 100.0;
    json_t *o = json_object();
    json_set(o, "label", json_string(label ? label : ""));
    json_set(o, "used_percent", json_number(p));
    json_set(o, "reset_at", reset_at);
    json_set(o, "detail", json_null());
    json_set(o, "scope", json_string("account"));
    return o;
}

/* PoP: ut_bp_window @ agent/account_usage_nous.py:_bp_window */
json_t *ut_bp_window(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return ut_bp_window_core(ut_str(ut_get(in, "label")), ut_get(in, "used_bp"), ut_get(in, "resets_at_ms"));
}

static json_t *ut_window_top_tier_line_core(const json_t *reset_at, const json_t *keep_going)
{
    char *resets = ut_xfmt("");
    if (ut_dt_present(reset_at)) {
        int mon = 0, mday = 0;
        if (ut_local_date(ut_dt_us(reset_at), &mon, &mday)) {
            free(resets);
            resets = ut_xfmt("%s%s %d%s", UT_RESETS_PREFIX, UT_MONTHS[mon], mday, UT_RESETS_DOT);
        }
    }
    const char *tail = (ut_is_bool(keep_going) && !keep_going->bool_val)
                           ? UT_TAIL_KEEP_FALSE : UT_TAIL_KEEP_OTHER;
    char *s = ut_xfmt("%s%s%s", UT_TOP_TIER_USED_UP, resets, tail);
    free(resets);
    json_t *r = json_string(s); free(s); return r;
}

/* PoP: ut_window_top_tier_line @ agent/account_usage_nous.py:_window_top_tier_line */
json_t *ut_window_top_tier_line(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return ut_window_top_tier_line_core(ut_get(in, "reset_at"), ut_get(in, "keep_going_on_credits"));
}

/* PoP: ut_nous_snapshot @ agent/account_usage_nous.py:_nous_snapshot */
json_t *ut_nous_snapshot(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *windows = ut_get(in, "windows");
    const json_t *details = ut_get(in, "details");
    if (!ut_truth(windows) && !ut_truth(details)) return NULL;
    const json_t *tail = ut_get(in, "tail");
    json_t *merged = details ? json_copy(details) : json_array();
    if (tail && tail->type == JSON_ARRAY)
        for (size_t i = 0; i < tail->c.count; i++)
            json_append(merged, json_copy(tail->c.items[i]));
    json_t *kw = json_object();
    json_set(kw, "title", json_string("Nous credits"));
    json_set(kw, "plan", ut_copy_or_null(ut_get(in, "plan")));
    json_set(kw, "usage_window", json_bool(ut_truth(ut_get(in, "usage_window"))));
    json_t *r = ut_snapshot_core(io, "nous", ut_str(ut_get(in, "source")), windows, merged, kw);
    json_free(kw); json_free(merged);
    return r;
}

static json_t *ut_gauge_title(ut_io_t *io, const json_t *gauge)
{
    json_t *a = ut_args();
    json_set(a, "gauge", gauge ? json_copy(gauge) : json_null());
    return ut_call(io, "gauge_title", a);
}

/* PoP: ut_usage_window_rows @ agent/account_usage_nous.py:_usage_window_rows */
json_t *ut_usage_window_rows(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *pa = ut_args();
    json_set(pa, "gauges", ut_copy_or_null(ut_get(in, "gauges")));
    json_t *parsed = ut_call(io, "parse_gauges", pa);
    if (ut_is_raise(parsed)) return parsed;
    if (!ut_truth(parsed)) { json_free(parsed); json_t *r = json_array(); json_append(r, json_array()); json_append(r, json_array()); return r; }
    json_t *aa = ut_args();
    json_set(aa, "gauges", json_copy(parsed));
    json_t *alls = ut_call(io, "all_scope", aa);
    if (ut_is_raise(alls)) { json_free(parsed); return alls; }
    if (!ut_truth(alls)) { json_free(parsed); json_free(alls); json_t *r = json_array(); json_append(r, json_array()); json_append(r, json_array()); return r; }
    json_free(alls);
    json_t *out = json_array();
    json_t *top_row = NULL; int top_full = 0;
    for (size_t i = 0; i < parsed->c.count; i++) {
        json_t *g = parsed->c.items[i];
        json_t *title = ut_gauge_title(io, g);
        if (ut_is_raise(title)) { json_free(parsed); json_free(out); return title; }
        char *ts = ut_pystr(title); json_free(title);
        json_t *row = ut_bp_window_core(ts, ut_get(g, "used_bp"), ut_get(g, "resets_at_ms"));
        free(ts);
        if (!row) continue;
        const char *scope = ut_str(ut_get(g, "scope"));
        if (scope && strcmp(scope, "top_tier") == 0) {
            top_row = row;
            top_full = ut_num(ut_get(g, "used_bp")) >= 10000.0;
        }
        json_append(out, row);
    }
    json_free(parsed);
    json_t *lines = json_array();
    const json_t *keep = ut_get(in, "keep_going_on_credits");
    if (top_full) {
        json_t *ttl = ut_window_top_tier_line_core(top_row ? ut_get(top_row, "reset_at") : NULL, keep);
        json_append(lines, ttl);
    }
    const json_t *paused_reason = ut_get(in, "paused_reason");
    int cta = ut_truth(ut_get(in, "cta"));
    if (paused_reason && paused_reason->type == JSON_STRING && paused_reason->str_val && paused_reason->str_val[0]) {
        json_t *pa2 = ut_args();
        json_set(pa2, "reason", json_copy(paused_reason));
        json_t *pf = ut_call(io, "paused_for_payment", pa2);
        if (ut_is_raise(pf)) { json_free(out); json_free(lines); return pf; }
        int is_pay = ut_truth(pf); json_free(pf);
        json_append(lines, json_string(is_pay ? UT_PAUSED_LINE : UT_PAUSED_OTHER));
    } else if (ut_is_bool(keep) && !keep->bool_val) {
        json_append(lines, json_string(cta ? UT_KEEP_OFF_LINE : UT_KEEP_OFF_STATUS));
    } else if (ut_is_bool(keep) && keep->bool_val) {
        json_append(lines, json_string(UT_KEEP_ON_LINE));
    }
    json_t *r = json_array();
    json_append(r, out);
    json_append(r, lines);
    return r;
}

/* ---- apply_header_gauge core ---- */
static json_t *ut_apply_header_gauge_core(ut_io_t *io, const json_t *snapshot, const json_t *credits_state)
{
    if (!ut_present(snapshot) || !ut_truth(ut_get(snapshot, "usage_window")) || !ut_present(credits_state))
        return ut_copy_or_null(snapshot);
    if (!ut_truth(ut_get(credits_state, "from_header")) || !ut_truth(ut_get(credits_state, "is_usage_window")))
        return ut_copy_or_null(snapshot);
    const json_t *gauges = ut_get(credits_state, "gauges");
    json_t *mapping = json_object();
    if (gauges && gauges->type == JSON_ARRAY) {
        for (size_t i = 0; i < gauges->c.count; i++) {
            json_t *g = gauges->c.items[i];
            json_t *title = ut_gauge_title(io, g);
            if (ut_is_raise(title)) { json_free(mapping); return title; }
            char *ts = ut_pystr(title); json_free(title);
            json_t *pair = json_array();
            json_append(pair, ut_copy_or_null(ut_get(g, "used_bp")));
            json_append(pair, ut_copy_or_null(ut_get(g, "resets_at_ms")));
            json_set(mapping, ts ? ts : "", pair);
            free(ts);
        }
    }
    json_t *tt = ut_call(io, "time.time", ut_args());
    if (ut_is_raise(tt)) { json_free(mapping); return tt; }
    long long now_ms = (long long)(ut_num(tt) * 1000.0);
    json_free(tt);
    const json_t *windows = ut_get(snapshot, "windows");
    size_t nw = (windows && windows->type == JSON_ARRAY) ? windows->c.count : 0;
    json_t *new_windows = json_array();
    int changed = 0;
    for (size_t i = 0; i < nw; i++) {
        const json_t *win = windows->c.items[i];
        const char *label = ut_str(ut_get(win, "label"));
        json_t *pair = label ? ut_get(mapping, label) : NULL;
        json_t *bp = pair ? json_get(pair, 0) : NULL;
        json_t *resets_at_ms = pair ? json_get(pair, 1) : NULL;
        int stale = ut_is_int(resets_at_ms) && now_ms >= (long long)ut_num(resets_at_ms);
        json_t *header = stale ? NULL : ut_header_gauge_percent_core(bp);
        const json_t *current = ut_get(win, "used_percent");
        if (!header || !ut_present(current) || ut_num(header) <= ut_num(current)) {
            json_append(new_windows, json_copy(win));
            json_free(header);
            continue;
        }
        changed = 1;
        json_t *nw2 = json_copy(win);
        json_set(nw2, "used_percent", header);
        json_append(new_windows, nw2);
    }
    json_free(mapping);
    if (!changed) {
        json_free(new_windows);
        return ut_copy_or_null(snapshot);
    }
    const json_t *details = ut_get(snapshot, "details");
    json_t *new_details = details ? json_copy(details) : json_array();
    const json_t *top_gauge = ut_get(credits_state, "top_tier_gauge");
    const json_t *top = NULL;
    if (ut_present(top_gauge)) {
        for (size_t i = 0; i < new_windows->c.count; i++) {
            json_t *w = new_windows->c.items[i];
            json_t *title = ut_gauge_title(io, top_gauge);
            if (ut_is_raise(title)) { json_free(new_windows); json_free(new_details); return title; }
            char *ts = ut_pystr(title); json_free(title);
            const char *wl = ut_str(ut_get(w, "label"));
            int match = ts && wl && strcmp(ts, wl) == 0;
            free(ts);
            if (match) { top = w; break; }
        }
    }
    if (top) {
        const json_t *up = ut_get(top, "used_percent");
        double val = ut_truth(up) ? ut_num(up) : 0.0;
        if (val >= 100.0) {
            int has = 0;
            if (details && details->type == JSON_ARRAY) {
                for (size_t i = 0; i < details->c.count; i++) {
                    const char *ls = ut_str(details->c.items[i]);
                    if (ls && ut_startswith(ls, UT_TOP_TIER_USED_UP)) { has = 1; break; }
                }
            }
            if (!has) {
                json_t *ttl = ut_window_top_tier_line_core(ut_get(top, "reset_at"),
                                                           ut_get(credits_state, "keep_going_on_credits"));
                json_append(new_details, ttl);
            }
        }
    }
    json_t *result = json_copy(snapshot);
    json_set(result, "windows", new_windows);
    json_set(result, "details", new_details);
    return result;
}

/* PoP: ut_apply_header_gauge @ agent/account_usage_nous.py:apply_header_gauge */
json_t *ut_apply_header_gauge(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ut_apply_header_gauge_core(io, ut_get(in, "snapshot"), ut_get(in, "credits_state"));
}

static json_t *ut_nous_logged_in_core(ut_io_t *io)
{
    json_t *a = ut_args();
    json_set(a, "provider", json_string("nous"));
    json_t *r = ut_call(io, "get_provider_auth_state", a);
    if (ut_is_raise(r)) {
        if (ut_is_exception(ut_kind(r))) { json_free(r); return json_bool(false); }
        return r;
    }
    json_t *tok = NULL;
    if (ut_truth(r)) {
        if (r->type != JSON_OBJECT) { json_free(r); return json_bool(false); }
        tok = ut_get(r, "access_token");
    }
    int result = 0;
    if (tok && tok->type == JSON_STRING) {
        char *s = ut_strip(tok->str_val);
        result = (s && s[0]) ? 1 : 0;
        free(s);
    }
    json_free(r);
    return json_bool(result);
}

/* PoP: ut_nous_logged_in @ agent/account_usage_nous.py:_nous_logged_in */
json_t *ut_nous_logged_in(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    return ut_nous_logged_in_core(io);
}

/* ---- _nous_account_view core: returns [lines, is_window, account] ---- */
static json_t *ut_nous_account_view_core(ut_io_t *io, int markdown, double timeout,
                                         const json_t *credits_state, int nous_route)
{
    json_t *account = NULL, *snapshot_raw = NULL, *snapshot = NULL, *lines = NULL;
    /* fixture try */
    json_t *fixture = ut_call(io, "dev_fixture_credits_state", ut_args());
    if (ut_is_raise(fixture)) {
        if (!ut_is_exception(ut_kind(fixture))) return fixture;
        json_free(fixture); fixture = NULL;
        json_t *lg = ut_log_exc(io, UT_LOG_FIXTURE);
        if (ut_is_raise(lg)) return lg;
        json_free(lg);
    }
    if (ut_present(fixture)) {
        json_t *sa = ut_args();
        json_set(sa, "state", fixture);
        snapshot = ut_call(io, "snapshot_from_credits_state", sa);
        if (ut_is_raise(snapshot)) return snapshot;
        json_t *ra = ut_args();
        json_set(ra, "snapshot", snapshot ? json_copy(snapshot) : json_null()); /* copy: snapshot stays owned */
        json_set(ra, "markdown", json_bool(markdown));
        lines = ut_call(io, "render_account_usage_lines", ra);
        if (ut_is_raise(lines)) { json_free(snapshot); return lines; }
        int is_window = ut_present(snapshot) && ut_truth(ut_get(snapshot, "usage_window"));
        json_t *r = json_array();
        json_append(r, lines ? lines : json_null());
        json_append(r, json_bool(is_window));
        json_append(r, json_null());
        json_free(snapshot);
        return r;
    }
    json_free(fixture);
    json_t *li = ut_nous_logged_in_core(io);
    if (ut_is_raise(li)) return li;
    if (!ut_truth(li)) {
        json_free(li);
        json_t *r = json_array();
        json_append(r, json_array());
        json_append(r, json_bool(false));
        json_append(r, json_null());
        return r;
    }
    json_free(li);
    /* fetch/render try (fail-open on Exception, BaseException propagates) */
    json_t *fa = ut_args();
    json_set(fa, "timeout", json_number(timeout));
    json_set(fa, "stored_token_only", json_bool(!nous_route));
    account = ut_call(io, "fetch_portal_account", fa);
    if (ut_is_raise(account)) {
        if (!ut_is_exception(ut_kind(account))) return account;
        goto fail_open;
    }
    json_t *ba = ut_args();
    json_set(ba, "account", account ? json_copy(account) : json_null()); /* copy: account stays owned */
    json_set(ba, "cta", json_bool(nous_route));
    snapshot_raw = ut_call(io, "build_nous_credits_snapshot", ba);
    if (ut_is_raise(snapshot_raw)) {
        if (!ut_is_exception(ut_kind(snapshot_raw))) { json_free(account); return snapshot_raw; }
        goto fail_open;
    }
    snapshot = ut_apply_header_gauge_core(io, snapshot_raw, credits_state);
    if (ut_is_raise(snapshot)) {
        if (!ut_is_exception(ut_kind(snapshot))) { json_free(account); json_free(snapshot_raw); return snapshot; }
        goto fail_open;
    }
    json_t *ra = ut_args();
    json_set(ra, "snapshot", snapshot ? json_copy(snapshot) : json_null()); /* copy: snapshot stays owned */
    json_set(ra, "markdown", json_bool(markdown));
    lines = ut_call(io, "render_account_usage_lines", ra);
    if (ut_is_raise(lines)) {
        if (!ut_is_exception(ut_kind(lines))) { json_free(account); json_free(snapshot_raw); json_free(snapshot); return lines; }
        goto fail_open;
    }
    {
        int is_window = ut_present(snapshot) && ut_truth(ut_get(snapshot, "usage_window"));
        json_t *r = json_array();
        json_append(r, lines ? lines : json_null());
        json_append(r, json_bool(is_window));
        json_append(r, account ? account : json_null());
        json_free(snapshot_raw); json_free(snapshot);
        return r;
    }
fail_open:
    json_free(account); account = NULL;
    json_free(snapshot_raw); snapshot_raw = NULL;
    json_free(snapshot); snapshot = NULL;
    json_free(lines); lines = NULL;
    {
        json_t *lg = ut_log_exc(io, UT_LOG_PORTAL);
        if (ut_is_raise(lg)) return lg;
        json_free(lg);
    }
    {
        json_t *r = json_array();
        json_append(r, json_array());
        json_append(r, json_bool(false));
        json_append(r, json_null());
        return r;
    }
}

/* PoP: ut_nous_account_view @ agent/account_usage_nous.py:_nous_account_view */
json_t *ut_nous_account_view(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ut_nous_account_view_core(io, ut_truth(ut_get(in, "markdown")),
                                     ut_num(ut_get(in, "timeout")),
                                     ut_get(in, "credits_state"), ut_truth(ut_get(in, "nous_route")));
}

static json_t *ut_nous_credits_view_core(ut_io_t *io, int markdown, double timeout,
                                         const json_t *credits_state, int nous_route)
{
    json_t *r = ut_nous_account_view_core(io, markdown, timeout, credits_state, nous_route);
    if (ut_is_raise(r)) return r;
    json_t *out = json_array();
    json_append(out, ut_copy_or_null(json_get(r, 0)));
    json_append(out, ut_copy_or_null(json_get(r, 1)));
    json_free(r);
    return out;
}

/* PoP: ut_nous_credits_view @ agent/account_usage_nous.py:nous_credits_view */
json_t *ut_nous_credits_view(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ut_nous_credits_view_core(io, ut_truth(ut_get(in, "markdown")),
                                     ut_num(ut_get(in, "timeout")),
                                     ut_get(in, "credits_state"), ut_truth(ut_get(in, "nous_route")));
}

/* PoP: ut_nous_credits_usage_fields @ agent/account_usage_nous.py:nous_credits_usage_fields */
json_t *ut_nous_credits_usage_fields(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *agent = ut_get(in, "agent");
    int nous_route = ut_truth(ut_get(in, "nous_route"));
    const json_t *credits_state = ut_present(agent) ? ut_get(agent, "_credits_state") : NULL;
    json_t *r = ut_nous_credits_view_core(io, 0, 10.0, credits_state, nous_route);
    if (ut_is_raise(r)) {
        if (!ut_is_exception(ut_kind(r))) return r;
        json_free(r);
        json_t *lg = ut_log_exc(io, UT_LOG_FIELDS);
        if (ut_is_raise(lg)) return lg;
        json_free(lg);
        return json_object();
    }
    json_t *lines = json_get(r, 0);
    int is_window = ut_truth(json_get(r, 1));
    json_t *eff;
    if (!nous_route && ut_truth(lines)) {
        eff = json_array();
        json_append(eff, json_string(UT_NOUS_HEADING));
        for (size_t i = 0; i < lines->c.count; i++)
            json_append(eff, json_copy(lines->c.items[i]));
    } else {
        eff = ut_copy_or_null(lines);
    }
    json_free(r);
    if (!ut_truth(eff)) { json_free(eff); return json_object(); }
    json_t *out = json_object();
    json_set(out, "credits_lines", eff);
    if (is_window) json_set(out, "credits_usage_window", json_bool(true));
    return out;
}

/* PoP: ut_nous_usage_lines @ agent/account_usage_nous.py:nous_usage_lines */
json_t *ut_nous_usage_lines(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    int nous_route = ut_truth(ut_get(in, "nous_route"));
    int markdown = ut_truth(ut_get(in, "markdown"));
    json_t *a = ut_args();
    json_set(a, "markdown", json_bool(markdown));
    json_set(a, "credits_state", ut_copy_or_null(ut_get(in, "credits_state")));
    json_set(a, "nous_route", json_bool(nous_route));
    json_t *lines = ut_call(io, "nous_credits_lines", a);
    if (ut_is_raise(lines)) return lines;
    if (nous_route || !ut_truth(lines)) return lines ? lines : json_null();
    char *heading = markdown ? ut_xfmt("**%s**", UT_NOUS_HEADING) : ut_xfmt("%s", UT_NOUS_HEADING);
    json_t *out = json_array();
    json_append(out, json_string(heading)); free(heading);
    for (size_t i = 0; i < lines->c.count; i++)
        json_append(out, json_copy(lines->c.items[i]));
    json_free(lines);
    return out;
}

/* PoP: ut_nous_usage_view @ agent/account_usage_nous.py:nous_usage_view */
json_t *ut_nous_usage_view(ut_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    int nous_route = ut_truth(ut_get(in, "nous_route"));
    json_t *r = ut_nous_account_view_core(io, 0, 10.0, ut_get(in, "credits_state"), nous_route);
    if (ut_is_raise(r)) return r;
    json_t *lines = ut_copy_or_null(json_get(r, 0));
    json_t *account = ut_copy_or_null(json_get(r, 2));
    json_free(r);
    if (!nous_route) {
        json_t *eff;
        if (ut_truth(lines)) {
            eff = json_array();
            json_append(eff, json_string(UT_NOUS_HEADING));
            for (size_t i = 0; i < lines->c.count; i++)
                json_append(eff, json_copy(lines->c.items[i]));
            json_free(lines);
        } else {
            eff = json_array();
            json_free(lines);
        }
        json_free(account);
        json_t *out = json_array();
        json_append(out, eff);
        json_append(out, json_null());
        return out;
    }
    json_t *dev = ut_call(io, "dev_fixture_usage_model", ut_args());
    if (ut_is_raise(dev)) { json_free(lines); json_free(account); return dev; }
    json_t *model;
    if (ut_truth(dev)) {
        model = dev;
    } else {
        json_free(dev);
        json_t *ua = ut_args();
        json_set(ua, "account", account ? json_copy(account) : json_null()); /* copy: account stays owned */
        model = ut_call(io, "usage_model_from_account", ua);
        if (ut_is_raise(model)) { json_free(lines); json_free(account); return model; }
    }
    json_free(account);
    json_t *out = json_array();
    json_append(out, lines ? lines : json_null());
    json_append(out, model ? model : json_null());
    return out;
}
