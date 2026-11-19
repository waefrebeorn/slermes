/* learning_graph_flow.c — FLOW + leaf port of agent/learning_graph.py.
 *
 * Ports the UNCLAIMED leaves (_fm_field, memory_fingerprint, memory_node_id,
 * _has_learning_signal) with PoP credit, plus the claimed siblings (_related,
 * _tokenize, build_edges, density_stats, _to_int_ts, _memory_skill_edges,
 * build_learning_graph) as UNCLAIMED internal helpers (no PoP re-claim — they
 * are already credited to src/agent/learning_graph.c / learning_graph_helpers.c,
 * the stale watercup-MISMATCH port this flow supersedes behaviourally). The
 * heavy-I/O claimed siblings (_load_usage, build_skill_nodes, _memory_cards)
 * are INJECTED through the universal seam, never re-implemented here.
 *
 * Every foreign sub-call and every CLAIMED sibling is a seam:
 *   io->call(io, name, args) appends {"name","args"} to io->trace and returns
 *   the scripted result (NULL/json_null == None; {"__raise__":cls,"message":m}
 *   == a raised exception). Unclaimed siblings ported here (_fm_field,
 *   memory_fingerprint, memory_node_id, _has_learning_signal) run INLINE.
 *   Claimed siblings ported here as helpers (_related, _tokenize, build_edges,
 *   density_stats, _memory_skill_edges) are SEAMED when composed and driven
 *   REAL by their own direct cases.
 *
 * Self-contained: libjson only (hermes_json.h) + libc. No filesystem, no
 * libyaml/libskillusage. SHA-256 is transcribed inline for memory_fingerprint.
 *
 * Representation contract (shared with tests/oracle_learning_graph_flow.py):
 *  - SkillNode double -> {"name","category","source","timestamp","use_count",
 *    "state","created_by","pinned","related"} (all nine fields; the oracle
 *    materializes to the real extracted SkillNode dataclass).
 *  - memory card double -> {"source","timestamp","title","body","fingerprint"}.
 *  - _tokenize ret -> SORTED token array (live returns a set; both sides sort).
 *    The _tokenize seam RESULT is a token array materialized to a set on the
 *    live side so `tokens & text_tokens` works.
 *  - build_edges / _memory_skill_edges ret -> array of 2-element [a,b] arrays
 *    (live returns list of tuples).
 *  - density_stats top_categories -> array of [category,count] arrays.
 *  - Integral floats normalize to ints on both sides; fixtures avoid
 *    integral-float JSON literals and non-finite floats. ASCII scope for
 *    strip/lstrip/lower/len/slice; the ellipsis "…" in _memory_cards titles is
 *    a verbatim UTF-8 constant (only reached via the seamed _memory_cards).
 *  - _to_int_ts fixture domain: None, int, non-integral float, plain
 *    decimal/scientific numeric strings, and ISO date/datetime (naive, Z, or
 *    +HH:MM offset). No underscores/hex/inf/nan literals, no >2^53 magnitudes.
 *
 * Seam name table (name -> live call):
 *   _tokenize            _tokenize(text)
 *   project_root_str     hermes_cli._startup_fast.project_root_str()
 *   get_hermes_home      hermes_constants.get_hermes_home()  (-> POSIX str)
 *   build_skill_nodes    build_skill_nodes(roots)            (claimed, seamed)
 *   build_edges          build_edges(nodes)                  (claimed, seamed)
 *   _memory_cards        _memory_cards()                     (claimed, seamed)
 *   _memory_skill_edges  _memory_skill_edges(cards, skills)  (claimed, seamed)
 *   density_stats        density_stats(nodes, edges)         (claimed, seamed)
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state
 * are harness-owned. lg_call copies args into the trace; the caller frees args.
 */
#ifndef LEARNING_GRAPH_FLOW_C
#define LEARNING_GRAPH_FLOW_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include "hermes_json.h"

typedef struct lg_io_t {
    json_t *(*call)(struct lg_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} lg_io_t;

/* ── universal seam ────────────────────────────────────────────────────── */
static json_t *lg_call(lg_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    if (io && io->trace && io->trace->type == JSON_ARRAY) json_append(io->trace, ev);
    else json_free(ev);
    json_t *r = (io && io->call) ? io->call(io, name, args) : NULL;
    return r ? r : json_null();
}

static bool lg_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *lg_exc_class(const json_t *r) {
    if (!lg_raised(r)) return "";
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}

/* Full builtin exception hierarchy (child -> parent). Used to decide whether a
 * scripted raise is caught by `except Exception` (descends from Exception) or
 * passes through (BaseException-only trio / BaseException itself). */
typedef struct { const char *child; const char *parent; } lg_exc_edge_t;
static const lg_exc_edge_t LG_EXC_PARENTS[] = {
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"Exception", "BaseException"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"UserWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {NULL, NULL}
};

static const char *lg_exc_parent(const char *cls) {
    for (size_t i = 0; LG_EXC_PARENTS[i].child; i++)
        if (strcmp(LG_EXC_PARENTS[i].child, cls) == 0) return LG_EXC_PARENTS[i].parent;
    return NULL;
}

/* True if cls descends from Exception (caught by `except Exception`). Unknown
 * classes default to Exception-descended (the oracle synthesizes them as
 * Exception subclasses); BaseException itself and the base-only trio do not. */
static bool lg_exc_is_exception(const char *cls) {
    if (!cls || !*cls) return false;
    if (strcmp(cls, "BaseException") == 0) return false;
    const char *cur = cls;
    for (int depth = 0; cur && depth < 64; depth++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        const char *p = lg_exc_parent(cur);
        if (!p) return true;  /* unknown leaf: assume Exception subclass */
        cur = p;
    }
    return true;
}

/* BaseException-only raise passes through `except Exception` sites. */
static bool lg_is_base_exc(const json_t *r) {
    return lg_raised(r) && !lg_exc_is_exception(lg_exc_class(r));
}
/* A raise caught by `except Exception`. */
static bool lg_is_exception(const json_t *r) {
    return lg_raised(r) && lg_exc_is_exception(lg_exc_class(r));
}

static json_t *lg_raise(const char *cls, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", cls);
    json_set_hdr(e, "message", msg ? msg : "");
    return e;
}

/* ── truthiness / type predicates ──────────────────────────────────────── */
static bool jtruth(const json_t *v) {
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
static bool j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool j_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool j_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool j_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static json_t *lg_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}
static const char *sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* Python type name of a JSON value (for error messages). */
static const char *lg_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == floor(d) && fabs(d) < 1e15) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* str() of a JSON scalar. Integral doubles render as ints (fixtures use int
 * literals); non-integral floats use the shortest round-trip form (Python
 * repr). Containers are out of fixture scope. */
