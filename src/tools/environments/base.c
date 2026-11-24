/*
 * base.c - C11 port of upstream Hermes tools/environments/base.py
 * (gates w30_base_0c99d4_p1, w30_base_0c99d4_p2).
 *
 * PoP: w30_base_0c99d4_sandbox_dir            @ tools/environments/base.py:get_sandbox_dir
 * PoP: w30_base_0c99d4_uses_host_filesystem   @ tools/environments/base.py:env_uses_host_filesystem
 * PoP: w30_base_0c99d4_activity_due           @ tools/environments/base.py:touch_activity_if_due
 * PoP: w30_base_0c99d4_env_name_valid         @ tools/environments/base.py:BaseEnvironment._snapshot_excluded_passthrough_names
 * PoP: w30_base_0c99d4_snapshot_exclusions    @ tools/environments/base.py:BaseEnvironment._snapshot_excluded_passthrough_names
 * PoP: w30_base_0c99d4_sudo_nopasswd          @ tools/environments/base.py:BaseEnvironment._sudo_nopasswd_works
 * PoP: w30_base_0c99d4_probe_nonlogin         @ tools/environments/base.py:BaseEnvironment._probe_nonlogin_fallback
 * PoP: w30_base_0c99d4_init_paths             @ tools/environments/base.py:BaseEnvironment.__init__
 * PoP: w30_base_0c99d4_session_markers        @ tools/environments/base.py:BaseEnvironment.__init__ (marker trio)
 * PoP: w30_base_0c99d4_task_snapshot_path     @ tools/environments/base.py:BaseEnvironment._task_snapshot_path
 * PoP: w30_base_0c99d4_update_cwd             @ tools/environments/base.py:BaseEnvironment._update_cwd
 * PoP: w30_base_0c99d4_fetch_realpath         @ tools/environments/base.py:BaseEnvironment.fetch_realpath
 * PoP: w30_base_0c99d4_usable_cwd             @ tools/environments/base.py:BaseEnvironment._usable_cwd
 * PoP: w30_base_0c99d4_temp_dir               @ tools/environments/base.py:BaseEnvironment.get_temp_dir
 * PoP: w30_base_0c99d4_script_kwargs          @ tools/environments/base.py:BaseEnvironment._snapshot_script_kwargs
 * PoP: w30_base_0c99d4_quote_cwd_for_cd       @ tools/environments/base.py:BaseEnvironment._quote_cwd_for_cd
 * PoP: w30_base_0c99d4_quote_shell_path       @ tools/environments/base.py:BaseEnvironment._quote_shell_path
 * PoP: w30_base_0c99d4_stdin_redirect         @ tools/environments/base.py:BaseEnvironment._redirect_stdin_from_file
 * PoP: w30_base_0c99d4_stdin_heredoc          @ tools/environments/base.py:BaseEnvironment._embed_stdin_heredoc
 * PoP: w30_base_0c99d4_staged_stdin_path      @ tools/environments/base.py:BaseEnvironment._staged_stdin_path
 * PoP: w30_base_0c99d4_split_cwd_marker       @ tools/environments/base.py:_split_cwd_marker
 * PoP: w30_base_0c99d4_parse_cwd_marker       @ tools/environments/base.py:BaseEnvironment._parse_cwd_marker
 * PoP: w30_base_0c99d4_wrap_command           @ tools/environments/base.py:BaseEnvironment._wrap_command
 * PoP: w30_base_0c99d4_bootstrap_script       @ tools/environments/base.py:BaseEnvironment.init_session (snapshot bootstrap)
 *
 * What is ported here is the value core of the base environment: path and
 * marker derivation, capability predicates, env-name validation, the sudo and
 * non-login probes' coercion, and the bash session scripting (quoting,
 * heredoc/redirect wrapping, the per-command wrapper and the snapshot
 * bootstrap).  Every seam stays in the caller and arrives as fixture input:
 * the host environ (sandbox_dir, exclusions), the backend's bash process (sudo
 * probe, non-login probe), the exec-channel result (fetch_realpath, cwd
 * parsing), the uuid token (heredoc, staged stdin) and the passthrough
 * allowlist.  Nothing here spawns a process, touches a socket, or reads the
 * host environment.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include "noc_w30_base_0c99d4_p1.h"
#include "noc_w30_base_0c99d4_p2.h"

/* ------------------------------------------------------------------ */
/* string builder                                                      */
/* ------------------------------------------------------------------ */

typedef struct { char *p; size_t n, cap; } sb;

static void sb_init(sb *b) { b->p = NULL; b->n = 0; b->cap = 0; }

static void sb_need(sb *b, size_t extra) {
    size_t want = b->n + extra + 1;
    char *np;
    if (want <= b->cap) return;
    if (b->cap == 0) b->cap = 64;
    while (b->cap < want) b->cap *= 2;
    np = (char *)realloc(b->p, b->cap);
    if (!np) { free(b->p); b->p = NULL; b->n = b->cap = 0; return; }
    b->p = np;
}

static void sb_addn(sb *b, const char *s, size_t n) {
    if (!s || !n) return;
    sb_need(b, n);
    if (!b->p) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void sb_add(sb *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }

static void sb_addc(sb *b, char c) { sb_addn(b, &c, 1); }

static void sb_printf(sb *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof(tmp)) { sb_addn(b, tmp, (size_t)n); return; }
    sb_need(b, (size_t)n);
    if (!b->p) return;
    va_start(ap, fmt);
    vsnprintf(b->p + b->n, b->cap - b->n, fmt, ap);
    va_end(ap);
    b->n += (size_t)n;
}

static char *sb_take(sb *b) {
    char *out;
    if (!b->p) { out = (char *)malloc(1); if (out) out[0] = '\0'; b->n = b->cap = 0; return out; }
    out = b->p;
    b->p = NULL; b->n = b->cap = 0;
    return out;
}

static char *xdup(const char *s) {
    size_t n;
    char *o;
    if (!s) return NULL;
    n = strlen(s);
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

/* ------------------------------------------------------------------ */
/* Python value coercions                                              */
/* ------------------------------------------------------------------ */

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static const char *jstr(const json_t *o, const char *k) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return NULL;
}

static const char *jstr_or(const json_t *o, const char *k, const char *def) {
    const char *s = jstr(o, k);
    return s ? s : def;
}

static const char *jnode_str(const json_t *v) {
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return NULL;
}

static double jnum(const json_t *o, const char *k, double def) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    if (v && v->type == JSON_NUMBER) return v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* bool(x), with a default only for an *absent* key (explicit null is false). */
static int jbool(const json_t *o, const char *k, int def) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    if (!v) return def;
    return py_truthy(v);
}

