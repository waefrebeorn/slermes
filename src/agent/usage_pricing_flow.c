/* ===========================================================================
 * usage_pricing_flow.c -- FLOW port of the UNCLAIMED remainder of
 * agent/usage_pricing.py (usage/cost pricing). A strict CRLF-aware PoP grep
 * over the src and include trees credits 17 live names (the upr_* / cli_agent_*
 * siblings in usage_pricing.c / usage_pricing_ports.c / helpers2.c). These 16
 * defs have NO PoP anywhere and are ported here:
 *
 *   top-level (13): _snap, _usage_field, _first_nonzero, _strip_prefix,
 *     with_served_service_tier, _served_openai_tier, _openai_served_tier_entry,
 *     _served_fast, _anthropic_fast_mode_entry, _models_dev_pricing_entry,
 *     cache_write_tiers, _cache_write_buckets, _unknown_cost
 *   nested closures (3): host (inside resolve_billing_route), rate and
 *     per_million (inside _pricing_entry_from_metadata) -- portable FLOW
 *     targets per the setup_flow2 / prompt_caching doctrine; their free
 *     variables travel as explicit case inputs.
 *
 * Self-contained: libjson only. Every live dependency NOT ported here is
 * injected through ONE universal seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * args is a BORROWED object (the port owns it and frees it after the call);
 * the result is a NEW json_t owned by the caller (NULL == None; a
 * {"__raise__":kind,"message":msg} object == a raised exception). A scripted
 * JSON null result is delivered as a NULL pointer so truthiness reads falsy.
 * Fail-closed: no channel installed, or a missing/exhausted script, yields
 * {"__raise__":"RuntimeError","message":"no script for <name>"} /
 * "no seam installed for <name>", recorded by the dispatcher BEFORE propagation.
 *
 * ENTRIES. static json_t *up2_<fn>(up2_io_t *io, const json_t *in, json_t *st)
 *   io  -- universal seam channel (NULL => fail-closed production default)
 *   in  -- borrowed case input object (never mutated)
 *   st  -- fresh object the entry fills with the final state projection.
 *          PURE entries leave st={}. The rate / per_million closures fill
 *          st={"rejected":[sorted keys]} on success AND raise (partial history).
 *          Caller owns ret. Tuples project to JSON arrays; None to NULL.
 *
 * Ported siblings compose REAL (no seam event):
 *   _first_nonzero / cache_write_tiers -> _usage_field;
 *   per_million -> rate.
 * Claimed siblings are SEAMED (never re-claimed): _to_decimal (to_decimal),
 * _normalize_anthropic_model_name (normalize_anthropic_model_name).
 *
 * BaseException kinds (SystemExit, KeyboardInterrupt, GeneratorExit,
 * CancelledError, BaseException, BaseExceptionGroup) PASS THROUGH every
 * `except Exception` site (_usage_field's int-coercion guard). A FULL builtin
 * exception-parent table (up2_exc_parent, probed on quarry CPython 3.14.9)
 * drives up2_is_exception / up2_caught_by.
 *
 * Seam name -> live call table:
 *   to_decimal                     agent.usage_pricing._to_decimal(value)
 *   normalize_anthropic_model_name agent.usage_pricing._normalize_anthropic_model_name(model)
 *   base_url_host_matches          utils.base_url_host_matches(url, name)
 *   base_url_origin                utils.base_url_origin(base_url) -> (scheme,host,port)
 *   get_model_info                 agent.models_dev.get_model_info(provider, model)
 *   utc_now                        agent.usage_pricing._UTC_NOW() -> datetime
 *   logger.debug                   logger.debug(fmt, *args)  {msg, args:[...]}
 *
 * Decimal model: a Decimal travels as {"__dec__": str(Decimal)} (exact decimal
 * text, trailing zeros preserved). C carries sign + coefficient digit-string +
 * exponent + special(NaN/sNaN/Inf) and does EXACT coefficient arithmetic
 * (grade-school multiply, exponent add) -- no float rounding. str() rendering
 * follows CPython: positional iff exp<=0 and adjusted>=-6, else scientific.
 * Multiplication domain: finite x finite, coefficients <=20 digits (products
 * <=27 sig digits, below the 28-digit context precision -- no rounding).
 *
 * Datetime model: fetched_at / utc_now travel as INTEGER epoch-MICROSECONDS
 * (UTC). The GPT-6.2 Ultrafast deadline datetime(2027,3,1,8,UTC) is pinned at
 * 1803888000000000 us (probed). _openai_served_tier_entry compares us < deadline.
 *
 * Carrier field sets (full dataclass projections, generator emits them):
 *   PricingEntry (15): input/output/cache_read/cache_write/cache_write_1h
 *     _cost_per_million, request_cost, source, source_url, pricing_version,
 *     fetched_at, tier_threshold_tokens, input/output/cache_read/cache_write
 *     _cost_per_million_above.
 *   CanonicalUsage (8): input/output/cache_read/cache_write/reasoning_tokens,
 *     request_count, raw_usage, cache_write_by_ttl ([[tier,n],...]).
 *   CostResult (7): amount_usd, status, source, label, fetched_at,
 *     pricing_version, notes ([str,...]).
 *   BillingRoute (4): provider, model, base_url, billing_mode.
 *
 * Constant tables (ULTRA / GPT62 / FAST / HOSTS / TIERPATHS) are byte-pinned
 * from the live module (built by the real _snap at import); the generator's
 * --check mode re-derives them from live and diffs the embedded JSON literals.
 *
 * Domain restrictions (documented, enforced by the generator):
 *   - ASCII inputs; strip()/lower() operate on ASCII whitespace/letters.
 *   - Decimal parse (_snap): str inputs (incl. underscores-anywhere, exponent,
 *     inf/nan specials); null inp/out -> TypeError. tiers keys are a subset of
 *     {tier_threshold_tokens, *_cost_per_million_above}; an UNKNOWN tier key ->
 *     TypeError "PricingEntry.__init__() got an unexpected keyword argument 'k'"
 *     (no module prefix, safe). Duplicate-tier collisions (module-prefix
 *     dependent) are EXCLUDED. Decimal parse errors fire BEFORE tier binding
 *     (kwarg expressions evaluate left-to-right; probed).
 *   - _usage_field: obj is a dict/attribute carrier (JSON object) or scalar;
 *     hop names are plain ASCII identifiers (no dunders). int() magnitude
 *     < 2^53 (numbers) / <=15 digits (strings); NaN/Inf excluded. dict vs
 *     attribute carriers are observationally equivalent on the JSON view
 *     (empty-carrier truthiness converges to 0), so no __attr__ wrapper needed.
 *   - base_url_origin scripts: null (TypeError unpack) or a 3-element array;
 *     wrong-length arrays -> ValueError unpack (py3.14 got-count messages).
 *     Other shapes (str unpack) excluded.
 *   - get_model_info scripts: null or a NON-EMPTY object carrying the full
 *     {has_cost_data, cost_input, cost_output, cost_cache_read, cost_cache_write}
 *     field set; has_cost_data() is REAL carrier logic (a bool field), not a seam.
 *   - to_decimal scripts: null, {"__dec__":str}, or a raise spec.
 *   - normalize_anthropic_model_name scripts: str or null (dict-key domain).
 *   - utc_now scripts: integer epoch-us (or a raise spec).
 *   - entry Decimal fields in mult positions are finite-or-null; integral
 *     floats excluded from str()-computed positions (libjson has no int/float
 *     identity).
 * =========================================================================== */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <limits.h>

typedef struct up2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} up2_io_t;

/* ---- truthiness / accessors ---- */

static int up2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

/* `is not None` identity: present-non-null. */
static int up2_present(const json_t *v)
{
    return (v && v->type != JSON_NULL) ? 1 : 0;
}

static const char *up2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *up2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double up2_num(const json_t *v)
{
    if (v && v->type == JSON_NUMBER) return v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return 0.0;
}

/* Python type-name of a JSON node (for AttributeError / unpack messages). */
static const char *up2_type_name(const json_t *v)
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

static int up2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *up2_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

/* FULL builtin exception-parent hierarchy (probed on quarry CPython 3.14.9).
 * ExceptionGroup has two bases (BaseExceptionGroup, Exception); it IS an
 * Exception, handled specially in up2_is_exception. CancelledError is the
 * asyncio extension (BaseException since 3.8). InvalidOperation (decimal) is
 * a DecimalException -> ArithmeticError -> Exception. */
