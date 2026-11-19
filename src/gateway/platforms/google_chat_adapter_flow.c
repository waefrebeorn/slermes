/*
 * google_chat_adapter_flow.c -- FLOW cores + leaves of
 * plugins/platforms/google_chat/adapter.py (hermes-agent): the Google Chat
 * plugin adapter's module-level decision chains (thread/create body builders,
 * MIME routing, HttpError status + retryable classification, SA/path/email
 * redaction, proxy-error text chain, Google-owned-host SSRF guard, ADC foreign
 * credential probe, plugin config gate + connected probe, scoped-env inbound
 * settings chain, registry check, env enablement seed, standalone error) and
 * its seamed adapter flows (int/str setting coercion, inbound envelope
 * extraction + prepare/dedup, thread-id resolution, rate-limit note, user-cred
 * invalidation, typing stop, outbound thread tracking, unadmitted release,
 * connect-failed fatal record, text chunking, HTTP bearer verification, cached
 * bot-id load, background-failure log, loop-callback acceptance).
 *
 * Decision sequences only: every live I/O / injected callable / sibling
 * method / foreign import enters through ONE universal seam:
 * gca_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and gca_-prefixed.
 *
 * Seam name table (name -> live call):
 *   logger.debug/warning/error/exception  logger.<level>(fmt,*args)
 *   get_scoped_secret        gateway.platforms._shared.get_scoped_secret
 *   seed_extra_from_env      ..._seed_extra_from_env(_ENV_SEED_KEYS, home_env=)
 *   send_error               ...send_error(msg)
 *   is_multiplex_active      agent.secret_scope.is_multiplex_active()
 *   os.environ.get           os.environ.get(key)
 *   urlparse                 urllib.parse.urlparse(url) -> {scheme,hostname}
 *   describe_proxy_error     base.describe_proxy_error(exc, proxies)
 *   env_proxy_for_redaction  base.env_proxy_for_redaction(*targets)
 *   _load_google_modules     module lazy-import probe (scripted)
 *   _verify_google_id_token  module id-token verify (scripted)
 *   _http_status / _proxy_error / _redact_sensitive / _exc_text /
 *   _validate_config / _env_inbound_settings / _env_inbound_configured /
 *   check_google_chat_requirements / _extract_message_payload
 *                            ported siblings (scripted when composed)
 *   dedup.is_duplicate / dedup.discard   self._dedup.<m>(name)
 *   thread_count_store.incr / .decr      self._thread_count_store.<m>(chat,thr)
 *   _set_fatal_error         self._set_fatal_error(code=,message=,retryable=)
 *   _bot_id_cache_path       self._bot_id_cache_path() -> POSIX path string
 *   bot_id_cache.read_text   path.read_text(encoding="utf-8-sig")
 *   future.result            future.result()
 *   loop.is_closed           loop.is_closed()
 *
 * Representation contract (shared with tests/oracle_google_chat_adapter_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc)} object
 *   (OSError adds errno, message "[Errno N] msg"). BaseException kinds
 *   (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) pass
 *   through every except-Exception site.
 * - Exception doubles (args.exc): {"cls": name, "msg": str(exc), "attrs":{...}}.
 *   attrs values are attribute carriers (resp -> SimpleNamespace). The spec is
 *   projected verbatim into seam args.
 * - Attribute carriers (config / loop / parsed) travel as {"__obj__": {attrs}};
 *   a plain JSON object IS a Python dict. loop.is_closed == "seam" marks a
 *   scripted callable; absent -> the default lambda (no seam event).
 * - MessageType returns project to their .value string; tuple returns project
 *   to JSON arrays.
 * - logger seams record {"fmt": <literal>, "args": [<raw args>]}; exc_info
 *   kwargs drop on both sides. seed_extra_from_env's constant keys arg elides.
 * - Path modeling is POSIX: _bot_id_cache_path = home / "google_chat_bot_id.json"
 *   (lexical join); read_text(utf-8-sig) strips ONE leading U+FEFF AFTER the
 *   seam call (trace ret carries the raw scripted text).
 * - get_scoped_secret args carry {name, default}; default is null when the
 *   live call site passes only the name.
 *
 * Fixture corpus domains (documented restrictions):
 * - ASCII scope: strip/lower/upper fold ASCII whitespace (0x09-0x0d,0x1c-0x1f,
 *   0x20) / ASCII letters; int() string parses accept ASCII digits/sign/
 *   underscore-between-digits with the int-strip set (White_Space MINUS
 *   0x1c-0x1f); Unicode digits / NBSP excluded.
 * - str() positions carry str/int/bool/None only (no container repr, no
 *   non-integral float repr).
 * - urlparse is SEAMED: its result is scripted as {scheme, hostname}; the
 *   urllib.parse bracket/userinfo/IPv6 surface is out of scope.
 * - json.loads in _load_cached_bot_id uses clearly-valid / clearly-invalid
 *   JSON (libjson parse agrees with json.loads on the corpus).
 * - _chunk_text uses the real 4000-char limit; long strings are generated.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GCA_MAX_TEXT_LENGTH 4000
#define GCA_RATE_LIMIT_WARN_THRESHOLD 5
#define GCA_LEGACY_USER_IDENTITY "__legacy__"
#define GCA_TYPING_CONSUMED_SENTINEL "<consumed>"

/* -- universal seam context -- */
typedef struct gca_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} gca_ctx_t;

/* -- basic helpers -- */
static bool gca_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static bool gca_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0 ||
                    strcmp(kind, "CancelledError") == 0);
}

/* A raise spec that an except-Exception site must PASS THROUGH. */
static bool gca_raise_passes(const json_t *r) {
    return gca_is_raise(r) && gca_is_base_exc(json_get_str(r, "__raise__", ""));
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool gca_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool gca_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static const char *gca_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool gca_str_eq(const json_t *v, const char *s) {
    const char *t = gca_str(v);
    return t && s && strcmp(t, s) == 0;
}

static json_t *gca_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static bool gca_is_oserror_type(const char *t) {
    static const char *fam[] = {
        "OSError", "IOError", "TimeoutError", "PermissionError",
        "FileNotFoundError", "FileExistsError", "BlockingIOError",
        "ConnectionError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", "BrokenPipeError", "InterruptedError",
        "NotADirectoryError", "IsADirectoryError", "ChildProcessError",
        "ProcessLookupError", "DirectoryNotEmptyError",
    };
    for (size_t i = 0; i < sizeof(fam) / sizeof(fam[0]); i++)
        if (strcmp(t, fam[i]) == 0) return true;
    return false;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *gca_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && gca_is_oserror_type(type)) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = gca_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return gca_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). A
 * missing/exhausted script RETURNS a fail-closed RuntimeError spec (the
 * event is recorded before the caller propagates it). */
static json_t *gca_call(gca_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = gca_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = gca_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = gca_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* -- string helpers (Python str semantics on the ASCII fixture corpus) -- */
static char *gca_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* str.strip() whitespace (ASCII scope): 0x09-0x0d, 0x1c-0x1f, 0x20. */
static bool gca_is_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f) || c == ' ';
}

/* int() strip set: White_Space MINUS 0x1c-0x1f (int('1\x1c') raises). */
static bool gca_is_int_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0d) || c == ' ';
}

static char *gca_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && gca_is_ws((unsigned char)s[a])) a++;
    while (b > a && gca_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}

static char *gca_lstrip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0;
    while (a < n && gca_is_ws((unsigned char)s[a])) a++;
    return gca_strdup(s + a);
}

static char *gca_lower(const char *s) {
    char *o = gca_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

static char *gca_upper(const char *s) {
    char *o = gca_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)toupper((unsigned char)*p);
    return o;
}

static bool gca_contains(const char *hay, const char *needle) {
    return hay && needle && strstr(hay, needle) != NULL;
}

static bool gca_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool gca_ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