/* int(v or 0) over the shapes an exec result's returncode can hold. */
static long long py_int_or_zero(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING && v->str_val) {
        const char *s = v->str_val;
        char *end = NULL;
        long long got;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        got = strtoll(s, &end, 10);
        if (end && end != s) {
            while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
            if (*end == '\0') return got;
        }
        return 0;
    }
    return 0;
}

/* Python str.strip() over ASCII whitespace; malloc'd. */
static char *s_strip(const char *s) {
    const char *a, *b;
    if (!s) return xdup("");
    a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    {
        size_t n = (size_t)(b - a);
        char *o = (char *)malloc(n + 1);
        if (!o) return NULL;
        memcpy(o, a, n);
        o[n] = '\0';
        return o;
    }
}

/* temp_dir.rstrip("/") or "/" */
static char *s_temp_root(const char *temp_dir) {
    size_t n = temp_dir ? strlen(temp_dir) : 0;
    while (n > 0 && temp_dir[n - 1] == '/') n--;
    if (n == 0) return xdup("/");
    {
        char *o = (char *)malloc(n + 1);
        if (!o) return NULL;
        memcpy(o, temp_dir, n);
        o[n] = '\0';
        return o;
    }
}

/* ------------------------------------------------------------------ */
/* shlex.quote                                                         */
/* ------------------------------------------------------------------ */

static int sh_safe_char(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return 1;
    switch (c) {
    case '_': case '@': case '%': case '+': case '=':
    case ':': case ',': case '.': case '/': case '-':
        return 1;
    default:
        return 0;
    }
}

static char *sh_quote(const char *s) {
    size_t i, n;
    int safe = 1;
    sb b;
    if (!s || !s[0]) return xdup("''");
    n = strlen(s);
    for (i = 0; i < n; i++)
        if (!sh_safe_char((unsigned char)s[i])) { safe = 0; break; }
    if (safe) return xdup(s);
    sb_init(&b);
    sb_addc(&b, '\'');
    for (i = 0; i < n; i++) {
        if (s[i] == '\'') sb_add(&b, "'\"'\"'");   /* shlex.quote */
        else sb_addc(&b, s[i]);
    }
    sb_addc(&b, '\'');
    return sb_take(&b);
}

/* ------------------------------------------------------------------ */
/* Path normalisation (posix form of a Path(...) join)                 */
/* ------------------------------------------------------------------ */

static char *posix_norm(const char *s) {
    sb b;
    int abs = 0;
    size_t i = 0, n = s ? strlen(s) : 0;
    sb_init(&b);
    if (n && s[0] == '/') { abs = 1; sb_addc(&b, '/'); }
    while (i < n) {
        size_t start, len;
        while (i < n && s[i] == '/') i++;
        start = i;
        while (i < n && s[i] != '/') i++;
        len = i - start;
        if (len == 0) continue;
        if (len == 1 && s[start] == '.') continue;
        if (b.n && b.p[b.n - 1] != '/') sb_addc(&b, '/');
        sb_addn(&b, s + start, len);
    }
    if (b.n == 0) {
        if (abs) sb_addc(&b, '/');
        else sb_addc(&b, '.');
    }
    return sb_take(&b);
}

/* ------------------------------------------------------------------ */
/* SHA-256 (stdlib only; _task_snapshot_path digests the task id)      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t h[8];
    uint64_t bits;
    unsigned char buf[64];
    size_t have;
} sha256_ctx;

static const uint32_t SHA256_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(sha256_ctx *c, const unsigned char *p) {
    uint32_t w[64], a, b, cc, d, e, f, g, h;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROR32(w[i - 15], 7) ^ ROR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR32(w[i - 2], 17) ^ ROR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3];
    e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + SHA256_K[i] + w[i];
        uint32_t S0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
        uint32_t mj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

static void sha256_init(sha256_ctx *c) {
    c->h[0] = 0x6a09e667u; c->h[1] = 0xbb67ae85u; c->h[2] = 0x3c6ef372u; c->h[3] = 0xa54ff53au;
    c->h[4] = 0x510e527fu; c->h[5] = 0x9b05688cu; c->h[6] = 0x1f83d9abu; c->h[7] = 0x5be0cd19u;
    c->bits = 0;
    c->have = 0;
}

static void sha256_update(sha256_ctx *c, const void *data, size_t n) {
    const unsigned char *p = (const unsigned char *)data;
    c->bits += (uint64_t)n * 8u;
    while (n > 0) {
        size_t take = 64 - c->have;
        if (take > n) take = n;
        memcpy(c->buf + c->have, p, take);
        c->have += take;
        p += take;
        n -= take;
        if (c->have == 64) { sha256_block(c, c->buf); c->have = 0; }
    }
}

static void sha256_hex(const void *data, size_t n, char out[65]) {
    sha256_ctx c;
    unsigned char tail[72];
    uint64_t bits;
    static const char *HEX = "0123456789abcdef";
    int i;
    sha256_init(&c);
    sha256_update(&c, data, n);
    bits = c.bits;
    memset(tail, 0, sizeof(tail));
    tail[0] = 0x80;
    {
        size_t pad = (c.have < 56) ? (56 - c.have) : (120 - c.have);
        size_t k;
        for (k = 0; k < 8; k++) tail[pad + k] = (unsigned char)(bits >> (56 - 8 * k));
        sha256_update(&c, tail, pad + 8);
    }
    for (i = 0; i < 8; i++) {
        int j;
        for (j = 0; j < 4; j++) {
            unsigned char byte = (unsigned char)(c.h[i] >> (24 - 8 * j));
            out[i * 8 + j * 2] = HEX[byte >> 4];
            out[i * 8 + j * 2 + 1] = HEX[byte & 0xf];
        }
    }
    out[64] = '\0';
}

/* ------------------------------------------------------------------ */
/* marker splitting / cwd parsing                                      */
/* ------------------------------------------------------------------ */

static long find_all(const char *hay, size_t haylen, const char *needle, size_t nl, long maxstart) {
    long best = -1, i, lim = maxstart;
    if (nl == 0) return -1;
    if (lim > (long)haylen) lim = (long)haylen;
    for (i = 0; i + (long)nl <= lim; i++)
        if (memcmp(hay + i, needle, nl) == 0) best = i;
    return best;
}

static long rfind_range(const char *hay, size_t haylen, const char *needle, size_t nl, long start, long end) {
    long best = -1, i;
    if (nl == 0) return -1;
    if (start < 0) start = 0;
    for (i = start; i + (long)nl <= end && (size_t)(i + (long)nl) <= haylen; i++)
        if (memcmp(hay + i, needle, nl) == 0) best = i;
    return best;
}

static long find_from(const char *hay, size_t haylen, const char *needle, size_t nl, long start) {
    long i;
    if (nl == 0) return -1;
    if (start < 0) start = 0;
    if ((size_t)start + nl > haylen) return -1;
    for (i = start; (size_t)i + nl <= haylen; i++)
        if (memcmp(hay + i, needle, nl) == 0) return i;
    return -1;
}