typedef struct { const char *name; const char *parent; } up2_exc_row;
static const up2_exc_row UP2_EXC_PARENT[] = {
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
    {"DecimalException", "ArithmeticError"},
    {"InvalidOperation", "DecimalException"},
    {"DivisionByZero", "DecimalException"},
    {"Inexact", "DecimalException"},
    {"InvalidContext", "DecimalException"},
    {"Rounded", "DecimalException"},
    {"Subnormal", "DecimalException"},
    {"Overflow", "DecimalException"},
    {"Underflow", "DecimalException"},
    {"Clamped", "DecimalException"},
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

static const char *up2_exc_parent(const char *kind)
{
    for (size_t i = 0; UP2_EXC_PARENT[i].name; i++)
        if (strcmp(UP2_EXC_PARENT[i].name, kind) == 0)
            return UP2_EXC_PARENT[i].parent;
    return NULL;
}

/* True iff the scripted kind is caught by `except Exception`. Unknown kinds are
 * synthetic Exception subclasses in the oracle, so they ARE exceptions. */
static int up2_is_exception(const char *kind)
{
    if (strcmp(kind, "ExceptionGroup") == 0) return 1;
    const char *cur = kind;
    for (int depth = 0; depth < 32 && cur; depth++) {
        if (strcmp(cur, "Exception") == 0) return 1;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return 0;
        cur = up2_exc_parent(cur);
        if (!cur) return 1; /* unknown -> synthetic Exception subclass */
    }
    return 1;
}

/* True iff kind is caught by `except (names...)` -- subclass-aware. */
static int up2_caught_by(const char *kind, const char *const *names, size_t n)
{
    const char *cur = kind;
    for (int depth = 0; depth < 32 && cur; depth++) {
        for (size_t i = 0; i < n; i++)
            if (strcmp(cur, names[i]) == 0) return 1;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return 0;
        cur = up2_exc_parent(cur);
        if (!cur) return 0; /* unknown -> synthetic; not listed */
    }
    return 0;
}

/* malloc'd printf-style string builder */
static char *up2_xfmt(const char *fmt, ...)
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
static json_t *up2_call(up2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = up2_xfmt("no seam installed for %s", name);
        r = up2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *up2_args(void)
{
    return json_object();
}

/* ---- string helpers (ASCII domain) ---- */

/* str.strip White_Space set (ASCII): incl. \x1c-\x1f (Decimal strips these). */
static int up2_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}

/* int() strips a NARROWER set: White_Space minus \x1c-\x1f. */
static int up2_is_int_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *up2_strip(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && up2_is_ws((unsigned char)s[a])) a++;
    while (b > a && up2_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *up2_lower(const char *s)
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

static int up2_startswith(const char *s, const char *p)
{
    return strncmp(s, p, strlen(p)) == 0;
}

static int up2_endswith(const char *s, const char *suf)
{
    size_t ns = strlen(s), nf = strlen(suf);
    if (nf > ns) return 0;
    return strcmp(s + ns - nf, suf) == 0;
}

/* str(x) projection (ASCII domain). Caller frees. */
static char *up2_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return up2_xfmt("None");
    if (v->type == JSON_BOOL) return up2_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == 0.0) return up2_xfmt("0");
        if (isfinite(d) && d == floor(d) && fabs(d) < 1e16) return up2_xfmt("%.0f", d);
        return up2_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return up2_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize(v);
    return s ? s : up2_xfmt("");
}

/* ---- Decimal model ----
 * special: 0 finite, 1 NaN, 2 sNaN, 3 Infinity. coeff: malloc'd digit string
 * (no leading zeros; "0" for zero); NULL for specials. exp: base-10 exponent. */
typedef struct up2_dec {
    int sign;       /* 1 if negative */
    int special;
    char *coeff;
    long exp;
} up2_dec_t;

static void up2_dec_free(up2_dec_t *d)
{
    if (d->coeff) { free(d->coeff); d->coeff = NULL; }
}

/* case-insensitive compare of a bounded span */
static int up2_ieq(const char *s, size_t n, const char *lit)
{
    if (strlen(lit) != n) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        char l = lit[i];
        if (l >= 'A' && l <= 'Z') l += 32;
        if (c != l) return 0;
    }
    return 1;
}

/* Parse a Decimal string. *err: 0 ok, 1 ConversionSyntax. ASCII domain.
 * Grammar: strip WS, remove ALL '_', optional sign, then specials
 * (inf/infinity/nan/snan) OR mantissa digits[.digits] (>=1 digit) with optional
 * exponent e[+-]digits (>=1 digit). */
static up2_dec_t up2_dec_parse(const char *s, int *err)
{
    up2_dec_t d; d.sign = 0; d.special = 0; d.coeff = NULL; d.exp = 0;
    *err = 0;
    if (!s) { *err = 1; return d; }
    /* strip whitespace */
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && up2_is_ws((unsigned char)s[a])) a++;
    while (b > a && up2_is_ws((unsigned char)s[b - 1])) b--;
    /* remove underscores into a clean buffer */
    char *clean = (char *)malloc(b - a + 1);
    if (!clean) { *err = 1; return d; }
    size_t cn = 0;
    for (size_t i = a; i < b; i++)
        if (s[i] != '_') clean[cn++] = s[i];
    clean[cn] = '\0';
    size_t pos = 0;
    if (cn == 0) { free(clean); *err = 1; return d; }
    if (clean[0] == '+' || clean[0] == '-') {
        d.sign = (clean[0] == '-') ? 1 : 0;
        pos = 1;
    }
    if (pos >= cn) { free(clean); *err = 1; return d; }
    /* specials */
    size_t rem = cn - pos;
    if (up2_ieq(clean + pos, rem, "infinity") || up2_ieq(clean + pos, rem, "inf")) {
        d.special = 3; free(clean); return d;
    }
    if (up2_ieq(clean + pos, rem, "nan")) { d.special = 1; d.sign = 0; free(clean); return d; }
    if (up2_ieq(clean + pos, rem, "snan")) { d.special = 2; d.sign = 0; free(clean); return d; }
    /* mantissa: digits* [. digits*] with >=1 digit total */
    size_t int_start = pos;
    while (pos < cn && clean[pos] >= '0' && clean[pos] <= '9') pos++;
    size_t int_end = pos;
    size_t frac_start = pos, frac_end = pos;
    if (pos < cn && clean[pos] == '.') {
        pos++;
        frac_start = pos;
        while (pos < cn && clean[pos] >= '0' && clean[pos] <= '9') pos++;
        frac_end = pos;
    }
    size_t int_digits = int_end - int_start;
    size_t frac_digits = frac_end - frac_start;
    if (int_digits + frac_digits == 0) { free(clean); *err = 1; return d; }
    long expval = 0;
    if (pos < cn && (clean[pos] == 'e' || clean[pos] == 'E')) {
        pos++;
        int esign = 1;
        if (pos < cn && (clean[pos] == '+' || clean[pos] == '-')) {
            esign = (clean[pos] == '-') ? -1 : 1;
            pos++;
        }
        size_t ed_start = pos;
        long ev = 0;
        while (pos < cn && clean[pos] >= '0' && clean[pos] <= '9') {
            ev = ev * 10 + (clean[pos] - '0');
            if (ev > 100000000L) { free(clean); *err = 1; return d; } /* domain cap */
            pos++;
        }
        if (pos == ed_start) { free(clean); *err = 1; return d; }
        expval = esign * ev;
    }
    if (pos != cn) { free(clean); *err = 1; return d; }
    /* coefficient = int digits + frac digits, strip leading zeros */
    size_t total = int_digits + frac_digits;
    char *raw = (char *)malloc(total + 1);
    if (!raw) { free(clean); *err = 1; return d; }
    memcpy(raw, clean + int_start, int_digits);
    memcpy(raw + int_digits, clean + frac_start, frac_digits);
    raw[total] = '\0';
    size_t lead = 0;
    while (lead + 1 < total && raw[lead] == '0') lead++;
    d.coeff = up2_xfmt("%s", raw + lead);
    free(raw);
    d.exp = expval - (long)frac_digits;
    free(clean);
    if (!d.coeff) *err = 1;
    return d;
}