static char *lg_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (isfinite(x) && x == floor(x) && fabs(x) < 1e16) {
            snprintf(buf, sizeof buf, "%.0f", x);
            return strdup(buf);
        }
        if (!isfinite(x)) return strdup(isnan(x) ? "nan" : (x < 0 ? "-inf" : "inf"));
        for (int prec = 1; prec <= 17; prec++) {
            snprintf(buf, sizeof buf, "%.*g", prec, x);
            if (strtod(buf, NULL) == x) return strdup(buf);
        }
        snprintf(buf, sizeof buf, "%.17g", x);
        return strdup(buf);
    }
    return strdup("");
}

/* ── ASCII string helpers ──────────────────────────────────────────────── */
static bool lg_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}
static char *lg_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && lg_isspace((unsigned char)p[a])) a++;
    while (b > a && lg_isspace((unsigned char)p[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, p + a, b - a);
    o[b - a] = '\0';
    return o;
}
/* str.strip(chars): remove leading/trailing bytes in the char set. */
static char *lg_strip_chars(const char *s, const char *chars) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && strchr(chars, (unsigned char)p[a])) a++;
    while (b > a && strchr(chars, (unsigned char)p[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, p + a, b - a);
    o[b - a] = '\0';
    return o;
}
/* str.lstrip(chars): remove leading bytes in the char set. */
static char *lg_lstrip_chars(const char *s, const char *chars) {
    const char *p = s ? s : "";
    while (*p && strchr(chars, (unsigned char)*p)) p++;
    return strdup(p);
}
static char *lg_lower(const char *s) {
    char *o = strdup(s ? s : "");
    for (char *p = o; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return o;
}
/* s[:n] (ASCII byte slice), n >= 0. */
static char *lg_truncn(const char *s, size_t n) {
    const char *p = s ? s : "";
    size_t l = strlen(p);
    if (n < l) l = n;
    char *o = malloc(l + 1);
    if (!o) return NULL;
    memcpy(o, p, l);
    o[l] = '\0';
    return o;
}
/* str.split(sep) with a single-char separator: keeps empty pieces. */
static json_t *lg_split_char(const char *s, char sep) {
    json_t *arr = json_array();
    const char *p = s ? s : "";
    const char *start = p;
    for (;; p++) {
        if (*p == sep || *p == '\0') {
            size_t n = (size_t)(p - start);
            char *piece = malloc(n + 1);
            memcpy(piece, start, n);
            piece[n] = '\0';
            json_append(arr, json_string(piece));
            free(piece);
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    return arr;
}
/* first line via splitlines()[0]; ASCII boundaries \n \r \v \f \x1c \x1d \x1e
 * (\r\n collapses). Empty string -> no lines -> IndexError (set *err). */
static bool lg_is_linebound(unsigned char c) {
    return c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || c == 0x1c || c == 0x1d || c == 0x1e;
}
static char *lg_first_line(const char *s, json_t **err) {
    *err = NULL;
    const char *p = s ? s : "";
    if (!*p) { *err = lg_raise("IndexError", "list index out of range"); return NULL; }
    size_t n = 0;
    while (p[n] && !lg_is_linebound((unsigned char)p[n])) n++;
    return lg_truncn(p, n);
}

/* ── SHA-256 (FIPS 180-4), inline for memory_fingerprint ───────────────── */
typedef struct { uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t buflen; } lg_sha256_t;
static const uint32_t LG_K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static uint32_t lg_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static void lg_sha256_block(lg_sha256_t *c, const uint8_t *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16)
             | ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = lg_rotr(w[i-15],7) ^ lg_rotr(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = lg_rotr(w[i-2],17) ^ lg_rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = lg_rotr(e,6) ^ lg_rotr(e,11) ^ lg_rotr(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + LG_K256[i] + w[i];
        uint32_t S0 = lg_rotr(a,2) ^ lg_rotr(a,13) ^ lg_rotr(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d; c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}
static void lg_sha256_init(lg_sha256_t *c) {
    c->h[0]=0x6a09e667; c->h[1]=0xbb67ae85; c->h[2]=0x3c6ef372; c->h[3]=0xa54ff53a;
    c->h[4]=0x510e527f; c->h[5]=0x9b05688c; c->h[6]=0x1f83d9ab; c->h[7]=0x5be0cd19;
    c->len = 0; c->buflen = 0;
}
static void lg_sha256_update(lg_sha256_t *c, const uint8_t *data, size_t n) {
    c->len += n;
    while (n) {
        size_t take = 64 - c->buflen;
        if (take > n) take = n;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take; data += take; n -= take;
        if (c->buflen == 64) { lg_sha256_block(c, c->buf); c->buflen = 0; }
    }
}
static void lg_sha256_final(lg_sha256_t *c, uint8_t out[32]) {
    uint64_t bits = c->len * 8;
    uint8_t pad = 0x80;
    lg_sha256_update(c, &pad, 1);
    uint8_t zero = 0;
    while (c->buflen != 56) lg_sha256_update(c, &zero, 1);
    uint8_t lenbe[8];
    for (int i = 0; i < 8; i++) lenbe[i] = (uint8_t)(bits >> (56 - 8*i));
    lg_sha256_update(c, lenbe, 8);
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(c->h[i] >> 24);
        out[i*4+1] = (uint8_t)(c->h[i] >> 16);
        out[i*4+2] = (uint8_t)(c->h[i] >> 8);
        out[i*4+3] = (uint8_t)(c->h[i]);
    }
}
/* hashlib.sha256(b).hexdigest() */
static char *lg_sha256_hex(const uint8_t *data, size_t n) {
    lg_sha256_t c; lg_sha256_init(&c);
    lg_sha256_update(&c, data, n);
    uint8_t dig[32]; lg_sha256_final(&c, dig);
    static const char *hexd = "0123456789abcdef";
    char *o = malloc(65);
    for (int i = 0; i < 32; i++) { o[i*2] = hexd[dig[i] >> 4]; o[i*2+1] = hexd[dig[i] & 15]; }
    o[64] = '\0';
    return o;
}

/* ── _to_int_ts helpers: float() and datetime.fromisoformat() ──────────── */
/* Parse a plain decimal/scientific float (Python float() restricted domain).
 * Returns 1 on success with *out; 0 on ValueError. No underscores/hex/inf/nan. */
static int lg_parse_float(const char *s, double *out) {
    if (!s || !*s) return 0;
    const char *p = s;
    if (*p == '+' || *p == '-') p++;
    int digits = 0;
    while (*p >= '0' && *p <= '9') { p++; digits++; }
    if (*p == '.') { p++; while (*p >= '0' && *p <= '9') { p++; digits++; } }
    if (!digits) return 0;
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        int ed = 0;
        while (*p >= '0' && *p <= '9') { p++; ed++; }
        if (!ed) return 0;
    }
    if (*p != '\0') return 0;
    char *end = NULL;
    double d = strtod(s, &end);
    if (end == s || *end != '\0') return 0;
    *out = d;
    return 1;
}

/* days from civil (Howard Hinnant): epoch 1970-01-01. */
static long long lg_days_from_civil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}
static int lg_is_leap(long long y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}
/* datetime.fromisoformat restricted domain:
 *   YYYY-MM-DD [T| ] HH:MM[:SS[.ffffff]] [Z|+HH:MM|-HH:MM]
 * Naive -> UTC. Returns 1 with epoch seconds *out; 0 on ValueError. */
static int lg_parse_iso(const char *s, long long *out) {
    if (!s) return 0;
    size_t n = strlen(s);
    if (n < 10) return 0;
    int Y, M, D;
    if (s[4] != '-' || s[7] != '-') return 0;
    for (int i = 0; i < 10; i++) if (i != 4 && i != 7 && !(s[i] >= '0' && s[i] <= '9')) return 0;
    Y = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
    M = (s[5]-'0')*10 + (s[6]-'0');
    D = (s[8]-'0')*10 + (s[9]-'0');
    if (M < 1 || M > 12 || D < 1) return 0;
    static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int md = mdays[M-1];
    if (M == 2 && lg_is_leap(Y)) md = 29;
    if (D > md) return 0;
    long long days = lg_days_from_civil(Y, (unsigned)M, (unsigned)D);
    int hh = 0, mm = 0, ss = 0;
    long long off_sec = 0;
    size_t i = 10;
    if (i < n) {
        if (s[i] != 'T' && s[i] != ' ' && s[i] != 't') return 0;
        i++;
        if (i + 5 > n || s[i+2] != ':') return 0;
        if (!(s[i]>='0'&&s[i]<='9'&&s[i+1]>='0'&&s[i+1]<='9'&&s[i+3]>='0'&&s[i+3]<='9'&&s[i+4]>='0'&&s[i+4]<='9')) return 0;
        hh = (s[i]-'0')*10 + (s[i+1]-'0');
        mm = (s[i+3]-'0')*10 + (s[i+4]-'0');
        i += 5;
        if (i < n && s[i] == ':') {
            if (i + 3 > n || !(s[i+1]>='0'&&s[i+1]<='9'&&s[i+2]>='0'&&s[i+2]<='9')) return 0;
            ss = (s[i+1]-'0')*10 + (s[i+2]-'0');
            i += 3;
            if (i < n && s[i] == '.') {
                i++;
                int fd = 0;
                while (i < n && s[i] >= '0' && s[i] <= '9') { i++; fd++; }
                if (!fd) return 0;
            }
        }
        if (hh > 23 || mm > 59 || ss > 59) return 0;
        if (i < n) {
            if (s[i] == 'Z' || s[i] == 'z') { i++; off_sec = 0; }
            else if (s[i] == '+' || s[i] == '-') {
                int sign = (s[i] == '-') ? -1 : 1;
                i++;
                if (i + 5 > n || s[i+2] != ':') return 0;
                if (!(s[i]>='0'&&s[i]<='9'&&s[i+1]>='0'&&s[i+1]<='9'&&s[i+3]>='0'&&s[i+3]<='9'&&s[i+4]>='0'&&s[i+4]<='9')) return 0;
                int oh = (s[i]-'0')*10 + (s[i+1]-'0');
                int om = (s[i+3]-'0')*10 + (s[i+4]-'0');
                if (oh > 23 || om > 59) return 0;
                off_sec = sign * (oh * 3600 + om * 60);
                i += 5;
            } else return 0;
        }
        if (i != n) return 0;
    }
    long long epoch = days * 86400 + hh * 3600 + mm * 60 + ss - off_sec;
    *out = epoch;
    return 1;
}

/* Python round(x, ndigits) for the clean fixture domain (no half-even ties):
 * format with %.*f then reparse. */
static double lg_pyround(double x, int nd) {
    char buf[64];
    snprintf(buf, sizeof buf, "%.*f", nd, x);
    return strtod(buf, NULL);
}

/* ── SkillNode / card field access (defaults match the dataclass) ──────── */
static const char *lg_node_source(const json_t *n) {
    json_t *v = lg_get(n, "source");
    return j_is_str(v) ? sval(v) : "profile";
}

/* ====================================================================== */
/* PoP: lg_fm_field @ agent/learning_graph.py:_fm_field */
/* Top-level key or metadata.hermes.<key>; tolerant of string frontmatter. */
static json_t *lg_fm_field(json_t *fm, const char *key) {
    /* if fm.get(key): return fm[key] */
    json_t *v = lg_get(fm, key);
    if (jtruth(v)) return json_copy(v);
    json_t *meta = lg_get(fm, "metadata");
    json_t *hermes = j_is_obj(meta) ? lg_get(meta, "hermes") : NULL;
    if (j_is_obj(hermes)) {
        json_t *hv = lg_get(hermes, key);
        return hv ? json_copy(hv) : json_null();
    }
    return json_null();
}

/* PoP: lg_memory_fingerprint @ agent/learning_graph.py:memory_fingerprint */
/* sha256(entry.strip().encode("utf-8")).hexdigest()[:12] */
static json_t *lg_memory_fingerprint(const char *entry) {
    char *stripped = lg_strip(entry);
    char *hex = lg_sha256_hex((const uint8_t *)(stripped ? stripped : ""),
                             stripped ? strlen(stripped) : 0);
    free(stripped);
    char *fp = lg_truncn(hex, 12);
    free(hex);
    json_t *r = json_string(fp);
    free(fp);
    return r;
}

/* PoP: lg_memory_node_id @ agent/learning_graph.py:memory_node_id */
/* f"memory:{card['source']}:{index}:{card['fingerprint']}" */
static json_t *lg_memory_node_id(json_t *card, long long index) {
    if (!j_is_obj(card)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not subscriptable", lg_type_name(card));
        return lg_raise("TypeError", msg);
    }
    json_t *src = lg_get(card, "source");
    if (!src) return lg_raise("KeyError", "'source'");
    json_t *fp = lg_get(card, "fingerprint");
    if (!fp) return lg_raise("KeyError", "'fingerprint'");
    char *s_src = lg_pystr(src);
    char *s_fp = lg_pystr(fp);
    size_t need = strlen(s_src) + strlen(s_fp) + 64;
    char *buf = malloc(need);
    snprintf(buf, need, "memory:%s:%lld:%s", s_src, index, s_fp);
    free(s_src); free(s_fp);
    json_t *r = json_string(buf);
    free(buf);
    return r;
}

/* PoP: lg_has_learning_signal @ agent/learning_graph.py:_has_learning_signal */
/* node.source != "external" and (created_by in {"agent","learn"} or use_count>0) */
static json_t *lg_has_learning_signal(json_t *node) {
    const char *source = lg_node_source(node);
    json_t *cb = lg_get(node, "created_by");
    const char *cb_s = j_is_str(cb) ? sval(cb) : NULL;
    bool created = cb_s && (strcmp(cb_s, "agent") == 0 || strcmp(cb_s, "learn") == 0);
    json_t *uc = lg_get(node, "use_count");
    double use_count = j_is_num(uc) ? uc->num_val : 0.0;
    bool sig = (strcmp(source, "external") != 0) && (created || use_count > 0);
    return json_bool(sig);
}

/* ── claimed siblings, ported as UNCLAIMED helpers (no PoP re-claim) ───── */

/* _related(fm): _fm_field inline; str.strip("[]").split(",") / list comp. */
static json_t *lg_related(json_t *fm) {
    json_t *raw = lg_fm_field(fm, "related_skills");
    json_t *list = NULL;
    if (j_is_str(raw)) {
        char *stripped = lg_strip_chars(sval(raw), "[]");
        list = lg_split_char(stripped, ',');
        free(stripped);
    } else if (j_is_arr(raw)) {
        list = json_copy(raw);
    }
    json_free(raw);
    json_t *out = json_array();
    if (list) {
        size_t n = json_len(list);
        for (size_t i = 0; i < n; i++) {
            json_t *r = json_get(list, i);
            char *sr = lg_pystr(r);
            char *t = lg_strip(sr);
            free(sr);
            if (t && t[0]) json_append(out, json_string(t));
            free(t);
        }
        json_free(list);
    }
    return out;
}

/* _tokenize(text): {t for t in re.split(r"[^a-z0-9]+", text.lower()) if len>=3}
 * Returns a SORTED token array (set semantics). ASCII. */
static int lg_tok_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static json_t *lg_tokenize(const char *text) {
    char *low = lg_lower(text);
    /* collect maximal [a-z0-9] runs of length >= 3 */
    char **toks = NULL; size_t ntok = 0, cap = 0;
    const char *p = low;
    while (*p) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) {
            const char *start = p;
            while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) p++;
            size_t len = (size_t)(p - start);
            if (len >= 3) {
                int dup = 0;
                for (size_t i = 0; i < ntok; i++) if (strncmp(toks[i], start, len) == 0 && toks[i][len] == '\0') { dup = 1; break; }
                if (!dup) {
                    if (ntok >= cap) { cap = cap ? cap * 2 : 16; toks = realloc(toks, cap * sizeof(char *)); }
                    toks[ntok] = lg_truncn(start, len);
                    ntok++;
                }
            }
        } else p++;
    }
    free(low);
    if (ntok > 1) qsort(toks, ntok, sizeof(char *), lg_tok_cmp);
    json_t *out = json_array();
    for (size_t i = 0; i < ntok; i++) { json_append(out, json_string(toks[i])); free(toks[i]); }
    free(toks);
    return out;
}

