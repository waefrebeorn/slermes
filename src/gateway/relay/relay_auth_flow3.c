/*
 * relay_auth_flow3.c — standalone FLOW port of the live quarry module
 * gateway/relay/auth.py (the C port's "relay_auth.py"). Self-contained: every
 * function is static ra3_* and the only dependency is libjson + libc. The
 * harness (#include "gateway/relay/relay_auth_flow3.c") wires ONE universal
 * seam to the SCRIPTED results in each case's "seams" object and records one
 * ordered trace event per seam call.
 *
 * HONEST CLAIM SPLIT (CRLF-aware PoP grep at authoring time):
 *   The live module has exactly three AST defs — sign, make_token,
 *   make_upgrade_token. All three already carry PoP annotations in the older
 *   stub ports relay_auth.c / relay_auth_helpers2.c, so the count of genuinely
 *   UNCLAIMED *names* is 0 (five further claims — _delivery_payload, _hmac_hex,
 *   verify_signature, verify_token, verify_delivery_signature — are STALE:
 *   upstream commit dbd7f76dd8 removed those defs). Per the remainder-scoping
 *   rule this is a standalone FLOW differential oracle for the WHOLE live
 *   module (3 defs), not a silent claim of non-existent gaps; the older claims
 *   stay in force. The three ra3_* cores below are re-derived from live, fully
 *   independent of the stubs (whose make_token is not even base64url).
 *
 * LIVE SURFACE (quarry gateway/relay/auth.py):
 *   sign(payload, secret)
 *       hmac.new(secret.encode(), payload.encode(), sha256).hexdigest()
 *   make_token(payload, secret, ttl_seconds=0)
 *       exp = int(time.time()) + ttl_seconds if ttl_seconds > 0 else 0
 *       signed = f"{payload}:{exp}"
 *       raw = f"{signed}:{sign(signed, secret)}".encode()
 *       return base64.urlsafe_b64encode(raw).decode("ascii").rstrip("=")
 *   make_upgrade_token(gateway_id, secret, ttl_seconds=300)
 *       return make_token(gateway_id, secret, ttl_seconds)
 *
 * SEAM DOCTRINE (identical to the other flow oracles):
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args) returns a
 *   NEW json_t (NULL == Python None). The three seams are:
 *     "time_time"      {}                     -> time.time() (a number)
 *     "hmac_hexdigest" {secret, payload}      -> hmac.new(...).hexdigest()
 *     "b64url_encode"  {raw}                  -> base64.urlsafe_b64encode(raw)
 *   The crypto/base64 primitives are seamed (the oracle monkeypatches the same
 *   three live callables); the C port still transcribes the f-string assembly,
 *   the ttl>0 branch, int() truncation, .decode().rstrip("=") and the composed
 *   make_upgrade_token -> make_token -> sign call order. A MISSING seam key
 *   fail-closes in the harness with a RuntimeError raise spec; a present JSON
 *   null is a valid None. A JSON array script is one result PER CALL (last
 *   repeats, empty -> None).
 *
 * ── Full builtin exception hierarchy ──
 *   ra3_exc_parent() mirrors py3.14's builtin parent chain: BaseException ->
 *   (SystemExit, KeyboardInterrupt, GeneratorExit, Exception); Exception ->
 *   (ValueError, TypeError, RuntimeError, OSError, ...); all OSError/ValueError
 *   /LookupError/ImportError/ArithmeticError/Unicode/Warning grandchildren are
 *   mapped. Unknown custom kinds default to Exception. ra3_caught() walks the
 *   chain; the BaseException trio stays OUTSIDE Exception and therefore always
 *   propagates. There are no try/except sites in the live module, so every
 *   raise spec propagates verbatim — the helper is still implemented in full
 *   and used by ra3_is_base_exc()/the harness propagation guard.
 *
 * Ownership: seam results and returned json_t* are malloc'd (caller frees);
 * args passed to a seam are borrowed; a propagated raise dict is returned
 * upward as-is. Inputs (payload, secret, ttl) are borrowed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include "json.h"
/* ── universal seam + trace ── */

typedef struct ra3_trace { json_t *events; } ra3_trace_t;

typedef struct ra3_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} ra3_seams_t;