/* str(Decimal) rendering. Caller frees. */
static char *up2_dec_str(const up2_dec_t *d)
{
    if (d->special == 1) return up2_xfmt("NaN");
    if (d->special == 2) return up2_xfmt("sNaN");
    if (d->special == 3) return up2_xfmt(d->sign ? "-Infinity" : "Infinity");
    const char *c = d->coeff ? d->coeff : "0";
    long exp = d->exp;
    int sign = d->sign;
    size_t len = strlen(c);
    int iszero = (len == 1 && c[0] == '0');
    if (iszero) {
        if (exp > 0) return up2_xfmt("%s0E+%ld", sign ? "-" : "", exp);
        if (exp == 0) return up2_xfmt("%s0", sign ? "-" : "");
        /* exp < 0: 0.000... */
        char *zeros = (char *)malloc((size_t)(-exp) + 1);
        if (!zeros) return up2_xfmt("0");
        memset(zeros, '0', (size_t)(-exp)); zeros[-exp] = '\0';
        char *r = up2_xfmt("%s0.%s", sign ? "-" : "", zeros);
        free(zeros);
        return r;
    }
    long adjusted = exp + (long)len - 1;
    if (exp <= 0 && adjusted >= -6) {
        /* positional */
        if (exp >= 0) {
            char *zeros = (char *)malloc((size_t)exp + 1);
            if (zeros) { memset(zeros, '0', (size_t)exp); zeros[exp] = '\0'; }
            char *r = up2_xfmt("%s%s%s", sign ? "-" : "", c, zeros ? zeros : "");
            free(zeros);
            return r;
        }
        long point = (long)len + exp; /* digits before the decimal point */
        if (point > 0) {
            char *r = up2_xfmt("%s%.*s.%s", sign ? "-" : "", (int)point, c, c + point);
            return r;
        }
        /* 0.000coeff */
        size_t nz = (size_t)(-point);
        char *zeros = (char *)malloc(nz + 1);
        if (!zeros) return up2_xfmt("0");
        memset(zeros, '0', nz); zeros[nz] = '\0';
        char *r = up2_xfmt("%s0.%s%s", sign ? "-" : "", zeros, c);
        free(zeros);
        return r;
    }
    /* scientific: d[.ddd]E[+-]adjusted */
    char *mant;
    if (len == 1) mant = up2_xfmt("%s", c);
    else mant = up2_xfmt("%c.%s", c[0], c + 1);
    char *r = up2_xfmt("%s%sE%c%ld", sign ? "-" : "", mant ? mant : "",
                       adjusted < 0 ? '-' : '+', adjusted < 0 ? -adjusted : adjusted);
    free(mant);
    return r;
}

/* grade-school coefficient multiply (no leading zeros in result). Caller frees. */
static char *up2_coeff_mul(const char *a, const char *b)
{
    size_t na = strlen(a), nb = strlen(b);
    int *res = (int *)calloc(na + nb, sizeof(int));
    if (!res) return NULL;
    for (size_t i = na; i-- > 0;) {
        for (size_t j = nb; j-- > 0;) {
            int mul = (a[i] - '0') * (b[j] - '0');
            size_t p1 = i + j, p2 = i + j + 1;
            int sum = mul + res[p2];
            res[p2] = sum % 10;
            res[p1] += sum / 10;
        }
    }
    size_t start = 0;
    while (start + 1 < na + nb && res[start] == 0) start++;
    char *out = (char *)malloc(na + nb - start + 1);
    if (!out) { free(res); return NULL; }
    size_t k = 0;
    for (size_t i = start; i < na + nb; i++) out[k++] = (char)('0' + res[i]);
    out[k] = '\0';
    free(res);
    return out;
}

/* Exact Decimal multiply. Domain: finite x finite (specials handled defensively). */
static up2_dec_t up2_dec_mul(const up2_dec_t *a, const up2_dec_t *b)
{
    up2_dec_t r; r.sign = 0; r.special = 0; r.coeff = NULL; r.exp = 0;
    if (a->special || b->special) {
        /* NaN/sNaN propagates; Inf x finite -> Inf; Inf x 0 -> NaN */
        if (a->special == 1 || a->special == 2 || b->special == 1 || b->special == 2) {
            r.special = 1; return r;
        }
        int azero = (!a->special && a->coeff && strcmp(a->coeff, "0") == 0);
        int bzero = (!b->special && b->coeff && strcmp(b->coeff, "0") == 0);
        if (azero || bzero) { r.special = 1; return r; }
        r.special = 3; r.sign = a->sign ^ b->sign; return r;
    }
    r.sign = a->sign ^ b->sign;
    r.coeff = up2_coeff_mul(a->coeff ? a->coeff : "0", b->coeff ? b->coeff : "0");
    r.exp = a->exp + b->exp;
    if (!r.coeff) r.coeff = up2_xfmt("0");
    return r;
}

static up2_dec_t up2_dec_from_long(long v)
{
    up2_dec_t d; d.special = 0; d.exp = 0;
    d.sign = v < 0 ? 1 : 0;
    unsigned long uv = v < 0 ? (unsigned long)(-v) : (unsigned long)v;
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%lu", uv);
    d.coeff = up2_xfmt("%s", tmp);
    return d;
}

static int up2_dec_finite(const up2_dec_t *d) { return d->special == 0; }

/* value < 0 (only called for finite values; live short-circuits on is_finite). */
static int up2_dec_lt_zero(const up2_dec_t *d)
{
    return d->special == 0 && d->sign && d->coeff && strcmp(d->coeff, "0") != 0;
}

/* bool(Decimal): specials truthy; finite truthy iff coefficient != 0. */
static int up2_dec_truth(const up2_dec_t *d)
{
    if (d->special) return 1;
    return d->coeff && strcmp(d->coeff, "0") != 0;
}

static json_t *up2_dec_to_json(const up2_dec_t *d)
{
    char *s = up2_dec_str(d);
    json_t *o = json_object();
    json_set(o, "__dec__", json_string(s ? s : "0"));
    free(s);
    return o;
}

/* Read a {"__dec__":str} node into a Decimal. *ok=0 if absent/null/not-a-dec. */
static up2_dec_t up2_dec_from_json(const json_t *v, int *ok)
{
    up2_dec_t d; d.sign = 0; d.special = 0; d.coeff = NULL; d.exp = 0;
    *ok = 0;
    const char *s = up2_str(up2_get(v, "__dec__"));
    if (!s) return d;
    int err = 0;
    d = up2_dec_parse(s, &err);
    if (err) { up2_dec_free(&d); return d; }
    *ok = 1;
    return d;
}

/* ---- int() coercion (for _usage_field) ----
 * Returns the int value; *ok=0 on TypeError/ValueError (caught -> 0 upstream).
 * number -> trunc toward zero; bool -> 1/0; string -> base-10 parse with
 * single underscores BETWEEN digits; other -> fail. ASCII domain. */
static long long up2_pyint(const json_t *v, int *ok)
{
    *ok = 0;
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *ok = 1; return v->bool_val ? 1 : 0; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) return 0;
        if (d >= 9.0e15 || d <= -9.0e15) return 0; /* domain cap < 2^53 */
        *ok = 1;
        return (long long)trunc(d);
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s);
        size_t a = 0, b = n;
        while (a < b && up2_is_int_ws((unsigned char)s[a])) a++;
        while (b > a && up2_is_int_ws((unsigned char)s[b - 1])) b--;
        if (a >= b) return 0;
        size_t pos = a;
        int sign = 1;
        if (s[pos] == '+' || s[pos] == '-') { sign = (s[pos] == '-') ? -1 : 1; pos++; }
        if (pos >= b) return 0;
        /* digits with single underscores between digits */
        char digits[64];
        size_t dn = 0;
        int prev_digit = 0;
        for (size_t i = pos; i < b; i++) {
            char c = s[i];
            if (c >= '0' && c <= '9') {
                if (dn >= 15) return 0; /* domain cap <=15 digits */
                digits[dn++] = c;
                prev_digit = 1;
            } else if (c == '_') {
                if (!prev_digit) return 0;       /* leading / double underscore */
                if (i + 1 >= b) return 0;        /* trailing underscore */
                char nx = s[i + 1];
                if (!(nx >= '0' && nx <= '9')) return 0;
                prev_digit = 0;
            } else {
                return 0;
            }
        }
        if (dn == 0) return 0;
        digits[dn] = '\0';
        errno = 0;
        long long val = strtoll(digits, NULL, 10);
        if (errno == ERANGE) return 0;
        *ok = 1;
        return sign * val;
    }
    return 0; /* object / array -> TypeError (caught) */
}