/* replace all occurrences of a literal substring (str.replace). */
static char *gca_replace(const char *s, const char *from, const char *to) {
    if (!s) s = "";
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0) return gca_strdup(s);
    size_t cap = strlen(s) + 1, n = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    const char *p = s;
    while (*p) {
        if (strncmp(p, from, fl) == 0) {
            size_t need = n + tl + strlen(p + fl) + 1;
            if (need > cap) { cap = need * 2; char *nb = realloc(o, cap); if (!nb) { free(o); return NULL; } o = nb; }
            memcpy(o + n, to, tl); n += tl; p += fl;
        } else {
            if (n + 2 > cap) { cap = cap * 2 + strlen(p); char *nb = realloc(o, cap); if (!nb) { free(o); return NULL; } o = nb; }
            o[n++] = *p++;
        }
    }
    o[n] = '\0';
    return o;
}

/* Python type name for a JSON value (attribute-error / int() messages). */
static const char *gca_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* str() projection for the fixture domain (str/int/bool/None). Containers
 * are OUT OF DOMAIN and return NULL. */
static char *gca_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return gca_strdup("None");
    switch (v->type) {
        case JSON_STRING: return gca_strdup(v->str_val);
        case JSON_BOOL: return gca_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 9.2e18)
                snprintf(buf, sizeof buf, "%.0f", d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return gca_strdup(buf);
        }
        default: return NULL;
    }
}

/* Python repr() of a str (ASCII corpus). */
static char *gca_py_repr_str(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *o = malloc(cap);
    if (!o) return NULL;
    size_t k = 0;
    o[k++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\' || c == (unsigned char)q) { o[k++] = '\\'; o[k++] = (char)c; }
        else if (c == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
        else if (c == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
        else if (c == '\t') { o[k++] = '\\'; o[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            static const char hex[] = "0123456789abcdef";
            o[k++] = '\\'; o[k++] = 'x'; o[k++] = hex[c >> 4]; o[k++] = hex[c & 15];
        } else o[k++] = (char)c;
    }
    o[k++] = q; o[k] = '\0';
    return o;
}

/* -- UTF-8 helpers (Python len counts codepoints) -- */
static size_t gca_utf8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xc0) != 0x80) n++;
    return n;
}

/* byte pointer at codepoint index i (clamped to [0,len]). */
static const char *gca_cp_ptr(const char *s, long long i) {
    if (!s) return "";
    if (i < 0) i = 0;
    const char *p = s;
    while (i > 0 && *p) {
        p++;
        while (*p && ((unsigned char)*p & 0xc0) == 0x80) p++;
        i--;
    }
    return p;
}

/* Python s[:b] on codepoints (b clamped; negative counts from end). */
static char *gca_slice_head(const char *s, long long b) {
    if (!s) s = "";
    long long n = (long long)gca_utf8_len(s);
    if (b < 0) b = n + b;
    if (b < 0) b = 0;
    if (b > n) b = n;
    const char *pb = gca_cp_ptr(s, b);
    size_t len = (size_t)(pb - s);
    char *o = malloc(len + 1);
    if (!o) return NULL;
    memcpy(o, s, len); o[len] = '\0';
    return o;
}

/* Python s[a:] on codepoints (a clamped; negative counts from end). */
static char *gca_slice_tail(const char *s, long long a) {
    if (!s) s = "";
    long long n = (long long)gca_utf8_len(s);
    if (a < 0) a = n + a;
    if (a < 0) a = 0;
    if (a > n) a = n;
    return gca_strdup(gca_cp_ptr(s, a));
}

/* rfind(needle, 0, end) on codepoints: last index < end where needle starts,
 * or -1. needle is a single ASCII char here ("\n"). */
static long long gca_rfind_char(const char *s, char needle, long long end) {
    if (!s) return -1;
    long long n = (long long)gca_utf8_len(s);
    if (end > n) end = n;
    if (end < 0) end = 0;
    const char *p = s;
    long long idx = -1;
    for (long long i = 0; i < end && *p; i++) {
        if (*p == needle) idx = i;
        p++;
        while (*p && ((unsigned char)*p & 0xc0) == 0x80) p++;
    }
    return idx;
}

/* -- object / attribute carriers -- */
/* {"__obj__": {attrs}} -> inner attrs object; plain object -> itself NULL
 * (a plain JSON object is a dict, not an attribute carrier). */
static const json_t *gca_obj_attrs(const json_t *v) {
    if (v && v->type == JSON_OBJECT) {
        json_t *inner = json_obj_get(v, "__obj__");
        if (inner && inner->type == JSON_OBJECT) return inner;
    }
    return NULL;
}

/* -- int() coercion on the fixture corpus -- */
/* int(str): ASCII digits, sign, underscores BETWEEN digits, int-strip set.
 * Returns true on success (value in *out); false on ValueError. */
static bool gca_int_str(const char *s, long long *out) {
    if (!s) return false;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && gca_is_int_ws((unsigned char)s[a])) a++;
    while (b > a && gca_is_int_ws((unsigned char)s[b - 1])) b--;
    if (a >= b) return false;
    const char *p = s + a;
    const char *end = s + b;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (p >= end) return false;
    /* digits with underscores between digits; require >=1 digit */
    char clean[256];
    size_t k = 0;
    bool prev_digit = false, any = false;
    for (; p < end; p++) {
        char c = *p;
        if (c >= '0' && c <= '9') {
            if (k + 1 >= sizeof clean) return false;
            clean[k++] = c; prev_digit = true; any = true;
        } else if (c == '_') {
            if (!prev_digit) return false;       /* underscore needs digit before */
            if (p + 1 >= end || !(p[1] >= '0' && p[1] <= '9')) return false; /* and after */
            prev_digit = false;
        } else {
            return false;
        }
    }
    if (!any) return false;
    clean[k] = '\0';
    char *ep = NULL;
    long long v = strtoll(clean, &ep, 10);
    if (ep == clean || *ep != '\0') return false;
    *out = neg ? -v : v;
    return true;
}

/* int(value) for a JSON node: number truncates toward zero, bool -> 0/1,
 * string parses; anything else is a TypeError. kind: 0 ok, 1 ValueError,
 * 2 TypeError. */
static int gca_int_of(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 2;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 0; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 0; }
    if (v->type == JSON_STRING) {
        return gca_int_str(v->str_val, out) ? 0 : 1;
    }
    return 2;
}
/* -- redaction regex scanners (_REDACTIONS, ASCII \s scope) -- */
/* [^/\s] : not '/' and not whitespace. */
static bool gca_path_char(unsigned char c) {
    return c != '/' && !gca_is_ws(c);
}

/* projects/[^/\s]+/<kind>/[^/\s]+  ->  projects/<redacted>/<kind>/<redacted>
 * kind is "subscriptions" or "topics". Leftmost, non-overlapping. */
static char *gca_redact_paths(const char *text, const char *kind) {
    if (!text) text = "";
    char lit[64];
    snprintf(lit, sizeof lit, "/%s/", kind);
    size_t litlen = strlen(lit);
    char repl[128];
    snprintf(repl, sizeof repl, "projects/<redacted>/%s/<redacted>", kind);
    size_t rlen = strlen(repl);
    size_t cap = strlen(text) + 1, n = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    const char *p = text;
    while (*p) {
        if (strncmp(p, "projects/", 9) == 0) {
            const char *q = p + 9;
            const char *r1 = q;
            while (*r1 && gca_path_char((unsigned char)*r1)) r1++;
            if (r1 > q && strncmp(r1, lit, litlen) == 0) {
                const char *r2 = r1 + litlen;
                const char *r3 = r2;
                while (*r3 && gca_path_char((unsigned char)*r3)) r3++;
                if (r3 > r2) {
                    size_t need = n + rlen + strlen(r3) + 1;
                    if (need > cap) { cap = need * 2; char *nb = realloc(o, cap); if (!nb) { free(o); return NULL; } o = nb; }
                    memcpy(o + n, repl, rlen); n += rlen;
                    p = r3;
                    continue;
                }
            }
        }
        if (n + 2 > cap) { cap = cap * 2 + strlen(p); char *nb = realloc(o, cap); if (!nb) { free(o); return NULL; } o = nb; }
        o[n++] = *p++;
    }
    o[n] = '\0';
    return o;
}

static bool gca_sa_local_char(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '+' || c == '-';
}
static bool gca_sa_host_char(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-';
}

/* [a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.iam\.gserviceaccount\.com
 *   -> <sa>@<project>.iam.gserviceaccount.com */