/* build_edges(nodes): undirected related_skills edges, both endpoints present,
 * deduped first-seen order. nodes is a dict name->node. */
static json_t *lg_build_edges(json_t *nodes) {
    json_t *out = json_array();
    if (!j_is_obj(nodes)) return out;
    char **seen = NULL; size_t nseen = 0, scap = 0;
    size_t nn = json_object_size(nodes);
    for (size_t i = 0; i < nn; i++) {
        const char *name = json_object_get_key_at(nodes, i);
        json_t *node = json_object_get_at(nodes, i);
        if (!name || !j_is_obj(node)) continue;
        json_t *rel = lg_get(node, "related");
        if (!j_is_arr(rel)) continue;
        size_t rn = json_len(rel);
        for (size_t r = 0; r < rn; r++) {
            json_t *te = json_get(rel, r);
            if (!j_is_str(te)) continue;
            const char *tgt = sval(te);
            if (strcmp(tgt, name) == 0) continue;
            if (!lg_get(nodes, tgt)) continue;   /* target in nodes */
            const char *a = name, *b = tgt;
            if (strcmp(a, b) > 0) { const char *t = a; a = b; b = t; }
            size_t kl = strlen(a) + strlen(b) + 2;
            char *key = malloc(kl);
            snprintf(key, kl, "%s\x01%s", a, b);
            int dup = 0;
            for (size_t s = 0; s < nseen; s++) if (strcmp(seen[s], key) == 0) { dup = 1; break; }
            if (dup) { free(key); continue; }
            if (nseen >= scap) { scap = scap ? scap * 2 : 16; seen = realloc(seen, scap * sizeof(char *)); }
            seen[nseen++] = key;
            json_t *pair = json_array();
            json_append(pair, json_string(a));
            json_append(pair, json_string(b));
            json_append(out, pair);
        }
    }
    for (size_t s = 0; s < nseen; s++) free(seen[s]);
    free(seen);
    return out;
}