/* ---- carrier field sets ---- */

static const char *UP2_ENTRY_FIELDS[] = {
    "input_cost_per_million", "output_cost_per_million", "cache_read_cost_per_million",
    "cache_write_cost_per_million", "cache_write_1h_cost_per_million", "request_cost",
    "source", "source_url", "pricing_version", "fetched_at", "tier_threshold_tokens",
    "input_cost_per_million_above", "output_cost_per_million_above",
    "cache_read_cost_per_million_above", "cache_write_cost_per_million_above", NULL
};
/* the 9 Decimal "cost_per_million" fields, in dataclass order (for the tier mult) */
static const char *UP2_COST_FIELDS[] = {
    "input_cost_per_million", "output_cost_per_million", "cache_read_cost_per_million",
    "cache_write_cost_per_million", "cache_write_1h_cost_per_million",
    "input_cost_per_million_above", "output_cost_per_million_above",
    "cache_read_cost_per_million_above", "cache_write_cost_per_million_above", NULL
};

static int up2_is_entry_field(const char *k)
{
    for (size_t i = 0; UP2_ENTRY_FIELDS[i]; i++)
        if (strcmp(UP2_ENTRY_FIELDS[i], k) == 0) return 1;
    return 0;
}

/* blank PricingEntry: all Optional null, source "none". */
static json_t *up2_entry_blank(void)
{
    json_t *e = json_object();
    for (size_t i = 0; UP2_ENTRY_FIELDS[i]; i++) {
        if (strcmp(UP2_ENTRY_FIELDS[i], "source") == 0)
            json_set(e, UP2_ENTRY_FIELDS[i], json_string("none"));
        else
            json_set(e, UP2_ENTRY_FIELDS[i], json_null());
    }
    return e;
}

/* ---- embedded constant tables (byte-pinned from live; generator --check) ---- */

/*UP2_TABLE_BEGIN:ULTRA*/
static const char *UP2_T_ULTRA =
    "{\"gpt-6-astra\":{\"input_cost_per_million\":{\"__dec__\":\"60.00\"},\"output_cost_per_million\":{\"__dec__\":\"300.00\"},\"cache_read_cost_per_million\":{\"__dec__\":\"6.00\"},\"cache_write_cost_per_million\":{\"__dec__\":\"75.00\"},\"cache_write_1h_cost_per_million\":null,\"request_cost\":null,\"source\":\"official_docs_snapshot\",\"source_url\":\"https://developers.openai.com/api/docs/pricing?latest-pricing=ultrafast\",\"pricing_version\":\"openai-ultrafast-2026-09\",\"fetched_at\":null,\"tier_threshold_tokens\":272000,\"input_cost_per_million_above\":{\"__dec__\":\"120.00\"},\"output_cost_per_million_above\":{\"__dec__\":\"450.00\"},\"cache_read_cost_per_million_above\":{\"__dec__\":\"12.00\"},\"cache_write_cost_per_million_above\":{\"__dec__\":\"150.00\"}},\"gpt-6.1-sol\":{\"input_cost_per_million\":{\"__dec__\":\"12.00\"},\"output_cost_per_million\":{\"__dec__\":\"60.00\"},\"cache_read_cost_per_million\":{\"__dec__\":\"0.60\"},\"cache_write_cost_per_million\":{\"__dec__\":\"15.00\"},\"cache_write_1h_cost_per_million\":null,\"request_cost\":null,\"source\":\"official_docs_snapshot\",\"source_url\":\"https://developers.openai.com/api/docs/pricing?latest-pricing=ultrafast\",\"pricing_version\":\"openai-ultrafast-2026-10-08\",\"fetched_at\":null,\"tier_threshold_tokens\":272000,\"input_cost_per_million_above\":{\"__dec__\":\"24.00\"},\"output_cost_per_million_above\":{\"__dec__\":\"90.00\"},\"cache_read_cost_per_million_above\":{\"__dec__\":\"1.20\"},\"cache_write_cost_per_million_above\":{\"__dec__\":\"30.00\"}}}";
/*UP2_TABLE_END:ULTRA*/

/*UP2_TABLE_BEGIN:GPT62*/
static const char *UP2_T_GPT62 =
    "{\"gpt-6.2-astra\":{\"input_cost_per_million\":{\"__dec__\":\"8.00\"},\"output_cost_per_million\":{\"__dec__\":\"40.00\"},\"cache_read_cost_per_million\":{\"__dec__\":\"0.40\"},\"cache_write_cost_per_million\":{\"__dec__\":\"10.00\"},\"cache_write_1h_cost_per_million\":null,\"request_cost\":null,\"source\":\"official_docs_snapshot\",\"source_url\":\"https://developers.openai.com/api/docs/models/gpt-6.2-astra\",\"pricing_version\":\"openai-gpt-6.2-2026-11\",\"fetched_at\":null,\"tier_threshold_tokens\":272000,\"input_cost_per_million_above\":{\"__dec__\":\"16.00\"},\"output_cost_per_million_above\":{\"__dec__\":\"60.000\"},\"cache_read_cost_per_million_above\":{\"__dec__\":\"0.80\"},\"cache_write_cost_per_million_above\":{\"__dec__\":\"20.00\"}},\"gpt-6.2-sol\":{\"input_cost_per_million\":{\"__dec__\":\"2.00\"},\"output_cost_per_million\":{\"__dec__\":\"10.00\"},\"cache_read_cost_per_million\":{\"__dec__\":\"0.10\"},\"cache_write_cost_per_million\":{\"__dec__\":\"2.50\"},\"cache_write_1h_cost_per_million\":null,\"request_cost\":null,\"source\":\"official_docs_snapshot\",\"source_url\":\"https://developers.openai.com/api/docs/models/gpt-6.2-sol\",\"pricing_version\":\"openai-gpt-6.2-2026-11\",\"fetched_at\":null,\"tier_threshold_tokens\":272000,\"input_cost_per_million_above\":{\"__dec__\":\"4.00\"},\"output_cost_per_million_above\":{\"__dec__\":\"15.000\"},\"cache_read_cost_per_million_above\":{\"__dec__\":\"0.20\"},\"cache_write_cost_per_million_above\":{\"__dec__\":\"5.00\"}}}";
/*UP2_TABLE_END:GPT62*/