static char *gca_redact_sa(const char *text) {
    if (!text) text = "";
    static const char tail[] = ".iam.gserviceaccount.com";
    static const char repl[] = "<sa>@<project>.iam.gserviceaccount.com";
    size_t taillen = sizeof(tail) - 1, rlen = sizeof(repl) - 1;
    size_t cap = strlen(text) + 1, n = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    const char *p = text;
    while (*p) {
        const char *l = p;
        while (*l && gca_sa_local_char((unsigned char)*l)) l++;
        if (l > p && *l == '@') {
            const char *h = l + 1;
            const char *h2 = h;
            while (*h2 && gca_sa_host_char((unsigned char)*h2)) h2++;
            if (h2 > h && strncmp(h2, tail, taillen) == 0) {
                const char *mend = h2 + taillen;
                size_t need = n + rlen + strlen(mend) + 1;
                if (need > cap) { cap = need * 2; char *nb = realloc(o, cap); if (!nb) { free(o); return NULL; } o = nb; }
                memcpy(o + n, repl, rlen); n += rlen;
                p = mend;
                continue;
            }
        }
        if (n + 2 > cap) { cap = cap * 2 + strlen(p); char *nb = realloc(o, cap); if (!nb) { free(o); return NULL; } o = nb; }
        o[n++] = *p++;
    }
    o[n] = '\0';
    return o;
}

/* == module leaves == */

/* PoP: gca__thread_body @ plugins/platforms/google_chat/adapter.py:_thread_body */
static json_t *gca__thread_body(gca_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *text = json_obj_get(args, "text");
    const json_t *thread_id = json_obj_get(args, "thread_id");
    json_t *body = json_object();
    json_set(body, "text", text ? json_copy(text) : json_null());
    if (gca_truthy(thread_id)) {
        json_t *th = json_object();
        json_set(th, "name", json_copy(thread_id));
        json_set(body, "thread", th);
    }
    return body;
}

/* PoP: gca__create_kwargs @ plugins/platforms/google_chat/adapter.py:_create_kwargs */
static json_t *gca__create_kwargs(gca_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *chat_id = json_obj_get(args, "chat_id");
    const json_t *body = json_obj_get(args, "body");
    json_t *kw = json_object();
    json_set(kw, "parent", chat_id ? json_copy(chat_id) : json_null());
    json_set(kw, "body", body ? json_copy(body) : json_null());
    /* (body.get("thread") or {}).get("name") */
    const json_t *thread = body ? json_obj_get(body, "thread") : NULL;
    const json_t *name = NULL;
    if (gca_truthy(thread) && thread->type == JSON_OBJECT)
        name = json_obj_get(thread, "name");
    if (gca_truthy(name))
        json_set_hdr(kw, "messageReplyOption", "REPLY_MESSAGE_FALLBACK_TO_NEW_THREAD");
    return kw;
}

/* PoP: gca__mime_for_message_type @ plugins/platforms/google_chat/adapter.py:_mime_for_message_type */
static json_t *gca__mime_for_message_type(gca_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *mime = gca_str(json_obj_get(args, "mime"));
    static const struct { const char *prefix; const char *value; } table[] = {
        {"image/", "photo"}, {"audio/", "audio"}, {"video/", "video"},
    };
    if (mime && mime[0]) {
        for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
            if (gca_starts_with(mime, table[i].prefix))
                return json_string(table[i].value);
    }
    return json_string("document");
}

/* PoP: gca__http_status @ plugins/platforms/google_chat/adapter.py:_http_status */
static json_t *gca__http_status(gca_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *exc = json_obj_get(args, "exc");
    const json_t *attrs = exc ? json_obj_get(exc, "attrs") : NULL;
    const json_t *resp = attrs ? json_obj_get(attrs, "resp") : NULL;
    /* getattr(getattr(exc,"resp",None),"status",None) */
    if (resp && resp->type == JSON_OBJECT) {
        json_t *status = json_obj_get(resp, "status");
        if (status) return json_copy(status);
    }
    return NULL;
}

/* PoP: gca__is_retryable_error @ plugins/platforms/google_chat/adapter.py:_is_retryable_error */
static json_t *gca__is_retryable_error(gca_ctx_t *ctx, const json_t *args) {
    const json_t *exc = json_obj_get(args, "exc");
    json_t *sa = json_object();
    json_set(sa, "exc", exc ? json_copy(exc) : json_null());
    json_t *status = gca_call(ctx, "_http_status", sa);
    if (gca_is_raise(status)) return status;  /* no try: propagate */
    /* isinstance(status, int) -- bool is an int instance */
    bool is_int = status && (status->type == JSON_NUMBER || status->type == JSON_BOOL);
    if (is_int) {
        double sv = status->type == JSON_BOOL ? (status->bool_val ? 1.0 : 0.0) : status->num_val;
        bool retry = (sv == 429 || sv == 500 || sv == 502 || sv == 503 || sv == 504);
        json_free(status);
        return json_bool(retry);
    }
    json_free(status);
    /* text = str(exc).lower() */
    const char *msg = exc ? json_get_str(exc, "msg", "") : "";
    char *text = gca_lower(msg);
    static const char *phrases[] = {"timeout", "timed out", "broken pipe", "remote disconnected"};
    bool hit = false;
    for (size_t i = 0; i < sizeof(phrases) / sizeof(phrases[0]); i++)
        if (gca_contains(text, phrases[i])) { hit = true; break; }
    if (!hit && gca_contains(text, "connection")) {
        static const char *words[] = {"reset", "refused", "aborted"};
        for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++)
            if (gca_contains(text, words[i])) { hit = true; break; }
    }
    free(text);
    return json_bool(hit);
}

/* PoP: gca__redact_sensitive @ plugins/platforms/google_chat/adapter.py:_redact_sensitive */
static json_t *gca__redact_sensitive(gca_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *text = json_obj_get(args, "text");
    if (!gca_truthy(text)) return text ? json_copy(text) : json_null();
    const char *s = gca_str(text);
    if (!s) return json_copy(text);  /* non-str truthy: out of corpus domain */
    char *a = gca_redact_paths(s, "subscriptions");
    char *b = gca_redact_paths(a, "topics");
    free(a);
    char *c = gca_redact_sa(b);
    free(b);
    json_t *out = json_string(c);
    free(c);
    return out;
}

/* PoP: gca__is_google_owned_host @ plugins/platforms/google_chat/adapter.py:_is_google_owned_host */
static json_t *gca__is_google_owned_host(gca_ctx_t *ctx, const json_t *args) {
    const json_t *url = json_obj_get(args, "url");
    json_t *ua = json_object();
    json_set(ua, "url", url ? json_copy(url) : json_null());
    json_t *parsed = gca_call(ctx, "urlparse", ua);
    if (gca_is_raise(parsed)) {
        /* except Exception: return False (BaseException passes) */
        if (gca_raise_passes(parsed)) return parsed;
        json_free(parsed);
        return json_bool(false);
    }
    const json_t *attrs = gca_obj_attrs(parsed);
    const char *scheme = attrs ? json_get_str(attrs, "scheme", "") : "";
    const json_t *hn = attrs ? json_obj_get(attrs, "hostname") : NULL;
    const char *hostraw = gca_str(hn);
    char *host = gca_lower(hostraw ? hostraw : "");
    bool ok = false;
    if (strcmp(scheme, "https") == 0 && host[0]) {
        static const char *trusted[] = {
            "googleapis.com", "chat.google.com", "drive.google.com", "docs.google.com",
            "lh3.googleusercontent.com", "lh4.googleusercontent.com",
            "lh5.googleusercontent.com", "lh6.googleusercontent.com",
        };
        for (size_t i = 0; i < sizeof(trusted) / sizeof(trusted[0]); i++) {
            if (strcmp(host, trusted[i]) == 0) { ok = true; break; }
            char dot[128];
            snprintf(dot, sizeof dot, ".%s", trusted[i]);
            if (gca_ends_with(host, dot)) { ok = true; break; }
        }
    }
    free(host);
    json_free(parsed);
    return json_bool(ok);
}
/* == module flows == */

/* get_scoped_secret(name) -- 1-arg call site (default null). */
static json_t *gca_scoped(gca_ctx_t *ctx, const char *name) {
    json_t *a = json_object();
    json_set_hdr(a, "name", name);
    json_set(a, "default", json_null());
    return gca_call(ctx, "get_scoped_secret", a);
}