/* density_stats(nodes, edges). nodes dict name->node; edges array of [a,b]. */
static json_t *lg_density_stats(json_t *nodes, json_t *edges) {
    json_t *res = json_object();
    size_t node_count = j_is_obj(nodes) ? json_object_size(nodes) : 0;
    size_t edge_count = j_is_arr(edges) ? json_len(edges) : 0;
    /* linked set */
    char **linked = NULL; size_t nlinked = 0, lcap = 0;
    for (size_t i = 0; i < edge_count; i++) {
        json_t *e = json_get(edges, i);
        if (!j_is_arr(e)) continue;
        size_t en = json_len(e);
        for (size_t p = 0; p < en; p++) {
            json_t *ep = json_get(e, p);
            if (!j_is_str(ep)) continue;
            const char *sv = sval(ep);
            int dup = 0;
            for (size_t j = 0; j < nlinked; j++) if (strcmp(linked[j], sv) == 0) { dup = 1; break; }
            if (!dup) {
                if (nlinked >= lcap) { lcap = lcap ? lcap * 2 : 16; linked = realloc(linked, lcap * sizeof(char *)); }
                linked[nlinked++] = strdup(sv);
            }
        }
    }
    /* categories first-seen, agent_created, used */
    char **cats = NULL; long *counts = NULL; size_t ncat = 0, ccap = 0;
    long agent_created = 0, used = 0;
    for (size_t i = 0; i < node_count; i++) {
        json_t *nd = json_object_get_at(nodes, i);
        if (!j_is_obj(nd)) continue;
        json_t *catv = lg_get(nd, "category");
        const char *cat = j_is_str(catv) ? sval(catv) : "general";
        long idx = -1;
        for (size_t j = 0; j < ncat; j++) if (strcmp(cats[j], cat) == 0) { idx = (long)j; break; }
        if (idx < 0) {
            if (ncat >= ccap) { ccap = ccap ? ccap * 2 : 16; cats = realloc(cats, ccap * sizeof(char *)); counts = realloc(counts, ccap * sizeof(long)); }
            cats[ncat] = strdup(cat); counts[ncat] = 1; ncat++;
        } else counts[idx]++;
        json_t *cb = lg_get(nd, "created_by");
        if (j_is_str(cb) && strcmp(sval(cb), "agent") == 0) agent_created++;
        json_t *uc = lg_get(nd, "use_count");
        if (j_is_num(uc) && uc->num_val > 0) used++;
    }
    long long denom = node_count ? (long long)node_count : 1;
    double epn = (double)edge_count / (double)denom;
    double iso = 100.0 * (double)(denom - (long long)nlinked) / (double)denom;
    json_set(res, "nodes", json_number((double)node_count));
    json_set(res, "related_edges", json_number((double)edge_count));
    json_set(res, "edges_per_node", json_number(lg_pyround(epn, 3)));
    json_set(res, "linked_nodes", json_number((double)nlinked));
    json_set(res, "isolated_pct", json_number(lg_pyround(iso, 1)));
    json_set(res, "categories", json_number((double)ncat));
    json_set(res, "agent_created", json_number((double)agent_created));
    json_set(res, "used", json_number((double)used));
    /* top_categories: stable sort by count desc (first-seen tie), top 8 */
    size_t *order = malloc((ncat ? ncat : 1) * sizeof(size_t));
    for (size_t i = 0; i < ncat; i++) order[i] = i;
    for (size_t i = 0; i < ncat; i++)
        for (size_t j = i + 1; j < ncat; j++)
            if (counts[order[j]] > counts[order[i]]) { size_t t = order[i]; order[i] = order[j]; order[j] = t; }
    json_t *top = json_array();
    size_t limit = ncat < 8 ? ncat : 8;
    for (size_t i = 0; i < limit; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(cats[order[i]]));
        json_append(pair, json_number((double)counts[order[i]]));
        json_append(top, pair);
    }
    json_set(res, "top_categories", top);
    free(order);
    for (size_t i = 0; i < nlinked; i++) free(linked[i]);
    free(linked);
    for (size_t i = 0; i < ncat; i++) free(cats[i]);
    free(cats); free(counts);
    return res;
}