/* _split_cwd_marker: 1 on a complete pair; *path malloc'd (NULL for None). */
static int split_marker(const char *output, const char *marker, char **path_out, char **clean_out) {
    size_t olen, mlen;
    long last, first, search_start, line_start, line_end;
    char *path, *clean;
    sb b;
    if (!output || !marker) return 0;
    olen = strlen(output);
    mlen = strlen(marker);
    if (mlen == 0) return 0;
    last = find_all(output, olen, marker, mlen, (long)olen);
    if (last == -1) return 0;
    search_start = last - 4096;
    if (search_start < 0) search_start = 0;
    first = rfind_range(output, olen, marker, mlen, search_start, last);
    if (first == -1 || first == last) return 0;
    {
        size_t pn = (size_t)(last - (first + (long)mlen));
        char *raw = (char *)malloc(pn + 1);
        char *stripped;
        if (!raw) return 0;
        memcpy(raw, output + first + (long)mlen, pn);
        raw[pn] = '\0';
        stripped = s_strip(raw);
        free(raw);
        if (stripped && stripped[0]) path = stripped;
        else { free(stripped); path = NULL; }
    }
    line_start = find_all(output, olen, "\n", 1, first);
    if (line_start == -1) line_start = first;
    {
        long e = find_from(output, olen, "\n", 1, last + (long)mlen);
        line_end = (e != -1) ? e + 1 : (long)olen;
    }
    sb_init(&b);
    sb_addn(&b, output, (size_t)line_start);
    sb_add(&b, output + line_end);
    clean = sb_take(&b);
    *path_out = path;
    *clean_out = clean;
    return 1;
}

/* result["pipeline_status"] from the split statuses, or NULL (key absent). */
/* int(token): optional sign, digits, single underscores between digits. */
static int py_parse_int_token(const char *s, long long *out) {
    size_t i = 0;
    int neg = 0, digits = 0;
    long long v = 0;
    if (s[i] == '+' || s[i] == '-') { neg = (s[i] == '-'); i++; }
    if (!s[i]) return 0;
    for (; s[i]; i++) {
        if (isdigit((unsigned char)s[i])) { v = v * 10 + (s[i] - '0'); digits++; }
        else if (s[i] == '_' && digits > 0 && isdigit((unsigned char)s[i + 1])) continue;
        else return 0;
    }
    if (!digits) return 0;
    *out = neg ? -v : v;
    return 1;
}

/* result["pipeline_status"] from the split statuses, or NULL (key absent). */
static json_t *pipeline_status(const char *statuses) {
    json_t *arr;
    sb tok;
    size_t i = 0, n = statuses ? strlen(statuses) : 0;
    int ok = 1;
    sb_init(&tok);
    arr = json_array();
    while (i <= n) {
        int end = (i == n);
        if (!end && !isspace((unsigned char)statuses[i])) { sb_addc(&tok, statuses[i]); i++; continue; }
        if (tok.n > 0) {
            char *t = sb_take(&tok);
            long long v = 0;
            if (!py_parse_int_token(t, &v)) ok = 0;
            else json_append(arr, json_number((double)v));
            free(t);
        }
        if (end) break;
        i++;
    }
    if (!ok) { json_free(arr); return NULL; }
    return arr;
}

/* Shared shape of _parse_cwd_marker (with_session=0) / _update_cwd (1). */
static json_t *cwd_parse_core(const json_t *in, int with_session) {
    const char *output = jstr_or(in, "output", "");
    const char *pm = jstr_or(in, "pipe_marker", "");
    const char *cm = jstr_or(in, "cwd_marker", "");
    char *cur = xdup(output ? output : "");
    json_t *statuses = NULL;
    int have_statuses = 0;
    json_t *out;

    if (pm && pm[0] && cur) {
        char *sp = NULL, *sc = NULL;
        if (split_marker(cur, pm, &sp, &sc)) {
            free(cur);
            cur = sc;
            statuses = pipeline_status(sp);
            have_statuses = (statuses != NULL);
            free(sp);
        }
    }
    out = json_object();
    if (cm && cm[0] && cur) {
        char *cpath = NULL, *cclean = NULL;
        if (split_marker(cur, cm, &cpath, &cclean)) {
            free(cur);
            cur = cclean;
            if (cpath) {
                json_set(out, "cwd_observed", json_bool(true));
                json_set(out, "cwd", json_string(cpath));
            }
            free(cpath);
        }
    }
    json_set(out, "output", json_string(cur ? cur : ""));
    if (have_statuses) json_set(out, "pipeline_status", statuses);
    if (with_session) {
        const char *env_cwd = jstr_or(in, "env_cwd", "");
        int move = jbool(in, "move_session", 1);
        int observed = py_truthy(json_obj_get(out, "cwd_observed"));
        const char *cwd = jstr(out, "cwd");
        json_set(out, "session_cwd", json_string((move && observed && cwd) ? cwd : env_cwd));
    }
    free(cur);
    return out;
}

/* ------------------------------------------------------------------ */
/* gate p1: env / capability / derivation                              */
/* ------------------------------------------------------------------ */

json_t *w30_base_0c99d4_sandbox_dir(const json_t *in) {
    const json_t *env = in ? json_obj_get(in, "env") : NULL;
    const char *custom;
    char *root = NULL, *joined;
    json_t *out;
    int append = 1;                 /* only the home-derived root gains /sandboxes */
    if (env && env->type != JSON_OBJECT) env = NULL;

    custom = env ? jstr_or(env, "TERMINAL_SANDBOX_DIR", "") : "";
    if (custom && custom[0]) {
        root = posix_norm(custom);
        append = 0;
    } else {
        const char *hh = env ? jstr_or(env, "HERMES_HOME", "") : "";
        char *stripped = s_strip(hh ? hh : "");
        int blank = (!stripped || !stripped[0]);
        free(stripped);
        if (!blank) {
            root = posix_norm(hh);
        } else {
            const char *la = env ? jstr_or(env, "LOCALAPPDATA", "") : "";
            const char *suffix = env ? jstr_or(env, "HERMES_DATA_DIR_SUFFIX", "") : "";
            char *ls = s_strip(la ? la : "");
            sb b;
            sb_init(&b);
            sb_add(&b, (ls && ls[0]) ? ls : ".");   /* Path.home() is a host seam */
            sb_add(&b, "/hermes");
            sb_add(&b, suffix ? suffix : "");
            free(ls);
            root = posix_norm(sb_take(&b));
        }
    }
    {
        sb b;
        size_t rn = root ? strlen(root) : 0;
        sb_init(&b);
        sb_add(&b, root ? root : "");
        if (append) {
            if (rn && root[rn - 1] != '/') sb_addc(&b, '/');
            sb_add(&b, "sandboxes");
        }
        joined = sb_take(&b);
    }
    free(root);
    out = json_string(joined ? joined : "");
    free(joined);
    return out;
}

