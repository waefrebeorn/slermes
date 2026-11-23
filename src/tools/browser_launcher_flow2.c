/* browser_launcher_flow2.c — FLOW port of the UNCLAIMED remainder of
 * tools/browser_launcher.py (quarry: C:/Users/eman5/hermes-agent). The claimed
 * siblings live in browser_launcher_flow.c (blf_*), port_browser_launcher.c
 * (_has_display / build_chromium_argv) and browser_tool_install.c
 * (_maybe_autoinstall_chromium); they are SEAMED here, never re-ported.
 *
 * This port covers the launch-record / lookup / stop / owned-launch-tail
 * cluster: profile + record path math (_ephemeral_root .. _record_path),
 * record identity and I/O (_record_is, _owner_alive, _load_record,
 * _write_record, _remove_record, _adopt_record), browser construction
 * (_make_browser), lookup/stop (_alive, _foreign_alive, _same_browser,
 * _heir_key, _tracked_browser, _terminate, _forget, get_browser,
 * _last_holder_out, _stop_locked, stop_browser, stop_all_browsers) and the
 * owned-launch tail (_abandon_launch, _launch_owned — the latter carries the
 * live `except BaseException: _abandon_launch(...); raise` passthrough).
 *
 * Decision-sequence port: ALL collaborator I/O (claimed siblings, foreign
 * imports, OS/process/file facts, holders leases, clock, logger, proc
 * methods) rides the injected universal seam bl2_call; the cores perform no
 * I/O. Ported functions call each other REAL (internal composition). The
 * harness scripts results and records the ordered trace the oracle diffs
 * against LIVE tools/browser_launcher.py under the same scripts.
 *
 * Representation contract (shared with tests/oracle_browser_launcher_flow2.py):
 *   - LaunchedBrowser doubles are attribute dicts with the full dataclass field
 *     set {key,pid,start_time,cdp_url,http_url,user_data_dir,owned,ephemeral,
 *     home_key,record_name,foreign}; tuple/set -> array (sets sorted);
 *     None -> JSON null; integral floats collapse to ints on both sides;
 *     Path -> POSIX str. Popen doubles are {"pid": int} with seamed methods.
 *   - module state travels in ctx->state = {"browsers":{key:browser},
 *     "procs":{key:proc}, "launching":{str(pid):browser}}; _launching keys are
 *     str(pid) on both sides. `is` identity on browsers is pointer equality
 *     (the harness aliases a browser_ref arg to the state node); state inserts
 *     COPY so a pop never frees a live local reference.
 *   - uncaught exception -> {"__raise__": cls}; an exception passed as a logger
 *     arg projects to {"__raise__": cls, "msg": str(e)}.
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion
 *     raises RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m}
 *     raises (trace ret is the WHOLE marker); any other value reused.
 *     None-valued seam arg keys dropped on BOTH sides.
 *   - out-state is {} on any propagating raise (both sides); the ordered trace
 *     still captures every seam call up to the raise.
 *
 * Fixture domain: paths POSIX-spelled, no "." / ".." / duplicate-slash segments
 * (posixpath.join / Path.parent math); record texts for _load_record are strict
 * JSON or clearly-invalid JSON (libjson json_parse is lenient where json.loads
 * is strict — leading-zero / bare-token records are excluded); owner_pid / pid
 * are integral ints or null (bool pids excluded); str() operands ASCII.
 *
 * libjson-only (gate links lib/libjson/json.o). Self-contained: the harness
 * #includes "tools/browser_launcher_flow2.c". blf_ prefix is taken by flow1;
 * this port uses bl2_. SHA-256 is inline (_profile_digest needs no libhash).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct bl2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted host facts/flags */
    json_t *state;     /* owned: {browsers,procs,launching} mutable module state */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} bl2_ctx;

static bool bl2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool bl2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool bl2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool bl2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool bl2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool bl2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool bl2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool bl2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *bl2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static double bl2_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
/* borrowed object field or NULL */
static json_t *bl2_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
/* dict.get(key, default): present-null keeps the null node; absent -> default. */
static json_t *bl2_get_def(const json_t *obj, const char *key, json_t *def) {
    if (obj && obj->type == JSON_OBJECT) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}
static json_t *bl2_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}
/* Deep Python-style ==; NULL pointer and JSON_NULL both read None. */
static bool bl2_json_eq(const json_t *a, const json_t *b) {
    if (bl2_is_none(a) && bl2_is_none(b)) return true;
    if (bl2_is_none(a) || bl2_is_none(b)) return false;
    if (a->type != b->type) {
        /* int/float numeric equality across the JSON number domain is already
         * one type; bool != number here (Python True == 1, but the fixture
         * domain keeps bools and numbers in separate fields). */
        return false;
    }
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return bl2_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!bl2_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !bl2_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* ── string / path helpers (caller frees) ────────────────────────────────── */

/* str() of an integral number (f"{pid}", f"{port}"); non-integral %.17g. */
static void bl2_num_str(char *buf, size_t cap, double d) {
    if (d == floor(d) && fabs(d) < 9.2e18)
        snprintf(buf, cap, "%.0f", d);
    else
        snprintf(buf, cap, "%.17g", d);
}
/* str(node) for a pid/port scalar: None -> "None", number -> integral str. */
static char *bl2_pid_str(const json_t *v) {
    if (bl2_is_none(v)) return strdup("None");
    if (bl2_is_str(v)) return strdup(bl2_jstr(v));
    char buf[64];
    bl2_num_str(buf, sizeof buf, bl2_num(v));
    return strdup(buf);
}
/* Normalize a POSIX-spelled path component the way PurePath + as_posix does:
 * backslashes -> '/', collapse duplicate separators (but keep a leading UNC
 * "//"), drop trailing separators, empty -> "". Caller frees. */
static char *bl2_posix_norm(const char *p) {
    if (!p) p = "";
    size_t n = strlen(p);
    char *s = (char *)malloc(n + 2);  /* +1 for a possible UNC trailing '/' */
    if (!s) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        char c = (p[i] == '\\') ? '/' : p[i];
        if (c == '/' && k > 0 && s[k - 1] == '/' && k != 1) continue;
        s[k++] = c;
    }
    size_t e = k;
    while (e > 1 && s[e - 1] == '/') e--;   /* drop trailing separators */
    int had_trailing = (e < k);
    /* PureWindowsPath UNC anchor: '//server/share' (share root) and
     * '//server/' keep a trailing separator; a bare '//server' and deeper
     * paths such as '//server/share/dir' drop it. */
    if (e >= 2 && s[0] == '/' && s[1] == '/') {
        size_t slashes = 0;
        for (size_t i = 2; i < e; i++) if (s[i] == '/') slashes++;
        if (slashes == 1 || (slashes == 0 && had_trailing)) s[e++] = '/';
    }
    s[e] = '\0';
    return s;
}
/* posixpath.join on POSIX-spelled inputs. Normalize each input like pathlib
 * (backslash -> '/', trailing separators dropped) before composing. */
static char *bl2_posix_join(const char *a, const char *b) {
    char *na = bl2_posix_norm(a);
    char *nb = bl2_posix_norm(b);
    a = na ? na : "";
    b = nb ? nb : "";
    if (b[0] == '/') { free(na); free(nb); return strdup(b); }
    if (!a[0]) { free(na); free(nb); return strdup(b); }
    size_t la = strlen(a), lb = strlen(b);
    int sep = (a[la - 1] != '/');
    char *out = (char *)malloc(la + (size_t)sep + lb + 1);
    if (out) {
        memcpy(out, a, la);
        size_t o = la;
        if (sep) out[o++] = '/';
        memcpy(out + o, b, lb);
        out[o + lb] = '\0';
    }
    free(na); free(nb);
    return out;
}
/* Path.parent on POSIX-spelled inputs. */
static char *bl2_posix_parent(const char *p) {
    if (!p) p = "";
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/') n--;
    if (n == 0) return strdup(".");
    size_t k = n;
    while (k > 0 && p[k - 1] != '/') k--;
    if (k == 0) return strdup(".");
    if (k == 1) return strdup("/");
    char *out = (char *)malloc(k);
    if (!out) return NULL;
    memcpy(out, p, k - 1);
    out[k - 1] = '\0';
    return out;
}
/* utf-8-sig BOM strip (EF BB BF). Returns a malloc'd copy. */
static char *bl2_strip_bom(const char *s) {
    if (!s) s = "";
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB
        && (unsigned char)s[2] == 0xBF)
        return strdup(s + 3);
    return strdup(s);
}

/* ── SHA-256 (inline; _profile_digest, no libhash object) ────────────────── */