/* _to_int_ts(value): epoch seconds from number/numeric-string/ISO; None else. */
static json_t *lg_to_int_ts(json_t *value) {
    if (j_is_null(value)) return json_null();
    char *sv = lg_pystr(value);
    char *s = lg_strip(sv);
    free(sv);
    if (!s || !s[0]) { free(s); return json_null(); }
    /* isinstance(value, (int, float)) — bool is an int subclass */
    if (j_is_num(value) || j_is_bool(value)) {
        double d = j_is_bool(value) ? (value->bool_val ? 1.0 : 0.0) : value->num_val;
        free(s);
        return json_number(trunc(d));
    }
    if (!j_is_str(value)) { free(s); return json_null(); }  /* containers -> None */
    double f;
    if (lg_parse_float(s, &f)) {
        free(s);
        if (!isfinite(f)) return json_null();   /* int(inf/nan) raises -> None */
        return json_number(trunc(f));
    }
    long long epoch;
    if (lg_parse_iso(s, &epoch)) { free(s); return json_number((double)epoch); }
    free(s);
    return json_null();
}

/* _memory_skill_edges(memory_cards, skills): _tokenize SEAMED (claimed),
 * memory_node_id INLINE (unclaimed). Top-4 lexical overlap per card. */
static json_t *lg_memory_skill_edges(lg_io_t *io, json_t *memory_cards, json_t *skills) {
    json_t *out = json_array();
    size_t scount = j_is_arr(skills) ? json_len(skills) : 0;
    /* skill_meta = [(name, _tokenize(name), name.lower())] — seam per skill */
    char **snames = malloc((scount ? scount : 1) * sizeof(char *));
    char **slower = malloc((scount ? scount : 1) * sizeof(char *));
    char ***stoks = malloc((scount ? scount : 1) * sizeof(char **));
    size_t *stokn = malloc((scount ? scount : 1) * sizeof(size_t));
    size_t sn = 0;
    for (size_t i = 0; i < scount; i++) {
        json_t *sk = json_get(skills, i);
        if (!j_is_obj(sk)) continue;
        json_t *nmv = lg_get(sk, "name");
        if (!j_is_str(nmv)) continue;
        const char *nm = sval(nmv);
        snames[sn] = strdup(nm);
        slower[sn] = lg_lower(nm);
        json_t *ta = json_object();
        json_set_hdr(ta, "text", nm);
        json_t *tokres = lg_call(io, "_tokenize", ta);
        json_free(ta);
        if (lg_raised(tokres)) {
            /* propagate: free partial meta */
            for (size_t k = 0; k < sn; k++) { free(snames[k]); free(slower[k]); for (size_t j=0;j<stokn[k];j++) free(stoks[k][j]); free(stoks[k]); }
            free(snames); free(slower); free(stoks); free(stokn);
            return tokres;
        }
        size_t tn = j_is_arr(tokres) ? json_len(tokres) : 0;
        stoks[sn] = malloc((tn ? tn : 1) * sizeof(char *));
        stokn[sn] = 0;
        for (size_t j = 0; j < tn; j++) {
            json_t *te = json_get(tokres, j);
            if (j_is_str(te)) stoks[sn][stokn[sn]++] = strdup(sval(te));
        }
        json_free(tokres);
        sn++;
    }
    size_t ccount = j_is_arr(memory_cards) ? json_len(memory_cards) : 0;
    json_t *prop = NULL;
    for (size_t ci = 0; ci < ccount && !prop; ci++) {
        json_t *card = json_get(memory_cards, ci);
        if (!j_is_obj(card)) continue;
        const char *title = sval(lg_get(card, "title"));
        const char *body = sval(lg_get(card, "body"));
        size_t tl = strlen(title) + strlen(body) + 2;
        char *text = malloc(tl);
        snprintf(text, tl, "%s\n%s", title, body);
        char *ltext = lg_lower(text);
        free(text);
        json_t *ta = json_object();
        json_set_hdr(ta, "text", ltext);
        json_t *tokres = lg_call(io, "_tokenize", ta);
        json_free(ta);
        if (lg_raised(tokres)) { free(ltext); prop = tokres; break; }
        size_t ttn = j_is_arr(tokres) ? json_len(tokres) : 0;
        char **ttoks = malloc((ttn ? ttn : 1) * sizeof(char *));
        size_t tt_n = 0;
        for (size_t j = 0; j < ttn; j++) {
            json_t *te = json_get(tokres, j);
            if (j_is_str(te)) ttoks[tt_n++] = strdup(sval(te));
        }
        json_free(tokres);
        /* score each skill */
        long *scores = malloc((sn ? sn : 1) * sizeof(long));
        size_t *idxs = malloc((sn ? sn : 1) * sizeof(size_t));
        size_t scored_n = 0;
        for (size_t s = 0; s < sn; s++) {
            long score = 0;
            if (slower[s][0] && strstr(ltext, slower[s])) score += 6;
            for (size_t a = 0; a < stokn[s]; a++)
                for (size_t b = 0; b < tt_n; b++)
                    if (strcmp(stoks[s][a], ttoks[b]) == 0) { score++; break; }
            if (score > 0) { scores[scored_n] = score; idxs[scored_n] = s; scored_n++; }
        }
        /* stable sort by (-score, name asc) */
        for (size_t i = 0; i < scored_n; i++)
            for (size_t j = i + 1; j < scored_n; j++) {
                int swap = 0;
                if (scores[j] > scores[i]) swap = 1;
                else if (scores[j] == scores[i] && strcmp(snames[idxs[j]], snames[idxs[i]]) < 0) swap = 1;
                if (swap) {
                    long ts = scores[i]; scores[i] = scores[j]; scores[j] = ts;
                    size_t ti = idxs[i]; idxs[i] = idxs[j]; idxs[j] = ti;
                }
            }
        size_t lim = scored_n < 4 ? scored_n : 4;
        for (size_t i = 0; i < lim && !prop; i++) {
            json_t *id = lg_memory_node_id(card, (long long)ci);
            if (lg_raised(id)) { prop = id; break; }
            json_t *pair = json_array();
            json_append(pair, id);
            json_append(pair, json_string(snames[idxs[i]]));
            json_append(out, pair);
        }
        free(scores); free(idxs);
        for (size_t j = 0; j < tt_n; j++) free(ttoks[j]);
        free(ttoks);
        free(ltext);
    }
    for (size_t k = 0; k < sn; k++) {
        free(snames[k]); free(slower[k]);
        for (size_t j = 0; j < stokn[k]; j++) free(stoks[k][j]);
        free(stoks[k]);
    }
    free(snames); free(slower); free(stoks); free(stokn);
    if (prop) { json_free(out); return prop; }
    return out;
}