json_t *w30_base_0c99d4_uses_host_filesystem(const json_t *in) {
    int present = jbool(in, "present", 1);
    const json_t *v = in ? json_obj_get(in, "value") : NULL;
    int yes = (present && v && v->type == JSON_BOOL && v->bool_val) ? 1 : 0;
    return json_bool(yes ? true : false);
}

json_t *w30_base_0c99d4_activity_due(const json_t *in) {
    double last = jnum(in, "last_touch", 0.0);
    double start = jnum(in, "start", 0.0);
    double now = jnum(in, "now", 0.0);
    const json_t *iv = in ? json_obj_get(in, "interval") : NULL;
    double interval = iv ? jnum(in, "interval", 10.0) : 10.0;
    int has_cb = jbool(in, "has_cb", 0);
    int raises = jbool(in, "raise", 0);
    json_t *out = json_object();
    if (now - last < interval) {
        json_set(out, "fired", json_bool(false));
        json_set(out, "last_touch", json_number(last));
        json_set(out, "message", json_null());
        return out;
    }
    json_set(out, "fired", json_bool(true));
    json_set(out, "last_touch", json_number(now));
    if (has_cb && !raises) {
        const char *label = jstr_or(in, "label", "");
        sb b;
        char *msg;
        sb_init(&b);
        sb_add(&b, label);
        sb_printf(&b, " (%llds elapsed)", (long long)(now - start));
        msg = sb_take(&b);
        json_set(out, "message", json_string(msg ? msg : ""));
        free(msg);
    } else {
        json_set(out, "message", json_null());
    }
    return out;
}

json_t *w30_base_0c99d4_env_name_valid(const json_t *in) {
    const char *name = jstr(in, "name");
    size_t i, n;
    int ok;
    if (!name) return json_bool(false);
    n = strlen(name);
    if (n == 0) return json_bool(false);
    ok = (isalpha((unsigned char)name[0]) || name[0] == '_');
    for (i = 1; ok && i < n; i++)
        if (!(isalnum((unsigned char)name[i]) || name[i] == '_')) ok = 0;
    return json_bool(ok ? true : false);
}

json_t *w30_base_0c99d4_snapshot_exclusions(const json_t *in) {
    const json_t *names = in ? json_obj_get(in, "names") : NULL;
    const json_t *extra = in ? json_obj_get(in, "additional") : NULL;
    json_t *out = json_array();
    char **kept = NULL;
    size_t nk = 0, cap = 0, i, j;
    if (!jbool(in, "profile_scoped", 0)) return out;
    if (!jbool(in, "multiplex", 0)) return out;
    for (i = 0; i < 2; i++) {
        const json_t *src = (i == 0) ? names : extra;
        size_t k, n = (src && src->type == JSON_ARRAY) ? json_len(src) : 0;
        for (k = 0; k < n; k++) {
            const json_t *v = json_get(src, k);
            const char *s;
            json_t *probe, *verdict;
            if (!v || v->type != JSON_STRING) continue;
            s = v->str_val ? v->str_val : "";
            if (!s[0]) continue;
            probe = json_object();
            json_set(probe, "name", json_string(s));
            verdict = w30_base_0c99d4_env_name_valid(probe);
            json_free(probe);
            if (!py_truthy(verdict)) { json_free(verdict); continue; }
            json_free(verdict);
            if (nk == cap) {
                size_t ncap = cap ? cap * 2 : 8;
                char **np = (char **)realloc(kept, ncap * sizeof(char *));
                if (!np) break;
                kept = np;
                cap = ncap;
            }
            kept[nk++] = xdup(s);
        }
    }
    for (i = 0; i < nk; i++)
        for (j = i + 1; j < nk; j++)
            if (strcmp(kept[j], kept[i]) < 0) {
                char *t = kept[i];
                kept[i] = kept[j];
                kept[j] = t;
            }
    for (i = 0; i < nk; i++) {
        if (i > 0 && strcmp(kept[i], kept[i - 1]) == 0) { free(kept[i]); continue; }
        json_append(out, json_string(kept[i]));
        free(kept[i]);
    }
    free(kept);
    return out;
}

json_t *w30_base_0c99d4_sudo_nopasswd(const json_t *in) {
    int supported = jbool(in, "supported", 0);
    int raises = jbool(in, "probe_raises", 0);
    const json_t *rc = in ? json_obj_get(in, "probe_rc") : NULL;
    int ok = 0;
    if (!supported || raises) return json_bool(false);
    /* returncode == 0 with Python equality (False == 0, 0.0 == 0) */
    if (rc && rc->type == JSON_NUMBER) ok = (rc->num_val == 0.0);
    else if (rc && rc->type == JSON_BOOL) ok = (rc->bool_val == 0);
    return json_bool(ok ? true : false);
}

json_t *w30_base_0c99d4_probe_nonlogin(const json_t *in) {
    const char *detail = jstr_or(in, "detail", "");
    int raises = jbool(in, "probe_raises", 0);
    const json_t *rc = in ? json_obj_get(in, "probe_rc") : NULL;
    const char *stdout_s = jstr(in, "probe_stdout");
    json_t *out = json_array();
    if (raises) {
        sb b;
        char *msg;
        sb_init(&b);
        sb_add(&b, detail);
        sb_add(&b, "; non-login probe: ");
        sb_add(&b, jstr_or(in, "probe_exc", "probe failed"));
        msg = sb_take(&b);
        json_append(out, json_bool(false));
        json_append(out, json_string(msg ? msg : ""));
        free(msg);
        return out;
    }
    {
        long long code = py_int_or_zero(rc);
        int prefer = (code == 0);
        const char *text = detail;
        char *stripped = NULL;
        if (!prefer) {
            const char *src = stdout_s ? stdout_s : detail;
            stripped = s_strip(src ? src : "");
            if (stripped && stripped[0]) text = stripped;
            else text = detail;
        }
        json_append(out, json_bool(prefer ? true : false));
        json_append(out, json_string(text ? text : ""));
        free(stripped);
    }
    return out;
}