/*UP2_TABLE_BEGIN:FAST*/
static const char *UP2_T_FAST =
    "{\"claude-opus-4-8\":{\"input_cost_per_million\":{\"__dec__\":\"10.00\"},\"output_cost_per_million\":{\"__dec__\":\"50.00\"},\"cache_read_cost_per_million\":{\"__dec__\":\"1.00\"},\"cache_write_cost_per_million\":{\"__dec__\":\"12.50\"},\"cache_write_1h_cost_per_million\":null,\"request_cost\":null,\"source\":\"official_docs_snapshot\",\"source_url\":\"https://platform.claude.com/docs/en/about-claude/pricing#fast-mode-pricing\",\"pricing_version\":\"anthropic-fast-mode-2026-09\",\"fetched_at\":null,\"tier_threshold_tokens\":null,\"input_cost_per_million_above\":null,\"output_cost_per_million_above\":null,\"cache_read_cost_per_million_above\":null,\"cache_write_cost_per_million_above\":null},\"claude-opus-5\":{\"input_cost_per_million\":{\"__dec__\":\"10.00\"},\"output_cost_per_million\":{\"__dec__\":\"50.00\"},\"cache_read_cost_per_million\":{\"__dec__\":\"1.00\"},\"cache_write_cost_per_million\":{\"__dec__\":\"12.50\"},\"cache_write_1h_cost_per_million\":null,\"request_cost\":null,\"source\":\"official_docs_snapshot\",\"source_url\":\"https://platform.claude.com/docs/en/about-claude/pricing#fast-mode-pricing\",\"pricing_version\":\"anthropic-fast-mode-2026-09\",\"fetched_at\":null,\"tier_threshold_tokens\":null,\"input_cost_per_million_above\":null,\"output_cost_per_million_above\":null,\"cache_read_cost_per_million_above\":null,\"cache_write_cost_per_million_above\":null},\"claude-opus-5-5\":{\"input_cost_per_million\":{\"__dec__\":\"8.00\"},\"output_cost_per_million\":{\"__dec__\":\"40.00\"},\"cache_read_cost_per_million\":{\"__dec__\":\"0.40\"},\"cache_write_cost_per_million\":{\"__dec__\":\"10.00\"},\"cache_write_1h_cost_per_million\":null,\"request_cost\":null,\"source\":\"official_docs_snapshot\",\"source_url\":\"https://platform.claude.com/docs/en/about-claude/pricing#fast-mode-pricing\",\"pricing_version\":\"anthropic-fast-mode-2026-09\",\"fetched_at\":null,\"tier_threshold_tokens\":null,\"input_cost_per_million_above\":null,\"output_cost_per_million_above\":null,\"cache_read_cost_per_million_above\":null,\"cache_write_cost_per_million_above\":null},\"claude-fable-5-2\":{\"input_cost_per_million\":{\"__dec__\":\"16.00\"},\"output_cost_per_million\":{\"__dec__\":\"80.00\"},\"cache_read_cost_per_million\":{\"__dec__\":\"0.40\"},\"cache_write_cost_per_million\":{\"__dec__\":\"20.00\"},\"cache_write_1h_cost_per_million\":null,\"request_cost\":null,\"source\":\"official_docs_snapshot\",\"source_url\":\"https://platform.claude.com/docs/en/about-claude/pricing#fast-mode-pricing\",\"pricing_version\":\"anthropic-fast-mode-2026-09\",\"fetched_at\":null,\"tier_threshold_tokens\":null,\"input_cost_per_million_above\":null,\"output_cost_per_million_above\":null,\"cache_read_cost_per_million_above\":null,\"cache_write_cost_per_million_above\":null},\"claude-mythos-5-2\":{\"input_cost_per_million\":{\"__dec__\":\"16.00\"},\"output_cost_per_million\":{\"__dec__\":\"80.00\"},\"cache_read_cost_per_million\":{\"__dec__\":\"0.40\"},\"cache_write_cost_per_million\":{\"__dec__\":\"20.00\"},\"cache_write_1h_cost_per_million\":null,\"request_cost\":null,\"source\":\"official_docs_snapshot\",\"source_url\":\"https://platform.claude.com/docs/en/about-claude/pricing#fast-mode-pricing\",\"pricing_version\":\"anthropic-fast-mode-2026-09\",\"fetched_at\":null,\"tier_threshold_tokens\":null,\"input_cost_per_million_above\":null,\"output_cost_per_million_above\":null,\"cache_read_cost_per_million_above\":null,\"cache_write_cost_per_million_above\":null}}";
/*UP2_TABLE_END:FAST*/

/*UP2_TABLE_BEGIN:HOSTS*/
static const char *UP2_T_HOSTS =
    "{\"openai\":\"openai.com\",\"xai\":\"x.ai\",\"anthropic\":\"anthropic.com\",\"google\":\"googleapis.com\",\"deepseek\":\"deepseek.com\",\"xiaomi\":\"xiaomimimo.com\"}";
/*UP2_TABLE_END:HOSTS*/

/*UP2_TABLE_BEGIN:TIERPATHS*/
static const char *UP2_T_TIERPATHS =
    "[[\"5m\",[\"cache_creation\",\"ephemeral_5m_input_tokens\"]],[\"1h\",[\"cache_creation\",\"ephemeral_1h_input_tokens\"]],[\"30m\",[\"input_tokens_details\",\"cache_write_tokens_details\",\"ttl_30m_tokens\"]],[\"12h\",[\"input_tokens_details\",\"cache_write_tokens_details\",\"ttl_12h_tokens\"]]]";
/*UP2_TABLE_END:TIERPATHS*/

static json_t *up2_table(const char *lit)
{
    return json_parse(lit, NULL);
}

/* lookup a model-keyed entry table; returns a NEW copy or NULL. */
static json_t *up2_table_get(const char *lit, const char *key)
{
    if (!key) return NULL;
    json_t *t = up2_table(lit);
    if (!t) return NULL;
    json_t *e = json_obj_get(t, key);
    json_t *r = e ? json_copy(e) : NULL;
    json_free(t);
    return r;
}

/* ---- _usage_field core (pure; shared by _first_nonzero / cache_write_tiers) ----
 * obj/path borrowed. Returns a NEW json number (the clamped int). */
static json_t *up2_usage_field_core(const json_t *obj, const json_t *path)
{
    json_t *cur = obj ? json_copy(obj) : json_null();
    size_t nhops = (path && path->type == JSON_ARRAY) ? json_len(path) : 0;
    for (size_t i = 0; i < nhops; i++) {
        if (!up2_truth(cur)) { json_free(cur); return json_number(0); }
        const char *hop = up2_str(json_get(path, i));
        if (!hop) hop = "";
        if (cur->type == JSON_OBJECT) {
            json_t *nxt = json_obj_get(cur, hop);
            json_t *copy = nxt ? json_copy(nxt) : json_number(0); /* .get(hop, 0) */
            json_free(cur);
            cur = copy;
        } else {
            /* getattr(non-dict, hop, 0) -> 0 for scalar/array carriers */
            json_free(cur);
            cur = json_number(0);
        }
    }
    /* max(0, int(obj or 0)) with except Exception -> 0 */
    long long result = 0;
    if (up2_truth(cur)) {
        int ok = 0;
        long long v = up2_pyint(cur, &ok);
        if (ok) result = v;
    }
    json_free(cur);
    if (result < 0) result = 0;
    return json_number((double)result);
}

/* ============================ ENTRIES ============================ */

/* PoP: up2_snap @ agent/usage_pricing.py:_snap */
static json_t *up2_snap(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *inp = up2_get(in, "inp");
    const json_t *outp = up2_get(in, "out");
    const json_t *cr = up2_get(in, "cache_read");
    const json_t *cw = up2_get(in, "cache_write");
    const json_t *version = up2_get(in, "version");
    const json_t *url = up2_get(in, "url");
    const json_t *tiers = up2_get(in, "tiers");

    /* Decimal(inp), Decimal(out) evaluate first (kwarg left-to-right). */
    up2_dec_t din, dout, dcr, dcw;
    int err = 0;
    const char *ins = up2_str(inp);
    if (!up2_present(inp)) {
        return up2_mkraise("TypeError", "conversion from NoneType to Decimal is not supported");
    }
    din = up2_dec_parse(ins ? ins : "", &err);
    if (err) { up2_dec_free(&din); return up2_mkraise("InvalidOperation", "[<class 'decimal.ConversionSyntax'>]"); }
    if (!up2_present(outp)) {
        up2_dec_free(&din);
        return up2_mkraise("TypeError", "conversion from NoneType to Decimal is not supported");
    }
    const char *outs = up2_str(outp);
    dout = up2_dec_parse(outs ? outs : "", &err);
    if (err) { up2_dec_free(&din); up2_dec_free(&dout); return up2_mkraise("InvalidOperation", "[<class 'decimal.ConversionSyntax'>]"); }
    /* cache_read / cache_write: Decimal(x) if x is not None else None */
    int have_cr = 0, have_cw = 0;
    dcr.sign = dcr.special = 0; dcr.coeff = NULL; dcr.exp = 0;
    dcw.sign = dcw.special = 0; dcw.coeff = NULL; dcw.exp = 0;
    if (up2_present(cr)) {
        const char *s = up2_str(cr);
        dcr = up2_dec_parse(s ? s : "", &err);
        if (err) { up2_dec_free(&din); up2_dec_free(&dout); up2_dec_free(&dcr);
            return up2_mkraise("InvalidOperation", "[<class 'decimal.ConversionSyntax'>]"); }
        have_cr = 1;
    }
    if (up2_present(cw)) {
        const char *s = up2_str(cw);
        dcw = up2_dec_parse(s ? s : "", &err);
        if (err) { up2_dec_free(&din); up2_dec_free(&dout); up2_dec_free(&dcr); up2_dec_free(&dcw);
            return up2_mkraise("InvalidOperation", "[<class 'decimal.ConversionSyntax'>]"); }
        have_cw = 1;
    }
    /* tier binding: unknown key -> TypeError (after Decimal parses). */
    if (tiers && tiers->type == JSON_OBJECT) {
        for (size_t i = 0; i < tiers->c.count; i++) {
            if (!up2_is_entry_field(tiers->c.keys[i])) {
                char *m = up2_xfmt("PricingEntry.__init__() got an unexpected keyword argument '%s'", tiers->c.keys[i]);
                json_t *r = up2_mkraise("TypeError", m ? m : "unexpected keyword");
                free(m);
                up2_dec_free(&din); up2_dec_free(&dout); up2_dec_free(&dcr); up2_dec_free(&dcw);
                return r;
            }
        }
    }
    json_t *e = up2_entry_blank();
    json_set(e, "input_cost_per_million", up2_dec_to_json(&din));
    json_set(e, "output_cost_per_million", up2_dec_to_json(&dout));
    json_set(e, "cache_read_cost_per_million", have_cr ? up2_dec_to_json(&dcr) : json_null());
    json_set(e, "cache_write_cost_per_million", have_cw ? up2_dec_to_json(&dcw) : json_null());
    json_set(e, "source", json_string("official_docs_snapshot"));
    json_set(e, "source_url", url ? json_copy(url) : json_null());
    json_set(e, "pricing_version", version ? json_copy(version) : json_null());
    if (tiers && tiers->type == JSON_OBJECT) {
        for (size_t i = 0; i < tiers->c.count; i++)
            json_set(e, tiers->c.keys[i], json_copy(tiers->c.items[i]));
    }
    up2_dec_free(&din); up2_dec_free(&dout); up2_dec_free(&dcr); up2_dec_free(&dcw);
    return e;
}