/* PoP: gca__proxy_error @ plugins/platforms/google_chat/adapter.py:_proxy_error */
static json_t *gca__proxy_error(gca_ctx_t *ctx, const json_t *args) {
    const json_t *exc = json_obj_get(args, "exc");
    const json_t *targets = json_obj_get(args, "targets");
    json_t *urls = json_array();
    if (gca_truthy(targets) && targets->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(targets); i++)
            json_append(urls, json_copy(json_get(targets, i)));
    } else {
        static const char *hosts[] = {"chat", "oauth2", "pubsub", "www"};
        for (size_t i = 0; i < 4; i++) {
            char buf[128];
            snprintf(buf, sizeof buf, "https://%s.googleapis.com", hosts[i]);
            json_append(urls, json_string(buf));
        }
    }
    json_t *ea = json_object();
    json_set(ea, "targets", urls);
    json_t *proxies = gca_call(ctx, "env_proxy_for_redaction", ea);
    if (gca_is_raise(proxies)) return proxies;
    json_t *da = json_object();
    json_set(da, "exc", exc ? json_copy(exc) : json_null());
    json_set(da, "proxies", proxies);
    return gca_call(ctx, "describe_proxy_error", da);
}

/* PoP: gca__exc_text @ plugins/platforms/google_chat/adapter.py:_exc_text */
static json_t *gca__exc_text(gca_ctx_t *ctx, const json_t *args) {
    const json_t *exc = json_obj_get(args, "exc");
    const json_t *targets = json_obj_get(args, "targets");
    json_t *pa = json_object();
    json_set(pa, "exc", exc ? json_copy(exc) : json_null());
    json_set(pa, "targets", targets ? json_copy(targets) : json_array());
    json_t *shown = gca_call(ctx, "_proxy_error", pa);
    if (gca_is_raise(shown)) return shown;
    if (gca_truthy(shown)) return shown;
    json_free(shown);
    const char *msg = exc ? json_get_str(exc, "msg", "") : "";
    json_t *ra = json_object();
    json_set_hdr(ra, "text", msg);
    return gca_call(ctx, "_redact_sensitive", ra);
}

/* PoP: gca__adc_would_borrow_foreign_credentials @ plugins/platforms/google_chat/adapter.py:_adc_would_borrow_foreign_credentials */
static json_t *gca__adc_would_borrow_foreign_credentials(gca_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *mx = gca_call(ctx, "is_multiplex_active", json_object());
    if (gca_is_raise(mx)) return mx;
    if (!gca_truthy(mx)) return mx;  /* `and` returns the falsy value verbatim */
    json_free(mx);
    json_t *e1a = json_object();
    json_set_hdr(e1a, "key", "GOOGLE_CHAT_SERVICE_ACCOUNT_JSON");
    json_t *e1 = gca_call(ctx, "os.environ.get", e1a);
    if (gca_is_raise(e1)) return e1;
    if (gca_truthy(e1)) { json_free(e1); return json_bool(true); }
    json_free(e1);
    json_t *e2a = json_object();
    json_set_hdr(e2a, "key", "GOOGLE_APPLICATION_CREDENTIALS");
    json_t *e2 = gca_call(ctx, "os.environ.get", e2a);
    if (gca_is_raise(e2)) return e2;
    bool hit = gca_truthy(e2);
    json_free(e2);
    return json_bool(hit);
}

/* PoP: gca__validate_config @ plugins/platforms/google_chat/adapter.py:_validate_config */
static json_t *gca__validate_config(gca_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *config = json_obj_get(args, "config");
    const json_t *attrs = gca_obj_attrs(config);
    const json_t *extra = attrs ? json_obj_get(attrs, "extra") : NULL;
    if (!extra || extra->type != JSON_OBJECT) extra = NULL;  /* `or {}` */
    const json_t *http = extra ? json_obj_get(extra, "http_events_url") : NULL;
    const json_t *proj = extra ? json_obj_get(extra, "project_id") : NULL;
    const json_t *sub = extra ? json_obj_get(extra, "subscription_name") : NULL;
    bool ok = gca_truthy(http) || (gca_truthy(proj) && gca_truthy(sub));
    return json_bool(ok);
}

/* PoP: gca__is_connected @ plugins/platforms/google_chat/adapter.py:_is_connected */
static json_t *gca__is_connected(gca_ctx_t *ctx, const json_t *args) {
    const json_t *config = json_obj_get(args, "config");
    const json_t *attrs = gca_obj_attrs(config);
    const json_t *enabled = attrs ? json_obj_get(attrs, "enabled") : NULL;
    if (!gca_truthy(enabled)) return json_bool(false);
    json_t *va = json_object();
    json_set(va, "config", config ? json_copy(config) : json_null());
    return gca_call(ctx, "_validate_config", va);
}

/* unpack a 3-sequence seam result; on bad shape return a raise spec. */
static json_t *gca_unpack3(const json_t *r, json_t **a, json_t **b, json_t **c) {
    if (gca_is_null(r))
        return gca_err("TypeError", "cannot unpack non-iterable NoneType object");
    if (r->type != JSON_ARRAY) {
        char buf[128];
        snprintf(buf, sizeof buf, "cannot unpack non-iterable %s object", gca_type_name(r));
        return gca_err("TypeError", buf);
    }
    size_t n = json_len(r);
    if (n != 3) {
        char buf[128];
        if (n < 3)
            snprintf(buf, sizeof buf, "not enough values to unpack (expected 3, got %zu)", n);
        else
            snprintf(buf, sizeof buf, "too many values to unpack (expected 3, got %zu)", n);
        return gca_err("ValueError", buf);
    }
    *a = json_get(r, 0); *b = json_get(r, 1); *c = json_get(r, 2);
    return NULL;
}

/* PoP: gca__env_inbound_settings @ plugins/platforms/google_chat/adapter.py:_env_inbound_settings */
static json_t *gca__env_inbound_settings(gca_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *p1 = gca_scoped(ctx, "GOOGLE_CHAT_PROJECT_ID");
    if (gca_is_raise(p1)) return p1;
    json_t *project;
    if (gca_truthy(p1)) project = p1;
    else {
        json_free(p1);
        json_t *p2 = gca_scoped(ctx, "GOOGLE_CLOUD_PROJECT");
        if (gca_is_raise(p2)) return p2;
        project = p2;
    }
    json_t *s1 = gca_scoped(ctx, "GOOGLE_CHAT_SUBSCRIPTION_NAME");
    if (gca_is_raise(s1)) { json_free(project); return s1; }
    json_t *sub;
    if (gca_truthy(s1)) sub = s1;
    else {
        json_free(s1);
        json_t *s2 = gca_scoped(ctx, "GOOGLE_CHAT_SUBSCRIPTION");
        if (gca_is_raise(s2)) { json_free(project); return s2; }
        sub = s2;
    }
    json_t *url = gca_scoped(ctx, "GOOGLE_CHAT_HTTP_EVENTS_URL");
    if (gca_is_raise(url)) { json_free(project); json_free(sub); return url; }
    json_t *out = json_array();
    json_append(out, json_copy(project));
    json_append(out, json_copy(sub));
    json_append(out, json_copy(url));
    json_free(project); json_free(sub); json_free(url);
    return out;
}

/* PoP: gca__env_inbound_configured @ plugins/platforms/google_chat/adapter.py:_env_inbound_configured */
static json_t *gca__env_inbound_configured(gca_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *r = gca_call(ctx, "_env_inbound_settings", json_object());
    if (gca_is_raise(r)) return r;
    json_t *p, *s, *u;
    json_t *bad = gca_unpack3(r, &p, &s, &u);
    if (bad) { json_free(r); return bad; }
    bool ok = gca_truthy(u) || (gca_truthy(p) && gca_truthy(s));
    json_free(r);
    return json_bool(ok);
}

/* PoP: gca_check_google_chat_requirements @ plugins/platforms/google_chat/adapter.py:check_google_chat_requirements */
static json_t *gca_check_google_chat_requirements(gca_ctx_t *ctx, const json_t *args) {
    (void)args;
    return gca_call(ctx, "_load_google_modules", json_object());
}