json_t *w30_base_0c99d4_init_paths(const json_t *in) {
    const char *temp_dir = jstr_or(in, "temp_dir", "/tmp");
    const char *raw_sid = jstr_or(in, "session_id", "");
    char sid[13];                      /* uuid.uuid4().hex[:12] is the seam answer */
    char *root;
    size_t sl = strlen(raw_sid);
    if (sl > 12) sl = 12;
    memcpy(sid, raw_sid, sl);
    sid[sl] = 0;
    root = s_temp_root(temp_dir);
    json_t *out = json_object();
    sb b;
    sb_init(&b);
    sb_add(&b, root ? root : "/");
    sb_add(&b, "/hermes-snap-");
    sb_add(&b, sid);
    sb_add(&b, ".sh");
    json_set(out, "snapshot_path", json_string(sb_take(&b)));
    sb_init(&b);
    sb_add(&b, root ? root : "/");
    sb_add(&b, "/hermes-cwd-");
    sb_add(&b, sid);
    sb_add(&b, ".txt");
    json_set(out, "cwd_file", json_string(sb_take(&b)));
    sb_init(&b);
    sb_add(&b, "__HERMES_CWD_");
    sb_add(&b, sid);
    sb_add(&b, "__");
    json_set(out, "cwd_marker", json_string(sb_take(&b)));
    sb_init(&b);
    sb_add(&b, "__HERMES_PIPE_");
    sb_add(&b, sid);
    sb_add(&b, "__");
    json_set(out, "pipe_marker", json_string(sb_take(&b)));
    sb_init(&b);
    sb_add(&b, "__HERMES_CWDFB_");
    sb_add(&b, sid);
    sb_add(&b, "__");
    json_set(out, "cwd_fallback_marker", json_string(sb_take(&b)));
    free(root);
    return out;
}

json_t *w30_base_0c99d4_session_markers(const json_t *in) {
    const char *sid = jstr_or(in, "session_id", "");
    json_t *out = json_object();
    sb b;
    sb_init(&b);
    sb_add(&b, "__HERMES_CWD_"); sb_add(&b, sid); sb_add(&b, "__");
    json_set(out, "cwd", json_string(sb_take(&b)));
    sb_init(&b);
    sb_add(&b, "__HERMES_CWDFB_"); sb_add(&b, sid); sb_add(&b, "__");
    json_set(out, "cwd_fallback", json_string(sb_take(&b)));
    sb_init(&b);
    sb_add(&b, "__HERMES_PIPE_"); sb_add(&b, sid); sb_add(&b, "__");
    json_set(out, "pipe", json_string(sb_take(&b)));
    return out;
}

/* sha256(task_id.encode()).hexdigest()[:16] */
static char *task_digest(const char *task_id) {
    char hex[65];
    char *o;
    sha256_hex(task_id ? task_id : "", task_id ? strlen(task_id) : 0, hex);
    o = (char *)malloc(17);
    if (!o) return NULL;
    memcpy(o, hex, 16);
    o[16] = '\0';
    return o;
}

static char *task_snapshot_of(const char *snapshot_path, const char *task_id) {
    size_t n = snapshot_path ? strlen(snapshot_path) : 0;
    char *digest = task_digest(task_id);
    sb b;
    sb_init(&b);
    sb_addn(&b, snapshot_path ? snapshot_path : "", n >= 3 ? n - 3 : 0);
    sb_add(&b, "-");
    sb_add(&b, digest ? digest : "");
    sb_add(&b, ".sh");
    free(digest);
    return sb_take(&b);
}

json_t *w30_base_0c99d4_task_snapshot_path(const json_t *in) {
    char *p = task_snapshot_of(jstr_or(in, "snapshot_path", ""), jstr_or(in, "task_id", ""));
    json_t *out = json_string(p ? p : "");
    free(p);
    return out;
}

json_t *w30_base_0c99d4_update_cwd(const json_t *in) {
    return cwd_parse_core(in, 1);
}

json_t *w30_base_0c99d4_fetch_realpath(const json_t *in) {
    const json_t *res = in ? json_obj_get(in, "result") : NULL;
    const char *output;
    long long code;
    if (!res || res->type != JSON_OBJECT) return json_null();
    code = py_int_or_zero(json_obj_get(res, "returncode"));
    if (code != 0) return json_null();
    output = jstr_or(res, "output", "");
    {
        size_t n = strlen(output), end = n;
        while (end > 0) {
            size_t start, llen;
            char *line, *stripped;
            while (end > 0 && (output[end - 1] == '\n' || output[end - 1] == '\r')) end--;
            if (end == 0) break;
            start = end;
            while (start > 0 && output[start - 1] != '\n' && output[start - 1] != '\r') start--;
            llen = end - start;
            line = (char *)malloc(llen + 1);
            if (line) {
                memcpy(line, output + start, llen);
                line[llen] = '\0';
                stripped = s_strip(line);
                free(line);
                if (stripped && stripped[0] == '/') return json_string(stripped);
                free(stripped);
            }
            end = start;
            while (end > 0 && (output[end - 1] == '\n' || output[end - 1] == '\r')) end--;
        }
    }
    return json_null();
}

json_t *w30_base_0c99d4_usable_cwd(const json_t *in) {
    json_t *out = json_object();
    json_set(out, "cwd", json_string(jstr_or(in, "cwd", "")));
    json_set(out, "fallback", json_null());
    return out;
}

json_t *w30_base_0c99d4_temp_dir(const json_t *in) {
    (void)in;
    return json_string("/tmp");
}

json_t *w30_base_0c99d4_script_kwargs(const json_t *in) {
    const char *cwd = jstr_or(in, "cwd", "");
    const char *snap = jstr(in, "snap");
    const char *snapshot_path = jstr_or(in, "snapshot_path", "");
    const char *cwd_marker = jstr_or(in, "cwd_marker", "");
    const char *source = (snap && snap[0]) ? snap : snapshot_path;
    json_t *out = json_object();
    json_t *qc_in = json_object();
    json_t *qc;
    char *qs, *tmpl, *qt;
    sb b;
    json_set(qc_in, "cwd", json_string(cwd));
    qc = w30_base_0c99d4_quote_cwd_for_cd(qc_in);
    json_free(qc_in);
    sb_init(&b);
    sb_add(&b, source ? source : "");
    sb_add(&b, ".tmp.XXXXXXXXXX");
    tmpl = sb_take(&b);
    qs = sh_quote(source ? source : "");
    qt = sh_quote(tmpl ? tmpl : "");
    json_set(out, "quoted_cwd", json_copy(qc));
    json_set(out, "quoted_snap", json_string(qs ? qs : ""));
    json_set(out, "snap_tmp_template", json_string(qt ? qt : ""));
    json_set(out, "cwd_marker", json_string(cwd_marker));
    json_free(qc);
    free(qs);
    free(tmpl);
    free(qt);
    return out;
}

/* ------------------------------------------------------------------ */
/* gate p2: command construction                                       */
/* ------------------------------------------------------------------ */

json_t *w30_base_0c99d4_quote_cwd_for_cd(const json_t *in) {
    const char *cwd = jstr_or(in, "cwd", "");
    if (strcmp(cwd, "~") == 0) return json_string("~");
    if (strcmp(cwd, "~/") == 0) return json_string("$HOME");
    if (strncmp(cwd, "~/", 2) == 0) {
        char *q = sh_quote(cwd + 2);
        sb b;
        char *o;
        sb_init(&b);
        sb_add(&b, "$HOME/");
        sb_add(&b, q ? q : "''");
        o = sb_take(&b);
        free(q);
        {
            json_t *out = json_string(o ? o : "");
            free(o);
            return out;
        }
    }
    {
        char *q = sh_quote(cwd);
        json_t *out = json_string(q ? q : "''");
        free(q);
        return out;
    }
}