/* PoP: up2_usage_field @ agent/usage_pricing.py:_usage_field */
static json_t *up2_usage_field(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return up2_usage_field_core(up2_get(in, "obj"), up2_get(in, "path"));
}

/* PoP: up2_first_nonzero @ agent/usage_pricing.py:_first_nonzero */
static json_t *up2_first_nonzero(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *obj = up2_get(in, "obj");
    const json_t *paths = up2_get(in, "paths");
    size_t n = (paths && paths->type == JSON_ARRAY) ? json_len(paths) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *v = up2_usage_field_core(obj, json_get(paths, i));
        if (up2_num(v) != 0.0) return v; /* first non-zero wins (lazy) */
        json_free(v);
    }
    return json_number(0);
}

/* PoP: up2_strip_prefix @ agent/usage_pricing.py:_strip_prefix */
static json_t *up2_strip_prefix(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const char *name = up2_str(up2_get(in, "name"));
    if (!name) name = "";
    const json_t *prefixes = up2_get(in, "prefixes");
    size_t n = (prefixes && prefixes->type == JSON_ARRAY) ? json_len(prefixes) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *p = up2_str(json_get(prefixes, i));
        if (p && up2_startswith(name, p))
            return json_string(name + strlen(p));
    }
    return json_string(name);
}

/* PoP: up2_with_served_service_tier @ agent/usage_pricing.py:with_served_service_tier */
static json_t *up2_with_served_service_tier(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *usage = up2_get(in, "usage");
    const json_t *response = up2_get(in, "response");
    /* tier = getattr(response, "service_tier", None) */
    const json_t *tier = (response && response->type == JSON_OBJECT)
        ? json_obj_get(response, "service_tier") : NULL;
    const char *ts = up2_str(tier);
    char *stripped = ts ? up2_strip(ts) : NULL;
    if (!ts || !stripped || !stripped[0]) {
        free(stripped);
        return usage ? json_copy(usage) : json_null();
    }
    char *low = up2_lower(stripped);
    free(stripped);
    /* replace(usage, raw_usage={**(usage.raw_usage or {}), ...}): the merge
     * expression reads usage.raw_usage BEFORE replace() runs, so a null usage
     * raises AttributeError on the attribute read (not the replace TypeError). */
    if (!usage || usage->type != JSON_OBJECT) {
        char *m = up2_xfmt("'%s' object has no attribute 'raw_usage'", up2_type_name(usage));
        json_t *r = up2_mkraise("AttributeError", m ? m : "attribute error");
        free(m); free(low);
        return r;
    }
    json_t *ru = up2_get(usage, "raw_usage");
    json_t *merged = (ru && ru->type == JSON_OBJECT && up2_truth(ru)) ? json_copy(ru) : json_object();
    json_set(merged, "service_tier", json_string(low ? low : ""));
    free(low);
    json_t *out = json_copy(usage);
    json_set(out, "raw_usage", merged);
    return out;
}

/* PoP: up2_served_openai_tier @ agent/usage_pricing.py:_served_openai_tier */
static json_t *up2_served_openai_tier(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *usage = up2_get(in, "usage");
    const json_t *ru = up2_get(usage, "raw_usage");
    if (ru && ru->type == JSON_OBJECT) {
        json_t *t = json_obj_get(ru, "service_tier");
        return t ? json_copy(t) : json_null();
    }
    return json_null();
}

/* PoP: up2_openai_served_tier_entry @ agent/usage_pricing.py:_openai_served_tier_entry */
static json_t *up2_openai_served_tier_entry(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *model = up2_str(up2_get(in, "model"));
    const json_t *tier = up2_get(in, "tier");
    const char *ts = up2_str(tier);
    if (!model) model = "";
    int is_gpt62 = (strcmp(model, "gpt-6.2-astra") == 0 || strcmp(model, "gpt-6.2-sol") == 0);
    if (!is_gpt62) {
        if (ts && strcmp(ts, "ultrafast") == 0)
            return up2_table_get(UP2_T_ULTRA, model);
        return json_null();
    }
    json_t *entry = up2_table_get(UP2_T_GPT62, model);
    if (!entry) return up2_mkraise("KeyError", model); /* out of domain (both pinned) */
    int multiplier;
    if (ts && strcmp(ts, "priority") == 0) {
        multiplier = 2;
    } else {
        json_t *now = up2_call(io, "utc_now", up2_args());
        if (up2_is_raise(now)) { json_free(entry); return now; }
        long long us = (long long)up2_num(now);
        json_free(now);
        multiplier = (us < 1803888000000000LL) ? 3 : 6;
    }
    /* rates = {f: value*multiplier for cost fields if value is not None} */
    up2_dec_t mult = up2_dec_from_long(multiplier);
    for (size_t i = 0; UP2_COST_FIELDS[i]; i++) {
        json_t *v = json_obj_get(entry, UP2_COST_FIELDS[i]);
        int ok = 0;
        up2_dec_t d = up2_dec_from_json(v, &ok);
        if (ok) {
            up2_dec_t prod = up2_dec_mul(&d, &mult);
            json_set(entry, UP2_COST_FIELDS[i], up2_dec_to_json(&prod));
            up2_dec_free(&prod);
        }
        up2_dec_free(&d);
    }
    up2_dec_free(&mult);
    /* pricing_version = f"{entry.pricing_version}-{tier}-{multiplier}x" */
    const char *pv = up2_str(up2_get(entry, "pricing_version"));
    char *tiers_str = up2_pystr(tier);
    char *npv = up2_xfmt("%s-%s-%dx", pv ? pv : "", tiers_str ? tiers_str : "", multiplier);
    free(tiers_str);
    json_set(entry, "pricing_version", json_string(npv ? npv : ""));
    free(npv);
    return entry;
}

/* PoP: up2_served_fast @ agent/usage_pricing.py:_served_fast */
static json_t *up2_served_fast(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *usage = up2_get(in, "usage");
    const json_t *ru = up2_get(usage, "raw_usage");
    int r = 0;
    if (ru && ru->type == JSON_OBJECT) {
        const char *sp = up2_str(json_obj_get(ru, "speed"));
        r = (sp && strcmp(sp, "fast") == 0) ? 1 : 0;
    }
    return json_bool(r);
}

/* PoP: up2_anthropic_fast_mode_entry @ agent/usage_pricing.py:_anthropic_fast_mode_entry */
static json_t *up2_anthropic_fast_mode_entry(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *model = up2_str(up2_get(in, "model"));
    char *name = up2_lower(model ? model : "");
    json_t *e = up2_table_get(UP2_T_FAST, name);
    if (e) { free(name); return e; }
    /* first lookup falsy -> seam the claimed normalizer, second lookup */
    json_t *a = up2_args();
    json_set(a, "model", json_string(name));
    free(name);
    json_t *norm = up2_call(io, "normalize_anthropic_model_name", a);
    if (up2_is_raise(norm)) return norm;
    const char *ns = up2_str(norm);
    json_t *e2 = up2_table_get(UP2_T_FAST, ns);
    json_free(norm);
    return e2 ? e2 : json_null();
}