/* PoP: gca__check_for_registry @ plugins/platforms/google_chat/adapter.py:_check_for_registry */
static json_t *gca__check_for_registry(gca_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *r1 = gca_call(ctx, "check_google_chat_requirements", json_object());
    if (gca_is_raise(r1)) return r1;
    if (!gca_truthy(r1)) return r1;
    json_free(r1);
    return gca_call(ctx, "_env_inbound_configured", json_object());
}

/* PoP: gca__env_enablement @ plugins/platforms/google_chat/adapter.py:_env_enablement */
static json_t *gca__env_enablement(gca_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *r0 = gca_call(ctx, "_env_inbound_configured", json_object());
    if (gca_is_raise(r0)) return r0;
    if (!gca_truthy(r0)) { json_free(r0); return NULL; }
    json_free(r0);
    json_t *r = gca_call(ctx, "_env_inbound_settings", json_object());
    if (gca_is_raise(r)) return r;
    json_t *p, *s, *u;
    json_t *bad = gca_unpack3(r, &p, &s, &u);
    if (bad) { json_free(r); return bad; }
    json_t *sa1 = gca_scoped(ctx, "GOOGLE_CHAT_SERVICE_ACCOUNT_JSON");
    if (gca_is_raise(sa1)) { json_free(r); return sa1; }
    json_t *sa;
    if (gca_truthy(sa1)) sa = sa1;
    else {
        json_free(sa1);
        json_t *sa2 = gca_scoped(ctx, "GOOGLE_APPLICATION_CREDENTIALS");
        if (gca_is_raise(sa2)) { json_free(r); return sa2; }
        sa = sa2;
    }
    json_t *seed = json_object();
    const char *keys[] = {"project_id", "subscription_name", "http_events_url", "service_account_json"};
    json_t *vals[] = {p, s, u, sa};
    for (size_t i = 0; i < 4; i++)
        if (gca_truthy(vals[i])) json_set(seed, keys[i], json_copy(vals[i]));
    json_free(sa);
    json_t *sea = json_object();
    json_set_hdr(sea, "home_env", "GOOGLE_CHAT_HOME_CHANNEL");
    json_t *r4 = gca_call(ctx, "seed_extra_from_env", sea);
    if (gca_is_raise(r4)) { json_free(seed); json_free(r); return r4; }
    if (gca_is_null(r4)) {
        json_free(seed); json_free(r); json_free(r4);
        return gca_err("TypeError", "'NoneType' object is not iterable");
    }
    if (r4->type != JSON_OBJECT) {
        char buf[128];
        snprintf(buf, sizeof buf, "'%s' object is not iterable", gca_type_name(r4));
        json_free(seed); json_free(r); json_free(r4);
        return gca_err("TypeError", buf);
    }
    for (size_t i = 0; i < r4->c.count; i++)
        json_set(seed, r4->c.keys[i], json_copy(r4->c.items[i]));
    json_free(r4); json_free(r);
    return seed;
}

/* PoP: gca__standalone_error @ plugins/platforms/google_chat/adapter.py:_standalone_error */
static json_t *gca__standalone_error(gca_ctx_t *ctx, const json_t *args) {
    const json_t *detail = json_obj_get(args, "detail");
    char *ds = gca_str_of(detail);
    size_t n = strlen("Google Chat standalone send: ") + (ds ? strlen(ds) : 0) + 1;
    char *msg = malloc(n);
    if (msg) snprintf(msg, n, "Google Chat standalone send: %s", ds ? ds : "");
    free(ds);
    json_t *sa = json_object();
    json_set_hdr(sa, "msg", msg ? msg : "");
    free(msg);
    return gca_call(ctx, "send_error", sa);
}
/* == adapter methods == */

static const json_t *gca_state(const gca_ctx_t *ctx, const char *key) {
    return ctx->state ? json_obj_get(ctx->state, key) : NULL;
}

/* ensure a dict exists in state for in-place mutation (dict.setdefault). */
static json_t *gca_state_obj(gca_ctx_t *ctx, const char *key) {
    json_t *v = ctx->state ? json_obj_get(ctx->state, key) : NULL;
    if (!v || v->type != JSON_OBJECT) {
        v = json_object();
        json_set(ctx->state, key, v);
    }
    return v;
}

/* PoP: gca__int_setting @ plugins/platforms/google_chat/adapter.py:_int_setting */
static json_t *gca__int_setting(gca_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    const char *key = json_get_str(args, "key", "");
    const char *env_name = json_get_str(args, "env_name", "");
    const json_t *deflt = json_obj_get(args, "default");
    const json_t *v = (extra && extra->type == JSON_OBJECT) ? json_obj_get(extra, key) : NULL;
    const json_t *chosen = NULL;
    json_t *owned = NULL;
    if (gca_truthy(v)) {
        chosen = v;
    } else {
        char *ds = gca_str_of(deflt);
        json_t *sa = json_object();
        json_set_hdr(sa, "name", env_name);
        json_set_hdr(sa, "default", ds ? ds : "");
        free(ds);
        json_t *r = gca_call(ctx, "get_scoped_secret", sa);
        if (gca_is_raise(r)) {
            const char *kind = json_get_str(r, "__raise__", "");
            if (strcmp(kind, "ValueError") == 0 || strcmp(kind, "TypeError") == 0) {
                json_free(r);
                return deflt ? json_copy(deflt) : json_null();
            }
            return r;
        }
        chosen = r; owned = r;
    }
    long long iv = 0;
    int rc = gca_int_of(chosen, &iv);
    if (owned) json_free(owned);
    if (rc == 0) return json_number((double)iv);
    return deflt ? json_copy(deflt) : json_null();  /* ValueError / TypeError -> default */
}

/* PoP: gca__str_setting @ plugins/platforms/google_chat/adapter.py:_str_setting */
static json_t *gca__str_setting(gca_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    const char *key = json_get_str(args, "key", "");
    const char *env_name = json_get_str(args, "env_name", "");
    const json_t *fallback = json_obj_get(args, "fallback");
    json_t *fb_owned = NULL;
    if (!fallback || fallback->type == JSON_NULL) {
        fb_owned = json_string("");  /* live signature default fallback="" */
        fallback = fb_owned;
    }
    const json_t *v = (extra && extra->type == JSON_OBJECT) ? json_obj_get(extra, key) : NULL;
    const json_t *chosen = NULL;
    json_t *owned = NULL;
    if (gca_truthy(v)) {
        chosen = v;
    } else {
        json_t *sa = json_object();
        json_set_hdr(sa, "name", env_name);
        json_set_hdr(sa, "default", "");
        json_t *r = gca_call(ctx, "get_scoped_secret", sa);
        if (gca_is_raise(r)) { json_free(fb_owned); return r; }  /* no try: propagate */
        if (gca_truthy(r)) { chosen = r; owned = r; }
        else { json_free(r); chosen = fallback; }
    }
    const char *cs = gca_str(chosen);
    if (cs) {
        char *t = gca_strip(cs);
        json_t *out = json_string(t);
        free(t);
        if (owned) json_free(owned);
        json_free(fb_owned);
        return out;
    }
    char buf[128];
    snprintf(buf, sizeof buf, "'%s' object has no attribute 'strip'", gca_type_name(chosen));
    if (owned) json_free(owned);
    json_free(fb_owned);
    return gca_err("AttributeError", buf);
}