json_t *w30_base_0c99d4_quote_shell_path(const json_t *in) {
    char *q = sh_quote(jstr_or(in, "path", ""));
    json_t *out = json_string(q ? q : "''");
    free(q);
    return out;
}

json_t *w30_base_0c99d4_stdin_redirect(const json_t *in) {
    const char *command = jstr_or(in, "command", "");
    char *q = sh_quote(jstr_or(in, "path", ""));
    sb b;
    char *o;
    sb_init(&b);
    sb_add(&b, "exec 0< ");
    sb_add(&b, q ? q : "''");
    sb_add(&b, " || exit $?\nrm -f -- ");
    sb_add(&b, q ? q : "''");
    sb_add(&b, " || exit $?\n");
    sb_add(&b, command);
    o = sb_take(&b);
    free(q);
    {
        json_t *out = json_string(o ? o : "");
        free(o);
        return out;
    }
}

json_t *w30_base_0c99d4_stdin_heredoc(const json_t *in) {
    const char *command = jstr_or(in, "command", "");
    const char *data = jstr_or(in, "stdin_data", "");
    const char *raw = jstr_or(in, "token", "");
    char token[13];                    /* uuid.uuid4().hex[:12] is the seam answer */
    size_t tl = strlen(raw);
    sb b;
    char *o;
    if (tl > 12) tl = 12;
    memcpy(token, raw, tl);
    token[tl] = 0;
    sb_init(&b);
    sb_add(&b, "{\n");
    sb_add(&b, command);
    sb_add(&b, "\n} << 'HERMES_STDIN_");
    sb_add(&b, token);
    sb_add(&b, "' < <(IFS= read -r -d '' s || :; printf '%s' \"${s%?}\")\n");
    sb_add(&b, data);
    sb_add(&b, "\nHERMES_STDIN_");
    sb_add(&b, token);
    o = sb_take(&b);
    {
        json_t *out = json_string(o ? o : "");
        free(o);
        return out;
    }
}

json_t *w30_base_0c99d4_staged_stdin_path(const json_t *in) {
    char *root = s_temp_root(jstr_or(in, "temp_dir", "/tmp"));
    sb b;
    char *o;
    sb_init(&b);
    sb_add(&b, root ? root : "/");
    sb_add(&b, "/.hermes-stdin-");
    sb_add(&b, jstr_or(in, "token", ""));
    o = sb_take(&b);
    free(root);
    {
        json_t *out = json_string(o ? o : "");
        free(o);
        return out;
    }
}

json_t *w30_base_0c99d4_split_cwd_marker(const json_t *in) {
    char *path = NULL, *clean = NULL;
    if (!split_marker(jstr_or(in, "output", ""), jstr_or(in, "marker", ""), &path, &clean))
        return json_null();
    {
        json_t *out = json_object();
        json_set(out, "path", path ? json_string(path) : json_null());
        json_set(out, "cleaned", json_string(clean ? clean : ""));
        free(path);
        free(clean);
        return out;
    }
}

json_t *w30_base_0c99d4_parse_cwd_marker(const json_t *in) {
    return cwd_parse_core(in, 0);
}

/* ---- wrapper / bootstrap script assembly ---- */

typedef struct { char **v; size_t n, cap; } pvec;

static void pv_push(pvec *p, char *own) {
    if (!own) return;
    if (p->n == p->cap) {
        size_t ncap = p->cap ? p->cap * 2 : 16;
        char **nv = (char **)realloc(p->v, ncap * sizeof(char *));
        if (!nv) { free(own); return; }
        p->v = nv;
        p->cap = ncap;
    }
    p->v[p->n++] = own;
}

static char *pv_join(pvec *p) {
    sb b;
    size_t i;
    char *o;
    sb_init(&b);
    for (i = 0; i < p->n; i++) {
        if (i) sb_addc(&b, '\n');
        sb_add(&b, p->v[i]);
        free(p->v[i]);
    }
    free(p->v);
    p->v = NULL; p->n = p->cap = 0;
    o = sb_take(&b);
    return o;
}

/* "{ ( unset ... ; export -p; ) || true; } > <tmp_path>" */
static char *dump_text(const char *tmp_path, const json_t *names) {
    sb b;
    char **kept = NULL;
    size_t nk = 0, cap = 0, i, j;
    sb_init(&b);
    sb_add(&b, "{ ( unset ${!HERMES_SESSION_*} ${!HERMES_CRON_AUTO_DELIVER_*} "
               "AI_AGENT HERMES_AGENT "
               "HERMES_DELEGATED_CHILD_CONTEXT HERMES_CRON_SESSION "
               "${!HERMES_RPC_*} HERMES_KERNEL_DIR "
               "HERMES_UI_SESSION_ID");
    if (names && names->type == JSON_ARRAY) {
        size_t k, n = json_len(names);
        for (k = 0; k < n; k++) {
            const json_t *v = json_get(names, k);
            const char *s;
            if (v && v->type == JSON_OBJECT) v = json_obj_get(v, "name");
            if (!v || v->type != JSON_STRING) continue;
            s = v->str_val ? v->str_val : "";
            if (!s[0]) continue;
            if (nk == cap) {
                size_t ncap = cap ? cap * 2 : 8;
                char **np = (char **)realloc(kept, ncap * sizeof(char *));
                if (!np) break;
                kept = np;
                cap = ncap;
            }
            kept[nk++] = xdup(s);
        }
    }
    for (i = 0; i < nk; i++)
        for (j = i + 1; j < nk; j++)
            if (strcmp(kept[j], kept[i]) < 0) {
                char *t = kept[i];
                kept[i] = kept[j];
                kept[j] = t;
            }
    for (i = 0; i < nk; i++) {
        char *q;
        if (i > 0 && strcmp(kept[i], kept[i - 1]) == 0) { free(kept[i]); continue; }
        q = sh_quote(kept[i]);
        sb_addc(&b, ' ');
        sb_add(&b, q ? q : "");
        free(q);
        free(kept[i]);
    }
    free(kept);
    sb_add(&b, " 2>/dev/null; export -p; ) || true; } > ");
    sb_add(&b, tmp_path);
    return sb_take(&b);
}