/* PoP: up2_models_dev_pricing_entry @ agent/usage_pricing.py:_models_dev_pricing_entry */
static json_t *up2_models_dev_pricing_entry(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *route = up2_get(in, "route");
    const char *provider = up2_str(up2_get(route, "provider"));
    const char *model = up2_str(up2_get(route, "model"));
    const char *base_url = up2_str(up2_get(route, "base_url"));
    /* domain = _MODELS_DEV_DIRECT_HOSTS.get(route.provider) */
    json_t *hosts = up2_table(UP2_T_HOSTS);
    const char *domain = provider ? up2_str(json_obj_get(hosts, provider)) : NULL;
    char *domain_dup = domain ? up2_xfmt("%s", domain) : NULL;
    json_free(hosts);
    if (!domain_dup || !domain_dup[0] || !model || !model[0]) {
        free(domain_dup);
        return json_null();
    }
    if (base_url && base_url[0]) {
        json_t *a = up2_args();
        json_set(a, "base_url", json_string(base_url));
        json_t *origin = up2_call(io, "base_url_origin", a);
        if (up2_is_raise(origin)) { free(domain_dup); return origin; }
        /* scheme, host, port = origin */
        if (!origin || origin->type != JSON_ARRAY) {
            char *m = up2_xfmt("cannot unpack non-iterable %s object", up2_type_name(origin));
            json_t *r = up2_mkraise("TypeError", m ? m : "cannot unpack");
            free(m); json_free(origin); free(domain_dup);
            return r;
        }
        size_t no = json_len(origin);
        if (no != 3) {
            char *m = up2_xfmt("%s values to unpack (expected 3, got %zu)",
                               no < 3 ? "not enough" : "too many", no);
            json_t *r = up2_mkraise("ValueError", m ? m : "unpack");
            free(m); json_free(origin); free(domain_dup);
            return r;
        }
        const char *scheme = up2_str(json_get(origin, 0));
        const char *host = up2_str(json_get(origin, 1));
        double port = up2_num(json_get(origin, 2));
        int ok_scheme = scheme && strcmp(scheme, "https") == 0;
        int ok_port = (port == 443.0);
        char *dotsuf = up2_xfmt(".%s", domain_dup);
        int ok_host = host && (strcmp(host, domain_dup) == 0 || up2_endswith(host, dotsuf));
        free(dotsuf);
        json_free(origin);
        if (!ok_scheme || !ok_port || !ok_host) { free(domain_dup); return json_null(); }
    }
    free(domain_dup);
    /* from agent.models_dev import get_model_info */
    json_t *a = up2_args();
    json_set(a, "provider", json_string(provider ? provider : ""));
    json_set(a, "model", json_string(model));
    json_t *mi = up2_call(io, "get_model_info", a);
    if (up2_is_raise(mi)) return mi;
    if (!up2_present(mi)) { json_free(mi); return json_null(); }
    /* model_info.has_cost_data() -- REAL carrier logic (bool field) */
    int has_cost = up2_truth(up2_get(mi, "has_cost_data"));
    if (!has_cost) { json_free(mi); return json_null(); }
    /* _to_decimal(model_info.cost_*) x4 (claimed sibling seamed) */
    const char *cost_keys[] = {"cost_input", "cost_output", "cost_cache_read", "cost_cache_write"};
    const char *entry_keys[] = {"input_cost_per_million", "output_cost_per_million",
                                "cache_read_cost_per_million", "cache_write_cost_per_million"};
    json_t *decvals[4];
    for (size_t i = 0; i < 4; i++) {
        json_t *aa = up2_args();
        json_t *cv = up2_get(mi, cost_keys[i]);
        json_set(aa, "value", cv ? json_copy(cv) : json_null());
        json_t *dv = up2_call(io, "to_decimal", aa);
        if (up2_is_raise(dv)) {
            for (size_t j = 0; j < i; j++) json_free(decvals[j]);
            json_free(mi);
            return dv;
        }
        decvals[i] = dv; /* NULL == None, else {"__dec__"} */
    }
    json_t *now = up2_call(io, "utc_now", up2_args());
    if (up2_is_raise(now)) {
        for (size_t j = 0; j < 4; j++) json_free(decvals[j]);
        json_free(mi);
        return now;
    }
    json_t *e = up2_entry_blank();
    for (size_t i = 0; i < 4; i++)
        json_set(e, entry_keys[i], decvals[i] ? decvals[i] : json_null());
    json_set(e, "source", json_string("provider_models_api"));
    json_set(e, "source_url", json_string("https://models.dev"));
    json_set(e, "pricing_version", json_string("models.dev"));
    json_set(e, "fetched_at", now ? now : json_null());
    json_free(mi);
    return e;
}

/* PoP: up2_cache_write_tiers @ agent/usage_pricing.py:cache_write_tiers */
static json_t *up2_cache_write_tiers(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *ru = up2_get(in, "response_usage");
    json_t *tp = up2_table(UP2_T_TIERPATHS);
    json_t *out = json_array();
    size_t n = tp ? json_len(tp) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_get(tp, i);
        const char *tier = up2_str(json_get(row, 0));
        json_t *path = json_get(row, 1);
        json_t *v = up2_usage_field_core(ru, path);
        if (up2_num(v) != 0.0) {
            json_t *pair = json_array();
            json_append(pair, json_string(tier ? tier : ""));
            json_append(pair, v);
            json_append(out, pair);
        } else {
            json_free(v);
        }
    }
    json_free(tp);
    return out;
}

/* PoP: up2_cache_write_buckets @ agent/usage_pricing.py:_cache_write_buckets */
static json_t *up2_cache_write_buckets(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *usage = up2_get(in, "usage");
    const json_t *entry = up2_get(in, "entry");
    long cw_tokens = (long)up2_num(up2_get(usage, "cache_write_tokens"));
    /* one_hour = min(dict(cache_write_by_ttl).get("1h", 0), cache_write_tokens) */
    const json_t *ttl = up2_get(usage, "cache_write_by_ttl");
    long one_hour_raw = 0;
    if (ttl && ttl->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ttl); i++) {
            json_t *pair = json_get(ttl, i);
            const char *t = up2_str(json_get(pair, 0));
            if (t && strcmp(t, "1h") == 0)
                one_hour_raw = (long)up2_num(json_get(pair, 1)); /* last wins */
        }
    }
    long one_hour = one_hour_raw < cw_tokens ? one_hour_raw : cw_tokens;
    const char *source = up2_str(up2_get(entry, "source"));
    json_t *cw_rate = up2_get(entry, "cache_write_cost_per_million");
    json_t *cw_above = up2_get(entry, "cache_write_cost_per_million_above");
    json_t *note = json_array();
    json_append(note, json_string("cache-write pricing unavailable for route"));

    json_t *out = json_array();
    if (one_hour != 0 && source && strcmp(source, "provider_models_api") == 0) {
        /* rate = cache_write_1h or cache_write (Decimal truthiness) */
        json_t *r1h = up2_get(entry, "cache_write_1h_cost_per_million");
        int ok = 0;
        up2_dec_t d1h = up2_dec_from_json(r1h, &ok);
        json_t *rate;
        if (ok && up2_dec_truth(&d1h)) rate = json_copy(r1h);
        else rate = cw_rate ? json_copy(cw_rate) : json_null();
        up2_dec_free(&d1h);
        json_t *row1 = json_array();
        json_append(row1, json_number((double)(cw_tokens - one_hour)));
        json_append(row1, cw_rate ? json_copy(cw_rate) : json_null());
        json_append(row1, json_null());
        json_append(row1, json_copy(note));
        json_append(out, row1);
        json_t *row2 = json_array();
        json_append(row2, json_number((double)one_hour));
        json_append(row2, rate);
        json_append(row2, json_null());
        json_append(row2, json_copy(note));
        json_append(out, row2);
        json_free(note);
        return out;
    }
    json_t *input_rate = up2_get(entry, "input_cost_per_million");
    int input_present = up2_present(input_rate);
    if (one_hour == 0 || !input_present) {
        json_t *row = json_array();
        json_append(row, json_number((double)cw_tokens));
        json_append(row, cw_rate ? json_copy(cw_rate) : json_null());
        json_append(row, cw_above ? json_copy(cw_above) : json_null());
        json_append(row, json_copy(note));
        json_append(out, row);
        json_free(note);
        return out;
    }
    /* above = input_cost_per_million_above; input*2, above*2 if above not None */
    json_t *above = up2_get(entry, "input_cost_per_million_above");
    up2_dec_t two = up2_dec_from_long(2);
    int oki = 0;
    up2_dec_t di = up2_dec_from_json(input_rate, &oki);
    up2_dec_t pi = up2_dec_mul(&di, &two);
    json_t *row1 = json_array();
    json_append(row1, json_number((double)(cw_tokens - one_hour)));
    json_append(row1, cw_rate ? json_copy(cw_rate) : json_null());
    json_append(row1, cw_above ? json_copy(cw_above) : json_null());
    json_append(row1, json_copy(note));
    json_append(out, row1);
    json_t *row2 = json_array();
    json_append(row2, json_number((double)one_hour));
    json_append(row2, up2_dec_to_json(&pi));
    if (up2_present(above)) {
        int oka = 0;
        up2_dec_t da = up2_dec_from_json(above, &oka);
        up2_dec_t pa = up2_dec_mul(&da, &two);
        json_append(row2, up2_dec_to_json(&pa));
        up2_dec_free(&da); up2_dec_free(&pa);
    } else {
        json_append(row2, json_null());
    }
    json_append(row2, json_copy(note));
    json_append(out, row2);
    up2_dec_free(&di); up2_dec_free(&pi); up2_dec_free(&two);
    json_free(note);
    return out;
}