/* ── build_learning_graph: THE FLOW (claimed; unclaimed helper, no PoP) ── */
/* Project a SkillNode double to the canonical nine-field dict (identity here;
 * the oracle materializes to the dataclass and projects back the same way). */
static json_t *lg_node_proj(json_t *node) {
    return node ? json_copy(node) : json_null();
}

static json_t *lg_build_learning_graph(lg_io_t *io) {
    /* roots = [("base", Path(project_root_str())/"skills"),
     *          ("profile", get_hermes_home()/"skills")] */
    json_t *pra = json_object();
    json_t *pr = lg_call(io, "project_root_str", pra);
    json_free(pra);
    if (lg_raised(pr)) return pr;
    json_t *ha = json_object();
    json_t *home = lg_call(io, "get_hermes_home", ha);
    json_free(ha);
    if (lg_raised(home)) { json_free(pr); return home; }
    char *pr_s = lg_pystr(pr);
    char *home_s = lg_pystr(home);
    json_free(pr); json_free(home);
    json_t *roots = json_array();
    json_t *r0 = json_object();
    json_set_hdr(r0, "source", "base");
    { size_t n = strlen(pr_s) + 16; char *p = malloc(n); snprintf(p, n, "%s/skills", pr_s); json_set_hdr(r0, "root", p); free(p); }
    json_append(roots, r0);
    json_t *r1 = json_object();
    json_set_hdr(r1, "source", "profile");
    { size_t n = strlen(home_s) + 16; char *p = malloc(n); snprintf(p, n, "%s/skills", home_s); json_set_hdr(r1, "root", p); free(p); }
    json_append(roots, r1);
    free(pr_s); free(home_s);

    /* learned_skills = {name:node for build_skill_nodes(roots)
     *                   if node.source != "base" and _has_learning_signal(node)} */
    json_t *bsa = json_object();
    json_set(bsa, "roots", roots);   /* bsa owns roots now */
    json_t *all_nodes = lg_call(io, "build_skill_nodes", bsa);
    json_free(bsa);
    if (lg_raised(all_nodes)) return all_nodes;
    json_t *learned = json_object();
    if (j_is_obj(all_nodes)) {
        size_t nn = json_object_size(all_nodes);
        for (size_t i = 0; i < nn; i++) {
            const char *name = json_object_get_key_at(all_nodes, i);
            json_t *node = json_object_get_at(all_nodes, i);
            if (!name || !j_is_obj(node)) continue;
            const char *src = lg_node_source(node);
            if (strcmp(src, "base") == 0) continue;
            json_t *sig = lg_has_learning_signal(node);
            bool keep = j_is_bool(sig) && sig->bool_val;
            json_free(sig);
            if (keep) json_set(learned, name, lg_node_proj(node));
        }
    }
    json_free(all_nodes);

    /* skill_edges, memory_cards = build_edges(learned), _memory_cards() */
    json_t *bea = json_object();
    json_set(bea, "nodes", json_copy(learned));
    json_t *skill_edges = lg_call(io, "build_edges", bea);
    json_free(bea);
    if (lg_raised(skill_edges)) { json_free(learned); return skill_edges; }
    json_t *mca = json_object();
    json_t *memory_cards = lg_call(io, "_memory_cards", mca);
    json_free(mca);
    if (lg_raised(memory_cards)) { json_free(learned); json_free(skill_edges); return memory_cards; }

    /* memory_edges = _memory_skill_edges(memory_cards, list(learned.values())) */
    json_t *skills_arr = json_array();
    size_t ln = json_object_size(learned);
    for (size_t i = 0; i < ln; i++) {
        json_t *nd = json_object_get_at(learned, i);
        json_append(skills_arr, lg_node_proj(nd));
    }
    json_t *msa = json_object();
    json_set(msa, "memory_cards", json_copy(memory_cards));
    json_set(msa, "skills", skills_arr);   /* msa owns skills_arr */
    json_t *memory_edges = lg_call(io, "_memory_skill_edges", msa);
    json_free(msa);
    if (lg_raised(memory_edges)) { json_free(learned); json_free(skill_edges); json_free(memory_cards); return memory_edges; }

    /* clusters = Counter(node.category); if memory_cards: clusters["memory"]=len */
    char **ck = NULL; long *cv = NULL; size_t nck = 0, ccap = 0;
    for (size_t i = 0; i < ln; i++) {
        json_t *nd = json_object_get_at(learned, i);
        json_t *catv = lg_get(nd, "category");
        const char *cat = j_is_str(catv) ? sval(catv) : "general";
        long idx = -1;
        for (size_t j = 0; j < nck; j++) if (strcmp(ck[j], cat) == 0) { idx = (long)j; break; }
        if (idx < 0) {
            if (nck >= ccap) { ccap = ccap ? ccap * 2 : 16; ck = realloc(ck, ccap * sizeof(char *)); cv = realloc(cv, ccap * sizeof(long)); }
            ck[nck] = strdup(cat); cv[nck] = 1; nck++;
        } else cv[idx]++;
    }
    size_t mem_count = j_is_arr(memory_cards) ? json_len(memory_cards) : 0;
    if (mem_count > 0) {
        long idx = -1;
        for (size_t j = 0; j < nck; j++) if (strcmp(ck[j], "memory") == 0) { idx = (long)j; break; }
        if (idx < 0) {
            if (nck >= ccap) { ccap = ccap ? ccap * 2 : 16; ck = realloc(ck, ccap * sizeof(char *)); cv = realloc(cv, ccap * sizeof(long)); }
            ck[nck] = strdup("memory"); cv[nck] = (long)mem_count; nck++;
        } else cv[idx] = (long)mem_count;
    }

    /* graph_nodes = [skill nodes] + [memory nodes] */
    json_t *gnodes = json_array();
    for (size_t i = 0; i < ln; i++) {
        const char *name = json_object_get_key_at(learned, i);
        json_t *nd = json_object_get_at(learned, i);
        if (!name || !j_is_obj(nd)) continue;
        json_t *g = json_object();
        json_set_hdr(g, "id", name);
        json_set_hdr(g, "label", name);
        json_set_hdr(g, "kind", "skill");
        json_t *ts = lg_get(nd, "timestamp"); json_set(g, "timestamp", ts ? json_copy(ts) : json_null());
        json_t *cat = lg_get(nd, "category"); json_set(g, "category", cat ? json_copy(cat) : json_null());
        json_t *uc = lg_get(nd, "use_count"); json_set(g, "useCount", uc ? json_copy(uc) : json_null());
        json_t *st = lg_get(nd, "state"); json_set(g, "state", st ? json_copy(st) : json_null());
        json_t *cb = lg_get(nd, "created_by"); json_set(g, "createdBy", cb ? json_copy(cb) : json_null());
        json_t *pin = lg_get(nd, "pinned"); json_set(g, "pinned", pin ? json_copy(pin) : json_null());
        json_append(gnodes, g);
    }
    json_t *prop = NULL;
    for (size_t i = 0; i < mem_count && !prop; i++) {
        json_t *card = json_get(memory_cards, i);
        json_t *id = lg_memory_node_id(card, (long long)i);
        if (lg_raised(id)) { prop = id; break; }
        json_t *g = json_object();
        json_set(g, "id", id);
        json_t *title = lg_get(card, "title");
        if (!title) { json_free(g); prop = lg_raise("KeyError", "'title'"); break; }
        json_set(g, "label", json_copy(title));
        json_set_hdr(g, "kind", "memory");
        json_t *msrc = lg_get(card, "source"); json_set(g, "memorySource", msrc ? json_copy(msrc) : json_null());
        json_t *mts = lg_get(card, "timestamp"); json_set(g, "timestamp", mts ? json_copy(mts) : json_null());
        json_set_hdr(g, "category", "memory");
        json_set(g, "useCount", json_number(0));
        json_set_hdr(g, "state", "active");
        json_set_hdr(g, "createdBy", "memory");
        json_set(g, "pinned", json_bool(false));
        json_append(gnodes, g);
    }
    if (prop) {
        json_free(gnodes); json_free(learned); json_free(skill_edges); json_free(memory_cards); json_free(memory_edges);
        for (size_t j = 0; j < nck; j++) free(ck[j]); free(ck); free(cv);
        return prop;
    }

    /* edges = [{source,target} for skill_edges + memory_edges] */
    json_t *gedges = json_array();
    json_t *both[2] = { skill_edges, memory_edges };
    for (int k = 0; k < 2; k++) {
        json_t *ea = both[k];
        if (!j_is_arr(ea)) continue;
        size_t en = json_len(ea);
        for (size_t i = 0; i < en; i++) {
            json_t *p = json_get(ea, i);
            if (!j_is_arr(p) || json_len(p) < 2) continue;
            json_t *e = json_object();
            json_set(e, "source", json_copy(json_get(p, 0)));
            json_set(e, "target", json_copy(json_get(p, 1)));
            json_append(gedges, e);
        }
    }

    /* clusters sorted desc (stable, first-seen tie) */
    size_t *order = malloc((nck ? nck : 1) * sizeof(size_t));
    for (size_t i = 0; i < nck; i++) order[i] = i;
    for (size_t i = 0; i < nck; i++)
        for (size_t j = i + 1; j < nck; j++)
            if (cv[order[j]] > cv[order[i]]) { size_t t = order[i]; order[i] = order[j]; order[j] = t; }
    json_t *clusters_arr = json_array();
    for (size_t i = 0; i < nck; i++) {
        json_t *obj = json_object();
        json_set_hdr(obj, "category", ck[order[i]]);
        json_set(obj, "count", json_number((double)cv[order[i]]));
        json_append(clusters_arr, obj);
    }
    free(order);
    for (size_t j = 0; j < nck; j++) free(ck[j]);
    free(ck); free(cv);

    /* stats = {**density_stats(learned, skill_edges), memory_nodes, ...} */
    json_t *dsa = json_object();
    json_set(dsa, "nodes", json_copy(learned));
    json_set(dsa, "edges", json_copy(skill_edges));
    json_t *stats = lg_call(io, "density_stats", dsa);
    json_free(dsa);
    if (lg_raised(stats)) {
        json_free(gnodes); json_free(gedges); json_free(clusters_arr);
        json_free(learned); json_free(skill_edges); json_free(memory_cards); json_free(memory_edges);
        return stats;
    }
    if (!j_is_obj(stats)) { json_free(stats); stats = json_object(); }
    json_set(stats, "memory_nodes", json_number((double)mem_count));
    size_t me_count = j_is_arr(memory_edges) ? json_len(memory_edges) : 0;
    json_set(stats, "memory_skill_edges", json_number((double)me_count));
    json_set(stats, "learned_skills", json_number((double)ln));

    json_t *result = json_object();
    json_set(result, "nodes", gnodes);
    json_set(result, "edges", gedges);
    json_set(result, "clusters", clusters_arr);
    json_set(result, "memory", json_copy(memory_cards));
    json_set(result, "stats", stats);

    json_free(learned); json_free(skill_edges); json_free(memory_cards); json_free(memory_edges);
    return result;
}