/* PoP: gca__extract_message_payload @ plugins/platforms/google_chat/adapter.py:_extract_message_payload */
static json_t *gca__extract_message_payload(gca_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *env = json_obj_get(args, "envelope");
    if (!env || env->type != JSON_OBJECT) env = NULL;
    /* (envelope.get("chat") or {}).get("messagePayload") */
    const json_t *chat = env ? json_obj_get(env, "chat") : NULL;
    const json_t *wrapper = NULL;
    if (gca_truthy(chat) && chat->type == JSON_OBJECT)
        wrapper = json_obj_get(chat, "messagePayload");
    if (gca_truthy(wrapper) && wrapper->type == JSON_OBJECT) {
        const json_t *m = json_obj_get(wrapper, "message");
        json_t *msg = gca_truthy(m) ? json_copy(m) : json_object();
        const json_t *sp = json_obj_get(wrapper, "space");
        const json_t *msp = json_obj_get(msg, "space");
        json_t *space = gca_truthy(sp) ? json_copy(sp)
                      : (gca_truthy(msp) ? json_copy(msp) : json_object());
        json_t *out = json_array();
        json_append(out, msg);
        json_append(out, space);
        json_append(out, json_string("workspace_addons"));
        return out;
    }
    const json_t *message = env ? json_obj_get(env, "message") : NULL;
    if (message && message->type == JSON_OBJECT) {
        const char *type = env ? json_get_str(env, "type", "") : "";
        if (strcmp(type, "MESSAGE") != 0) return NULL;
        json_t *msg = json_copy(message);
        const json_t *sp = json_obj_get(env, "space");
        const json_t *msp = json_obj_get(msg, "space");
        json_t *space = gca_truthy(sp) ? json_copy(sp)
                      : (gca_truthy(msp) ? json_copy(msp) : json_object());
        json_t *out = json_array();
        json_append(out, msg);
        json_append(out, space);
        json_append(out, json_string("native_chat_api"));
        return out;
    }
    bool relay = env && (json_has(env, "event_type") || json_has(env, "sender_email"));
    if (relay) {
        const char *et = json_get_str(env, "event_type", "MESSAGE");
        if (strcmp(et, "MESSAGE") != 0) return NULL;
        const json_t *se = json_obj_get(env, "sender_email");
        char *sender_email = gca_strip(gca_truthy(se) ? gca_str(se) : "");
        const json_t *tx = json_obj_get(env, "text");
        const char *text = (gca_truthy(tx) && gca_str(tx)) ? gca_str(tx) : "";
        const json_t *st = json_obj_get(env, "sender_type");
        char *st_raw = gca_str_of(gca_truthy(st) ? st : NULL);
        if (!gca_truthy(st)) { free(st_raw); st_raw = gca_strdup("HUMAN"); }
        char *st_strip = gca_strip(st_raw); free(st_raw);
        char *sender_type = gca_upper(st_strip); free(st_strip);
        /* surrogate name */
        const char *base = sender_email[0] ? sender_email : "unknown";
        char *r1 = gca_replace(base, "@", "_at_");
        char *r2 = gca_replace(r1, ".", "_"); free(r1);
        size_t nn = strlen("users/relay-") + strlen(r2) + 1;
        char *surrogate = malloc(nn);
        if (surrogate) snprintf(surrogate, nn, "users/relay-%s", r2);
        free(r2);
        const json_t *sdn = json_obj_get(env, "sender_display_name");
        const char *display = gca_truthy(sdn) ? gca_str(sdn)
                            : (sender_email[0] ? sender_email : "Unknown");
        const char *stype = (strcmp(sender_type, "HUMAN") == 0 || strcmp(sender_type, "BOT") == 0)
                          ? sender_type : "HUMAN";
        json_t *sender = json_object();
        json_set_hdr(sender, "name", surrogate ? surrogate : "");
        json_set_hdr(sender, "email", sender_email);
        json_set_hdr(sender, "displayName", display ? display : "");
        json_set_hdr(sender, "type", stype);
        free(surrogate); free(sender_type);
        const json_t *mn = json_obj_get(env, "message_name");
        json_t *msg = json_object();
        json_set_hdr(msg, "name", (gca_truthy(mn) && gca_str(mn)) ? gca_str(mn) : "");
        json_set(msg, "sender", sender);
        json_set_hdr(msg, "text", text);
        json_set_hdr(msg, "argumentText", text);
        const json_t *tn = json_obj_get(env, "thread_name");
        if (gca_truthy(tn)) {
            json_t *th = json_object();
            json_set(th, "name", json_copy(tn));
            json_set(msg, "thread", th);
        }
        free(sender_email);
        const json_t *sn = json_obj_get(env, "space_name");
        const json_t *stp = json_obj_get(env, "space_type");
        json_t *space = json_object();
        json_set_hdr(space, "name", (gca_truthy(sn) && gca_str(sn)) ? gca_str(sn) : "");
        json_set_hdr(space, "spaceType", (gca_truthy(stp) && gca_str(stp)) ? gca_str(stp) : "SPACE");
        json_t *out = json_array();
        json_append(out, msg);
        json_append(out, space);
        json_append(out, json_string("relay_flat"));
        return out;
    }
    return NULL;
}

/* PoP: gca__prepare_inbound @ plugins/platforms/google_chat/adapter.py:_prepare_inbound */
static json_t *gca__prepare_inbound(gca_ctx_t *ctx, const json_t *args) {
    const json_t *env = json_obj_get(args, "envelope");
    const json_t *ce_type = json_obj_get(args, "ce_type");
    bool ce_present = !gca_is_null(ce_type);
    const char *cet = gca_str(ce_type);
    json_t *ea = json_object();
    json_set(ea, "envelope", env ? json_copy(env) : json_null());
    json_set_hdr(ea, "ce_type", (gca_truthy(ce_type) && cet) ? cet : "");
    json_t *extracted = gca_call(ctx, "_extract_message_payload", ea);
    if (gca_is_raise(extracted)) return extracted;
    if (gca_is_null(extracted)) {
        json_free(extracted);
        if (ce_present) {
            json_t *keys = json_array();
            if (env && env->type == JSON_OBJECT)
                for (size_t i = 0; i < env->c.count; i++)
                    json_append(keys, json_string(env->c.keys[i]));
            json_t *la = json_object();
            json_set_hdr(la, "fmt", "[GoogleChat] Envelope did not match a known message format; ce-type=%s, keys=%s");
            json_t *aa = json_array();
            json_append(aa, ce_type ? json_copy(ce_type) : json_null());
            json_append(aa, keys);
            json_set(la, "args", aa);
            json_t *lr = gca_call(ctx, "logger.debug", la);
            if (gca_is_raise(lr)) return lr;
            json_free(lr);
        }
        return NULL;
    }
    json_t *msg, *space, *fmt;
    json_t *bad = gca_unpack3(extracted, &msg, &space, &fmt);
    if (bad) { json_free(extracted); return bad; }
    /* self-filter */
    const json_t *sender = json_obj_get(msg, "sender");
    const json_t *stype = (gca_truthy(sender) && sender->type == JSON_OBJECT)
                        ? json_obj_get(sender, "type") : NULL;
    if (gca_str_eq(stype, "BOT")) { json_free(extracted); return NULL; }
    const json_t *mn = json_obj_get(msg, "name");
    const char *msg_name = (gca_truthy(mn) && gca_str(mn)) ? gca_str(mn) : "";
    if (msg_name[0]) {
        json_t *da = json_object();
        json_set_hdr(da, "name", msg_name);
        json_t *dup = gca_call(ctx, "dedup.is_duplicate", da);
        if (gca_is_raise(dup)) { json_free(extracted); return dup; }
        bool isdup = gca_truthy(dup);
        json_free(dup);
        if (isdup) {
            if (ce_present) {
                json_t *la = json_object();
                json_set_hdr(la, "fmt", "[GoogleChat] Dedup drop for %s");
                json_t *aa = json_array();
                json_append(aa, json_string(msg_name));
                json_set(la, "args", aa);
                json_t *lr = gca_call(ctx, "logger.debug", la);
                if (gca_is_raise(lr)) { json_free(extracted); return lr; }
                json_free(lr);
            }
            json_free(extracted);
            return NULL;
        }
    }
    json_t *mws = json_copy(msg);
    json_t *ee = env ? json_copy(env) : json_object();
    if (!json_has(mws, "space") && gca_truthy(space)) json_set(mws, "space", json_copy(space));
    if (!json_has(ee, "space") && gca_truthy(space)) json_set(ee, "space", json_copy(space));
    json_free(extracted);
    json_t *out = json_array();
    json_append(out, mws);
    json_append(out, ee);
    return out;
}