static char *cd_with_fallback(const char *quoted_cwd, const char *marker) {
    sb b;
    sb_init(&b);
    if (!marker || !marker[0]) {
        sb_add(&b, "builtin cd -- ");
        sb_add(&b, quoted_cwd);
        sb_add(&b, " || exit 126");
        return sb_take(&b);
    }
    sb_add(&b, "builtin cd -- ");
    sb_add(&b, quoted_cwd);
    sb_add(&b, " 2>/dev/null || { __hermes_d=");
    sb_add(&b, quoted_cwd);
    sb_add(&b, "\nwhile [ -n \"$__hermes_d\" ] && ! builtin cd -- \"$__hermes_d\" 2>/dev/null; do\n"
               "  case $__hermes_d in /*/*|[!/]*/*) __hermes_d=${__hermes_d%/*};; /?*) __hermes_d=/;; *) __hermes_d=;; esac\n"
               "done\n"
               "[ -n \"$__hermes_d\" ] || builtin cd -- \"$HOME\" 2>/dev/null || builtin cd / || exit 126\n");
    sb_add(&b, "printf '\\n");
    sb_add(&b, marker);
    sb_add(&b, "%s");
    sb_add(&b, marker);
    sb_add(&b, "\\n' \"$(pwd -P)\"; }");
    return sb_take(&b);
}

static char *eval_capturing(const char *escaped) {
    sb b;
    sb_init(&b);
    sb_add(&b, "eval '");
    sb_add(&b, escaped);
    sb_add(&b, "\n\n__hermes_rc=$? __hermes_ps=(\"${PIPESTATUS[@]}\"); (exit $__hermes_rc) && :'");
    return sb_take(&b);
}

static char *cwd_marker_printf(const char *marker) {
    sb b;
    sb_init(&b);
    sb_add(&b, "printf '\\n");
    sb_add(&b, marker);
    sb_add(&b, "%s");
    sb_add(&b, marker);
    sb_add(&b, "\\n' \"$(pwd -P)\"");
    return sb_take(&b);
}

static char *pipe_marker_printf(const char *marker) {
    sb b;
    sb_init(&b);
    sb_add(&b, "printf '\\n");
    sb_add(&b, marker);
    sb_add(&b, "%s");
    sb_add(&b, marker);
    sb_add(&b, "\\n' \"${__hermes_ps[*]-}\"");
    return sb_take(&b);
}

static char *escape_single_quotes(const char *s) {
    sb b;
    size_t i, n = s ? strlen(s) : 0;
    sb_init(&b);
    for (i = 0; i < n; i++) {
        if (s[i] == '\'') sb_add(&b, "'\\''");
        else sb_addc(&b, s[i]);
    }
    return sb_take(&b);
}

/* names arrive as ["NAME", ...] or [{"name": "NAME"}, ...] */
static const char *name_at(const json_t *names, size_t i) {
    const json_t *v;
    if (!names || names->type != JSON_ARRAY) return NULL;
    v = json_get(names, i);
    if (v && v->type == JSON_OBJECT) v = json_obj_get(v, "name");
    return jnode_str(v);
}

static size_t name_count(const json_t *names) {
    return (names && names->type == JSON_ARRAY) ? json_len(names) : 0;
}

json_t *w30_base_0c99d4_wrap_command(const json_t *in) {
    const char *command = jstr_or(in, "command", "");
    const char *cwd = jstr_or(in, "cwd", "");
    const char *snapshot_path = jstr_or(in, "snapshot_path", "");
    const char *pipe_marker = jstr_or(in, "pipe_marker", "");
    const char *cwd_marker = jstr_or(in, "cwd_marker", "");
    const char *fb_marker = jbool(in, "cwd_fallback", 0) ? jstr_or(in, "cwd_fallback_marker", "") : "";
    const json_t *names = in ? json_obj_get(in, "passthrough_names") : NULL;
    const json_t *chain = in ? json_obj_get(in, "chain") : NULL;
    int ready = jbool(in, "snapshot_ready", 0);
    int persist = jbool(in, "persist_env", 1);
    char *snap = NULL, *quoted_cwd = NULL, *quoted_snap = NULL, *tmpl = NULL, *quoted_tmpl = NULL;
    char *escaped = NULL;
    char *script;
    char **seeds = NULL;
    size_t nseeds = 0, i, nchain = 0, nnames = name_count(names);
    int capture;
    pvec P = {0};
    json_t *qc_in, *qc;

    if (chain && chain->type == JSON_ARRAY) nchain = json_len(chain);
    if (nchain > 0) {
        snap = task_snapshot_of(snapshot_path, name_at(chain, 0) ? name_at(chain, 0) : "");
        seeds = (char **)calloc(nchain, sizeof(char *));
        if (seeds) {
            size_t idx = 0;
            for (i = 1; i < nchain; i++) {
                char *tp = task_snapshot_of(snapshot_path, name_at(chain, i) ? name_at(chain, i) : "");
                seeds[idx++] = sh_quote(tp ? tp : "");
                free(tp);
            }
            seeds[idx++] = sh_quote(snapshot_path);
            nseeds = idx;
        }
    } else {
        snap = xdup(snapshot_path);
    }

    qc_in = json_object();
    json_set(qc_in, "cwd", json_string(cwd));
    qc = w30_base_0c99d4_quote_cwd_for_cd(qc_in);
    json_free(qc_in);
    quoted_cwd = xdup(jnode_str(qc));
    json_free(qc);

    quoted_snap = sh_quote(snap ? snap : "");
    {
        sb b;
        sb_init(&b);
        sb_add(&b, snap ? snap : "");
        sb_add(&b, ".tmp.XXXXXXXXXX");
        tmpl = sb_take(&b);
    }
    quoted_tmpl = sh_quote(tmpl ? tmpl : "");

    /* passthrough save lines */
    for (i = 0; i < nnames; i++) {
        const char *name = name_at(names, i);
        sb b;
        if (!name) continue;
        sb_init(&b);
        sb_add(&b, "_HERMES_RUNTIME_PASSTHROUGH_");
        sb_add(&b, name);
        sb_add(&b, "_PRESENT=${");
        sb_add(&b, name);
        sb_add(&b, "+x}");
        pv_push(&P, sb_take(&b));
        sb_init(&b);
        sb_add(&b, "_HERMES_RUNTIME_PASSTHROUGH_");
        sb_add(&b, name);
        sb_add(&b, "_VALUE=${");
        sb_add(&b, name);
        sb_add(&b, "-}");
        pv_push(&P, sb_take(&b));
    }

    /* snapshot sourcing */
    if (ready && nseeds > 0) {
        sb b;
        sb_init(&b);
        sb_add(&b, "for __hermes_s in ");
        sb_add(&b, quoted_snap ? quoted_snap : "");
        for (i = 0; i < nseeds; i++) {
            sb_addc(&b, ' ');
            sb_add(&b, seeds[i]);
        }
        sb_add(&b, "; do if [ -f \"$__hermes_s\" ]; then source \"$__hermes_s\" >/dev/null 2>&1 || true; break; fi; done");
        pv_push(&P, sb_take(&b));
    } else if (ready) {
        sb b;
        sb_init(&b);
        sb_add(&b, "source ");
        sb_add(&b, quoted_snap ? quoted_snap : "");
        sb_add(&b, " >/dev/null 2>&1 || true");
        pv_push(&P, sb_take(&b));
    }

    /* passthrough restore lines */
    for (i = 0; i < nnames; i++) {
        const char *name = name_at(names, i);
        sb b;
        if (!name) continue;
        sb_init(&b);
        sb_add(&b, "if [ \"$_HERMES_RUNTIME_PASSTHROUGH_");
        sb_add(&b, name);
        sb_add(&b, "_PRESENT\" = x ]; then export ");
        sb_add(&b, name);
        sb_add(&b, "=\"$_HERMES_RUNTIME_PASSTHROUGH_");
        sb_add(&b, name);
        sb_add(&b, "_VALUE\"; else unset ");
        sb_add(&b, name);
        sb_add(&b, "; fi");
        pv_push(&P, sb_take(&b));
        sb_init(&b);
        sb_add(&b, "unset _HERMES_RUNTIME_PASSTHROUGH_");
        sb_add(&b, name);
        sb_add(&b, "_PRESENT _HERMES_RUNTIME_PASSTHROUGH_");
        sb_add(&b, name);
        sb_add(&b, "_VALUE");
        pv_push(&P, sb_take(&b));
    }

    pv_push(&P, xdup("export AI_AGENT=\"${AI_AGENT:-hermes-agent}\" HERMES_AGENT=\"${HERMES_AGENT:-true}\""));
    pv_push(&P, xdup("export GIT_PAGER=\"${GIT_PAGER:-cat}\" PAGER=\"${PAGER:-cat}\""));

    pv_push(&P, cd_with_fallback(quoted_cwd ? quoted_cwd : "", fb_marker));

    escaped = escape_single_quotes(command);
    capture = (pipe_marker && pipe_marker[0]) ? 1 : 0;
    if (strstr(command, "<<") != NULL) capture = 0;
    if (command[0] && command[strlen(command) - 1] == '\\') capture = 0;
    if (capture) {
        pv_push(&P, eval_capturing(escaped));
    } else {
        sb b;
        sb_init(&b);
        sb_add(&b, "eval '");
        sb_add(&b, escaped);
        sb_add(&b, "'");
        pv_push(&P, sb_take(&b));
    }
    pv_push(&P, xdup("__hermes_ec=$?"));
    pv_push(&P, xdup("umask 077"));

    if (ready && persist) {
        sb b;
        char *dump;
        sb_init(&b);
        sb_add(&b, "__hermes_snap_tmp=$(mktemp ");
        sb_add(&b, quoted_tmpl ? quoted_tmpl : "");
        sb_add(&b, ") && { ");
        dump = dump_text("\"$__hermes_snap_tmp\"", names);
        sb_add(&b, dump ? dump : "");
        sb_add(&b, " && mv -f \"$__hermes_snap_tmp\" ");
        sb_add(&b, quoted_snap ? quoted_snap : "");
        sb_add(&b, "; } 2>/dev/null || rm -f \"$__hermes_snap_tmp\" 2>/dev/null || true");
        pv_push(&P, sb_take(&b));
        free(dump);
    }
    if (capture) pv_push(&P, pipe_marker_printf(pipe_marker));
    pv_push(&P, cwd_marker_printf(cwd_marker));
    pv_push(&P, xdup("exit $__hermes_ec"));

    script = pv_join(&P);
    for (i = 0; i < nseeds; i++) free(seeds[i]);
    free(seeds);
    free(snap);
    free(quoted_cwd);
    free(quoted_snap);
    free(tmpl);
    free(quoted_tmpl);
    free(escaped);
    {
        json_t *out = json_string(script ? script : "");
        free(script);
        return out;
    }
}