/* ── dispatch ──────────────────────────────────────────────────────────── */
json_t *lg_dispatch(lg_io_t *io, const char *func, json_t *in) {
    if (strcmp(func, "_fm_field") == 0)
        return lg_fm_field(json_obj_get(in, "fm"), json_get_str(in, "key", ""));
    if (strcmp(func, "memory_fingerprint") == 0)
        return lg_memory_fingerprint(json_get_str(in, "entry", ""));
    if (strcmp(func, "memory_node_id") == 0)
        return lg_memory_node_id(json_obj_get(in, "card"), (long long)json_get_num(in, "index", 0));
    if (strcmp(func, "_has_learning_signal") == 0)
        return lg_has_learning_signal(json_obj_get(in, "node"));
    if (strcmp(func, "_related") == 0)
        return lg_related(json_obj_get(in, "fm"));
    if (strcmp(func, "_tokenize") == 0)
        return lg_tokenize(json_get_str(in, "text", ""));
    if (strcmp(func, "build_edges") == 0)
        return lg_build_edges(json_obj_get(in, "nodes"));
    if (strcmp(func, "density_stats") == 0)
        return lg_density_stats(json_obj_get(in, "nodes"), json_obj_get(in, "edges"));
    if (strcmp(func, "_to_int_ts") == 0)
        return lg_to_int_ts(json_obj_get(in, "value"));
    if (strcmp(func, "_memory_skill_edges") == 0)
        return lg_memory_skill_edges(io, json_obj_get(in, "memory_cards"), json_obj_get(in, "skills"));
    if (strcmp(func, "build_learning_graph") == 0)
        return lg_build_learning_graph(io);
    json_t *e = json_object();
    json_set_hdr(e, "error", "unknown func");
    return e;
}

#endif /* LEARNING_GRAPH_FLOW_C */