static uint32_t bl2_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static void bl2_sha256_hex(const unsigned char *data, size_t len, char out[65]) {
    static const uint32_t K[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
        0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
        0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
        0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
    };
    uint32_t h[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    size_t total = len + 1 + 8;
    size_t padded = ((total + 63) / 64) * 64;
    unsigned char *msg = (unsigned char *)calloc(padded, 1);
    if (!msg) { out[0] = '\0'; return; }
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bitlen = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        msg[padded - 1 - (size_t)i] = (unsigned char)(bitlen >> (8 * i));
    for (size_t off = 0; off < padded; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)msg[off + (size_t)i * 4] << 24)
                 | ((uint32_t)msg[off + (size_t)i * 4 + 1] << 16)
                 | ((uint32_t)msg[off + (size_t)i * 4 + 2] << 8)
                 | (uint32_t)msg[off + (size_t)i * 4 + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = bl2_rotr(w[i - 15], 7) ^ bl2_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = bl2_rotr(w[i - 2], 17) ^ bl2_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = bl2_rotr(e, 6) ^ bl2_rotr(e, 11) ^ bl2_rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = bl2_rotr(a, 2) ^ bl2_rotr(a, 13) ^ bl2_rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    free(msg);
    static const char hexd[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 4; j++) {
            unsigned char byte = (unsigned char)(h[i] >> (24 - 8 * j));
            out[i * 8 + j * 2] = hexd[byte >> 4];
            out[i * 8 + j * 2 + 1] = hexd[byte & 0x0F];
        }
    out[64] = '\0';
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

static bool bl2_exc_pending(const bl2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void bl2_exc_clear(bl2_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void bl2_raise(bl2_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *bl2_exc_class(const bl2_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
static const char *bl2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"TimeoutError", "OSError"},
        {"JSONDecodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeError", "ValueError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"TimeoutExpired", "SubprocessError"}, {"SubprocessError", "Exception"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (bl2_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool bl2_exc_matches(const bl2_ctx *ctx, const char *const *classes, size_t n) {
    if (!bl2_exc_pending(ctx)) return false;
    const char *cls = bl2_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (bl2_str_eq(cls, classes[i])) return true;
        const char *p = bl2_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
static bool bl2_exc_is_oserror(const bl2_ctx *ctx) {
    static const char *E[] = {"OSError"};
    return bl2_exc_matches(ctx, E, 1);
}
static bool bl2_exc_is_oserror_or_valueerror(const bl2_ctx *ctx) {
    static const char *E[] = {"OSError", "ValueError"};
    return bl2_exc_matches(ctx, E, 2);
}
static bool bl2_exc_is_fileexists(const bl2_ctx *ctx) {
    static const char *E[] = {"FileExistsError"};
    return bl2_exc_matches(ctx, E, 1);
}
static bool bl2_exc_is_timeoutexpired(const bl2_ctx *ctx) {
    static const char *E[] = {"TimeoutExpired"};
    return bl2_exc_matches(ctx, E, 1);
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *bl2_script_result(bl2_ctx *ctx, const json_t *script) {
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
/* args ownership TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *bl2_call(bl2_ctx *ctx, const char *name, json_t *args) {
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
                snprintf(msg, sizeof msg, "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = bl2_script_result(ctx, chosen);
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

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). */
static void bl2_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void bl2_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void bl2_arg_bool(json_t *args, const char *key, bool val) {
    json_set(args, key, json_bool(val));
}
static void bl2_arg_num(json_t *args, const char *key, double val) {
    json_set(args, key, json_number(val));
}
/* logger seam: {"msg","args"}. args_arr ownership transferred. */
static void bl2_log(bl2_ctx *ctx, const char *level, const char *msg, json_t *args_arr) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = bl2_call(ctx, name, a);
    json_free(r);
}
/* project the pending exception as a logger arg node {"__raise__","msg"}. */
static json_t *bl2_exc_arg(const bl2_ctx *ctx) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(bl2_exc_class(ctx)));
    json_set(e, "msg", json_string(json_get_str(ctx->exc, "msg", "")));
    return e;
}
/* tuple result helpers */
static json_t *bl2_t0(const json_t *r) { return bl2_is_arr(r) ? json_get(r, 0) : NULL; }
static json_t *bl2_t1(const json_t *r) { return bl2_is_arr(r) ? json_get(r, 1) : NULL; }
static json_t *bl2_pair(json_t *a, json_t *b) {
    json_t *r = json_array();
    json_append(r, a ? a : json_null());
    json_append(r, b ? b : json_null());
    return r;
}

/* ── state accessors ─────────────────────────────────────────────────────── */

static json_t *bl2_browsers(bl2_ctx *ctx) { return bl2_get(ctx->state, "browsers"); }
static json_t *bl2_procs(bl2_ctx *ctx) { return bl2_get(ctx->state, "procs"); }
static json_t *bl2_launching(bl2_ctx *ctx) { return bl2_get(ctx->state, "launching"); }
static double bl2_proc_pid(const json_t *proc) { return bl2_num(bl2_get(proc, "pid")); }

/* forward declarations (internal composition) */
static json_t *bl2_ephemeral_root(bl2_ctx *ctx, const char *home);
static json_t *bl2_records_dir(bl2_ctx *ctx, const char *home);
static json_t *bl2_profile_digest(bl2_ctx *ctx, const char *udd);
static json_t *bl2_same_profile(bl2_ctx *ctx, const json_t *recorded, const char *udd);
static json_t *bl2_devtools_id(bl2_ctx *ctx, const json_t *browser);
static json_t *bl2_record_name(bl2_ctx *ctx, const json_t *browser);
static json_t *bl2_shared_profiles_root(bl2_ctx *ctx);
static json_t *bl2_is_private_dir(bl2_ctx *ctx, const char *path);
static json_t *bl2_private_dir(bl2_ctx *ctx, const char *path);
static json_t *bl2_shared_state_dir(bl2_ctx *ctx, const char *udd);
static json_t *bl2_state_dir(bl2_ctx *ctx, const json_t *browser);
static json_t *bl2_record_path(bl2_ctx *ctx, const json_t *browser);
static json_t *bl2_make_browser(bl2_ctx *ctx, const json_t *in);
static json_t *bl2_record_is(bl2_ctx *ctx, const json_t *record, const json_t *browser);
static json_t *bl2_owner_alive(bl2_ctx *ctx, const json_t *record);
static json_t *bl2_load_record(bl2_ctx *ctx, const char *path);
static json_t *bl2_write_record(bl2_ctx *ctx, const json_t *browser);
static json_t *bl2_remove_record(bl2_ctx *ctx, const json_t *browser);
static json_t *bl2_adopt_record(bl2_ctx *ctx, const json_t *browser);
static json_t *bl2_alive(bl2_ctx *ctx, const json_t *browser);
static json_t *bl2_foreign_alive(bl2_ctx *ctx, const json_t *browser);
static json_t *bl2_same_browser(bl2_ctx *ctx, const json_t *a, const json_t *b);
static json_t *bl2_heir_key(bl2_ctx *ctx, const json_t *browser);
static json_t *bl2_tracked_browser(bl2_ctx *ctx, const json_t *record);
static json_t *bl2_terminate(bl2_ctx *ctx, const json_t *pid, const json_t *start_time, const json_t *proc);
static json_t *bl2_forget(bl2_ctx *ctx, const char *key, const json_t *browser, bool drop_record);
static json_t *bl2_get_browser(bl2_ctx *ctx, const char *key);
static json_t *bl2_last_holder_out(bl2_ctx *ctx, const char *record, const json_t *browser);
static json_t *bl2_stop_locked(bl2_ctx *ctx, const char *key);
static json_t *bl2_stop_browser(bl2_ctx *ctx, const char *key);
static json_t *bl2_stop_all_browsers(bl2_ctx *ctx);
static json_t *bl2_abandon_launch(bl2_ctx *ctx, const json_t *proc, const json_t *starting);
static json_t *bl2_launch_owned(bl2_ctx *ctx, const json_t *in);

/* ── 1. _ephemeral_root ──────────────────────────────────────────────────── */
/* PoP: bl2_ephemeral_root @ tools/browser_launcher.py:_ephemeral_root */
static json_t *bl2_ephemeral_root(bl2_ctx *ctx, const char *home) {
    (void)ctx;
    char *a = bl2_posix_join(home ? home : "", "cache");
    char *b = bl2_posix_join(a, "browser");
    char *c = bl2_posix_join(b, "chromium");
    free(a); free(b);
    json_t *r = json_string(c);
    free(c);
    return r;
}

/* ── 2. _records_dir ─────────────────────────────────────────────────────── */
/* PoP: bl2_records_dir @ tools/browser_launcher.py:_records_dir */
static json_t *bl2_records_dir(bl2_ctx *ctx, const char *home) {
    (void)ctx;
    char *a = bl2_posix_join(home ? home : "", "cache");
    char *b = bl2_posix_join(a, "browser");
    char *c = bl2_posix_join(b, "chromium-records");
    free(a); free(b);
    json_t *r = json_string(c);
    free(c);
    return r;
}

/* ── 3. _profile_digest ──────────────────────────────────────────────────── */
/* PoP: bl2_profile_digest @ tools/browser_launcher.py:_profile_digest */
static json_t *bl2_profile_digest(bl2_ctx *ctx, const char *udd) {
    json_t *a = json_object();
    bl2_arg_str(a, "path", udd ? udd : "");
    json_t *rp = bl2_call(ctx, "realpath", a);
    if (bl2_exc_pending(ctx)) { json_free(rp); return NULL; }
    const char *real = bl2_jstr(rp);
    char hex[65];
    bl2_sha256_hex((const unsigned char *)real, strlen(real), hex);
    json_free(rp);
    return json_string(hex);
}

/* ── 4. _same_profile ────────────────────────────────────────────────────── */
/* PoP: bl2_same_profile @ tools/browser_launcher.py:_same_profile */
static json_t *bl2_same_profile(bl2_ctx *ctx, const json_t *recorded, const char *udd) {
    if (!bl2_is_str(recorded)) return json_bool(false);  /* isinstance short-circuit */
    json_t *a1 = json_object();
    bl2_arg_str(a1, "path", bl2_jstr(recorded));
    json_t *r1 = bl2_call(ctx, "realpath", a1);
    if (bl2_exc_pending(ctx)) { json_free(r1); return NULL; }
    json_t *a2 = json_object();
    bl2_arg_str(a2, "path", udd ? udd : "");
    json_t *r2 = bl2_call(ctx, "realpath", a2);
    if (bl2_exc_pending(ctx)) { json_free(r1); json_free(r2); return NULL; }
    bool eq = bl2_str_eq(bl2_jstr(r1), bl2_jstr(r2));
    json_free(r1); json_free(r2);
    return json_bool(eq);
}

/* ── 5. _devtools_id ─────────────────────────────────────────────────────── */
/* PoP: bl2_devtools_id @ tools/browser_launcher.py:_devtools_id */
static json_t *bl2_devtools_id(bl2_ctx *ctx, const json_t *browser) {
    (void)ctx;
    const char *cdp = bl2_jstr(bl2_get(browser, "cdp_url"));
    const char *at = strstr(cdp, "/devtools/browser/");
    if (!at) return json_null();
    return json_string(at);
}

/* ── 6. _record_name ─────────────────────────────────────────────────────── */
/* PoP: bl2_record_name @ tools/browser_launcher.py:_record_name */
static json_t *bl2_record_name(bl2_ctx *ctx, const json_t *browser) {
    json_t *rn = bl2_get(browser, "record_name");
    if (bl2_is_str(rn) && bl2_jstr(rn)[0] != '\0') return json_copy(rn);  /* `or` truthy */
    json_t *dig = bl2_profile_digest(ctx, bl2_jstr(bl2_get(browser, "user_data_dir")));
    if (bl2_exc_pending(ctx)) { json_free(dig); return NULL; }
    json_t *tag = bl2_call(ctx, "ns_tag", json_object());
    if (bl2_exc_pending(ctx)) { json_free(dig); json_free(tag); return NULL; }
    char *pids = bl2_pid_str(bl2_get(browser, "pid"));
    const char *d = bl2_jstr(dig);
    char prefix[17];
    memcpy(prefix, d, 16); prefix[16] = '\0';
    size_t cap = strlen(prefix) + strlen(pids) + strlen(bl2_jstr(tag)) + 8;
    char *buf = (char *)malloc(cap);
    snprintf(buf, cap, "%s-%s%s.json", prefix, pids, bl2_jstr(tag));
    json_t *r = json_string(buf);
    free(buf); free(pids); json_free(dig); json_free(tag);
    return r;
}

/* ── 7. _shared_profiles_root ────────────────────────────────────────────── */
/* PoP: bl2_shared_profiles_root @ tools/browser_launcher.py:_shared_profiles_root */
static json_t *bl2_shared_profiles_root(bl2_ctx *ctx) {
    json_t *ld = bl2_call(ctx, "_get_lock_dir", json_object());
    if (bl2_exc_pending(ctx)) { json_free(ld); return NULL; }
    char *j = bl2_posix_join(bl2_jstr(ld), "browser-shared-profiles");
    json_t *r = json_string(j);
    free(j); json_free(ld);
    return r;
}

/* ── 8. _is_private_dir ──────────────────────────────────────────────────── */
/* PoP: bl2_is_private_dir @ tools/browser_launcher.py:_is_private_dir */
static json_t *bl2_is_private_dir(bl2_ctx *ctx, const char *path) {
    json_t *a = json_object();
    bl2_arg_str(a, "path", path ? path : "");
    json_t *st = bl2_call(ctx, "lstat", a);
    if (bl2_exc_pending(ctx)) {
        if (bl2_exc_is_oserror(ctx)) { bl2_exc_clear(ctx); json_free(st); return json_bool(false); }
        json_free(st); return NULL;
    }
    long mode = (long)bl2_num(bl2_get(st, "st_mode"));
    if ((mode & 0170000) != 0040000) { json_free(st); return json_bool(false); }  /* S_ISDIR */
    bool has_getuid = json_get_bool(ctx->env, "has_getuid", true);
    if (!has_getuid) { json_free(st); return json_bool(true); }
    json_t *uid = bl2_call(ctx, "getuid", json_object());
    if (bl2_exc_pending(ctx)) { json_free(st); json_free(uid); return NULL; }
    long st_uid = (long)bl2_num(bl2_get(st, "st_uid"));
    bool ok = (st_uid == (long)bl2_num(uid)) && ((mode & 0022) == 0);
    json_free(st); json_free(uid);
    return json_bool(ok);
}

/* ── 9. _private_dir ─────────────────────────────────────────────────────── */
/* PoP: bl2_private_dir @ tools/browser_launcher.py:_private_dir */
static json_t *bl2_private_dir(bl2_ctx *ctx, const char *path) {
    char *parent = bl2_posix_parent(path);
    json_t *a1 = json_object();
    bl2_arg_str(a1, "path", parent);
    bl2_arg_num(a1, "mode", 0777);
    bl2_arg_bool(a1, "parents", true);
    bl2_arg_bool(a1, "exist_ok", true);
    json_t *m1 = bl2_call(ctx, "mkdir", a1);
    json_free(m1);
    free(parent);
    if (bl2_exc_pending(ctx)) return NULL;
    json_t *a2 = json_object();
    bl2_arg_str(a2, "path", path ? path : "");
    bl2_arg_num(a2, "mode", 0700);
    bl2_arg_bool(a2, "parents", false);
    bl2_arg_bool(a2, "exist_ok", false);
    json_t *m2 = bl2_call(ctx, "mkdir", a2);
    json_free(m2);
    if (bl2_exc_pending(ctx)) {
        if (bl2_exc_is_fileexists(ctx)) bl2_exc_clear(ctx);
        else return NULL;
    }
    json_t *priv = bl2_is_private_dir(ctx, path);
    if (bl2_exc_pending(ctx)) { json_free(priv); return NULL; }
    if (bl2_truthy(priv)) { json_free(priv); return json_string(path ? path : ""); }
    json_free(priv);
    json_t *la = json_array();
    json_append(la, json_string(path ? path : ""));
    bl2_log(ctx, "debug",
            "browser: not using %s for launcher state: not a private directory of this user", la);
    if (bl2_exc_pending(ctx)) return NULL;
    return json_null();
}

/* ── 10. _shared_state_dir ───────────────────────────────────────────────── */
/* PoP: bl2_shared_state_dir @ tools/browser_launcher.py:_shared_state_dir */
static json_t *bl2_shared_state_dir(bl2_ctx *ctx, const char *udd) {
    json_t *rootpath = bl2_shared_profiles_root(ctx);
    if (bl2_exc_pending(ctx)) { json_free(rootpath); return NULL; }
    json_t *root = bl2_private_dir(ctx, bl2_jstr(rootpath));
    json_free(rootpath);
    if (bl2_exc_pending(ctx)) { json_free(root); return NULL; }
    if (bl2_is_none(root)) { json_free(root); return json_null(); }
    json_t *dig = bl2_profile_digest(ctx, udd);
    if (bl2_exc_pending(ctx)) { json_free(root); json_free(dig); return NULL; }
    const char *d = bl2_jstr(dig);
    char prefix[33];
    memcpy(prefix, d, 32); prefix[32] = '\0';
    char *sub = bl2_posix_join(bl2_jstr(root), prefix);
    json_free(dig); json_free(root);
    json_t *r = bl2_private_dir(ctx, sub);
    free(sub);
    return r;
}

/* ── 11. _state_dir ──────────────────────────────────────────────────────── */
/* PoP: bl2_state_dir @ tools/browser_launcher.py:_state_dir */
static json_t *bl2_state_dir(bl2_ctx *ctx, const json_t *browser) {
    if (json_get_bool(browser, "ephemeral", false)) {
        json_t *rd = bl2_records_dir(ctx, bl2_jstr(bl2_get(browser, "home_key")));
        if (bl2_exc_pending(ctx)) { json_free(rd); return NULL; }
        json_t *r = bl2_private_dir(ctx, bl2_jstr(rd));
        json_free(rd);
        return r;
    }
    return bl2_shared_state_dir(ctx, bl2_jstr(bl2_get(browser, "user_data_dir")));
}

/* ── 12. _record_path ────────────────────────────────────────────────────── */
/* PoP: bl2_record_path @ tools/browser_launcher.py:_record_path */
static json_t *bl2_record_path(bl2_ctx *ctx, const json_t *browser) {
    json_t *records = bl2_state_dir(ctx, browser);
    if (bl2_exc_pending(ctx)) { json_free(records); return NULL; }
    if (bl2_is_none(records)) { json_free(records); return json_null(); }
    json_t *name = bl2_record_name(ctx, browser);
    if (bl2_exc_pending(ctx)) { json_free(records); json_free(name); return NULL; }
    char *j = bl2_posix_join(bl2_jstr(records), bl2_jstr(name));
    json_t *r = json_string(j);
    free(j); json_free(records); json_free(name);
    return r;
}

/* ── 13. _make_browser ───────────────────────────────────────────────────── */
/* PoP: bl2_make_browser @ tools/browser_launcher.py:_make_browser */
static json_t *bl2_make_browser(bl2_ctx *ctx, const json_t *in) {
    (void)ctx;
    double port = bl2_num(bl2_get(in, "port"));
    const char *path = bl2_jstr(bl2_get(in, "path"));
    char pbuf[64];
    bl2_num_str(pbuf, sizeof pbuf, port);
    size_t ccap = strlen("ws://127.0.0.1:") + strlen(pbuf) + strlen(path) + 1;
    char *cdp = (char *)malloc(ccap);
    snprintf(cdp, ccap, "ws://127.0.0.1:%s%s", pbuf, path);
    size_t hcap = strlen("http://127.0.0.1:") + strlen(pbuf) + 1;
    char *http = (char *)malloc(hcap);
    snprintf(http, hcap, "http://127.0.0.1:%s", pbuf);
    json_t *b = json_object();
    bl2_arg_str(b, "key", bl2_jstr(bl2_get(in, "key")));
    json_set(b, "pid", bl2_copy_or_null(bl2_get(in, "pid")));
    json_set(b, "start_time", bl2_copy_or_null(bl2_get(in, "start_time")));
    json_set(b, "cdp_url", json_string(cdp));
    json_set(b, "http_url", json_string(http));
    bl2_arg_str(b, "user_data_dir", bl2_jstr(bl2_get(in, "user_data_dir")));
    bl2_arg_bool(b, "owned", json_get_bool(in, "owned", false));
    bl2_arg_bool(b, "ephemeral", json_get_bool(in, "ephemeral", false));
    bl2_arg_str(b, "home_key", bl2_jstr(bl2_get(in, "home_key")));
    json_set(b, "record_name", json_null());
    json_set(b, "foreign", json_bool(false));
    free(cdp); free(http);
    return b;
}

/* ── 14. _record_is ──────────────────────────────────────────────────────── */
/* PoP: bl2_record_is @ tools/browser_launcher.py:_record_is */
static json_t *bl2_record_is(bl2_ctx *ctx, const json_t *record, const json_t *browser) {
    json_t *rpid = bl2_get_def(record, "pid", NULL);
    json_t *bpid = bl2_get(browser, "pid");
    if (!bl2_json_eq(rpid, bpid)) return json_bool(false);
    json_t *sp = bl2_same_profile(ctx, bl2_get_def(record, "user_data_dir", NULL),
                                  bl2_jstr(bl2_get(browser, "user_data_dir")));
    if (bl2_exc_pending(ctx)) { json_free(sp); return NULL; }
    if (!bl2_truthy(sp)) { json_free(sp); return json_bool(false); }
    json_free(sp);
    if (json_get_bool(browser, "foreign", false)) {
        json_t *a = json_object();
        bl2_arg(a, "record", record);
        json_t *ns = bl2_call(ctx, "same_namespace", a);
        if (bl2_exc_pending(ctx)) { json_free(ns); return NULL; }
        bool same = bl2_truthy(ns);
        json_free(ns);
        if (same) return json_bool(false);  /* not same_namespace -> False */
        json_t *dev = bl2_get_def(record, "devtools", NULL);
        if (bl2_is_none(dev)) return json_bool(false);  /* is not None */
        json_t *did = bl2_devtools_id(ctx, browser);
        bool eq = bl2_json_eq(dev, did);
        json_free(did);
        return json_bool(eq);
    }
    json_t *rst = bl2_get_def(record, "start_time", NULL);
    json_t *bst = bl2_get(browser, "start_time");
    return json_bool(bl2_json_eq(rst, bst));
}

/* ── 15. _owner_alive ────────────────────────────────────────────────────── */
/* PoP: bl2_owner_alive @ tools/browser_launcher.py:_owner_alive */
static json_t *bl2_owner_alive(bl2_ctx *ctx, const json_t *record) {
    json_t *a = json_object();
    bl2_arg(a, "record", record);
    json_t *ns = bl2_call(ctx, "same_namespace", a);
    if (bl2_exc_pending(ctx)) { json_free(ns); return NULL; }
    bool same = bl2_truthy(ns);
    json_free(ns);
    if (!same) return json_bool(true);
    json_t *owner = bl2_get_def(record, "owner_pid", NULL);
    if (!bl2_is_num(owner)) return json_bool(false);  /* isinstance(owner, int) */
    json_t *pa = json_object();
    bl2_arg(pa, "pid", owner);
    json_t *alive = bl2_call(ctx, "_pid_alive", pa);
    if (bl2_exc_pending(ctx)) { json_free(alive); return NULL; }
    if (!bl2_truthy(alive)) { json_free(alive); return json_bool(false); }
    json_free(alive);
    json_t *expected = bl2_get_def(record, "owner_start_time", NULL);
    if (bl2_is_none(expected)) return json_bool(true);
    json_t *sa = json_object();
    bl2_arg(sa, "pid", owner);
    json_t *st = bl2_call(ctx, "_start_time", sa);
    if (bl2_exc_pending(ctx)) { json_free(st); return NULL; }
    bool eq = bl2_json_eq(st, expected);
    json_free(st);
    return json_bool(eq);
}

/* ── 16. _load_record ────────────────────────────────────────────────────── */
/* PoP: bl2_load_record @ tools/browser_launcher.py:_load_record */
static json_t *bl2_load_record(bl2_ctx *ctx, const char *path) {
    json_t *a = json_object();
    bl2_arg_str(a, "path", path ? path : "");
    bl2_arg_str(a, "encoding", "utf-8-sig");
    json_t *txt = bl2_call(ctx, "read_text", a);
    bool fell_through = false;
    if (bl2_exc_pending(ctx)) {
        if (bl2_exc_is_oserror_or_valueerror(ctx)) { bl2_exc_clear(ctx); fell_through = true; }
        else { json_free(txt); return NULL; }
    }
    json_t *record = NULL;
    if (!fell_through) {
        char *clean = bl2_strip_bom(bl2_jstr(txt));
        record = json_parse(clean, NULL);
        free(clean);
        if (record && record->type == JSON_OBJECT) { json_free(txt); return record; }
        json_free(record);
        record = NULL;
    }
    json_free(txt);
    /* path.unlink(missing_ok=True) — outside the try; its OSError propagates */
    json_t *ua = json_object();
    bl2_arg_str(ua, "path", path ? path : "");
    bl2_arg_bool(ua, "missing_ok", true);
    json_t *u = bl2_call(ctx, "unlink", ua);
    json_free(u);
    if (bl2_exc_pending(ctx)) return NULL;
    return json_null();
}

/* ── 17. _write_record ───────────────────────────────────────────────────── */
/* PoP: bl2_write_record @ tools/browser_launcher.py:_write_record */
static json_t *bl2_write_record(bl2_ctx *ctx, const json_t *browser) {
    json_t *g1 = bl2_call(ctx, "getpid", json_object());
    if (bl2_exc_pending(ctx)) { json_free(g1); return NULL; }
    json_t *g2 = bl2_call(ctx, "getpid", json_object());
    if (bl2_exc_pending(ctx)) { json_free(g1); json_free(g2); return NULL; }
    json_t *sa = json_object();
    bl2_arg(sa, "pid", g2);
    json_t *ost = bl2_call(ctx, "_start_time", sa);
    if (bl2_exc_pending(ctx)) { json_free(g1); json_free(g2); json_free(ost); return NULL; }
    json_t *ns = bl2_call(ctx, "ns_identity", json_object());
    if (bl2_exc_pending(ctx)) { json_free(g1); json_free(g2); json_free(ost); json_free(ns); return NULL; }
    json_t *did = bl2_devtools_id(ctx, browser);
    json_t *record = json_object();
    json_set(record, "owner_pid", g1);
    json_set(record, "owner_start_time", ost ? ost : json_null());
    json_set(record, "pid", bl2_copy_or_null(bl2_get(browser, "pid")));
    json_set(record, "start_time", bl2_copy_or_null(bl2_get(browser, "start_time")));
    bl2_arg_str(record, "user_data_dir", bl2_jstr(bl2_get(browser, "user_data_dir")));
    bl2_arg_str(record, "home_key", bl2_jstr(bl2_get(browser, "home_key")));
    bl2_arg_bool(record, "ephemeral", json_get_bool(browser, "ephemeral", false));
    json_set(record, "ns", ns ? ns : json_null());
    json_set(record, "devtools", did ? did : json_null());
    /* try: path = _record_path(browser); if path is not None: atomic_json_write */
    json_t *path = bl2_record_path(ctx, browser);
    if (bl2_exc_pending(ctx)) {
        if (bl2_exc_is_oserror(ctx)) goto log_debug;
        json_free(record); json_free(path); return NULL;
    }
    if (!bl2_is_none(path)) {
        json_t *wa = json_object();
        bl2_arg_str(wa, "path", bl2_jstr(path));
        bl2_arg(wa, "record", record);
        json_t *w = bl2_call(ctx, "atomic_json_write", wa);
        json_free(w);
        if (bl2_exc_pending(ctx)) {
            if (bl2_exc_is_oserror(ctx)) goto log_debug;
            json_free(record); json_free(path); return NULL;
        }
    }
    json_free(record); json_free(path);
    return json_null();
log_debug:
    {
        json_t *la = json_array();
        json_append(la, bl2_copy_or_null(bl2_get(browser, "pid")));
        json_append(la, bl2_exc_arg(ctx));
        bl2_exc_clear(ctx);
        bl2_log(ctx, "debug", "browser: could not write launch record for pid %s: %s", la);
        json_free(record); json_free(path);
        if (bl2_exc_pending(ctx)) return NULL;
        return json_null();
    }
}

/* ── 18. _remove_record ──────────────────────────────────────────────────── */
/* PoP: bl2_remove_record @ tools/browser_launcher.py:_remove_record */
static json_t *bl2_remove_record(bl2_ctx *ctx, const json_t *browser) {
    json_t *path = bl2_record_path(ctx, browser);
    if (bl2_exc_pending(ctx)) {
        if (bl2_exc_is_oserror(ctx)) goto log_debug;
        json_free(path); return NULL;
    }
    if (!bl2_is_none(path)) {
        json_t *ua = json_object();
        bl2_arg_str(ua, "path", bl2_jstr(path));
        bl2_arg_bool(ua, "missing_ok", true);
        json_t *u = bl2_call(ctx, "unlink", ua);
        json_free(u);
        if (bl2_exc_pending(ctx)) {
            if (bl2_exc_is_oserror(ctx)) goto log_debug;
            json_free(path); return NULL;
        }
    }
    json_free(path);
    return json_null();
log_debug:
    {
        json_t *la = json_array();
        json_append(la, bl2_copy_or_null(bl2_get(browser, "pid")));
        json_append(la, bl2_exc_arg(ctx));
        bl2_exc_clear(ctx);
        bl2_log(ctx, "debug", "browser: could not remove launch record for pid %s: %s", la);
        json_free(path);
        if (bl2_exc_pending(ctx)) return NULL;
        return json_null();
    }
}

/* ── 19. _adopt_record ───────────────────────────────────────────────────── */
/* PoP: bl2_adopt_record @ tools/browser_launcher.py:_adopt_record */
static json_t *bl2_adopt_record(bl2_ctx *ctx, const json_t *browser) {
    json_t *path = bl2_record_path(ctx, browser);
    if (bl2_exc_pending(ctx)) { json_free(path); return NULL; }
    if (bl2_is_none(path)) { json_free(path); return json_null(); }
    json_t *ea = json_object();
    bl2_arg_str(ea, "path", bl2_jstr(path));
    json_t *ex = bl2_call(ctx, "exists", ea);
    if (bl2_exc_pending(ctx)) { json_free(path); json_free(ex); return NULL; }
    bool exists = bl2_truthy(ex);
    json_free(ex);
    if (!exists) { json_free(path); return json_null(); }
    json_t *record = bl2_load_record(ctx, bl2_jstr(path));
    if (bl2_exc_pending(ctx)) { json_free(path); json_free(record); return NULL; }
    if (bl2_is_none(record)) { json_free(path); json_free(record); return json_null(); }
    json_t *ri = bl2_record_is(ctx, record, browser);
    if (bl2_exc_pending(ctx)) { json_free(path); json_free(record); json_free(ri); return NULL; }
    if (!bl2_truthy(ri)) { json_free(path); json_free(record); json_free(ri); return json_null(); }
    json_free(ri);
    json_t *oa = bl2_owner_alive(ctx, record);
    if (bl2_exc_pending(ctx)) { json_free(path); json_free(record); json_free(oa); return NULL; }
    if (bl2_truthy(oa)) { json_free(path); json_free(record); json_free(oa); return json_null(); }
    json_free(oa);
    json_t *me = bl2_call(ctx, "getpid", json_object());
    if (bl2_exc_pending(ctx)) { json_free(path); json_free(record); json_free(me); return NULL; }
    json_t *sa = json_object();
    bl2_arg(sa, "pid", me);
    json_t *ost = bl2_call(ctx, "_start_time", sa);
    if (bl2_exc_pending(ctx)) { json_free(path); json_free(record); json_free(me); json_free(ost); return NULL; }
    json_t *newrec = json_copy(record);
    json_set(newrec, "owner_pid", me);
    json_set(newrec, "owner_start_time", ost ? ost : json_null());
    json_t *wa = json_object();
    bl2_arg_str(wa, "path", bl2_jstr(path));
    bl2_arg(wa, "record", newrec);
    json_t *w = bl2_call(ctx, "atomic_json_write", wa);
    json_free(w);
    json_free(newrec); json_free(record); json_free(path);
    if (bl2_exc_pending(ctx)) return NULL;
    return json_null();
}

/* ── 20. _alive ──────────────────────────────────────────────────────────── */
/* PoP: bl2_alive @ tools/browser_launcher.py:_alive */
static json_t *bl2_alive(bl2_ctx *ctx, const json_t *browser) {
    json_t *proc = bl2_get(bl2_procs(ctx), bl2_jstr(bl2_get(browser, "key")));
    if (!bl2_is_none(proc) && json_get_bool(browser, "owned", false)) {
        json_t *pa = json_object();
        bl2_arg_num(pa, "pid", bl2_proc_pid(proc));
        json_t *poll = bl2_call(ctx, "proc.poll", pa);
        if (bl2_exc_pending(ctx)) { json_free(poll); return NULL; }
        bool running = bl2_is_none(poll);
        json_free(poll);
        return json_bool(running);
    }
    json_t *pid = bl2_get(browser, "pid");
    if (bl2_is_none(pid)) return json_bool(false);
    json_t *pa = json_object();
    bl2_arg(pa, "pid", pid);
    json_t *alive = bl2_call(ctx, "_pid_alive", pa);
    if (bl2_exc_pending(ctx)) { json_free(alive); return NULL; }
    if (!bl2_truthy(alive)) { json_free(alive); return json_bool(false); }
    json_free(alive);
    json_t *st = bl2_get(browser, "start_time");
    if (bl2_is_none(st)) return json_bool(true);
    json_t *sa = json_object();
    bl2_arg(sa, "pid", pid);
    json_t *stime = bl2_call(ctx, "_start_time", sa);
    if (bl2_exc_pending(ctx)) { json_free(stime); return NULL; }
    bool eq = bl2_json_eq(stime, st);
    json_free(stime);
    return json_bool(eq);
}

/* ── 21. _foreign_alive ──────────────────────────────────────────────────── */
/* PoP: bl2_foreign_alive @ tools/browser_launcher.py:_foreign_alive */
static json_t *bl2_foreign_alive(bl2_ctx *ctx, const json_t *browser) {
    json_t *a = json_object();
    bl2_arg_str(a, "user_data_dir", bl2_jstr(bl2_get(browser, "user_data_dir")));
    json_t *found = bl2_call(ctx, "_read_devtools_file", a);
    if (bl2_exc_pending(ctx)) { json_free(found); return NULL; }
    if (bl2_is_none(found)) { json_free(found); return json_bool(false); }
    json_t *port = bl2_t0(found);
    json_t *path = bl2_t1(found);
    char pbuf[64];
    bl2_num_str(pbuf, sizeof pbuf, bl2_num(port));
    const char *ps = bl2_jstr(path);
    size_t cap = strlen("ws://127.0.0.1:") + strlen(pbuf) + strlen(ps) + 1;
    char *expected = (char *)malloc(cap);
    snprintf(expected, cap, "ws://127.0.0.1:%s%s", pbuf, ps);
    bool match = bl2_str_eq(bl2_jstr(bl2_get(browser, "cdp_url")), expected);
    free(expected);
    if (!match) { json_free(found); return json_bool(false); }
    json_t *la = json_object();
    bl2_arg(la, "port", port);
    bl2_arg(la, "path", path);
    json_t *live = bl2_call(ctx, "_devtools_endpoint_live", la);
    json_free(found);
    if (bl2_exc_pending(ctx)) { json_free(live); return NULL; }
    bool r = bl2_truthy(live);
    json_free(live);
    return json_bool(r);
}

/* ── 22. _same_browser ───────────────────────────────────────────────────── */
/* PoP: bl2_same_browser @ tools/browser_launcher.py:_same_browser */
static json_t *bl2_same_browser(bl2_ctx *ctx, const json_t *a, const json_t *b) {
    if (json_get_bool(a, "foreign", false) != json_get_bool(b, "foreign", false))
        return json_bool(false);
    if (!bl2_json_eq(bl2_get(a, "pid"), bl2_get(b, "pid"))) return json_bool(false);
    if (!bl2_json_eq(bl2_get(a, "start_time"), bl2_get(b, "start_time"))) return json_bool(false);
    json_t *rna = bl2_record_name(ctx, a);
    if (bl2_exc_pending(ctx)) { json_free(rna); return NULL; }
    json_t *rnb = bl2_record_name(ctx, b);
    if (bl2_exc_pending(ctx)) { json_free(rna); json_free(rnb); return NULL; }
    bool name_eq = bl2_json_eq(rna, rnb);
    json_free(rna); json_free(rnb);
    if (!name_eq) return json_bool(false);
    json_t *sp = bl2_same_profile(ctx, bl2_get(a, "user_data_dir"),
                                  bl2_jstr(bl2_get(b, "user_data_dir")));
    if (bl2_exc_pending(ctx)) { json_free(sp); return NULL; }
    if (!bl2_truthy(sp)) { json_free(sp); return json_bool(false); }
    json_free(sp);
    if (!json_get_bool(a, "foreign", false)) return json_bool(true);
    return json_bool(bl2_json_eq(bl2_get(a, "cdp_url"), bl2_get(b, "cdp_url")));
}

/* ── 23. _heir_key ───────────────────────────────────────────────────────── */
/* PoP: bl2_heir_key @ tools/browser_launcher.py:_heir_key */
static json_t *bl2_heir_key(bl2_ctx *ctx, const json_t *browser) {
    if (json_get_bool(browser, "ephemeral", false)) return json_null();
    json_t *browsers = bl2_browsers(ctx);
    if (!bl2_is_obj(browsers)) return json_null();
    for (size_t i = 0; i < browsers->c.count; i++) {
        json_t *b = browsers->c.items[i];
        if (b == browser) continue;  /* b is not browser */
        if (json_get_bool(b, "ephemeral", false)) continue;
        json_t *sb = bl2_same_browser(ctx, b, browser);
        if (bl2_exc_pending(ctx)) { json_free(sb); return NULL; }
        bool match = bl2_truthy(sb);
        json_free(sb);
        if (match) return json_string(browsers->c.keys[i]);
    }
    return json_null();
}

/* ── 24. _tracked_browser ────────────────────────────────────────────────── */
/* PoP: bl2_tracked_browser @ tools/browser_launcher.py:_tracked_browser */
static json_t *bl2_tracked_browser(bl2_ctx *ctx, const json_t *record) {
    json_t *browsers = bl2_browsers(ctx);
    if (bl2_is_obj(browsers)) {
        for (size_t i = 0; i < browsers->c.count; i++) {
            json_t *b = browsers->c.items[i];
            if (json_get_bool(b, "foreign", false)) continue;
            json_t *ri = bl2_record_is(ctx, record, b);
            if (bl2_exc_pending(ctx)) { json_free(ri); return NULL; }
            bool match = bl2_truthy(ri);
            json_free(ri);
            if (match) return json_copy(b);
        }
    }
    json_t *launching = bl2_launching(ctx);
    if (bl2_is_obj(launching)) {
        for (size_t i = 0; i < launching->c.count; i++) {
            json_t *b = launching->c.items[i];
            if (json_get_bool(b, "foreign", false)) continue;
            json_t *ri = bl2_record_is(ctx, record, b);
            if (bl2_exc_pending(ctx)) { json_free(ri); return NULL; }
            bool match = bl2_truthy(ri);
            json_free(ri);
            if (match) return json_copy(b);
        }
    }
    return json_null();
}

/* ── 25. _terminate ──────────────────────────────────────────────────────── */
/* PoP: bl2_terminate @ tools/browser_launcher.py:_terminate */
static json_t *bl2_terminate(bl2_ctx *ctx, const json_t *pid, const json_t *start_time,
                             const json_t *proc) {
    if (!bl2_truthy(pid)) return json_null();  /* if not pid */
    json_t *a = json_object();
    bl2_arg(a, "pid", pid);
    bl2_arg(a, "start_time", start_time);
    json_t *t = bl2_call(ctx, "ProcessRegistry._terminate_host_pid", a);
    json_free(t);
    if (bl2_exc_pending(ctx)) {
        if (bl2_exc_is_oserror(ctx)) {
            json_t *la = json_array();
            json_append(la, bl2_copy_or_null(pid));
            json_append(la, bl2_exc_arg(ctx));
            bl2_exc_clear(ctx);
            bl2_log(ctx, "debug", "browser: terminating Chromium pid %s failed: %s", la);
            if (bl2_exc_pending(ctx)) return NULL;
        } else {
            return NULL;
        }
    }
    if (!bl2_is_none(proc)) {
        json_t *wa = json_object();
        bl2_arg_num(wa, "pid", bl2_proc_pid(proc));
        bl2_arg_num(wa, "timeout", 5);
        json_t *w = bl2_call(ctx, "proc.wait", wa);
        json_free(w);
        if (bl2_exc_pending(ctx)) {
            if (bl2_exc_is_timeoutexpired(ctx)) {
                bl2_exc_clear(ctx);
                json_t *ka = json_object();
                bl2_arg_num(ka, "pid", bl2_proc_pid(proc));
                json_t *k = bl2_call(ctx, "proc.kill", ka);
                json_free(k);
                if (bl2_exc_pending(ctx)) return NULL;
            } else {
                return NULL;
            }
        }
    }
    return json_null();
}

/* ── 26. _forget ─────────────────────────────────────────────────────────── */
/* PoP: bl2_forget @ tools/browser_launcher.py:_forget */
static json_t *bl2_forget(bl2_ctx *ctx, const char *key, const json_t *browser_arg,
                          bool drop_record) {
    /* copy the browser so popping it from state never frees our local reference */
    json_t *browser = bl2_copy_or_null(browser_arg);
    json_t *heir = bl2_heir_key(ctx, browser_arg);
    if (bl2_exc_pending(ctx)) { json_free(browser); json_free(heir); return NULL; }
    json_t *proc = NULL;
    json_t *browsers = bl2_browsers(ctx);
    json_t *procs = bl2_procs(ctx);
    if (bl2_get(browsers, key) == browser_arg) {  /* _browsers.get(key) is browser */
        json_obj_del(browsers, key);
        json_t *pn = bl2_get(procs, key);
        proc = pn ? json_copy(pn) : NULL;
        json_obj_del(procs, key);
    }
    if (proc && !bl2_is_none(heir)) {
        if (!json_has(procs, bl2_jstr(heir)))
            json_set(procs, bl2_jstr(heir), proc);  /* setdefault transfers ownership */
        else
            json_free(proc);
        proc = NULL;
    }
    if (proc) {
        json_t *wa = json_object();
        bl2_arg_num(wa, "pid", bl2_proc_pid(proc));
        bl2_arg_num(wa, "timeout", 0);
        json_t *w = bl2_call(ctx, "proc.wait", wa);
        json_free(w);
        json_free(proc);
        proc = NULL;
        if (bl2_exc_pending(ctx)) {
            if (bl2_exc_is_timeoutexpired(ctx)) bl2_exc_clear(ctx);
            else { json_free(browser); json_free(heir); return NULL; }
        }
    }
    if (!json_get_bool(browser, "ephemeral", false) && bl2_is_none(heir)) {
        json_t *record = bl2_record_path(ctx, browser);
        if (bl2_exc_pending(ctx)) { json_free(browser); json_free(heir); json_free(record); return NULL; }
        if (!bl2_is_none(record)) {
            json_t *da = json_object();
            bl2_arg_str(da, "record", bl2_jstr(record));
            json_t *d = bl2_call(ctx, "drop_lease", da);
            json_free(d);
            if (bl2_exc_pending(ctx)) { json_free(browser); json_free(heir); json_free(record); return NULL; }
        }
        json_free(record);
    }
    if (json_get_bool(browser, "owned", false) && drop_record) {
        json_t *rr = bl2_remove_record(ctx, browser);
        json_free(rr);
        if (bl2_exc_pending(ctx)) { json_free(browser); json_free(heir); return NULL; }
        if (json_get_bool(browser, "ephemeral", false)) {
            json_t *ra = json_object();
            bl2_arg_str(ra, "path", bl2_jstr(bl2_get(browser, "user_data_dir")));
            bl2_arg_bool(ra, "ignore_errors", true);
            json_t *rm = bl2_call(ctx, "rmtree", ra);
            json_free(rm);
            if (bl2_exc_pending(ctx)) bl2_exc_clear(ctx);  /* ignore_errors=True swallows */
        }
    }
    json_free(browser); json_free(heir);
    return json_null();
}

/* ── 27. get_browser ─────────────────────────────────────────────────────── */
/* PoP: bl2_get_browser @ tools/browser_launcher.py:get_browser */
static json_t *bl2_get_browser(bl2_ctx *ctx, const char *key) {
    json_t *browsers = bl2_browsers(ctx);
    json_t *browser = bl2_get(browsers, key);
    if (bl2_is_none(browser)) return json_null();
    if (!json_get_bool(browser, "foreign", false)) {
        json_t *al = bl2_alive(ctx, browser);
        if (bl2_exc_pending(ctx)) { json_free(al); return NULL; }
        if (bl2_truthy(al)) { json_free(al); return json_copy(browser); }
        json_free(al);
        json_t *la = json_array();
        json_append(la, json_string(key));
        json_append(la, bl2_copy_or_null(bl2_get(browser, "pid")));
        bl2_log(ctx, "info", "browser: Chromium for %s (pid %s) is gone", la);
        if (bl2_exc_pending(ctx)) return NULL;
        json_t *f = bl2_forget(ctx, key, browser, true);
        json_free(f);
        if (bl2_exc_pending(ctx)) return NULL;
        return json_null();
    }
    json_t *fa = bl2_foreign_alive(ctx, browser);
    if (bl2_exc_pending(ctx)) { json_free(fa); return NULL; }
    if (bl2_truthy(fa)) { json_free(fa); return json_copy(browser); }
    json_free(fa);
    if (bl2_get(browsers, key) == browser) {
        json_t *la = json_array();
        json_append(la, json_string(key));
        bl2_log(ctx, "info", "browser: the Chromium another environment ran for %s is gone", la);
        if (bl2_exc_pending(ctx)) return NULL;
        json_t *f = bl2_forget(ctx, key, browser, true);
        json_free(f);
        if (bl2_exc_pending(ctx)) return NULL;
    }
    return json_null();
}

/* ── 28. _last_holder_out ────────────────────────────────────────────────── */
/* PoP: bl2_last_holder_out @ tools/browser_launcher.py:_last_holder_out */
static json_t *bl2_last_holder_out_norm(bl2_ctx *ctx, const char *record, const json_t *browser) {
    json_t *da = json_object();
    bl2_arg_str(da, "record", record ? record : "");
    json_t *d = bl2_call(ctx, "drop_lease", da);
    json_free(d);
    if (bl2_exc_pending(ctx)) return NULL;
    if (json_get_bool(browser, "foreign", false)) return json_bool(false);
    json_t *oa = json_object();
    bl2_arg_str(oa, "record", record ? record : "");
    bl2_arg(oa, "start_time", bl2_get(browser, "start_time"));
    json_t *others = bl2_call(ctx, "other_live_holders", oa);
    if (bl2_exc_pending(ctx)) { json_free(others); return NULL; }
    json_t *data = NULL;
    json_t *ea = json_object();
    bl2_arg_str(ea, "path", record ? record : "");
    json_t *ex = bl2_call(ctx, "exists", ea);
    if (bl2_exc_pending(ctx)) { json_free(others); json_free(ex); return NULL; }
    if (bl2_truthy(ex)) {
        data = bl2_load_record(ctx, record);
        if (bl2_exc_pending(ctx)) { json_free(others); json_free(ex); json_free(data); return NULL; }
    }
    json_free(ex);
    if (!bl2_is_none(data)) {
        json_t *ri = bl2_record_is(ctx, data, browser);
        if (bl2_exc_pending(ctx)) { json_free(others); json_free(data); json_free(ri); return NULL; }
        if (!bl2_truthy(ri)) { json_free(data); data = NULL; }
        json_free(ri);
    }
    bool held_elsewhere = false;
    if (!bl2_is_none(data)) {
        json_t *ha = json_object();
        bl2_arg(ha, "data", data);
        bl2_arg_str(ha, "field", "owner_pid");
        json_t *hh = bl2_call(ctx, "held_here", ha);
        if (bl2_exc_pending(ctx)) { json_free(others); json_free(data); json_free(hh); return NULL; }
        bool here = bl2_truthy(hh);
        json_free(hh);
        if (!here) {
            json_t *oalive = bl2_owner_alive(ctx, data);
            if (bl2_exc_pending(ctx)) { json_free(others); json_free(data); json_free(oalive); return NULL; }
            held_elsewhere = bl2_truthy(oalive);
            json_free(oalive);
        }
    }
    json_t *sa = json_object();
    bl2_arg(sa, "others", others);
    json_t *heir = bl2_call(ctx, "successor", sa);
    if (bl2_exc_pending(ctx)) { json_free(others); json_free(data); json_free(heir); return NULL; }
    if (!bl2_is_none(heir) && !bl2_is_none(data) && !held_elsewhere) {
        json_t *hoa = json_object();
        bl2_arg_str(hoa, "record", record ? record : "");
        bl2_arg(hoa, "data", data);
        bl2_arg(hoa, "heir", heir);
        json_t *ho = bl2_call(ctx, "hand_over", hoa);
        json_free(ho);
        if (bl2_exc_pending(ctx)) { json_free(others); json_free(data); json_free(heir); return NULL; }
    }
    bool result;
    if (bl2_truthy(others) || held_elsewhere) result = false;
    else result = json_get_bool(browser, "owned", false) || !bl2_is_none(data);
    json_free(others); json_free(data); json_free(heir);
    return json_bool(result);
}

/* LIVE _last_holder_out takes a Path; every recorded seam arg is that Path
 * projected with as_posix(). Normalize the incoming record string the same
 * PurePath way BEFORE it reaches any seam (record strings may carry
 * backslashes, duplicate separators, or a trailing separator). */
static json_t *bl2_last_holder_out(bl2_ctx *ctx, const char *record, const json_t *browser) {
    char *nrecord = bl2_posix_norm(record);
    json_t *r = bl2_last_holder_out_norm(ctx, nrecord ? nrecord : "", browser);
    free(nrecord);
    return r;
}

/* ── 29. _stop_locked ────────────────────────────────────────────────────── */
/* PoP: bl2_stop_locked @ tools/browser_launcher.py:_stop_locked */
static json_t *bl2_stop_locked(bl2_ctx *ctx, const char *key) {
    json_t *browser = bl2_get(bl2_browsers(ctx), key);
    json_t *proc = bl2_get(bl2_procs(ctx), key);
    if (bl2_is_none(browser)) return json_null();
    json_t *record = NULL;
    if (!json_get_bool(browser, "ephemeral", false)) {
        record = bl2_record_path(ctx, browser);
        if (bl2_exc_pending(ctx)) { json_free(record); return NULL; }
    }
    if (bl2_is_none(record)) {
        json_free(record);
        if (json_get_bool(browser, "owned", false)) {
            json_t *t = bl2_terminate(ctx, bl2_get(browser, "pid"),
                                      bl2_get(browser, "start_time"), proc);
            json_free(t);
            if (bl2_exc_pending(ctx)) return NULL;
        }
        json_t *f = bl2_forget(ctx, key, browser, true);
        json_free(f);
        if (bl2_exc_pending(ctx)) return NULL;
        return json_null();
    }
    /* with records_lock(record.parent): */
    char *parent = bl2_posix_parent(bl2_jstr(record));
    json_t *la = json_object();
    bl2_arg_str(la, "path", parent);
    json_t *lock = bl2_call(ctx, "records_lock", la);
    json_free(lock);
    free(parent);
    if (bl2_exc_pending(ctx)) { json_free(record); return NULL; }
    json_t *heir = bl2_heir_key(ctx, browser);
    if (bl2_exc_pending(ctx)) { json_free(record); json_free(heir); return NULL; }
    if (bl2_is_none(heir)) {
        json_t *lh = bl2_last_holder_out(ctx, bl2_jstr(record), browser);
        if (bl2_exc_pending(ctx)) { json_free(record); json_free(heir); json_free(lh); return NULL; }
        if (bl2_truthy(lh)) {
            json_t *t = bl2_terminate(ctx, bl2_get(browser, "pid"),
                                      bl2_get(browser, "start_time"), proc);
            json_free(t);
            if (bl2_exc_pending(ctx)) { json_free(record); json_free(heir); json_free(lh); return NULL; }
            json_t *ua = json_object();
            bl2_arg_str(ua, "path", bl2_jstr(record));
            bl2_arg_bool(ua, "missing_ok", true);
            json_t *u = bl2_call(ctx, "unlink", ua);
            json_free(u);
            if (bl2_exc_pending(ctx)) { json_free(record); json_free(heir); json_free(lh); return NULL; }
        }
        json_free(lh);
    }
    json_free(heir);
    json_t *f = bl2_forget(ctx, key, browser, false);
    json_free(f);
    json_free(record);
    if (bl2_exc_pending(ctx)) return NULL;
    return json_null();
}

/* ── 30. stop_browser ────────────────────────────────────────────────────── */
/* PoP: bl2_stop_browser @ tools/browser_launcher.py:stop_browser */
static json_t *bl2_stop_browser(bl2_ctx *ctx, const char *key) {
    json_t *ka = json_object();
    bl2_arg_str(ka, "key", key ? key : "");
    json_t *lock = bl2_call(ctx, "_key_lock", ka);
    json_free(lock);
    if (bl2_exc_pending(ctx)) return NULL;
    return bl2_stop_locked(ctx, key);
}

/* ── 31. stop_all_browsers ───────────────────────────────────────────────── */
/* PoP: bl2_stop_all_browsers @ tools/browser_launcher.py:stop_all_browsers */
static json_t *bl2_stop_all_browsers(bl2_ctx *ctx) {
    json_t *browsers = bl2_browsers(ctx);
    /* keys = list(_browsers) — snapshot before stopping mutates the dict */
    json_t *keys = json_array();
    if (bl2_is_obj(browsers))
        for (size_t i = 0; i < browsers->c.count; i++)
            json_append(keys, json_string(browsers->c.keys[i]));
    for (size_t i = 0; i < keys->c.count; i++) {
        json_t *r = bl2_stop_browser(ctx, bl2_jstr(keys->c.items[i]));
        json_free(r);
        if (bl2_exc_pending(ctx)) { json_free(keys); return NULL; }
    }
    json_free(keys);
    return json_null();
}

/* ── 32. _abandon_launch ─────────────────────────────────────────────────── */
/* PoP: bl2_abandon_launch @ tools/browser_launcher.py:_abandon_launch */
static json_t *bl2_abandon_launch(bl2_ctx *ctx, const json_t *proc_arg, const json_t *starting_arg) {
    json_t *proc = bl2_copy_or_null(proc_arg);
    json_t *starting = bl2_copy_or_null(starting_arg);
    char pidbuf[64];
    bl2_num_str(pidbuf, sizeof pidbuf, bl2_proc_pid(proc));
    json_obj_del(bl2_launching(ctx), pidbuf);  /* _launching.pop(proc.pid, None) */
    json_t *pa = json_object();
    bl2_arg_num(pa, "pid", bl2_proc_pid(proc));
    json_t *poll = bl2_call(ctx, "proc.poll", pa);
    if (bl2_exc_pending(ctx)) { json_free(proc); json_free(starting); json_free(poll); return NULL; }
    if (bl2_is_none(poll)) {
        json_t *t = bl2_terminate(ctx, bl2_get(proc, "pid"),
                                  bl2_get(starting, "start_time"), proc);
        json_free(t);
        if (bl2_exc_pending(ctx)) { json_free(proc); json_free(starting); json_free(poll); return NULL; }
    }
    json_free(poll);
    json_t *rr = bl2_remove_record(ctx, starting);
    json_free(rr);
    if (bl2_exc_pending(ctx)) { json_free(proc); json_free(starting); return NULL; }
    if (json_get_bool(starting, "ephemeral", false)) {
        json_t *ra = json_object();
        bl2_arg_str(ra, "path", bl2_jstr(bl2_get(starting, "user_data_dir")));
        bl2_arg_bool(ra, "ignore_errors", true);
        json_t *rm = bl2_call(ctx, "rmtree", ra);
        json_free(rm);
        if (bl2_exc_pending(ctx)) bl2_exc_clear(ctx);  /* ignore_errors=True */
    }
    json_free(proc); json_free(starting);
    return json_null();
}

/* ── 33. _launch_owned ───────────────────────────────────────────────────── */
/* PoP: bl2_launch_owned @ tools/browser_launcher.py:_launch_owned */
static json_t *bl2_launch_owned(bl2_ctx *ctx, const json_t *in) {
    const char *key = bl2_jstr(bl2_get(in, "key"));
    const char *executable = bl2_jstr(bl2_get(in, "executable"));
    const char *udd = bl2_jstr(bl2_get(in, "user_data_dir"));
    bool ephemeral = json_get_bool(in, "ephemeral", false);
    bool packaged = json_get_bool(in, "packaged", false);
    bool sandbox_bypass = json_get_bool(in, "sandbox_bypass", true);
    const char *home_key = bl2_jstr(bl2_get(in, "home_key"));
    json_t *env = NULL;
    json_t *env_in = bl2_get(in, "browser_env");
    if (bl2_is_none(env_in)) {
        env = bl2_call(ctx, "build_browser_env", json_object());
        if (bl2_exc_pending(ctx)) { json_free(env); return NULL; }
    } else {
        env = json_copy(env_in);
    }
    /* try: _prepare_dir(udd, ephemeral) except OSError -> (None, msg) */
    json_t *pda = json_object();
    bl2_arg_str(pda, "user_data_dir", udd);
    bl2_arg_bool(pda, "ephemeral", ephemeral);
    json_t *pd = bl2_call(ctx, "_prepare_dir", pda);
    json_free(pd);
    if (bl2_exc_pending(ctx)) {
        if (bl2_exc_is_oserror(ctx)) {
            char *msg = (char *)malloc(strlen(udd) + strlen(json_get_str(ctx->exc, "msg", "")) + 64);
            sprintf(msg, "Could not create the browser profile directory %s: %s",
                    udd, json_get_str(ctx->exc, "msg", ""));
            bl2_exc_clear(ctx);
            json_free(env);
            return bl2_pair(NULL, json_string(msg));
        }
        json_free(env); return NULL;
    }
    /* argv = build_chromium_argv(executable, udd, packaged=, headed=_resolve_headed(...), ...) */
    json_t *ha = json_object();
    bl2_arg(ha, "headed", bl2_get(in, "headed"));
    bl2_arg(ha, "env", env);
    json_t *headed = bl2_call(ctx, "_resolve_headed", ha);
    if (bl2_exc_pending(ctx)) { json_free(env); json_free(headed); return NULL; }
    json_t *ba = json_object();
    bl2_arg_str(ba, "executable", executable);
    bl2_arg_str(ba, "user_data_dir", udd);
    bl2_arg_bool(ba, "packaged", packaged);
    bl2_arg(ba, "headed", headed);
    bl2_arg_bool(ba, "sandbox_bypass", sandbox_bypass);
    json_t *extra = bl2_get(in, "extra_args");
    if (bl2_is_arr(extra)) bl2_arg(ba, "extra_args", extra);
    else bl2_arg(ba, "extra_args", json_array());
    json_t *argv = bl2_call(ctx, "build_chromium_argv", ba);
    json_free(headed);
    if (bl2_exc_pending(ctx)) { json_free(env); json_free(argv); return NULL; }
    /* try: proc = _spawn(argv, env, udd) except (OSError, SubprocessError) -> (None, msg) */
    json_t *spa = json_object();
    bl2_arg(spa, "argv", argv);
    bl2_arg(spa, "env", env);
    bl2_arg_str(spa, "user_data_dir", udd);
    json_t *proc = bl2_call(ctx, "_spawn", spa);
    if (bl2_exc_pending(ctx)) {
        static const char *SE[] = {"OSError", "SubprocessError"};
        if (bl2_exc_matches(ctx, SE, 2)) {
            char *msg = (char *)malloc(strlen(executable) + strlen(json_get_str(ctx->exc, "msg", "")) + 64);
            sprintf(msg, "Failed to launch Chromium (%s): %s",
                    executable, json_get_str(ctx->exc, "msg", ""));
            bl2_exc_clear(ctx);
            json_free(env); json_free(argv); json_free(proc);
            return bl2_pair(NULL, json_string(msg));
        }
        json_free(env); json_free(argv); json_free(proc); return NULL;
    }
    json_t *sta = json_object();
    bl2_arg_num(sta, "pid", bl2_proc_pid(proc));
    json_t *start_time = bl2_call(ctx, "_start_time", sta);
    if (bl2_exc_pending(ctx)) { json_free(env); json_free(argv); json_free(proc); json_free(start_time); return NULL; }
    /* starting = _make_browser(key, 0, "", pid=proc.pid, start_time=..., owned=True, ...) */
    json_t *mb = json_object();
    bl2_arg_str(mb, "key", key);
    bl2_arg_num(mb, "port", 0);
    bl2_arg_str(mb, "path", "");
    json_set(mb, "pid", json_number(bl2_proc_pid(proc)));
    json_set(mb, "start_time", bl2_copy_or_null(start_time));
    bl2_arg_str(mb, "user_data_dir", udd);
    bl2_arg_bool(mb, "owned", true);
    bl2_arg_bool(mb, "ephemeral", ephemeral);
    bl2_arg_str(mb, "home_key", home_key);
    json_t *starting = bl2_make_browser(ctx, mb);
    json_free(mb);
    char pidbuf[64];
    bl2_num_str(pidbuf, sizeof pidbuf, bl2_proc_pid(proc));
    json_set(bl2_launching(ctx), pidbuf, json_copy(starting));  /* _launching[proc.pid] = starting */
    json_t *wr = bl2_write_record(ctx, starting);
    json_free(wr);
    if (bl2_exc_pending(ctx)) {
        json_free(env); json_free(argv); json_free(proc); json_free(start_time); json_free(starting);
        return NULL;
    }
    /* found, err = _wait_for_devtools(proc, udd, time.monotonic() + _launch_timeout(timeout)) */
    json_t *mo = bl2_call(ctx, "monotonic", json_object());
    if (bl2_exc_pending(ctx)) {
        json_free(env); json_free(argv); json_free(proc); json_free(start_time); json_free(starting); json_free(mo);
        return NULL;
    }
    json_t *ta = json_object();
    bl2_arg(ta, "timeout", bl2_get(in, "timeout"));
    json_t *lt = bl2_call(ctx, "_launch_timeout", ta);
    if (bl2_exc_pending(ctx)) {
        json_free(env); json_free(argv); json_free(proc); json_free(start_time); json_free(starting); json_free(mo); json_free(lt);
        return NULL;
    }
    double deadline = bl2_num(mo) + bl2_num(lt);
    json_free(mo); json_free(lt);
    json_t *wa = json_object();
    bl2_arg_num(wa, "pid", bl2_proc_pid(proc));
    bl2_arg_str(wa, "user_data_dir", udd);
    bl2_arg_num(wa, "deadline", deadline);
    json_t *waitret = bl2_call(ctx, "_wait_for_devtools", wa);
    if (bl2_exc_pending(ctx)) {
        /* except BaseException: _abandon_launch(proc, starting); raise */
        json_t *saved = ctx->exc;
        ctx->exc = NULL;
        json_t *ab = bl2_abandon_launch(ctx, proc, starting);
        json_free(ab);
        if (bl2_exc_pending(ctx)) {
            json_free(saved);  /* a new raise inside abandon replaces the original */
        } else {
            ctx->exc = saved;  /* bare raise re-raises the original */
        }
        json_free(env); json_free(argv); json_free(proc); json_free(start_time); json_free(starting); json_free(waitret);
        return NULL;
    }
    json_t *found = bl2_t0(waitret);
    json_t *err = bl2_t1(waitret);
    if (bl2_is_none(found)) {
        json_t *ab = bl2_abandon_launch(ctx, proc, starting);
        json_free(ab);
        if (bl2_exc_pending(ctx)) {
            json_free(env); json_free(argv); json_free(proc); json_free(start_time); json_free(starting); json_free(waitret);
            return NULL;
        }
        json_t *r = bl2_pair(NULL, bl2_copy_or_null(err));
        json_free(env); json_free(argv); json_free(proc); json_free(start_time); json_free(starting); json_free(waitret);
        return r;
    }
    /* browser = _make_browser(key, found[0], found[1], ...) */
    json_t *mb2 = json_object();
    bl2_arg_str(mb2, "key", key);
    bl2_arg(mb2, "port", bl2_t0(found));
    bl2_arg(mb2, "path", bl2_t1(found));
    json_set(mb2, "pid", json_number(bl2_proc_pid(proc)));
    json_set(mb2, "start_time", bl2_copy_or_null(start_time));
    bl2_arg_str(mb2, "user_data_dir", udd);
    bl2_arg_bool(mb2, "owned", true);
    bl2_arg_bool(mb2, "ephemeral", ephemeral);
    bl2_arg_str(mb2, "home_key", home_key);
    json_t *browser = bl2_make_browser(ctx, mb2);
    json_free(mb2);
    json_t *wr2 = bl2_write_record(ctx, browser);
    json_free(wr2);
    if (bl2_exc_pending(ctx)) {
        json_free(env); json_free(argv); json_free(proc); json_free(start_time); json_free(starting); json_free(waitret); json_free(browser);
        return NULL;
    }
    json_obj_del(bl2_launching(ctx), pidbuf);  /* _launching.pop(proc.pid, None) */
    json_set(bl2_browsers(ctx), key, json_copy(browser));
    json_set(bl2_procs(ctx), key, json_copy(proc));
    json_t *la = json_array();
    json_append(la, json_number(bl2_proc_pid(proc)));
    json_append(la, json_string(key));
    json_append(la, bl2_copy_or_null(found));
    bl2_log(ctx, "info", "browser: launched Chromium pid %s for %s (port %s)", la);
    json_t *r = bl2_pair(browser, NULL);  /* browser ownership transfers */
    json_free(env); json_free(argv); json_free(proc); json_free(start_time); json_free(starting); json_free(waitret);
    if (bl2_exc_pending(ctx)) { json_free(r); return NULL; }
    return r;
}