json_t *w30_base_0c99d4_bootstrap_script(const json_t *in) {
    const char *cwd = jstr_or(in, "cwd", "");
    const char *snapshot_path = jstr_or(in, "snapshot_path", "");
    const char *cwd_marker = jstr_or(in, "cwd_marker", "");
    const json_t *names = in ? json_obj_get(in, "excluded_names") : NULL;
    json_t *qc_in, *qc;
    char *quoted_cwd, *quoted_snap, *tmpl, *quoted_tmpl, *dump, *mark_p;
    sb b, t;
    char *o;

    qc_in = json_object();
    json_set(qc_in, "cwd", json_string(cwd));
    qc = w30_base_0c99d4_quote_cwd_for_cd(qc_in);
    json_free(qc_in);
    quoted_cwd = xdup(jnode_str(qc));
    json_free(qc);
    quoted_snap = sh_quote(snapshot_path);
    sb_init(&t);
    sb_add(&t, snapshot_path);
    sb_add(&t, ".tmp.XXXXXXXXXX");
    tmpl = sb_take(&t);
    quoted_tmpl = sh_quote(tmpl ? tmpl : "");

    sb_init(&b);
    sb_add(&b, "umask 077\n__hermes_snap_tmp=$(mktemp ");
    sb_add(&b, quoted_tmpl ? quoted_tmpl : "");
    sb_add(&b, ") || exit 1\n");
    dump = dump_text("\"$__hermes_snap_tmp\"", names);
    sb_add(&b, dump ? dump : "");
    sb_add(&b, "\n__hermes_fns=$(declare -F | awk '{print $3}' | grep -vE '^_[^_]') || true\n");
    sb_add(&b, "[ -n \"$__hermes_fns\" ] && declare -f $__hermes_fns >> \"$__hermes_snap_tmp\" 2>/dev/null || true\n");
    sb_add(&b, "alias -p >> \"$__hermes_snap_tmp\"\n");
    sb_add(&b, "echo 'shopt -s expand_aliases' >> \"$__hermes_snap_tmp\"\n");
    sb_add(&b, "echo 'set +e' >> \"$__hermes_snap_tmp\"\n");
    sb_add(&b, "echo 'set +u' >> \"$__hermes_snap_tmp\"\n");
    sb_add(&b, "mv -f \"$__hermes_snap_tmp\" ");
    sb_add(&b, quoted_snap ? quoted_snap : "");
    sb_add(&b, " || rm -f \"$__hermes_snap_tmp\"\n");
    sb_add(&b, "builtin cd -- ");
    sb_add(&b, quoted_cwd ? quoted_cwd : "");
    sb_add(&b, " 2>/dev/null || true\n");
    mark_p = cwd_marker_printf(cwd_marker);
    sb_add(&b, mark_p ? mark_p : "");
    sb_add(&b, "\n");
    o = sb_take(&b);
    free(quoted_cwd);
    free(quoted_snap);
    free(tmpl);
    free(quoted_tmpl);
    free(dump);
    free(mark_p);
    {
        json_t *out = json_string(o ? o : "");
        free(o);
        return out;
    }
}