/* PoP: gca__resolve_thread_id @ plugins/platforms/google_chat/adapter.py:_resolve_thread_id */
static json_t *gca__resolve_thread_id(gca_ctx_t *ctx, const json_t *args) {
    const json_t *reply_to = json_obj_get(args, "reply_to");
    const json_t *metadata = json_obj_get(args, "metadata");
    const json_t *chat_id = json_obj_get(args, "chat_id");
    if (gca_truthy(metadata) && metadata->type == JSON_OBJECT) {
        static const char *keys[] = {"thread_id", "thread_name", "thread_ts"};
        for (size_t i = 0; i < 3; i++) {
            const json_t *v = json_obj_get(metadata, keys[i]);
            if (gca_truthy(v)) {
                char *s = gca_str_of(v);
                json_t *out = json_string(s ? s : "");
                free(s);
                return out;
            }
        }
    }
    const char *rt = gca_str(reply_to);
    if (gca_truthy(reply_to) && rt && gca_contains(rt, "/threads/") && !gca_contains(rt, "/messages/"))
        return json_copy(reply_to);
    if (gca_truthy(metadata) && metadata->type == JSON_OBJECT) {
        const json_t *job = json_obj_get(metadata, "job_id");
        if (gca_truthy(job)) return NULL;
    }
    if (gca_truthy(chat_id)) {
        const json_t *lit = gca_state(ctx, "_last_inbound_thread");
        const char *cid = gca_str(chat_id);
        const json_t *v = (lit && lit->type == JSON_OBJECT && cid) ? json_obj_get(lit, cid) : NULL;
        return gca_truthy(v) ? json_copy(v) : NULL;
    }
    return NULL;
}
/* PoP: gca__note_rate_limit @ plugins/platforms/google_chat/adapter.py:_note_rate_limit */
static json_t *gca__note_rate_limit(gca_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    json_t *hits = gca_state_obj(ctx, "_rate_limit_hits");
    double prev = json_get_num(hits, chat_id, 0);
    double nv = prev + 1;
    json_set(hits, chat_id, json_number(nv));
    return json_number(nv);
}

/* PoP: gca__invalidate_user_creds @ plugins/platforms/google_chat/adapter.py:_invalidate_user_creds */
static json_t *gca__invalidate_user_creds(gca_ctx_t *ctx, const json_t *args) {
    const json_t *identity = json_obj_get(args, "identity");
    if (!gca_truthy(identity)) return NULL;
    if (gca_str_eq(identity, GCA_LEGACY_USER_IDENTITY)) {
        json_set(ctx->state, "_user_credentials", json_null());
        json_set(ctx->state, "_user_chat_api", json_null());
        return NULL;
    }
    const char *id = gca_str(identity);
    if (id) {
        json_t *creds = gca_state_obj(ctx, "_user_creds_by_email");
        json_obj_del(creds, id);
        json_t *api = gca_state_obj(ctx, "_user_chat_api_by_email");
        json_obj_del(api, id);
    }
    return NULL;
}

/* PoP: gca_stop_typing @ plugins/platforms/google_chat/adapter.py:stop_typing */
static json_t *gca_stop_typing(gca_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    json_t *tm = gca_state_obj(ctx, "_typing_messages");
    const json_t *v = json_obj_get(tm, chat_id);
    if (gca_str_eq(v, GCA_TYPING_CONSUMED_SENTINEL))
        json_obj_del(tm, chat_id);
    return NULL;
}

/* PoP: gca__track_outbound_thread @ plugins/platforms/google_chat/adapter.py:_track_outbound_thread */
static json_t *gca__track_outbound_thread(gca_ctx_t *ctx, const json_t *args) {
    const json_t *chat_id = json_obj_get(args, "chat_id");
    const json_t *resp = json_obj_get(args, "resp");
    const json_t *thread = (resp && resp->type == JSON_OBJECT) ? json_obj_get(resp, "thread") : NULL;
    const json_t *name = (gca_truthy(thread) && thread->type == JSON_OBJECT)
                       ? json_obj_get(thread, "name") : NULL;
    const char *resp_thread = (gca_truthy(name) && gca_str(name)) ? gca_str(name) : "";
    if (gca_truthy(chat_id) && resp_thread[0]) {
        json_t *ia = json_object();
        json_set(ia, "chat_id", json_copy(chat_id));
        json_set_hdr(ia, "thread_name", resp_thread);
        json_t *r = gca_call(ctx, "thread_count_store.incr", ia);
        if (gca_is_raise(r)) {
            if (gca_raise_passes(r)) return r;
            json_free(r);
            json_t *la = json_object();
            json_set_hdr(la, "fmt", "[GoogleChat] outbound thread-count incr failed");
            json_set(la, "args", json_array());
            json_t *lr = gca_call(ctx, "logger.debug", la);
            if (gca_is_raise(lr)) return lr;
            json_free(lr);
        } else {
            json_free(r);
        }
    }
    return NULL;
}

/* PoP: gca__release_unadmitted @ plugins/platforms/google_chat/adapter.py:_release_unadmitted */
static json_t *gca__release_unadmitted(gca_ctx_t *ctx, const json_t *args) {
    const json_t *msg = json_obj_get(args, "msg");
    const json_t *env = json_obj_get(args, "envelope");
    const json_t *mn = (msg && msg->type == JSON_OBJECT) ? json_obj_get(msg, "name") : NULL;
    const char *name = (gca_truthy(mn) && gca_str(mn)) ? gca_str(mn) : "";
    json_t *da = json_object();
    json_set_hdr(da, "name", name);
    json_t *dr = gca_call(ctx, "dedup.discard", da);
    if (gca_is_raise(dr)) return dr;
    json_free(dr);
    /* (envelope.get("space") or msg.get("space") or {}).get("name") or "" */
    const json_t *esp = (env && env->type == JSON_OBJECT) ? json_obj_get(env, "space") : NULL;
    const json_t *msp = (msg && msg->type == JSON_OBJECT) ? json_obj_get(msg, "space") : NULL;
    const json_t *sp = gca_truthy(esp) ? esp : (gca_truthy(msp) ? msp : NULL);
    const json_t *sn = (sp && sp->type == JSON_OBJECT) ? json_obj_get(sp, "name") : NULL;
    const char *space_name = (gca_truthy(sn) && gca_str(sn)) ? gca_str(sn) : "";
    const json_t *th = (msg && msg->type == JSON_OBJECT) ? json_obj_get(msg, "thread") : NULL;
    const json_t *tn = (gca_truthy(th) && th->type == JSON_OBJECT) ? json_obj_get(th, "name") : NULL;
    const char *thread_name = (gca_truthy(tn) && gca_str(tn)) ? gca_str(tn) : "";
    if (space_name[0] && thread_name[0]) {
        json_t *ca = json_object();
        json_set_hdr(ca, "chat_id", space_name);
        json_set_hdr(ca, "thread_name", thread_name);
        json_t *cr = gca_call(ctx, "thread_count_store.decr", ca);
        if (gca_is_raise(cr)) return cr;
        json_free(cr);
    }
    return NULL;
}

/* PoP: gca__connect_failed @ plugins/platforms/google_chat/adapter.py:_connect_failed */
static json_t *gca__connect_failed(gca_ctx_t *ctx, const json_t *args) {
    const json_t *log_fmt = json_obj_get(args, "log_fmt");
    const json_t *exc = json_obj_get(args, "exc");
    const json_t *code = json_obj_get(args, "code");
    const json_t *retryable = json_obj_get(args, "retryable");
    json_t *ea = json_object();
    json_set(ea, "exc", exc ? json_copy(exc) : json_null());
    json_set(ea, "targets", json_array());
    json_t *msg = gca_call(ctx, "_exc_text", ea);
    if (gca_is_raise(msg)) return msg;
    json_t *la = json_object();
    json_set(la, "fmt", log_fmt ? json_copy(log_fmt) : json_null());
    json_t *aa = json_array();
    json_append(aa, json_copy(msg));
    json_set(la, "args", aa);
    json_t *lr = gca_call(ctx, "logger.error", la);
    if (gca_is_raise(lr)) { json_free(msg); return lr; }
    json_free(lr);
    json_t *fa = json_object();
    json_set(fa, "code", code ? json_copy(code) : json_null());
    json_set(fa, "message", json_copy(msg));
    json_set(fa, "retryable", retryable ? json_copy(retryable) : json_null());
    json_free(msg);
    json_t *fr = gca_call(ctx, "_set_fatal_error", fa);
    if (gca_is_raise(fr)) return fr;
    json_free(fr);
    return json_bool(false);
}