/* PoP: up2_unknown_cost @ agent/usage_pricing.py:_unknown_cost */
static json_t *up2_unknown_cost(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *source = up2_get(in, "source");
    const json_t *notes = up2_get(in, "notes");
    json_t *r = json_object();
    json_set(r, "amount_usd", json_null());
    json_set(r, "status", json_string("unknown"));
    json_set(r, "source", source ? json_copy(source) : json_null());
    json_set(r, "label", json_string("n/a"));
    json_set(r, "fetched_at", json_null());
    json_set(r, "pricing_version", json_null());
    json_t *na = json_array();
    if (notes && notes->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(notes); i++)
            json_append(na, json_copy(json_get(notes, i)));
    json_set(r, "notes", na);
    return r;
}

/* PoP: up2_host @ agent/usage_pricing.py:host */
static json_t *up2_host(up2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *url = up2_get(in, "url");
    const json_t *name = up2_get(in, "name");
    json_t *a = up2_args();
    json_set(a, "url", url ? json_copy(url) : json_null());
    json_set(a, "name", name ? json_copy(name) : json_null());
    return up2_call(io, "base_url_host_matches", a);
}

/* rate closure core. Mutates st["rejected"] (sorted array). Returns Decimal-or-null
 * ({"__dec__"} / NULL) or a raise. pricing/key/aliases/model_id borrowed. */
static json_t *up2_rate_core(up2_io_t *io, const json_t *pricing, const char *key,
                             const json_t *aliases, const char *model_id, json_t *st)
{
    /* raw = pricing.get(key); for alias: raw = raw or pricing.get(alias) */
    json_t *raw = pricing ? json_obj_get(pricing, key ? key : "") : NULL;
    size_t na = (aliases && aliases->type == JSON_ARRAY) ? json_len(aliases) : 0;
    for (size_t i = 0; i < na; i++) {
        if (up2_truth(raw)) break;
        const char *alias = up2_str(json_get(aliases, i));
        raw = pricing ? json_obj_get(pricing, alias ? alias : "") : NULL;
    }
    /* value = _to_decimal(raw) -- claimed sibling seamed */
    json_t *a = up2_args();
    json_set(a, "value", raw ? json_copy(raw) : json_null());
    json_t *value = up2_call(io, "to_decimal", a);
    if (up2_is_raise(value)) return value;
    int ok = 0;
    up2_dec_t d = up2_dec_from_json(value, &ok);
    if (ok && (!up2_dec_finite(&d) || up2_dec_lt_zero(&d))) {
        /* logger.debug(fmt, key, raw, model_id) */
        json_t *la = up2_args();
        json_set(la, "msg", json_string("Ignoring unusable %s rate %s for %s (variable-pricing sentinel)"));
        json_t *largs = json_array();
        json_append(largs, json_string(key ? key : ""));
        json_append(largs, raw ? json_copy(raw) : json_null());
        json_append(largs, json_string(model_id ? model_id : ""));
        json_set(la, "args", largs);
        json_t *lr = up2_call(io, "logger.debug", la);
        if (up2_is_raise(lr)) { up2_dec_free(&d); json_free(value); return lr; }
        json_free(lr);
        /* rejected.add(key) -- sorted insert into st["rejected"] */
        json_t *rej = up2_get(st, "rejected");
        if (!rej || rej->type != JSON_ARRAY) { rej = json_array(); json_set(st, "rejected", rej); }
        const char *ks = key ? key : "";
        int found = 0;
        for (size_t i = 0; i < json_len(rej); i++) {
            const char *e = up2_str(json_get(rej, i));
            if (e && strcmp(e, ks) == 0) { found = 1; break; }
        }
        if (!found) {
            size_t n = json_len(rej), pos = n;
            for (size_t i = 0; i < n; i++) {
                const char *e = up2_str(json_get(rej, i));
                if (e && strcmp(ks, e) < 0) { pos = i; break; }
            }
            json_t *nr = json_array();
            for (size_t i = 0; i < n; i++) {
                if (i == pos) json_append(nr, json_string(ks));
                json_append(nr, json_copy(json_get(rej, i)));
            }
            if (pos == n) json_append(nr, json_string(ks));
            json_set(st, "rejected", nr);
        }
        up2_dec_free(&d);
        json_free(value);
        return json_null();
    }
    up2_dec_free(&d);
    return value; /* {"__dec__"} or NULL (None) */
}

/* seed st["rejected"] from in.state.rejected as a SORTED, deduped array (the
 * live rejected is a set; project_state sorts it). Sorted-insert in rate_core
 * then maintains the order. */
static int up2_cmp_str(const void *a, const void *b)
{
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

static void up2_rate_seed_state(const json_t *in, json_t *st)
{
    const json_t *state = up2_get(in, "state");
    const json_t *rej = up2_get(state, "rejected");
    json_t *arr = json_array();
    if (rej && rej->type == JSON_ARRAY && json_len(rej) > 0) {
        size_t n = json_len(rej);
        const char **tmp = (const char **)malloc(n * sizeof(char *));
        size_t cnt = 0;
        if (tmp) {
            for (size_t i = 0; i < n; i++) {
                const char *s = up2_str(json_get(rej, i));
                if (s) tmp[cnt++] = s;
            }
            qsort(tmp, cnt, sizeof(char *), up2_cmp_str);
            for (size_t i = 0; i < cnt; i++)
                if (i == 0 || strcmp(tmp[i], tmp[i - 1]) != 0)
                    json_append(arr, json_string(tmp[i]));
            free(tmp);
        }
    }
    json_set(st, "rejected", arr);
}

/* PoP: up2_rate @ agent/usage_pricing.py:rate */
static json_t *up2_rate(up2_io_t *io, const json_t *in, json_t *st)
{
    up2_rate_seed_state(in, st);
    const json_t *pricing = up2_get(in, "pricing");
    const char *key = up2_str(up2_get(in, "key"));
    const json_t *aliases = up2_get(in, "aliases");
    const char *model_id = up2_str(up2_get(in, "model_id"));
    return up2_rate_core(io, pricing, key, aliases, model_id, st);
}

/* PoP: up2_per_million @ agent/usage_pricing.py:per_million */
static json_t *up2_per_million(up2_io_t *io, const json_t *in, json_t *st)
{
    up2_rate_seed_state(in, st);
    const json_t *pricing = up2_get(in, "pricing");
    const char *key = up2_str(up2_get(in, "key"));
    const json_t *aliases = up2_get(in, "aliases");
    const char *model_id = up2_str(up2_get(in, "model_id"));
    json_t *value = up2_rate_core(io, pricing, key, aliases, model_id, st);
    if (up2_is_raise(value)) return value;
    if (!up2_present(value)) { json_free(value); return json_null(); }
    int ok = 0;
    up2_dec_t d = up2_dec_from_json(value, &ok);
    json_free(value);
    if (!ok) return json_null();
    up2_dec_t million = up2_dec_from_long(1000000);
    up2_dec_t prod = up2_dec_mul(&d, &million);
    json_t *r = up2_dec_to_json(&prod);
    up2_dec_free(&d); up2_dec_free(&million); up2_dec_free(&prod);
    return r;
}
