/* conversation_compression_flow2.c — second-wave FLOW port of
 * agent/conversation_compression.py (LIVE authority).
 *
 * Ports 20 UNCLAIMED functions: the commit-fence deadline arming / extension
 * (set_precommit_deadline_monotonic, extend_deadline), the activity-heartbeat
 * suppression / touch (_should_suppress, _touch), the compaction-lifecycle
 * terminal edge (complete), the durable-lease status / refresher / holder
 * release / nested-finally release (status_emitted, start_refresher,
 * release_holder_only, release), the attempt telemetry-hold CM and compressor
 * restore (_hold_attempt_telemetry, restore_compressor), the context-engine
 * session-db binding / close (context_engine_session_db,
 * close_context_engine_session_db), the aux-threshold lowering notice
 * (_lower_threshold_to_aux_context), the custom-provider config read
 * (_configured_custom_providers), and five run_compress_context_with_progress_
 * timeout / summary-dispatch closures (_fence_gated_worker,
 * _is_unchanged_snapshot, _compression_cancel_requested, _stop_heartbeat,
 * _resolve_fallback_prompt). Claimed siblings (_swallow, _emit_compaction_done,
 * _emit_feasibility_notice, _release_compression_lock,
 * _restore_compressor_attempt_state, clear_cancelled_lock_release,
 * finish_lock_setup, the fence is_cancelled / deadline_exceeded properties) and
 * all foreign I/O enter through the cp2_io_t universal seam; ported siblings
 * compose for REAL (complete / release_holder_only inside release,
 * _should_suppress inside _touch).
 *
 * Decision sequences only: Python truthiness, `or` / `and` short-circuits,
 * `is None` identity vs truthy tests, kwarg / operand evaluation order, dict
 * insertion order, nested try/finally unwinding (last exception wins), and
 * except-Exception catch scopes are transcribed per call site. A scripted
 * BaseException (SystemExit / KeyboardInterrupt / GeneratorExit) propagates past
 * every except Exception guard exactly as live.
 *
 * Value model (shared with tests/oracle_conversation_compression_flow2.py):
 *   - attribute doubles: {"__attr__": {...}} <-> SimpleNamespace; the state
 *     window keeps the wrapper form and is mutated in place.
 *   - class-tagged doubles {"__class__": "Fence"|"ContextCompressor"|
 *     "Persistence"} give type() / isinstance identity (Fence carries the seamed
 *     is_cancelled / deadline_exceeded properties).
 *   - callable doubles {"__callable__": "<seam>"} (callable() is True).
 *   - identity refs {"__ref__": "<id>"} (_is_unchanged_snapshot: result[0] is
 *     messages, modeled by matching the messages_id tag).
 *   - path doubles {"__path__": "/h/p1"} <-> PurePosixPath (POSIX separators).
 *   - raise specs {"__raise__": "<ClassName>", "message": str(e)}.
 *   - integral floats normalize to ints on both sides.
 *
 * Domain (ASCII scope, documented for the generator):
 *   - float() coercion in set_precommit_deadline_monotonic accepts number /
 *     bool / signed-decimal-string (ASCII space padding, underscores between
 *     digits, exponent, inf/nan); the float strip set is the narrow ASCII ws.
 *   - _lower_threshold_to_aux_context str() / f-string positions carry ASCII
 *     text + integral numbers; the {:,} grouping and %02d are transcribed; the
 *     urlparse hostname parser covers scheme://[userinfo@]host[:port]/... and
 *     //host ASCII URLs (invalid IPv6 bracket -> ValueError 'Invalid IPv6 URL').
 *     The notice prefix carries U+26A0 and the em-dash U+2014 (hex-escaped).
 *   - _hold_attempt_telemetry setattr restores are plain state writes (the
 *     contextlib.suppress(Exception) guard never fires on a writable double;
 *     no frozen-double fixture exercises a setattr raise).
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The harness
 * #includes this .c via the area-qualified
 * "agent/conversation_compression_flow2.c". Every symbol is static cp2_*.
 *
 * Gate: bash tests/run_one_oracle.sh conversation_compression_flow2 \
 *           oracle_conversation_compression_flow2.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── live module constants (byte-pinned to agent/conversation_compression.py
 *    + agent/session_activity.py) ── */
/* _TERMINAL_COMPRESSION_PROVENANCES = frozenset({AGENT_COMPRESSION_TIMEOUT,
 * AGENT_COMPRESSION_COOLDOWN}); ActivityProvenance is a str Enum, so membership
 * is modeled on the value strings. */
static const char *CP2_TERMINAL_PROVENANCES[] = {
    "agent.compression_timeout", "agent.compression_cooldown", NULL,
};
/* ActivityProvenance.AGENT_COMPRESSION.value */
#define CP2_PROV_AGENT_COMPRESSION "agent.compression"
/* _ATTEMPT_TELEMETRY_FIELDS (tuple order preserved) */
static const char *CP2_TELEMETRY_FIELDS[] = {
    "_last_compression_telemetry", "_active_compression_telemetry",
    "_compression_telemetry_seed", NULL,
};
/* U+26A0 warning sign, U+2014 em dash (hex-escaped, split before a following
 * ASCII hex-digit char where needed). */
#define CP2_WARN "\xe2\x9a\xa0"
#define CP2_EMDASH "\xe2\x80\x94"

/* ── universal seam ── */
typedef struct cp2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cp2_io_t;

/* ── truthiness / accessors ── */
static int cp2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int cp2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int cp2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int cp2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int cp2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int cp2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int cp2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *cp2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *cp2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double cp2_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

static json_t *cp2_copy_or_null(const json_t *v) { return v ? json_copy(v) : NULL; }

/* isinstance(x, dict): a PLAIN object, never an __attr__ double. */
static int cp2_is_plain_dict(const json_t *v)
{
    return cp2_is_obj(v) && json_obj_get(v, "__attr__") == NULL;
}

/* isinstance(x, (int, float)): a number OR a bool (bool is an int subclass). */
static int cp2_is_int_or_float(const json_t *v)
{
    return cp2_is_num(v) || cp2_is_bool(v);
}

/* callable(x): only a {"__callable__": ...} double is callable here. */
static int cp2_is_callable(const json_t *v)
{
    return cp2_is_obj(v) && json_obj_get(v, "__callable__") != NULL;
}

/* class tag of a double wrapper (NULL when untagged). */
static const char *cp2_class(const json_t *v)
{
    return cp2_str(cp2_get(v, "__class__"));
}

/* ── state-window helpers ──
 * A state carrier st.<key> is a double wrapper {"__attr__": {...}}; the self
 * inner object is the mutable __attr__ dict. cp2_self auto-vivifies so writes
 * land. cp2_dattr / cp2_dset read / write an attribute of a double wrapper. */