/* PoP: gca__chunk_text @ plugins/platforms/google_chat/adapter.py:_chunk_text */
static json_t *gca__chunk_text(gca_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = gca_str(json_obj_get(args, "text"));
    json_t *chunks = json_array();
    char *remaining = gca_strdup(text ? text : "");
    while (remaining && remaining[0]) {
        long long len = (long long)gca_utf8_len(remaining);
        if (len <= GCA_MAX_TEXT_LENGTH) {
            json_append(chunks, json_string(remaining));
            break;
        }
        long long cut = gca_rfind_char(remaining, '\n', GCA_MAX_TEXT_LENGTH);
        if (cut < GCA_MAX_TEXT_LENGTH / 2) cut = GCA_MAX_TEXT_LENGTH;
        char *head = gca_slice_head(remaining, cut);
        json_append(chunks, json_string(head));
        free(head);
        char *tail = gca_slice_tail(remaining, cut);
        char *next = gca_lstrip(tail);
        free(tail);
        free(remaining);
        remaining = next;
    }
    free(remaining);
    return chunks;
}

/* project a raise spec into an exc double for _exc_text. */
static json_t *gca_exc_from_raise(const json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "cls", json_get_str(spec, "__raise__", "Exception"));
    json_set_hdr(e, "msg", json_get_str(spec, "message", ""));
    json_set(e, "attrs", json_object());
    return e;
}

/* PoP: gca_verify_http_event_request @ plugins/platforms/google_chat/adapter.py:verify_http_event_request */
static json_t *gca_verify_http_event_request(gca_ctx_t *ctx, const json_t *args) {
    const json_t *aud = gca_state(ctx, "_http_events_audience");
    const json_t *email = gca_state(ctx, "_http_events_service_account_email");
    json_t *pair = json_array();
    if (!gca_truthy(aud) || !gca_truthy(email)) {
        json_append(pair, json_bool(false));
        json_append(pair, json_string("google_chat_http_events_not_configured"));
        return pair;
    }
    const char *auth = gca_str(json_obj_get(args, "auth_header"));
    char *token = NULL;
    if (auth && gca_starts_with(auth, "Bearer ")) {
        char *tail = gca_slice_tail(auth, 7);
        token = gca_strip(tail);
        free(tail);
    } else {
        token = gca_strdup("");
    }
    if (!token[0]) {
        free(token);
        json_append(pair, json_bool(false));
        json_append(pair, json_string("missing_google_bearer"));
        return pair;
    }
    json_t *va = json_object();
    json_set_hdr(va, "token", token);
    json_set(va, "audience", json_copy(aud));
    json_t *claims = gca_call(ctx, "_verify_google_id_token", va);
    free(token);
    if (gca_is_raise(claims)) {
        if (gca_raise_passes(claims)) return claims;
        json_t *exc = gca_exc_from_raise(claims);
        json_free(claims);
        json_t *ea = json_object();
        json_set(ea, "exc", exc);
        json_set(ea, "targets", json_array());
        json_t *shown = gca_call(ctx, "_exc_text", ea);
        if (gca_is_raise(shown)) return shown;
        json_t *la = json_object();
        json_set_hdr(la, "fmt", "[GoogleChat] HTTP event bearer verification failed: %s");
        json_t *aa = json_array();
        json_append(aa, json_copy(shown));
        json_set(la, "args", aa);
        json_free(shown);
        json_t *lr = gca_call(ctx, "logger.warning", la);
        if (gca_is_raise(lr)) return lr;
        json_free(lr);
        json_append(pair, json_bool(false));
        json_append(pair, json_string("invalid_google_bearer"));
        return pair;
    }
    /* str(claims.get("email") or "").strip().lower() */
    const json_t *em = (claims && claims->type == JSON_OBJECT) ? json_obj_get(claims, "email") : NULL;
    char *em_str = gca_str_of(gca_truthy(em) ? em : NULL);
    if (!gca_truthy(em)) { free(em_str); em_str = gca_strdup(""); }
    char *em_strip = gca_strip(em_str); free(em_str);
    char *em_low = gca_lower(em_strip); free(em_strip);
    /* expected set from email.split(",") */
    const char *email_s = gca_str(email);
    bool found = false;
    if (email_s) {
        const char *p = email_s;
        while (*p) {
            const char *comma = strchr(p, ',');
            size_t seglen = comma ? (size_t)(comma - p) : strlen(p);
            char *seg = malloc(seglen + 1);
            if (seg) { memcpy(seg, p, seglen); seg[seglen] = '\0'; }
            char *seg_strip = gca_strip(seg ? seg : "");
            free(seg);
            if (seg_strip[0]) {
                char *seg_low = gca_lower(seg_strip);
                if (strcmp(seg_low, em_low) == 0) found = true;
                free(seg_low);
            }
            free(seg_strip);
            if (!comma) break;
            p = comma + 1;
        }
    }
    free(em_low);
    json_free(claims);
    if (!found) {
        json_append(pair, json_bool(false));
        json_append(pair, json_string("unexpected_google_bearer_identity"));
    } else {
        json_append(pair, json_bool(true));
        json_append(pair, json_string(""));
    }
    return pair;
}

/* PoP: gca__load_cached_bot_id @ plugins/platforms/google_chat/adapter.py:_load_cached_bot_id */
static json_t *gca__load_cached_bot_id(gca_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *path = gca_call(ctx, "_bot_id_cache_path", json_object());
    if (gca_is_raise(path)) {
        const char *kind = json_get_str(path, "__raise__", "");
        if (gca_is_oserror_type(kind) || strcmp(kind, "JSONDecodeError") == 0) {
            json_free(path); return NULL;
        }
        return path;
    }
    const char *path_s = gca_str(path);
    json_t *ra = json_object();
    json_set_hdr(ra, "path", path_s ? path_s : "");
    json_set_hdr(ra, "encoding", "utf-8-sig");
    json_t *raw = gca_call(ctx, "bot_id_cache.read_text", ra);
    json_free(path);
    if (gca_is_raise(raw)) {
        const char *kind = json_get_str(raw, "__raise__", "");
        if (gca_is_oserror_type(kind) || strcmp(kind, "JSONDecodeError") == 0) {
            json_free(raw); return NULL;
        }
        return raw;
    }
    const char *text = gca_str(raw);
    if (!text) text = "";
    /* strip ONE leading U+FEFF BOM (EF BB BF) */
    if ((unsigned char)text[0] == 0xef && (unsigned char)text[1] == 0xbb && (unsigned char)text[2] == 0xbf)
        text += 3;
    json_t *parsed = json_parse(text, NULL);
    json_free(raw);
    if (!parsed) return NULL;  /* json.JSONDecodeError -> caught -> None */
    if (parsed->type == JSON_OBJECT) {
        json_t *v = json_obj_get(parsed, "bot_user_id");
        json_t *out = gca_truthy(v) ? json_copy(v) : NULL;
        json_free(parsed);
        return out;
    }
    char buf[128];
    snprintf(buf, sizeof buf, "'%s' object has no attribute 'get'", gca_type_name(parsed));
    json_free(parsed);
    return gca_err("AttributeError", buf);
}

/* PoP: gca__log_background_failure @ plugins/platforms/google_chat/adapter.py:_log_background_failure */
static json_t *gca__log_background_failure(gca_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *r = gca_call(ctx, "future.result", json_object());
    if (gca_is_raise(r)) {
        if (gca_raise_passes(r)) return r;
        json_free(r);
        json_t *la = json_object();
        json_set_hdr(la, "fmt", "[GoogleChat] Background inbound processing failed");
        json_set(la, "args", json_array());
        json_t *lr = gca_call(ctx, "logger.exception", la);
        if (gca_is_raise(lr)) return lr;
        json_free(lr);
    } else {
        json_free(r);
    }
    return NULL;
}

/* PoP: gca__loop_accepts_callbacks @ plugins/platforms/google_chat/adapter.py:_loop_accepts_callbacks */
static json_t *gca__loop_accepts_callbacks(gca_ctx_t *ctx, const json_t *args) {
    const json_t *loop = json_obj_get(args, "loop");
    if (gca_is_null(loop)) return json_bool(false);
    const json_t *attrs = gca_obj_attrs(loop);
    if (attrs && json_has(attrs, "is_closed")) {
        json_t *r = gca_call(ctx, "loop.is_closed", json_object());
        if (gca_is_raise(r)) return r;
        bool closed = gca_truthy(r);
        json_free(r);
        return json_bool(!closed);
    }
    return json_bool(true);  /* default lambda: False -> not -> True */
}