static void ra3_trace_add(ra3_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void ra3_trace_free(ra3_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *ra3_seam(const ra3_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}

/* ── JSON value helpers ── */

static bool ra3_jtruth(const json_t *v) {
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
static const char *ra3_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool ra3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ra3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }

static char *ra3_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = (char *)malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

/* ── exception model (FULL builtin parent hierarchy, py3.14) ── */

static const char *ra3_raise_class(const json_t *r) {
    if (ra3_is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool ra3_is_raise(const json_t *r) { return ra3_raise_class(r) != NULL; }

static json_t *ra3_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static const char *ra3_raise_msg(const json_t *r) {
    if (!ra3_is_obj(r)) return "";
    return json_get_str(r, "msg", "");
}

static bool ra3_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
static const char *ra3_exc_parent(const char *k) {
    if (!k) return NULL;
    if (!strcmp(k, "BaseException")) return NULL;
    if (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
        !strcmp(k, "GeneratorExit")) return "BaseException";
    if (!strcmp(k, "Exception")) return "BaseException";
    static const char *const EXC_CHILDREN[] = {
        "StopIteration", "StopAsyncIteration", "ArithmeticError", "AssertionError",
        "AttributeError", "BufferError", "EOFError", "ImportError", "LookupError",
        "MemoryError", "NameError", "OSError", "ReferenceError", "RuntimeError",
        "SyntaxError", "SystemError", "TypeError", "ValueError", "Warning", NULL
    };
    if (ra3_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (ra3_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (ra3_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (ra3_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (ra3_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (ra3_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = { "NotImplementedError", "RecursionError", NULL };
    if (ra3_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (ra3_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (ra3_in_names(k, WARNING_CHILDREN)) return "Warning";
    static const char *const NAME_CHILDREN[] = { "UnboundLocalError", NULL };
    if (ra3_in_names(k, NAME_CHILDREN)) return "NameError";
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    if (!strcmp(k, "PythonFinalizationError")) return "RuntimeError";
    if (!strcmp(k, "_IncompleteInputError")) return "SyntaxError";
    return "Exception";
}
static bool ra3_caught(const json_t *r, const char *cls) {
    const char *k = ra3_raise_class(r);
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = ra3_exc_parent(k);
    }
    return false;
}
static bool ra3_is_base_exc(const json_t *r) {
    return ra3_caught(r, "BaseException");
}

/* ── pure string helpers ── */

/* Python f"{payload}:{exp}" with ASCII inputs. */
static char *ra3_signed(const char *payload, const char *exp) {
    return ra3_xsf("%s:%s", payload ? payload : "", exp ? exp : "");
}
/* Python s.rstrip("=") — remove ALL trailing '=' (not a single trim). */
static char *ra3_rstrip_eq(const char *s) {
    size_t n = s ? strlen(s) : 0;
    while (n > 0 && s[n - 1] == '=') n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, s ? s : "", n);
    out[n] = '\0';
    return out;
}

/* ── sign: hmac.new(...).hexdigest() ── */

static json_t *ra3_sign(const ra3_seams_t *s, const char *payload, const char *secret) {
    json_t *args = json_object();
    json_set(args, "secret", json_string(secret ? secret : ""));
    json_set(args, "payload", json_string(payload ? payload : ""));
    json_t *r = ra3_seam(s, "hmac_hexdigest", args);
    json_free(args);
    return r;  /* string hex digest or a raise spec; live has no try/except */
}

/* ── make_token: exp branch + f-strings + sign + base64url ── */

static json_t *ra3_make_token(const ra3_seams_t *s, const char *payload,
                              const char *secret, double ttl_seconds) {
    json_t *signed_node = NULL;
    char *exp = NULL;

    if (ttl_seconds > 0.0) {
        json_t *args = json_object();
        json_t *tm = ra3_seam(s, "time_time", args);
        json_free(args);
        if (ra3_is_raise(tm)) return tm;  /* BaseException passthrough included */
        if (!tm || tm->type != JSON_NUMBER) {
            /* Domain guard: live int() on a non-number is out of the fixture
             * domain; emit the Python TypeError rather than coerce to 0. */
            json_free(tm);
            return ra3_raise_spec("TypeError",
                                  "'str' object cannot be interpreted as an integer");
        }
        double t = tm->num_val;
        json_free(tm);
        /* Python int() truncates toward zero — C trunc() matches, including
         * negative time values. The ttl is an int on the live signature. */
        long long exp_i = (long long)trunc(t) + (long long)trunc(ttl_seconds);
        exp = ra3_xsf("%lld", exp_i);
    } else {
        exp = strdup("0");
    }

    char *sd = ra3_signed(payload, exp);   /* signed = f"{payload}:{exp}" */
    free(exp);

    json_t *sig = ra3_sign(s, sd, secret);
    if (ra3_is_raise(sig)) { free(sd); return sig; }

    const char *hex = ra3_jstr(sig);
    if (!hex) {  /* scripted non-string digest: Python f-string would stringify,
                  * but the fixture domain is a hex string; fail closed. */
        free(sd);
        json_free(sig);
        return ra3_raise_spec("TypeError", "sequence item 1: expected str instance");
    }

    /* raw = f"{signed}:{sign(signed, secret)}" — the SIGNED string, not the
     * bare payload; dropping the exp here is the classic transcription slip. */
    char *raw = ra3_xsf("%s:%s", sd, hex);
    free(sd);
    char *raw_owned = raw;
    json_free(sig);

    json_t *bargs = json_object();
    json_set(bargs, "raw", json_string(raw_owned));
    json_t *enc = ra3_seam(s, "b64url_encode", bargs);
    json_free(bargs);
    free(raw_owned);
    if (ra3_is_raise(enc)) return enc;

    const char *es = ra3_jstr(enc);
    if (!es) { json_free(enc); return NULL; }  /* Python None -> NULL */
    /* .decode("ascii").rstrip("=") */
    char *tok = ra3_rstrip_eq(es);
    json_free(enc);
    signed_node = json_string(tok);
    free(tok);
    return signed_node;
}

/* ── make_upgrade_token: composed sibling ── */

static json_t *ra3_make_upgrade_token(const ra3_seams_t *s, const char *gateway_id,
                                      const char *secret, double ttl_seconds) {
    return ra3_make_token(s, gateway_id, secret, ttl_seconds);
}