static json_t *cp2_self(json_t *st, const char *key)
{
    json_t *node = json_obj_get(st, key);
    if (!node || node->type != JSON_OBJECT) {
        node = json_object();
        json_set(st, key, node);
    }
    json_t *inner = json_obj_get(node, "__attr__");
    if (!inner || inner->type != JSON_OBJECT) {
        inner = json_object();
        json_set(node, "__attr__", inner);
    }
    return inner;
}

static json_t *cp2_dattr(const json_t *d, const char *name)
{
    return cp2_get(cp2_get(d, "__attr__"), name);
}

static void cp2_dset(const json_t *d, const char *name, json_t *val)
{
    json_t *inner = cp2_get(d, "__attr__");
    if (inner && inner->type == JSON_OBJECT) json_set(inner, name, val);
    else json_free(val);
}

/* remove every key of an object in place (read-only closures emit no state). */
static void cp2_clear_obj(json_t *o)
{
    if (!o || o->type != JSON_OBJECT) return;
    while (o->c.count > 0) json_obj_del(o, o->c.keys[0]);
}

/* ── raise specs ── */
static int cp2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *cp2_kind(const json_t *r)
{
    const char *k = cp2_str(cp2_get(r, "__raise__"));
    return k ? k : "";
}

static int cp2_is_base(const char *kind)
{
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "GeneratorExit") == 0;
}

static json_t *cp2_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static char *cp2_xfmt(const char *fmt, ...)
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

static const char *cp2_typename(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (isfinite(d) && d == (double)(long long)d && fabs(d) < 9.2e18) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT:
            return json_obj_get(v, "__attr__") ? "types.SimpleNamespace" : "dict";
        default: return "object";
    }
}

/* ── universal seam call: args OWNED by caller, borrowed by dispatcher, freed
 * here. Result caller-owned (NULL == None). Fail-closed with no channel. ── */
static json_t *cp2_call(cp2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cp2_xfmt("no seam installed for %s", name);
        r = cp2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *cp2_args(void) { return json_object(); }

/* arg builders: None-valued keys dropped on both sides (harness strips nulls
 * recursively; oracle norm_args drops None). */
static void cp2_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void cp2_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void cp2_arg_b(json_t *a, const char *k, int v)
{
    json_set(a, k, json_bool(v ? true : false));
}
static void cp2_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* ── string helpers (ASCII domain) ── */
static int cp2_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *cp2_strip(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && cp2_is_ws((unsigned char)s[a])) a++;
    while (b > a && cp2_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *cp2_lower(const char *s)
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

/* Python str() projection (domain: str / integral number / bool / None). */
static char *cp2_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return cp2_xfmt("None");
    if (v->type == JSON_BOOL) return cp2_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isfinite(d) && d == (double)(long long)d) return cp2_xfmt("%.0f", d);
        return cp2_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return cp2_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize(v);
    return s ? s : cp2_xfmt("");
}

/* {:,} thousands grouping of an integral value (caller frees). */
static char *cp2_thousands(double d)
{
    long long n = (long long)d;
    int neg = n < 0;
    unsigned long long u = neg ? (unsigned long long)(-(n + 1)) + 1ULL : (unsigned long long)n;
    char digits[32];
    int k = 0;
    if (u == 0) digits[k++] = '0';
    while (u > 0) { digits[k++] = (char)('0' + (u % 10)); u /= 10; }
    /* digits reversed; emit groups of 3 with commas */
    char buf[64];
    int b = 0;
    if (neg) buf[b++] = '-';
    for (int i = k - 1; i >= 0; i--) {
        buf[b++] = digits[i];
        int remaining = i;          /* digits still to emit after this one */
        if (remaining > 0 && remaining % 3 == 0) buf[b++] = ',';
    }
    buf[b] = '\0';
    return cp2_xfmt("%s", buf);
}

/* ── float(str) parser (Python grammar, ASCII). Returns 0 ok, 2 ValueError. ── */
static int cp2_float_digits(const char **p)
{
    const char *s = *p;
    int n = 0;
    for (;;) {
        if (*s < '0' || *s > '9') break;
        s++; n++;
        if (s[0] == '_' && s[1] >= '0' && s[1] <= '9') { s++; continue; }
        if (s[0] == '_') break;
    }
    *p = s;
    return n;
}

static int cp2_parse_float_str(const char *t, double *out)
{
    if (!t) return 2;
    char *s = cp2_strip(t);
    if (!s) return 2;
    const char *p = s;
    /* inf / infinity / nan (case-insensitive, optional sign) */
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    char low[16];
    size_t li = 0;
    for (const char *q = p; *q && li < 15; q++) low[li++] = (char)tolower((unsigned char)*q);
    low[li] = '\0';
    if (!strcmp(low, "inf") || !strcmp(low, "infinity")) {
        *out = neg ? -INFINITY : INFINITY; free(s); return 0;
    }
    if (!strcmp(low, "nan")) { *out = NAN; free(s); return 0; }
    /* mantissa: digits[.digits] | .digits */
    const char *start = p;
    int intd = cp2_float_digits(&p);
    int fracd = 0;
    if (*p == '.') { p++; fracd = cp2_float_digits(&p); }
    if (intd == 0 && fracd == 0) { free(s); return 2; }
    /* exponent */
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        int ed = cp2_float_digits(&p);
        if (ed == 0) { free(s); return 2; }
    }
    if (*p != '\0') { free(s); return 2; }
    /* build clean buffer (drop underscores) */
    size_t n = (size_t)(p - start);
    char *clean = (char *)malloc(n + 2);
    if (!clean) { free(s); return 2; }
    size_t k = 0;
    if (neg) clean[k++] = '-';
    for (const char *q = start; q < p; q++) if (*q != '_') clean[k++] = *q;
    clean[k] = '\0';
    char *end = NULL;
    errno = 0;
    double v = strtod(clean, &end);
    int ok = (end && *end == '\0' && end != clean);
    free(clean);
    free(s);
    if (!ok) return 2;
    *out = v;
    return 0;
}

/* float(value) projection. Returns a NEW number node, or a raise spec. */
static json_t *cp2_pyfloat(const json_t *v)
{
    if (!v || v->type == JSON_NULL)
        return cp2_mkraise("TypeError",
            "float() argument must be a string or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v->type == JSON_NUMBER) return json_number(v->num_val);
    if (v->type == JSON_STRING) {
        double d = 0.0;
        if (cp2_parse_float_str(v->str_val ? v->str_val : "", &d) == 0)
            return json_number(d);
        char *m = cp2_xfmt("could not convert string to float: '%s'",
                           v->str_val ? v->str_val : "");
        json_t *r = cp2_mkraise("ValueError", m ? m : "");
        free(m);
        return r;
    }
    char *m = cp2_xfmt("float() argument must be a string or a real number, not '%s'",
                       cp2_typename(v));
    json_t *r = cp2_mkraise("TypeError", m ? m : "");
    free(m);
    return r;
}

/* int() truncation toward zero of a numeric value. */
static double cp2_int_trunc(double d)
{
    if (!isfinite(d)) return d;
    return (double)(long long)d;
}

/* ── urlparse hostname (ASCII URL domain). Returns malloc'd lowercase hostname
 * or NULL (None); *fail set on ValueError 'Invalid IPv6 URL'. ── */
static char *cp2_hostname(const char *url, int *fail)
{
    *fail = 0;
    if (!url) return NULL;
    const char *nl = NULL;
    const char *sep = strstr(url, "://");
    if (sep) nl = sep + 3;
    else if (url[0] == '/' && url[1] == '/') nl = url + 2;
    else return NULL;                 /* no netloc -> hostname None */
    const char *end = nl;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    size_t nlen = (size_t)(end - nl);
    char *netloc = (char *)malloc(nlen + 1);
    if (!netloc) return NULL;
    memcpy(netloc, nl, nlen);
    netloc[nlen] = '\0';
    /* strip userinfo (after last '@') */
    char *at = strrchr(netloc, '@');
    char *host = at ? at + 1 : netloc;
    char *out = NULL;
    if (host[0] == '[') {
        char *close = strchr(host, ']');
        if (!close) { *fail = 1; free(netloc); return NULL; }
        size_t hlen = (size_t)(close - host - 1);
        char *h = (char *)malloc(hlen + 1);
        if (h) { memcpy(h, host + 1, hlen); h[hlen] = '\0'; out = cp2_lower(h); free(h); }
    } else {
        char *colon = strchr(host, ':');
        size_t hlen = colon ? (size_t)(colon - host) : strlen(host);
        char *h = (char *)malloc(hlen + 1);
        if (h) { memcpy(h, host, hlen); h[hlen] = '\0'; out = cp2_lower(h); free(h); }
    }
    free(netloc);
    if (out && out[0] == '\0') { free(out); return NULL; }
    return out;
}

/* _swallow.__exit__ log: exc_info=True -> logger.debug(msg, exc_info=True)
 * (args=[]); exc_info=False -> logger.debug(msg, exc) (args=[str(exc)]).
 * Returns NULL when the log succeeded (Exception suppressed), or a raise spec
 * when logger.debug itself raised (propagates). Caller frees exc separately. */
static json_t *cp2_swallow_log(cp2_io_t *io, const char *message, int exc_info, const json_t *exc)
{
    json_t *lg = cp2_args();
    cp2_arg_s(lg, "msg", message);
    json_t *arr = json_array();
    if (exc_info) {
        cp2_arg_b(lg, "exc_info", 1);
    } else {
        const char *em = cp2_str(cp2_get(exc, "message"));
        json_append(arr, json_string(em ? em : ""));
    }
    json_set(lg, "args", arr);
    json_t *lr = cp2_call(io, "logger.debug", lg);
    if (cp2_is_raise(lr)) return lr;
    json_free(lr);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. CompressionCommitFence.set_precommit_deadline_monotonic
 *    deadline = float(deadline); if deadline <= 0: raise ValueError(...);
 *    self._deadline = deadline.
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_set_precommit_deadline_monotonic @ agent/conversation_compression.py:set_precommit_deadline_monotonic */
static json_t *cp2_set_precommit_deadline_monotonic(cp2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    json_t *fence = cp2_self(st, "fence");
    json_t *dl = cp2_pyfloat(cp2_get(in, "deadline"));
    if (cp2_is_raise(dl)) return dl;
    double d = cp2_num(dl);
    if (d <= 0) {
        json_free(dl);
        return cp2_mkraise("ValueError", "compression deadline must be positive");
    }
    json_set(fence, "_deadline", dl);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. CompressionCommitFence.extend_deadline
 *    if seconds <= 0 or not self._lock.acquire(blocking=False): return False
 *    try: alive = _deadline is not None and _deadline > time.monotonic()
 *                 and not self.is_cancelled
 *         if alive: _deadline += seconds
 *         return alive
 *    finally: self._lock.release()
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_extend_deadline @ agent/conversation_compression.py:extend_deadline */
static json_t *cp2_extend_deadline(cp2_io_t *io, const json_t *in, json_t *st)
{
    json_t *fence = cp2_self(st, "fence");
    double seconds = cp2_num(cp2_get(in, "seconds"));
    if (seconds <= 0) return json_bool(false);
    json_t *acq_args = cp2_args();
    cp2_arg_b(acq_args, "blocking", 0);
    json_t *acq = cp2_call(io, "lock_acquire", acq_args);
    if (cp2_is_raise(acq)) return acq;
    int acquired = cp2_truth(acq);
    json_free(acq);
    if (!acquired) return json_bool(false);
    /* try body */
    json_t *exc = NULL;
    int alive = 0;
    json_t *deadline = cp2_get(fence, "_deadline");
    if (!cp2_is_null(deadline)) {
        json_t *mono = cp2_call(io, "monotonic", cp2_args());
        if (cp2_is_raise(mono)) { exc = mono; }
        else {
            double now = cp2_num(mono);
            json_free(mono);
            if (cp2_num(deadline) > now) {
                json_t *canc = cp2_call(io, "fence_is_cancelled", cp2_args());
                if (cp2_is_raise(canc)) { exc = canc; }
                else {
                    int cancelled = cp2_truth(canc);
                    json_free(canc);
                    alive = !cancelled;
                    if (alive) {
                        double nd = cp2_num(cp2_get(fence, "_deadline")) + seconds;
                        json_set(fence, "_deadline", json_number(nd));
                    }
                }
            }
        }
    }
    /* finally: lock.release() runs even when the body raised */
    json_t *rel = cp2_call(io, "lock_release", cp2_args());
    if (cp2_is_raise(rel)) {
        json_free(exc);
        return rel;                   /* a finally raise replaces the in-flight */
    }
    json_free(rel);
    if (exc) return exc;
    return json_bool(alive ? true : false);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. _CompressionActivityHeartbeat._should_suppress
 *    if not _suppressed and _commit_fence is not None
 *       and _commit_fence.is_cancelled: _suppressed = True
 *    return _suppressed
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_should_suppress @ agent/conversation_compression.py:_should_suppress */
static json_t *cp2_should_suppress_core(cp2_io_t *io, json_t *hb)
{
    json_t *sup = cp2_get(hb, "_suppressed");
    if (!cp2_truth(sup)) {
        json_t *fence = cp2_get(hb, "_commit_fence");
        if (!cp2_is_null(fence)) {
            json_t *canc = cp2_call(io, "fence_is_cancelled", cp2_args());
            if (cp2_is_raise(canc)) return canc;
            int cancelled = cp2_truth(canc);
            json_free(canc);
            if (cancelled) json_set(hb, "_suppressed", json_bool(true));
        }
    }
    return cp2_copy_or_null(cp2_get(hb, "_suppressed"));
}

static json_t *cp2_should_suppress(cp2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    return cp2_should_suppress_core(io, cp2_self(st, "hb"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. _CompressionActivityHeartbeat._touch
 *    with _swallow('compression activity heartbeat touch failed', exc_info=True):
 *      if not allow_terminal_overwrite:
 *        if _should_suppress(): return
 *        current = normalize_activity_provenance(
 *            getattr(_agent, "_last_activity_provenance", None))
 *        if current in _TERMINAL_COMPRESSION_PROVENANCES:
 *          _suppressed = True; return
 *      touch = getattr(_agent, "_touch_activity", None)
 *      if callable(touch):
 *        if not allow_terminal_overwrite and _should_suppress(): return
 *        touch(desc, provenance=AGENT_COMPRESSION, force_persist=force_persist)
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_touch @ agent/conversation_compression.py:_touch */
static json_t *cp2_touch(cp2_io_t *io, const json_t *in, json_t *st)
{
    json_t *hb = cp2_self(st, "hb");
    const char *desc = cp2_str(cp2_get(in, "desc"));
    int allow = cp2_truth(cp2_get(in, "allow_terminal_overwrite"));
    int force = cp2_truth(cp2_get(in, "force_persist"));
    /* with _swallow(message, exc_info=True): factory seam */
    json_t *sw_args = cp2_args();
    cp2_arg_s(sw_args, "message", "compression activity heartbeat touch failed");
    cp2_arg_b(sw_args, "exc_info", 1);
    json_t *sw = cp2_call(io, "swallow", sw_args);
    if (cp2_is_raise(sw)) return sw;
    json_free(sw);
    /* body */
    json_t *exc = NULL;
    json_t *agent = cp2_get(hb, "_agent");
    if (!allow) {
        json_t *s = cp2_should_suppress_core(io, hb);
        if (cp2_is_raise(s)) { exc = s; goto swallow_exit; }
        int suppressed = cp2_truth(s);
        json_free(s);
        if (suppressed) goto swallow_exit;
        json_t *prov_arg = cp2_args();
        cp2_arg_j(prov_arg, "provenance", cp2_dattr(agent, "_last_activity_provenance"));
        json_t *cur = cp2_call(io, "normalize_activity_provenance", prov_arg);
        if (cp2_is_raise(cur)) { exc = cur; goto swallow_exit; }
        const char *curs = cp2_str(cur);
        int terminal = 0;
        for (int i = 0; CP2_TERMINAL_PROVENANCES[i]; i++)
            if (curs && !strcmp(curs, CP2_TERMINAL_PROVENANCES[i])) terminal = 1;
        json_free(cur);
        if (terminal) {
            json_set(hb, "_suppressed", json_bool(true));
            goto swallow_exit;
        }
    }
    {
        json_t *touch = cp2_dattr(agent, "_touch_activity");
        if (cp2_is_callable(touch)) {
            if (!allow) {
                json_t *s = cp2_should_suppress_core(io, hb);
                if (cp2_is_raise(s)) { exc = s; goto swallow_exit; }
                int suppressed = cp2_truth(s);
                json_free(s);
                if (suppressed) goto swallow_exit;
            }
            const char *seam = cp2_str(cp2_get(touch, "__callable__"));
            json_t *t_args = cp2_args();
            cp2_arg_s(t_args, "desc", desc);
            cp2_arg_s(t_args, "provenance", CP2_PROV_AGENT_COMPRESSION);
            cp2_arg_b(t_args, "force_persist", force);
            json_t *tr = cp2_call(io, seam ? seam : "touch_activity", t_args);
            if (cp2_is_raise(tr)) { exc = tr; goto swallow_exit; }
            json_free(tr);
        }
    }
swallow_exit:
    /* _swallow.__exit__: on Exception log debug + suppress; BaseException propagates */
    if (exc) {
        if (cp2_is_base(cp2_kind(exc))) return exc;
        json_t *lr = cp2_swallow_log(io, "compression activity heartbeat touch failed", 1, exc);
        json_free(exc);
        if (lr) return lr;
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. _CompactionLifecycle.complete
 *    if _done_emitted: return
 *    _done_emitted = True
 *    if status_emitted and (commit_status == "committed" or force_terminal):
 *      _emit_compaction_done(_agent)
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_complete @ agent/conversation_compression.py:complete */
static json_t *cp2_complete_core(cp2_io_t *io, json_t *lc, int force)
{
    if (cp2_truth(cp2_get(lc, "_done_emitted"))) return NULL;
    json_set(lc, "_done_emitted", json_bool(true));
    int status_emitted = cp2_truth(cp2_get(lc, "status_emitted"));
    const char *cs = cp2_str(cp2_get(lc, "commit_status"));
    int committed = cs && !strcmp(cs, "committed");
    if (status_emitted && (committed || force)) {
        json_t *a = cp2_args();
        cp2_arg_j(a, "agent", cp2_get(lc, "_agent"));
        json_t *r = cp2_call(io, "_emit_compaction_done", a);
        if (cp2_is_raise(r)) return r;
        json_free(r);
    }
    return NULL;
}

static json_t *cp2_complete(cp2_io_t *io, const json_t *in, json_t *st)
{
    int force = cp2_truth(cp2_get(in, "force_terminal"));
    return cp2_complete_core(io, cp2_self(st, "lifecycle"), force);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. _CompressionLease.status_emitted (property)
 *    return self._lifecycle.status_emitted
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_status_emitted @ agent/conversation_compression.py:status_emitted */
static json_t *cp2_status_emitted(cp2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    json_t *lease = cp2_self(st, "lease");
    json_t *lc = cp2_get(lease, "_lifecycle");
    return cp2_copy_or_null(cp2_dattr(lc, "status_emitted"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7. _CompressionLease.start_refresher
 *    if holder is None: return
 *    candidate = _CompressionLockLeaseRefresher(db, sid, holder, ttl,
 *                _refresh_interval, store=store)
 *    with _release_guard:
 *      if not _released: _refresher = candidate.start()
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_start_refresher @ agent/conversation_compression.py:start_refresher */
static json_t *cp2_start_refresher(cp2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *lease = cp2_self(st, "lease");
    json_t *holder = cp2_get(lease, "holder");
    if (cp2_is_null(holder)) return NULL;
    json_t *a = cp2_args();
    cp2_arg_j(a, "db", cp2_get(lease, "db"));
    cp2_arg_j(a, "sid", cp2_get(lease, "sid"));
    cp2_arg_j(a, "holder", holder);
    cp2_arg_j(a, "ttl", cp2_get(lease, "ttl"));
    cp2_arg_j(a, "refresh_interval", cp2_get(lease, "_refresh_interval"));
    cp2_arg_j(a, "store", cp2_get(lease, "store"));
    json_t *cand = cp2_call(io, "refresher_ctor", a);
    if (cp2_is_raise(cand)) return cand;
    /* with _release_guard (silent lock): if not _released: _refresher = start() */
    json_t *exc = NULL;
    if (!cp2_truth(cp2_get(lease, "_released"))) {
        json_t *start = cp2_dattr(cand, "start");
        const char *seam = cp2_str(cp2_get(start, "__callable__"));
        json_t *r = cp2_call(io, seam ? seam : "refresher_start", cp2_args());
        if (cp2_is_raise(r)) exc = r;
        else json_set(lease, "_refresher", r);
    }
    json_free(cand);
    if (exc) return exc;
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. _CompressionLease.release_holder_only
 *    with _release_guard:
 *      if _released: return
 *      _released = True
 *      if getattr(_agent, "_active_compression_lock_holder", None) == holder:
 *        _agent._active_compression_lock_holder = None
 *      if _refresher is not None:
 *        with _swallow('compression lock refresher stop failed: %s'):
 *          _refresher.stop()
 *      if db is not None and sid and holder:
 *        with _swallow('compression lock release failed: %s'):
 *          _release_compression_lock(self)
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_release_holder_only @ agent/conversation_compression.py:release_holder_only */
static json_t *cp2_release_holder_only_core(cp2_io_t *io, json_t *lease_wrap)
{
    json_t *lease = cp2_get(lease_wrap, "__attr__");
    /* with _release_guard (silent) */
    if (cp2_truth(cp2_get(lease, "_released"))) return NULL;
    json_set(lease, "_released", json_bool(true));
    json_t *agent = cp2_get(lease, "_agent");
    json_t *holder = cp2_get(lease, "holder");
    json_t *active = cp2_dattr(agent, "_active_compression_lock_holder");
    /* equality: both strings equal (holder is a str token) */
    const char *as = cp2_str(active);
    const char *hs = cp2_str(holder);
    int eq = (cp2_is_null(active) && cp2_is_null(holder))
             || (as && hs && !strcmp(as, hs));
    if (eq) cp2_dset(agent, "_active_compression_lock_holder", json_null());
    /* refresher stop under _swallow */
    json_t *refresher = cp2_get(lease, "_refresher");
    if (!cp2_is_null(refresher)) {
        json_t *sw_args = cp2_args();
        cp2_arg_s(sw_args, "message", "compression lock refresher stop failed: %s");
        json_t *sw = cp2_call(io, "swallow", sw_args);
        if (cp2_is_raise(sw)) return sw;
        json_free(sw);
        json_t *stop = cp2_dattr(refresher, "stop");
        const char *seam = cp2_str(cp2_get(stop, "__callable__"));
        json_t *r = cp2_call(io, seam ? seam : "refresher_stop", cp2_args());
        json_t *exc = cp2_is_raise(r) ? r : NULL;
        if (!exc) json_free(r);
        if (exc) {
            if (cp2_is_base(cp2_kind(exc))) return exc;
            json_t *lr = cp2_swallow_log(io, "compression lock refresher stop failed: %s", 0, exc);
            json_free(exc);
            if (lr) return lr;
        }
    }
    /* lock release under _swallow */
    json_t *db = cp2_get(lease, "db");
    json_t *sid = cp2_get(lease, "sid");
    if (!cp2_is_null(db) && cp2_truth(sid) && cp2_truth(holder)) {
        json_t *sw_args = cp2_args();
        cp2_arg_s(sw_args, "message", "compression lock release failed: %s");
        json_t *sw = cp2_call(io, "swallow", sw_args);
        if (cp2_is_raise(sw)) return sw;
        json_free(sw);
        json_t *a = cp2_args();
        cp2_arg_j(a, "lease", lease_wrap);
        json_t *r = cp2_call(io, "_release_compression_lock", a);
        json_t *exc = cp2_is_raise(r) ? r : NULL;
        if (!exc) json_free(r);
        if (exc) {
            if (cp2_is_base(cp2_kind(exc))) return exc;
            json_t *lr = cp2_swallow_log(io, "compression lock release failed: %s", 0, exc);
            json_free(exc);
            if (lr) return lr;
        }
    }
    return NULL;
}

static json_t *cp2_release_holder_only(cp2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    cp2_self(st, "lease");
    return cp2_release_holder_only_core(io, cp2_get(st, "lease"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 9. _CompressionLease.release
 *    try: self._lifecycle.complete()
 *    finally:
 *      try: self.release_holder_only()
 *      finally:
 *        try: if _commit_fence is not None:
 *               _commit_fence.clear_cancelled_lock_release(release_holder_only)
 *        finally: self.finish_lock_setup()
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_release @ agent/conversation_compression.py:release */
static json_t *cp2_release(cp2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *lease = cp2_self(st, "lease");
    json_t *lc = cp2_get(lease, "_lifecycle");
    json_t *fence = cp2_get(lease, "_commit_fence");
    json_t *exc = NULL;
    /* try: complete() */
    {
        json_t *r = cp2_complete_core(io, cp2_get(lc, "__attr__"), 0);
        if (cp2_is_raise(r)) exc = r;
        else json_free(r);
    }
    /* finally: release_holder_only() */
    {
        json_t *r = cp2_release_holder_only_core(io, cp2_get(st, "lease"));
        if (cp2_is_raise(r)) { json_free(exc); exc = r; }
        else json_free(r);
    }
    /* finally: clear_cancelled_lock_release(release_holder_only) */
    {
        if (!cp2_is_null(fence)) {
            json_t *a = cp2_args();
            json_t *cb = json_object();
            json_set(cb, "__callable__", json_bool(true));
            json_set(a, "release", cb);
            json_t *r = cp2_call(io, "clear_cancelled_lock_release", a);
            if (cp2_is_raise(r)) { json_free(exc); exc = r; }
            else json_free(r);
        }
    }
    /* finally: finish_lock_setup() */
    {
        json_t *r = cp2_call(io, "finish_lock_setup", cp2_args());
        if (cp2_is_raise(r)) { json_free(exc); exc = r; }
        else json_free(r);
    }
    if (exc) return exc;
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 10. _Attempt.restore_compressor
 *     _restore_compressor_attempt_state(compressor, snapshot,
 *       durable_cooldown_authoritative=..., durable_cooldown_state=...,
 *       attempt_generation=generation)
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_restore_compressor @ agent/conversation_compression.py:restore_compressor */
static json_t *cp2_restore_compressor(cp2_io_t *io, const json_t *in, json_t *st)
{
    json_t *attempt = cp2_self(st, "attempt");
    json_t *a = cp2_args();
    cp2_arg_j(a, "compressor", cp2_get(in, "compressor"));
    cp2_arg_j(a, "snapshot", cp2_get(attempt, "snapshot"));
    cp2_arg_j(a, "durable_cooldown_authoritative", cp2_get(attempt, "durable_cooldown_authoritative"));
    cp2_arg_j(a, "durable_cooldown_state", cp2_get(attempt, "durable_cooldown_state"));
    cp2_arg_j(a, "attempt_generation", cp2_get(attempt, "generation"));
    json_t *r = cp2_call(io, "_restore_compressor_attempt_state", a);
    if (cp2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 11. context_engine_session_db
 *     if type(agent.context_compressor) is ContextCompressor
 *        or not isinstance(session_db, Persistence): return session_db
 *     plugin_db = getattr(agent, "_context_engine_session_db", None)
 *     if plugin_db is None:
 *       plugin_db = agent._context_engine_session_db =
 *         hermes_state._engine_session_db(agent.context_compressor,
 *           session_db.scope.profile_dir / "state.db", read_only=session_db.read_only)
 *     return plugin_db
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_context_engine_session_db @ agent/conversation_compression.py:context_engine_session_db */
static json_t *cp2_context_engine_session_db(cp2_io_t *io, const json_t *in, json_t *st)
{
    json_t *agent = cp2_self(st, "agent");
    json_t *session_db = cp2_get(in, "session_db");
    json_t *compressor = cp2_get(agent, "context_compressor");
    const char *cc_cls = cp2_class(compressor);
    int is_cc = cc_cls && !strcmp(cc_cls, "ContextCompressor");
    const char *db_cls = cp2_class(session_db);
    int is_pers = db_cls && !strcmp(db_cls, "Persistence");
    if (is_cc || !is_pers) return cp2_copy_or_null(session_db);
    json_t *plugin_db = cp2_get(agent, "_context_engine_session_db");
    if (cp2_is_null(plugin_db)) {
        /* path = str(scope.profile_dir / "state.db"); profile_dir is a path
         * double {"__path__": "..."} (PurePosixPath in live). */
        json_t *scope = cp2_dattr(session_db, "scope");
        json_t *pd_node = cp2_dattr(scope, "profile_dir");
        const char *pd = cp2_str(cp2_get(pd_node, "__path__"));
        if (!pd) pd = cp2_str(pd_node);
        char *path = cp2_xfmt("%s/state.db", pd ? pd : "");
        json_t *a = cp2_args();
        cp2_arg_j(a, "compressor", compressor);
        cp2_arg_s(a, "path", path);
        free(path);
        cp2_arg_j(a, "read_only", cp2_dattr(session_db, "read_only"));
        json_t *r = cp2_call(io, "engine_session_db", a);
        if (cp2_is_raise(r)) return r;
        json_set(agent, "_context_engine_session_db", json_copy(r));
        plugin_db = r;
    } else {
        plugin_db = json_copy(plugin_db);
    }
    return plugin_db;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 12. close_context_engine_session_db
 *     plugin_db, agent._context_engine_session_db =
 *       getattr(agent, "_context_engine_session_db", None), None
 *     if plugin_db is not None: plugin_db.close()
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_close_context_engine_session_db @ agent/conversation_compression.py:close_context_engine_session_db */
static json_t *cp2_close_context_engine_session_db(cp2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *agent = cp2_self(st, "agent");
    json_t *plugin_db = cp2_copy_or_null(cp2_get(agent, "_context_engine_session_db"));
    json_set(agent, "_context_engine_session_db", json_null());
    if (!cp2_is_null(plugin_db)) {
        json_t *close = cp2_dattr(plugin_db, "close");
        const char *seam = cp2_str(cp2_get(close, "__callable__"));
        json_t *r = cp2_call(io, seam ? seam : "plugin_db_close", cp2_args());
        json_free(plugin_db);
        if (cp2_is_raise(r)) return r;
        json_free(r);
    } else {
        json_free(plugin_db);
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 13. _lower_threshold_to_aux_context  (the aux-window lowering notice)
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_lower_threshold_to_aux_context @ agent/conversation_compression.py:_lower_threshold_to_aux_context */
static json_t *cp2_lower_threshold_to_aux_context(cp2_io_t *io, const json_t *in, json_t *st)
{
    json_t *agent = cp2_self(st, "agent");
    json_t *compressor = cp2_get(agent, "context_compressor");
    const char *aux_model = cp2_str(cp2_get(in, "aux_model"));
    double aux_context = cp2_num(cp2_get(in, "aux_context"));
    const char *aux_provider = cp2_str(cp2_get(in, "aux_provider"));
    const char *aux_base_url = cp2_str(cp2_get(in, "aux_base_url"));
    if (!aux_model) aux_model = "";
    if (!aux_provider) aux_provider = "";
    if (!aux_base_url) aux_base_url = "";

    double old_threshold = cp2_num(cp2_dattr(compressor, "threshold_tokens"));
    double new_threshold = aux_context;
    cp2_dset(compressor, "threshold_tokens", json_number(new_threshold));
    cp2_dset(compressor, "_aux_context_ceiling", json_number(aux_context));
    json_t *ratio = cp2_dattr(compressor, "summary_target_ratio");
    const char *tail_mode = cp2_str(cp2_dattr(compressor, "tail_mode"));
    if (tail_mode && !strcmp(tail_mode, "lean")) {
        cp2_dset(compressor, "_tail_token_budget", json_null());
    } else if (cp2_is_int_or_float(ratio)) {
        double r = cp2_is_bool(ratio) ? (ratio->bool_val ? 1.0 : 0.0) : cp2_num(ratio);
        cp2_dset(compressor, "tail_token_budget", json_number(cp2_int_trunc(new_threshold * r)));
    }
    double main_ctx = cp2_num(cp2_dattr(compressor, "context_length"));
    if (cp2_truth(cp2_dattr(compressor, "context_length"))) {
        cp2_dset(compressor, "threshold_percent", json_number(new_threshold / main_ctx));
    }
    int safe_pct = main_ctx ? (int)cp2_int_trunc((aux_context / main_ctx) * 100) : 50;
    /* recomputed threshold (only when main_ctx and isinstance(compressor, CC)) */
    json_t *recomputed = NULL;
    const char *ccls = cp2_class(compressor);
    int is_cc = ccls && !strcmp(ccls, "ContextCompressor");
    if (main_ctx && is_cc) {
        json_t *ep_args = cp2_args();
        cp2_arg_n(ep_args, "arg0", main_ctx);
        cp2_arg_n(ep_args, "arg1", (double)safe_pct / 100.0);
        json_t *pct = cp2_call(io, "effective_threshold_percent", ep_args);
        if (cp2_is_raise(pct)) return pct;
        json_t *ct_args = cp2_args();
        cp2_arg_n(ct_args, "arg0", main_ctx);
        cp2_arg_j(ct_args, "arg1", pct);
        cp2_arg_j(ct_args, "arg2", cp2_dattr(compressor, "max_tokens"));
        json_free(pct);
        recomputed = cp2_call(io, "compute_threshold_tokens", ct_args);
        if (cp2_is_raise(recomputed)) return recomputed;
    }
    int viable = cp2_is_null(recomputed) || cp2_num(recomputed) <= aux_context;
    /* labels */
    json_t *mm = cp2_get(agent, "model");
    json_t *mp = cp2_get(agent, "provider");
    char *main_model = cp2_truth(mm) ? cp2_pystr(mm) : cp2_xfmt("?");
    char *main_provider = cp2_truth(mp) ? cp2_pystr(mp) : cp2_xfmt("");
    char *aux_provider_label = NULL;
    if (aux_provider[0] && strcmp(aux_provider, "auto") != 0) {
        aux_provider_label = cp2_xfmt("%s", aux_provider);
    } else {
        int fail = 0;
        char *hn = cp2_hostname(aux_base_url, &fail);
        if (fail) {
            aux_provider_label = cp2_xfmt("%s", aux_base_url[0] ? aux_base_url : "auto");
        } else if (hn) {
            aux_provider_label = hn;
        } else {
            aux_provider_label = cp2_xfmt("%s", aux_base_url);
        }
    }
    char *main_label = main_provider[0]
        ? cp2_xfmt("%s (%s)", main_model, main_provider)
        : cp2_xfmt("%s", main_model);
    char *aux_label = cp2_xfmt("%s (%s)", aux_model, aux_provider_label);
    /* message */
    char *aux_ctx_s = cp2_thousands(aux_context);
    char *old_s = cp2_thousands(old_threshold);
    char *new_s = cp2_thousands(new_threshold);
    char *msg = cp2_xfmt(
        CP2_WARN " Compression model %s context is %s tokens, but the main model "
        "%s's compression threshold was %s tokens. "
        "Auto-lowered this session's threshold to %s tokens so compression can run.\n",
        aux_label, aux_ctx_s, main_label, old_s, new_s);
    char *tail = NULL;
    if (viable) {
        tail = cp2_xfmt(
            "  To make this permanent, edit config.yaml " CP2_EMDASH " either:\n"
            "  1. Use a larger compression model:\n"
            "       auxiliary:\n"
            "         compression:\n"
            "           model: <model-with-%s+-context>\n"
            "  2. Lower the compression threshold:\n"
            "       compression:\n"
            "         threshold: 0.%02d",
            old_s, safe_pct);
    } else {
        char *main_ctx_s = cp2_thousands(main_ctx);
        char *rec_s = cp2_thousands(cp2_num(recomputed));
        tail = cp2_xfmt(
            "  To make this permanent, use a larger compression model in config.yaml:\n"
            "       auxiliary:\n"
            "         compression:\n"
            "           model: <model-with-%s+-context>\n"
            "  (Lowering compression.threshold cannot help here " CP2_EMDASH " with %s's %s-token window, "
            "Hermes's small-context floor and output reservation would recompute the trigger to "
            "%s tokens, still above the compression model's %s.)",
            old_s, main_label, main_ctx_s, rec_s, aux_ctx_s);
        free(main_ctx_s);
        free(rec_s);
    }
    char *fullmsg = cp2_xfmt("%s%s", msg ? msg : "", tail ? tail : "");
    free(msg); free(tail);
    /* _emit_feasibility_notice(agent, msg) */
    json_t *fn_args = cp2_args();
    cp2_arg_j(fn_args, "agent", cp2_get(st, "agent"));
    cp2_arg_s(fn_args, "msg", fullmsg);
    json_t *fr = cp2_call(io, "_emit_feasibility_notice", fn_args);
    if (cp2_is_raise(fr)) {
        free(fullmsg); free(aux_ctx_s); free(old_s); free(new_s);
        free(main_model); free(main_provider); free(aux_provider_label);
        free(main_label); free(aux_label); json_free(recomputed);
        return fr;
    }
    json_free(fr);
    /* logger.warning(fmt, aux_model, aux_context, old_threshold, new_threshold) */
    json_t *lg = cp2_args();
    cp2_arg_s(lg, "msg",
        "Auxiliary compression model %s has %d token context, below the main model's "
        "compression threshold of %d tokens " CP2_EMDASH " auto-lowered session threshold to %d "
        "to keep compression working.");
    json_t *largs = json_array();
    json_append(largs, json_string(aux_model));
    json_append(largs, json_number(aux_context));
    json_append(largs, json_number(old_threshold));
    json_append(largs, json_number(new_threshold));
    json_set(lg, "args", largs);
    json_t *lr = cp2_call(io, "logger.warning", lg);
    free(fullmsg); free(aux_ctx_s); free(old_s); free(new_s);
    free(main_model); free(main_provider); free(aux_provider_label);
    free(main_label); free(aux_label); json_free(recomputed);
    if (cp2_is_raise(lr)) return lr;
    json_free(lr);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 14. _configured_custom_providers
 *     if _SESSION_AUXILIARY.get() is not None:
 *       return getattr(agent, "_custom_providers", None)
 *     try: return get_compatible_custom_providers(load_config_readonly())
 *     except Exception:
 *       logger.debug("Provider entries unreadable; using the ones from agent start",
 *                    exc_info=True)
 *       return getattr(agent, "_custom_providers", None)
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_configured_custom_providers @ agent/conversation_compression.py:_configured_custom_providers */
static json_t *cp2_configured_custom_providers(cp2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *agent = cp2_self(st, "agent");
    json_t *aux = cp2_call(io, "session_auxiliary_get", cp2_args());
    if (cp2_is_raise(aux)) return aux;
    if (!cp2_is_null(aux)) {
        json_free(aux);
        return cp2_copy_or_null(cp2_get(agent, "_custom_providers"));
    }
    json_free(aux);
    json_t *cfg = cp2_call(io, "load_config_readonly", cp2_args());
    if (cp2_is_raise(cfg)) {
        if (cp2_is_base(cp2_kind(cfg))) return cfg;
        json_free(cfg);
        goto except_branch;
    }
    {
        json_t *a = cp2_args();
        cp2_arg_j(a, "arg0", cfg);
        json_free(cfg);
        json_t *prov = cp2_call(io, "get_compatible_custom_providers", a);
        if (cp2_is_raise(prov)) {
            if (cp2_is_base(cp2_kind(prov))) return prov;
            json_free(prov);
            goto except_branch;
        }
        return prov;
    }
except_branch:
    {
        json_t *lg = cp2_args();
        cp2_arg_s(lg, "msg", "Provider entries unreadable; using the ones from agent start");
        json_set(lg, "args", json_array());
        cp2_arg_b(lg, "exc_info", 1);
        json_t *lr = cp2_call(io, "logger.debug", lg);
        if (cp2_is_raise(lr)) return lr;
        json_free(lr);
    }
    return cp2_copy_or_null(cp2_get(agent, "_custom_providers"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 15. _hold_attempt_telemetry  (@contextlib.contextmanager)
 *     held = tuple(getattr(compressor, name, None) for name in FIELDS)
 *     try: yield
 *     finally: for name, value in zip(FIELDS, held):
 *                with contextlib.suppress(Exception): setattr(compressor, name, value)
 *     The harness drives: enter (snapshot), apply in.reset mutations, exit (restore).
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_hold_attempt_telemetry @ agent/conversation_compression.py:_hold_attempt_telemetry */
static json_t *cp2_hold_attempt_telemetry(cp2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    json_t *compressor = cp2_self(st, "compressor");
    /* snapshot held values (getattr default None) */
    json_t *held[3];
    for (int i = 0; i < 3; i++)
        held[i] = cp2_copy_or_null(cp2_get(compressor, CP2_TELEMETRY_FIELDS[i]));
    /* yield: apply the reset mutations the body performs */
    json_t *reset = cp2_get(in, "reset");
    if (cp2_is_obj(reset)) {
        for (size_t i = 0; i < reset->c.count; i++)
            json_set(compressor, reset->c.keys[i], json_copy(reset->c.items[i]));
    }
    /* finally: restore each held value (suppress(Exception) setattr; a writable
     * double never raises, so the restore always lands) */
    for (int i = 0; i < 3; i++) {
        json_set(compressor, CP2_TELEMETRY_FIELDS[i], held[i] ? held[i] : json_null());
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 16. _fence_gated_worker  (closure over messages, worker)
 *     if worker_fence.deadline_exceeded:
 *       raise concurrent.futures.TimeoutError("compression deadline expired before worker start")
 *     if worker_fence.is_cancelled:
 *       logger.info("Skipping stale compression job: fence cancelled before start")
 *       return messages, ""
 *     return worker(worker_fence)
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_fence_gated_worker @ agent/conversation_compression.py:_fence_gated_worker */
static json_t *cp2_fence_gated_worker(cp2_io_t *io, const json_t *in, json_t *st)
{
    cp2_clear_obj(st);
    json_t *fence = cp2_get(in, "worker_fence");
    json_t *de = cp2_call(io, "fence_deadline_exceeded", cp2_args());
    if (cp2_is_raise(de)) return de;
    int exceeded = cp2_truth(de);
    json_free(de);
    if (exceeded)
        return cp2_mkraise("TimeoutError", "compression deadline expired before worker start");
    json_t *canc = cp2_call(io, "fence_is_cancelled", cp2_args());
    if (cp2_is_raise(canc)) return canc;
    int cancelled = cp2_truth(canc);
    json_free(canc);
    json_t *messages = cp2_get(cp2_get(in, "state"), "messages");
    if (cancelled) {
        json_t *lg = cp2_args();
        cp2_arg_s(lg, "msg", "Skipping stale compression job: fence cancelled before start");
        json_set(lg, "args", json_array());
        json_t *lr = cp2_call(io, "logger.info", lg);
        if (cp2_is_raise(lr)) return lr;
        json_free(lr);
        json_t *tup = json_array();
        /* return messages, "" — a 2-tuple even when messages is None;
         * json_append(NULL) is a no-op, so append an explicit null node. */
        json_append(tup, messages ? json_copy(messages) : json_null());
        json_append(tup, json_string(""));
        return tup;
    }
    json_t *worker = cp2_get(in, "worker");
    const char *seam = cp2_str(cp2_get(worker, "__callable__"));
    json_t *a = cp2_args();
    cp2_arg_j(a, "worker_fence", fence);
    json_t *r = cp2_call(io, seam ? seam : "worker", a);
    if (cp2_is_raise(r)) return r;
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 17. _is_unchanged_snapshot  (closure over messages)
 *     return result[0] is messages
 *     identity modeled by the messages_id tag vs result[0].__ref__.
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_is_unchanged_snapshot @ agent/conversation_compression.py:_is_unchanged_snapshot */
static json_t *cp2_is_unchanged_snapshot(cp2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    cp2_clear_obj(st);
    json_t *result = cp2_get(in, "result");
    if (!cp2_is_arr(result)) {
        char *m = cp2_xfmt("'%s' object is not subscriptable", cp2_typename(result));
        json_t *r = cp2_mkraise("TypeError", m ? m : "");
        free(m);
        return r;
    }
    if (json_len(result) == 0)
        return cp2_mkraise("IndexError", "list index out of range");
    json_t *first = json_get(result, 0);
    const char *mid = cp2_str(cp2_get(in, "messages_id"));
    const char *ref = cp2_str(cp2_get(first, "__ref__"));
    int same = mid && ref && !strcmp(mid, ref);
    return json_bool(same ? true : false);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 18. _compression_cancel_requested  (closure over hard_cancel_event, commit_fence)
 *     return bool((hard_cancel_event is not None and hard_cancel_event.is_set())
 *                 or (commit_fence is not None and commit_fence.is_cancelled))
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_compression_cancel_requested @ agent/conversation_compression.py:_compression_cancel_requested */
static json_t *cp2_compression_cancel_requested(cp2_io_t *io, const json_t *in, json_t *st)
{
    cp2_clear_obj(st);
    json_t *event = cp2_get(in, "hard_cancel_event");
    json_t *fence = cp2_get(in, "commit_fence");
    int result = 0;
    if (!cp2_is_null(event)) {
        json_t *is_set = cp2_dattr(event, "is_set");
        const char *seam = cp2_str(cp2_get(is_set, "__callable__"));
        json_t *r = cp2_call(io, seam ? seam : "event_is_set", cp2_args());
        if (cp2_is_raise(r)) return r;
        result = cp2_truth(r);
        json_free(r);
    }
    if (!result && !cp2_is_null(fence)) {
        json_t *r = cp2_call(io, "fence_is_cancelled", cp2_args());
        if (cp2_is_raise(r)) return r;
        result = cp2_truth(r);
        json_free(r);
    }
    return json_bool(result ? true : false);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 19. _stop_heartbeat  (closure, nonlocal _activity_heartbeat)
 *     if _activity_heartbeat is not None:
 *       _activity_heartbeat.stop(desc); _activity_heartbeat = None
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_stop_heartbeat @ agent/conversation_compression.py:_stop_heartbeat */
static json_t *cp2_stop_heartbeat(cp2_io_t *io, const json_t *in, json_t *st)
{
    json_t *hb = cp2_get(st, "heartbeat");
    if (!cp2_is_null(hb)) {
        json_t *stop = cp2_dattr(hb, "stop");
        const char *seam = cp2_str(cp2_get(stop, "__callable__"));
        json_t *a = cp2_args();
        cp2_arg_s(a, "desc", cp2_str(cp2_get(in, "desc")));
        json_t *r = cp2_call(io, seam ? seam : "heartbeat_stop", a);
        if (cp2_is_raise(r)) return r;   /* heartbeat NOT cleared on raise */
        json_free(r);
        json_set(st, "heartbeat", json_null());
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 20. _resolve_fallback_prompt  (closure over system_prompt_fallback)
 *     return system_prompt_fallback() if callable(system_prompt_fallback)
 *            else system_prompt_fallback
 * ═══════════════════════════════════════════════════════════════════════════ */
/* PoP: cp2_resolve_fallback_prompt @ agent/conversation_compression.py:_resolve_fallback_prompt */
static json_t *cp2_resolve_fallback_prompt(cp2_io_t *io, const json_t *in, json_t *st)
{
    cp2_clear_obj(st);
    json_t *spf = cp2_get(in, "system_prompt_fallback");
    if (cp2_is_callable(spf)) {
        const char *seam = cp2_str(cp2_get(spf, "__callable__"));
        json_t *r = cp2_call(io, seam ? seam : "system_prompt_fallback", cp2_args());
        if (cp2_is_raise(r)) return r;
        return r;
    }
    return cp2_copy_or_null(spf);
}