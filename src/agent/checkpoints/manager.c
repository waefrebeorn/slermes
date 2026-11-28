/*
 * manager.c — C11 port of upstream agent/checkpoints/manager.py
 * (gates w28_manager_87eef2_p1, _p2, _p3).
 *
 * PoP: w28_manager_87eef2_p1_validate_commit_hash @ agent/checkpoints/manager.py:_validate_commit_hash
 * PoP: w28_manager_87eef2_p1_ref_name @ agent/checkpoints/manager.py:_ref_name
 * PoP: w28_manager_87eef2_p1_gitlink_paths @ agent/checkpoints/manager.py:_gitlink_paths
 * PoP: w28_manager_87eef2_p1_uncaptured_note @ agent/checkpoints/manager.py:_uncaptured_note
 * PoP: w28_manager_87eef2_p1_project_hash @ agent/checkpoints/manager.py:_project_hash
 * PoP: w28_manager_87eef2_p1_store_path @ agent/checkpoints/manager.py:_store_path
 * PoP: w28_manager_87eef2_p1_index_path @ agent/checkpoints/manager.py:_index_path
 * PoP: w28_manager_87eef2_p1_ledger_path @ agent/checkpoints/manager.py:_ledger_path
 * PoP: w28_manager_87eef2_p1_meta_path @ agent/checkpoints/manager.py:_project_meta_path
 * PoP: w28_manager_87eef2_p1_store_pathspecs @ agent/checkpoints/manager.py:_store_pathspecs
 * PoP: w28_manager_87eef2_p1_stage_all_args @ agent/checkpoints/manager.py:_stage_all_args
 * PoP: w28_manager_87eef2_p1_exclude_store_args @ agent/checkpoints/manager.py:_exclude_store_args
 * PoP: w28_manager_87eef2_p2_coerce_init @ agent/checkpoints/manager.py:CheckpointManager.__init__
 * PoP: w28_manager_87eef2_p2_parse_shortstat @ agent/checkpoints/manager.py:CheckpointManager._parse_shortstat
 * PoP: w28_manager_87eef2_p2_list_checkpoints @ agent/checkpoints/manager.py:CheckpointManager.list_checkpoints
 * PoP: w28_manager_87eef2_p2_ledger_load @ agent/checkpoints/manager.py:_load_ledger
 * PoP: w28_manager_87eef2_p2_ledger_trim @ agent/checkpoints/manager.py:_save_ledger
 * PoP: w28_manager_87eef2_p2_list_projects @ agent/checkpoints/manager.py:_list_projects
 * PoP: w28_manager_87eef2_p2_format_list @ agent/checkpoints/manager.py:format_checkpoint_list
 * PoP: w28_manager_87eef2_p3_safe_restore_plan @ agent/checkpoints/manager.py:CheckpointManager._safe_restore_plan
 * PoP: w28_manager_87eef2_p3_diff_result @ agent/checkpoints/manager.py:CheckpointManager._diff
 * PoP: w28_manager_87eef2_p3_session_diff @ agent/checkpoints/manager.py:CheckpointManager.session_diff
 * PoP: w28_manager_87eef2_p3_exceeds_size_cap @ agent/checkpoints/manager.py:CheckpointManager._exceeds_size_cap
 *
 * Seams left in the caller (never ported here): the filesystem (resolve,
 * exists/stat, directory walks), git (subprocess), the clock, and the store
 * lock.  What is ported is the pure value core those seams feed: name and path
 * derivation, the checkpoint id, git argument shaping, coercion of the manager
 * budgets, git-output and manifest parsing, the retention cap, listing text,
 * and the restore/diff decisions.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>

#include "hermes_json.h"
#include "noc_w28_manager_87eef2_p1.h"
#include "noc_w28_manager_87eef2_p2.h"
#include "noc_w28_manager_87eef2_p3.h"

/* Agent-write ledger cap: newest entries retained per project. */
#define CM_LEDGER_MAX_ENTRIES 2000

/* =================================================================== */
/* small helpers                                                       */
/* =================================================================== */

typedef struct { char *p; size_t n, cap; } sb_t;

static void sb_init(sb_t *b) {
    b->cap = 96;
    b->n = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void sb_putn(sb_t *b, const char *s, size_t k) {
    if (!b->p || !s) return;
    if (b->n + k + 1 > b->cap) {
        size_t nc = b->cap * 2 + k + 1;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np;
        b->cap = nc;
    }
    memcpy(b->p + b->n, s, k);
    b->n += k;
    b->p[b->n] = '\0';
}

static void sb_puts(sb_t *b, const char *s) { if (s) sb_putn(b, s, strlen(s)); }
static void sb_putc(sb_t *b, char c) { sb_putn(b, &c, 1); }

static void sb_printf(sb_t *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    int k;
    va_start(ap, fmt);
    k = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (k < 0) return;
    if ((size_t)k < sizeof(tmp)) { sb_putn(b, tmp, (size_t)k); return; }
    {
        char *big = (char *)malloc((size_t)k + 1);
        if (!big) return;
        va_start(ap, fmt);
        vsnprintf(big, (size_t)k + 1, fmt, ap);
        va_end(ap);
        sb_putn(b, big, (size_t)k);
        free(big);
    }
}

static void sb_drop(sb_t *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

/* Python truthiness of a JSON value (an absent value is None). */
static int jtruth(const json_t *v) {
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

static int jnumber(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python '==' over the shapes these values hold; absent and null are equal. */
static int jeq(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (jnumber(a, &x) && jnumber(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!jeq(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    return 0;
}

/* A JSON value as a C string view ("" for null / non-string). */
static const char *jstr(const json_t *v) {
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return "";
}

/* Object member as a string, with a default for an absent/wrong-typed key. */
static const char *jstr_or(const json_t *o, const char *k, const char *def) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    if (!v || v->type != JSON_STRING) return def;
    return v->str_val ? v->str_val : def;
}

static int jbool_or(const json_t *o, const char *k, int def) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return jtruth(v);
}

/* =================================================================== */
/* PurePosixPath string form: collapse '//', drop '.', keep '..',      */
/* no trailing separator ('/' and '.' for the degenerate roots).       */
/* =================================================================== */

static void posix_norm(const char *p, char *out, size_t cap) {
    size_t o = 0;
    const char *s = p ? p : "";
    int absolute = (s[0] == '/');
    int any = 0;
    if (cap == 0) return;
    while (*s) {
        const char *seg;
        size_t len;
        while (*s && *s == '/') s++;
        seg = s;
        while (*s && *s != '/') s++;
        len = (size_t)(s - seg);
        if (len == 0) continue;
        if (len == 1 && seg[0] == '.') continue;
        if (o + len + 2 >= cap) break;
        if (any || absolute) out[o++] = '/';
        memcpy(out + o, seg, len);
        o += len;
        any = 1;
    }
    if (!any) {
        if (absolute) { if (o + 2 < cap) out[o++] = '/'; }
        else { if (o + 2 < cap) out[o++] = '.'; }
    }
    if (o >= cap) o = cap - 1;
    out[o] = '\0';
}

/* posixpath.join(a, b): an absolute b wins; '.' and '' vanish. */
static void posix_join(const char *a, const char *b, char *out, size_t cap) {
    char na[2048], nb[2048], tmp[4200];
    posix_norm(a, na, sizeof(na));
    posix_norm(b, nb, sizeof(nb));
    if (nb[0] == '/') { snprintf(out, cap, "%s", nb); return; }
    if (!strcmp(nb, ".")) { snprintf(out, cap, "%s", na); return; }
    if (!strcmp(na, ".") || !na[0]) { snprintf(out, cap, "%s", nb); return; }
    snprintf(tmp, sizeof(tmp), "%s/%s", na, nb);
    posix_norm(tmp, out, cap);
}

/* Split a normalized posix path into parts; returns count. */
static size_t posix_parts(const char *p, char parts[][256], size_t max) {
    size_t n = 0;
    const char *s = p;
    while (*s && n < max) {
        const char *seg;
        size_t len;
        while (*s && *s == '/') s++;
        seg = s;
        while (*s && *s != '/') s++;
        len = (size_t)(s - seg);
        if (len == 0) continue;
        if (len > 255) len = 255;
        memcpy(parts[n], seg, len);
        parts[n][len] = '\0';
        n++;
    }
    return n;
}

/* Python repr() of an ASCII str. */
static void py_repr(const char *s, sb_t *b) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    sb_putc(b, '\'');
    for (; *p; p++) {
        switch (*p) {
        case '\\': sb_puts(b, "\\\\"); break;
        case '\'': sb_puts(b, "\\'"); break;
        case '\n': sb_puts(b, "\\n"); break;
        case '\r': sb_puts(b, "\\r"); break;
        case '\t': sb_puts(b, "\\t"); break;
        default:
            if (*p < 0x20 || *p == 0x7f) sb_printf(b, "\\x%02x", (unsigned)*p);
            else sb_putc(b, (char)*p);
        }
    }
    sb_putc(b, '\'');
}

static int is_blank(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    for (; *p; p++) if (!isspace(*p)) return 0;
    return 1;
}

/* =================================================================== */
/* SHA-256 (hashlib.sha256(...).hexdigest())                           */
/* =================================================================== */

typedef struct {
    uint32_t h[8];
    uint64_t len;
    unsigned char buf[64];
    size_t used;
} cm_sha256;

static const uint32_t CM_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

#define CM_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void cm_sha_block(cm_sha256 *s, const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = CM_ROR(w[i - 15], 7) ^ CM_ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = CM_ROR(w[i - 2], 17) ^ CM_ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = CM_ROR(e, 6) ^ CM_ROR(e, 11) ^ CM_ROR(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t S0 = CM_ROR(a, 2) ^ CM_ROR(a, 13) ^ CM_ROR(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        t1 = h + S1 + ch + CM_K[i] + w[i];
        t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void cm_sha_init(cm_sha256 *s) {
    memset(s, 0, sizeof(*s));
    s->h[0] = 0x6a09e667u; s->h[1] = 0xbb67ae85u; s->h[2] = 0x3c6ef372u; s->h[3] = 0xa54ff53au;
    s->h[4] = 0x510e527fu; s->h[5] = 0x9b05688cu; s->h[6] = 0x1f83d9abu; s->h[7] = 0x5be0cd19u;
}

static void cm_sha_update(cm_sha256 *s, const unsigned char *p, size_t n) {
    s->len += (uint64_t)n * 8u;
    while (n) {
        size_t room = 64 - s->used;
        size_t take = n < room ? n : room;
        memcpy(s->buf + s->used, p, take);
        s->used += take;
        p += take;
        n -= take;
        if (s->used == 64) { cm_sha_block(s, s->buf); s->used = 0; }
    }
}

static void cm_sha_final(cm_sha256 *s, unsigned char out[32]) {
    uint64_t bits = s->len;
    unsigned char pad = 0x80;
    unsigned char zero = 0x00;
    unsigned char lenb[8];
    int i;
    cm_sha_update(s, &pad, 1);
    while (s->used != 56) cm_sha_update(s, &zero, 1);
    for (i = 0; i < 8; i++) lenb[i] = (unsigned char)((bits >> (56 - 8 * i)) & 0xff);
    cm_sha_update(s, lenb, 8);
    for (i = 0; i < 8; i++) {
        out[i * 4]     = (unsigned char)((s->h[i] >> 24) & 0xff);
        out[i * 4 + 1] = (unsigned char)((s->h[i] >> 16) & 0xff);
        out[i * 4 + 2] = (unsigned char)((s->h[i] >> 8) & 0xff);
        out[i * 4 + 3] = (unsigned char)(s->h[i] & 0xff);
    }
}

/* hashlib.sha256(data).hexdigest()[:16] */
static void sha256_hex16(const char *data, char out[17]) {
    static const char *hexd = "0123456789abcdef";
    unsigned char d[32];
    cm_sha256 s;
    int i;
    cm_sha_init(&s);
    cm_sha_update(&s, (const unsigned char *)(data ? data : ""), strlen(data ? data : ""));
    cm_sha_final(&s, d);
    for (i = 0; i < 8; i++) {
        out[i * 2] = hexd[(d[i] >> 4) & 0xf];
        out[i * 2 + 1] = hexd[d[i] & 0xf];
    }
    out[16] = '\0';
}

/* =================================================================== */
/* Gate p1 — validation, identifiers, path + git-argument derivation    */
/* =================================================================== */

/* `^[0-9a-fA-F]{4,64}$` — '$' also matches just before one trailing \n. */
static int commit_hash_regex_ok(const char *h) {
    size_t i = 0, run;
    while (h[i] && isxdigit((unsigned char)h[i])) i++;
    run = i;
    if (run < 4 || run > 64) return 0;
    if (h[run] == '\0') return 1;
    if (h[run] == '\n' && h[run + 1] == '\0') return 1;
    return 0;
}

json_t *w28_manager_87eef2_p1_validate_commit_hash(const char *h) {
    sb_t b;
    json_t *out;
    if (!h) h = "";
    if (is_blank(h)) return json_string("Empty commit hash");
    if (h[0] == '-') {
        sb_init(&b);
        sb_puts(&b, "Invalid commit hash (must not start with '-'): ");
        py_repr(h, &b);
        out = json_string(b.p ? b.p : "");
        sb_drop(&b);
        return out;
    }
    if (!commit_hash_regex_ok(h)) {
        sb_init(&b);
        sb_puts(&b, "Invalid commit hash (expected 4-64 hex characters): ");
        py_repr(h, &b);
        out = json_string(b.p ? b.p : "");
        sb_drop(&b);
        return out;
    }
    return json_null();
}

json_t *w28_manager_87eef2_p1_ref_name(const char *dir_hash) {
    char buf[4200];
    snprintf(buf, sizeof(buf), "refs/hermes/%s", dir_hash ? dir_hash : "");
    return json_string(buf);
}

json_t *w28_manager_87eef2_p1_gitlink_paths(const json_t *records) {
    json_t *out = json_array();
    size_t i, n;
    if (!records || records->type != JSON_ARRAY) return out;
    n = json_len(records);
    for (i = 0; i < n; i++) {
        const json_t *r = json_get(records, i);
        const char *s;
        const char *tab;
        if (!r || r->type != JSON_STRING) continue;
        s = r->str_val ? r->str_val : "";
        if (strncmp(s, "160000 commit ", 14) != 0) continue;
        tab = strchr(s, '\t');
        if (!tab) continue;
        json_append(out, json_string(tab + 1));
    }
    return out;
}

json_t *w28_manager_87eef2_p1_uncaptured_note(const json_t *repos) {
    sb_t b;
    json_t *out;
    size_t i, n = 0;
    sb_init(&b);
    sb_puts(&b, " [nested git repos not captured: ");
    if (repos && repos->type == JSON_ARRAY) n = json_len(repos);
    for (i = 0; i < n && i < 3; i++) {
        if (i) sb_puts(&b, ", ");
        sb_puts(&b, jstr(json_get(repos, i)));
    }
    if (n > 3) sb_printf(&b, " (+%lu)", (unsigned long)(n - 3));
    sb_putc(&b, ']');
    out = json_string(b.p ? b.p : "");
    sb_drop(&b);
    return out;
}

json_t *w28_manager_87eef2_p1_project_hash(const char *abs_path) {
    char norm[4096], hex[17];
    posix_norm(abs_path, norm, sizeof(norm));
    sha256_hex16(norm, hex);
    return json_string(hex);
}

json_t *w28_manager_87eef2_p1_store_path(const char *base) {
    char buf[4200];
    posix_join(base ? base : ".", "store", buf, sizeof(buf));
    return json_string(buf);
}

json_t *w28_manager_87eef2_p1_index_path(const char *store, const char *dir_hash) {
    char leaf[1024], buf[4200];
    snprintf(leaf, sizeof(leaf), "indexes/%s", dir_hash ? dir_hash : "");
    posix_join(store, leaf, buf, sizeof(buf));
    return json_string(buf);
}

json_t *w28_manager_87eef2_p1_ledger_path(const char *store, const char *dir_hash) {
    char leaf[1024], buf[4200];
    snprintf(leaf, sizeof(leaf), "ledgers/%s.json", dir_hash ? dir_hash : "");
    posix_join(store, leaf, buf, sizeof(buf));
    return json_string(buf);
}

json_t *w28_manager_87eef2_p1_meta_path(const char *store, const char *dir_hash) {
    char leaf[1024], buf[4200];
    snprintf(leaf, sizeof(leaf), "projects/%s.json", dir_hash ? dir_hash : "");
    posix_join(store, leaf, buf, sizeof(buf));
    return json_string(buf);
}

/* Path.resolve() half of _normalize_path: an empty or relative path is anchored
 * at the working directory, so "" means "here" rather than "nothing". Only the
 * common prefix matters below (relative_to cancels it), but the prefix has to
 * exist on both sides or a relative base is compared against an absolute root. */
static void posix_abs(const char *norm, char *out, size_t cap) {
    size_t i;
    if (norm[0] == '/' || (norm[0] && norm[1] == ':')) {
        snprintf(out, cap, "%s", norm);
    } else {
        char cwd[2048];
        if (!getcwd(cwd, sizeof cwd)) snprintf(out, cap, "%s", norm);
        else if (norm[0]) snprintf(out, cap, "%s/%s", cwd, norm);
        else snprintf(out, cap, "%s", cwd);
    }
    /* getcwd() on Windows answers with backslashes; the relative_to walk below
     * splits on '/', so a mixed-separator absolute path never matches itself. */
    for (i = 0; out[i]; i++) if (out[i] == '\\') out[i] = '/';
}

/* _normalize_path(base).relative_to(_normalize_path(working_dir)) + lock. */
json_t *w28_manager_87eef2_p1_store_pathspecs(const char *base, const char *working_dir) {
    json_t *out = json_array();
    char nb[4096], nw[4096];
    char pb[64][256], pw[64][256];
    size_t nbn, nwn, i;
    sb_t rel, lock;
    int absolute;

    {
        char tmp[4096];
        /* anchor first, then normalise: posix_norm() is written for absolute
         * paths and answers "" for a relative one, which would have made a
         * relative base look like the working directory itself. */
        posix_abs(base, tmp, sizeof(tmp));
        posix_norm(tmp, nb, sizeof(nb));
        posix_abs(working_dir, tmp, sizeof(tmp));
        posix_norm(tmp, nw, sizeof(nw));
    }
    absolute = (nb[0] == '/') || (nb[0] && nb[1] == ':');
    if (absolute != ((nw[0] == '/') || (nw[0] && nw[1] == ':'))) return out; /* different drive/root */
    nbn = posix_parts(nb, pb, 64);
    nwn = posix_parts(nw, pw, 64);
    if (nbn < nwn) return out;                          /* not relative_to */
    for (i = 0; i < nwn; i++)
        if (strcmp(pb[i], pw[i])) return out;
    if (nbn == nwn) return out;                         /* rel.parts empty */

    sb_init(&rel);
    for (i = nwn; i < nbn; i++) {
        if (i > nwn) sb_putc(&rel, '/');
        sb_puts(&rel, pb[i]);
    }
    sb_init(&lock);
    for (i = nwn; i + 1 < nbn; i++) {
        if (i > nwn) sb_putc(&lock, '/');
        sb_puts(&lock, pb[i]);
    }
    if (nbn - 1 > nwn) sb_putc(&lock, '/');
    sb_printf(&lock, ".%s.lock", pb[nbn - 1]);

    json_append(out, json_string(rel.p ? rel.p : ""));
    json_append(out, json_string(lock.p ? lock.p : ""));
    sb_drop(&rel);
    sb_drop(&lock);
    return out;
}

json_t *w28_manager_87eef2_p1_exclude_store_args(const char *base, const char *working_dir) {
    json_t *out = json_array();
    json_t *paths = w28_manager_87eef2_p1_store_pathspecs(base, working_dir);
    size_t i, n = json_len(paths);
    if (n == 0) { json_free(paths); return out; }
    json_append(out, json_string("--"));
    for (i = 0; i < n; i++) {
        sb_t b;
        sb_init(&b);
        sb_puts(&b, ":(exclude)");
        sb_puts(&b, jstr(json_get(paths, i)));
        json_append(out, json_string(b.p ? b.p : ""));
        sb_drop(&b);
    }
    json_free(paths);
    return out;
}

json_t *w28_manager_87eef2_p1_stage_all_args(const char *base, const char *working_dir) {
    json_t *out = json_array();
    json_t *extra = w28_manager_87eef2_p1_exclude_store_args(base, working_dir);
    size_t i, n = json_len(extra);
    json_append(out, json_string("add"));
    json_append(out, json_string("-A"));
    for (i = 0; i < n; i++) json_append(out, json_copy(json_get(extra, i)));
    json_free(extra);
    return out;
}

/* =================================================================== */
/* Gate p2 — coercion, git-output parsing, manifests, retention, text   */
/* =================================================================== */

/* int(x) for the shapes a config value holds. */
static long py_int(const json_t *v, long def) {
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (long)v->num_val;
    if (v->type == JSON_STRING) {
        char *end = NULL;
        long r;
        if (!v->str_val) return def;
        r = strtol(v->str_val, &end, 10);
        while (end && isspace((unsigned char)*end)) end++;
        if (end && *end == '\0' && end != v->str_val) return r;
        return def;
    }
    return def;
}

json_t *w28_manager_87eef2_p2_coerce_init(const json_t *cfg) {
    json_t *out = json_object();
    long snaps = py_int(json_obj_get(cfg, "max_snapshots"), 20);
    long total = py_int(json_obj_get(cfg, "max_total_size_mb"), 500);
    long fsize = py_int(json_obj_get(cfg, "max_file_size_mb"), 10);
    if (snaps < 1) snaps = 1;
    if (total < 0) total = 0;
    if (fsize < 0) fsize = 0;
    json_set(out, "enabled", json_bool(jtruth(json_obj_get(cfg, "enabled")) ? true : false));
    json_set(out, "max_snapshots", json_number((double)snaps));
    json_set(out, "max_total_size_mb", json_number((double)total));
    json_set(out, "max_file_size_mb", json_number((double)fsize));
    return out;
}

/* re.search(r'(\d+) <word>', line) — leftmost, greedy digit run. */
static int scan_count(const char *line, const char *word, long *out) {
    size_t i = 0;
    size_t wl = strlen(word);
    while (line[i]) {
        if (isdigit((unsigned char)line[i]) && (i == 0 || !isdigit((unsigned char)line[i - 1]))) {
            size_t j = i;
            long val = 0;
            while (isdigit((unsigned char)line[j])) {
                if (val < 100000000L) val = val * 10 + (line[j] - '0');
                j++;
            }
            if (strncmp(line + j, word, wl) == 0) { *out = val; return 1; }
            i = j;
            continue;
        }
        i++;
    }
    return 0;
}

json_t *w28_manager_87eef2_p2_parse_shortstat(const char *stat_line) {
    json_t *out = json_object();
    long files = 0, ins = 0, dele = 0;
    const char *s = stat_line ? stat_line : "";
    if (!scan_count(s, " file", &files)) files = 0;
    if (!scan_count(s, " insertion", &ins)) ins = 0;
    if (!scan_count(s, " deletion", &dele)) dele = 0;
    json_set(out, "files_changed", json_number((double)files));
    json_set(out, "insertions", json_number((double)ins));
    json_set(out, "deletions", json_number((double)dele));
    return out;
}

/* Python str.splitlines() over the ASCII line boundaries these outputs use. */
static size_t split_lines(const char *s, char ***out) {
    size_t cap = 8, n = 0;
    char **v = (char **)calloc(cap, sizeof(char *));
    const char *p = s;
    if (!v) { *out = NULL; return 0; }
    while (*p) {
        const char *start = p;
        size_t len;
        while (*p && *p != '\n' && *p != '\r' && *p != '\v' && *p != '\f') p++;
        len = (size_t)(p - start);
        if (n + 1 >= cap) {
            char **nv = (char **)realloc(v, (cap * 2) * sizeof(char *));
            if (!nv) break;
            v = nv;
            cap *= 2;
        }
        v[n] = (char *)malloc(len + 1);
        if (!v[n]) break;
        memcpy(v[n], start, len);
        v[n][len] = '\0';
        n++;
        if (*p == '\r' && p[1] == '\n') p++;
        if (*p) p++;
    }
    *out = v;
    return n;
}

json_t *w28_manager_87eef2_p2_list_checkpoints(const json_t *req) {
    json_t *out = json_array();
    const char *log;
    char **lines = NULL;
    size_t nlines, i;
    const json_t *stats;

    if (!jbool_or(req, "has_head", 0)) return out;
    log = jstr_or(req, "log", "");
    if (!log[0]) return out;
    stats = json_obj_get(req, "stats");
    nlines = split_lines(log, &lines);
    for (i = 0; i < nlines; i++) {
        const char *line = lines[i];
        const char *bar1, *bar2, *bar3;
        json_t *entry;
        char *f_hash, *f_short, *f_time;
        const json_t *st;
        size_t hl, sl, tl;

        bar1 = strchr(line, '|');
        if (!bar1) { free(lines[i]); continue; }
        bar2 = strchr(bar1 + 1, '|');
        if (!bar2) { free(lines[i]); continue; }
        bar3 = strchr(bar2 + 1, '|');
        if (!bar3) { free(lines[i]); continue; }

        hl = (size_t)(bar1 - line);
        sl = (size_t)(bar2 - bar1 - 1);
        tl = (size_t)(bar3 - bar2 - 1);
        f_hash = (char *)malloc(hl + 1);
        f_short = (char *)malloc(sl + 1);
        f_time = (char *)malloc(tl + 1);
        if (!f_hash || !f_short || !f_time) {
            free(f_hash); free(f_short); free(f_time); free(lines[i]);
            break;
        }
        memcpy(f_hash, line, hl); f_hash[hl] = '\0';
        memcpy(f_short, bar1 + 1, sl); f_short[sl] = '\0';
        memcpy(f_time, bar2 + 1, tl); f_time[tl] = '\0';

        entry = json_object();
        json_set(entry, "hash", json_string(f_hash));
        json_set(entry, "short_hash", json_string(f_short));
        json_set(entry, "timestamp", json_string(f_time));
        json_set(entry, "reason", json_string(bar3 + 1));
        json_set(entry, "files_changed", json_number(0));
        json_set(entry, "insertions", json_number(0));
        json_set(entry, "deletions", json_number(0));

        st = (stats && stats->type == JSON_OBJECT) ? json_obj_get(stats, f_hash) : NULL;
        if (st && st->type == JSON_OBJECT && jbool_or(st, "ok", 0)) {
            const char *so = jstr_or(st, "out", "");
            if (so[0]) {
                json_t *parsed = w28_manager_87eef2_p2_parse_shortstat(so);
                json_set(entry, "files_changed", json_copy(json_obj_get(parsed, "files_changed")));
                json_set(entry, "insertions", json_copy(json_obj_get(parsed, "insertions")));
                json_set(entry, "deletions", json_copy(json_obj_get(parsed, "deletions")));
                json_free(parsed);
            }
        }
        json_append(out, entry);
        free(f_hash); free(f_short); free(f_time);
        free(lines[i]);
    }
    free(lines);
    return out;
}

/* json.loads(read_text(encoding='utf-8-sig')) when the result is a dict. */
static json_t *parse_manifest(const char *text) {
    const char *s = text ? text : "";
    json_t *v;
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        s += 3;
    v = json_parse(s, NULL);
    if (!v) return NULL;
    if (v->type != JSON_OBJECT) { json_free(v); return NULL; }
    return v;
}

json_t *w28_manager_87eef2_p2_ledger_load(const char *text) {
    json_t *v = parse_manifest(text);
    return v ? v : json_object();
}

json_t *w28_manager_87eef2_p2_ledger_trim(const json_t *ledger) {
    size_t n, i, kept = 0, cap = CM_LEDGER_MAX_ENTRIES;
    const char **keys;
    double *ts;
    size_t *order;
    json_t *out;

    if (!ledger || ledger->type != JSON_OBJECT) return json_object();
    n = json_object_size(ledger);
    if (n <= cap) return json_copy(ledger);

    keys = (const char **)calloc(n, sizeof(char *));
    ts = (double *)calloc(n, sizeof(double));
    order = (size_t *)calloc(n, sizeof(size_t));
    if (!keys || !ts || !order) {
        free(keys); free(ts); free(order);
        return json_copy(ledger);
    }
    for (i = 0; i < n; i++) {
        const json_t *entry = json_object_get_at(ledger, i);
        keys[i] = json_object_get_key_at(ledger, i);
        ts[i] = 0.0;
        if (entry && entry->type == JSON_OBJECT) {
            const json_t *t = json_obj_get(entry, "ts");
            if (t && t->type == JSON_NUMBER) ts[i] = t->num_val;
        }
        order[i] = i;
    }
    /* Stable sort, descending by ts: equal ts keep insertion order. */
    for (i = 1; i < n; i++) {
        size_t cur = order[i];
        size_t j = i;
        while (j > 0 && ts[order[j - 1]] < ts[cur]) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = cur;
    }
    out = json_object();
    for (i = 0; i < n && kept < cap; i++) {
        size_t idx = order[i];
        if (!keys[idx]) continue;
        json_set(out, keys[idx], json_copy(json_object_get_at(ledger, idx)));
        kept++;
    }
    free(keys); free(ts); free(order);
    return out;
}

/* _list_projects: glob('*.json') order -> sorted by hash by the harness. */
json_t *w28_manager_87eef2_p2_list_projects(const json_t *entries) {
    json_t *out = json_array();
    size_t i, n;
    if (!entries || entries->type != JSON_ARRAY) return out;
    n = json_len(entries);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(entries, i);
        json_t *meta;
        if (!e || e->type != JSON_OBJECT) continue;
        meta = parse_manifest(jstr_or(e, "text", ""));
        if (!meta) continue;
        json_set(meta, "_hash", json_string(jstr_or(e, "hash", "")));
        json_append(out, meta);
    }
    return out;
}

/* Path(workdir).name or workdir */
static void path_basename(const char *p, char *out, size_t cap) {
    char norm[4096];
    size_t i, last = 0;
    posix_norm(p, norm, sizeof(norm));
    if (!strcmp(norm, "/") || !strcmp(norm, ".")) { snprintf(out, cap, "%s", norm); return; }
    for (i = 0; norm[i]; i++) if (norm[i] == '/') last = i + 1;
    snprintf(out, cap, "%s", norm + last);
}

json_t *w28_manager_87eef2_p2_format_list(const json_t *checkpoints, const char *directory) {
    sb_t out;
    size_t i, n;
    const char *dir = directory ? directory : "";
    int all = (strcmp(dir, "all directories") == 0);

    if (!checkpoints || checkpoints->type != JSON_ARRAY || json_len(checkpoints) == 0) {
        sb_init(&out);
        sb_printf(&out, "No checkpoints found for %s", dir);
        {
            json_t *r = json_string(out.p ? out.p : "");
            sb_drop(&out);
            return r;
        }
    }
    n = json_len(checkpoints);
    sb_init(&out);
    sb_printf(&out, "\xF0\x9F\x93\xB8 Checkpoints for %s:\n", dir);
    for (i = 0; i < n; i++) {
        const json_t *cp = json_get(checkpoints, i);
        const char *ts_full = jstr_or(cp, "timestamp", "");
        const char *reason = jstr_or(cp, "reason", "");
        const char *short_hash = jstr_or(cp, "short_hash", "");
        const char *workdir = jstr_or(cp, "workdir", "");
        const json_t *f;
        long files = 0, ins = 0, dele = 0;
        char tsbuf[512];
        char statbuf[256] = "";

        f = json_obj_get(cp, "files_changed");
        if (f && f->type == JSON_NUMBER) files = (long)f->num_val;
        else if (f && f->type == JSON_BOOL) files = f->bool_val ? 1 : 0;
        f = json_obj_get(cp, "insertions");
        if (f && f->type == JSON_NUMBER) ins = (long)f->num_val;
        f = json_obj_get(cp, "deletions");
        if (f && f->type == JSON_NUMBER) dele = (long)f->num_val;

        snprintf(tsbuf, sizeof(tsbuf), "%s", ts_full);
        if (strchr(ts_full, 'T')) {
            const char *after = strchr(ts_full, 'T') + 1;
            const char *cut = after;
            char clock[256];
            size_t len;
            while (*cut && *cut != '+' && *cut != '-') cut++;
            len = (size_t)(cut - after);
            if (len > 5) len = 5;
            memcpy(clock, after, len);
            clock[len] = '\0';
            snprintf(tsbuf, sizeof(tsbuf), "%.*s %s", (int)(after - ts_full - 1), ts_full, clock);
        }
        if (files) {
            snprintf(statbuf, sizeof(statbuf), "  (%ld file%s, +%ld/-%ld)",
                     files, files == 1 ? "" : "s", ins, dele);
        }
        sb_printf(&out, "\n  %lu. %s  %s  ", (unsigned long)(i + 1), short_hash, tsbuf);
        if (workdir[0] && all) {
            char base[512];
            path_basename(workdir, base, sizeof(base));
            sb_printf(&out, "[%s]  ", base[0] ? base : workdir);
        }
        sb_printf(&out, "%s%s", reason, statbuf);
    }
    /* "\n".join(lines) where the first footer line itself begins with "\n". */
    sb_puts(&out, "\n\n  /rollback <N>             restore to checkpoint N");
    sb_puts(&out, "\n  /rollback diff <N>        preview changes since checkpoint N");
    sb_puts(&out, "\n  /rollback <N> <file>      restore a single file from checkpoint N");
    {
        json_t *r = json_string(out.p ? out.p : "");
        sb_drop(&out);
        return r;
    }
}

/* =================================================================== */
/* Gate p3 — restore + diff decisions                                  */
/* =================================================================== */

json_t *w28_manager_87eef2_p3_safe_restore_plan(const json_t *req) {
    json_t *out;
    json_t *hash_err;
    const char *commit = jstr_or(req, "commit", "");
    const char *dir = jstr_or(req, "dir", "");
    const json_t *names = json_obj_get(req, "names");
    const json_t *ledger = json_obj_get(req, "ledger");
    const json_t *current = json_obj_get(req, "current");
    json_t *restore, *skipped;
    size_t i, n;

    hash_err = w28_manager_87eef2_p1_validate_commit_hash(commit);
    if (hash_err && hash_err->type == JSON_STRING) {
        out = json_object();
        json_set(out, "success", json_bool(false));
        json_set(out, "error", hash_err);
        return out;
    }
    json_free(hash_err);

    out = json_object();
    if (!jbool_or(req, "has_head", 1)) {
        json_set(out, "success", json_bool(false));
        json_set(out, "error", json_string("No checkpoints exist for this directory"));
        return out;
    }
    if (!jbool_or(req, "ok", 1)) {
        sb_t b;
        sb_init(&b);
        sb_puts(&b, "Could not compute changed files: ");
        sb_puts(&b, jstr_or(req, "err", ""));
        json_set(out, "success", json_bool(false));
        json_set(out, "error", json_string(b.p ? b.p : ""));
        sb_drop(&b);
        return out;
    }
    if (!ledger || ledger->type != JSON_OBJECT || json_object_size(ledger) == 0) {
        json_set(out, "success", json_bool(true));
        json_set(out, "restore", json_array());
        json_set(out, "skipped", json_array());
        json_set(out, "ledger_empty", json_bool(true));
        return out;
    }

    restore = json_array();
    skipped = json_array();
    n = (names && names->type == JSON_ARRAY) ? json_len(names) : 0;
    for (i = 0; i < n; i++) {
        const char *rel = jstr(json_get(names, i));
        char abs[4200];
        const json_t *entry, *recorded, *cur;
        if (!rel[0]) continue;                 /* filter(None, ...) */
        posix_join(dir, rel, abs, sizeof(abs));
        entry = json_obj_get(ledger, abs);
        recorded = (entry && entry->type == JSON_OBJECT) ? json_obj_get(entry, "sha256") : NULL;
        if (!recorded || recorded->type == JSON_NULL) {
            json_append(skipped, json_string(rel));
            continue;
        }
        cur = (current && current->type == JSON_OBJECT) ? json_obj_get(current, abs) : NULL;
        if (!cur || cur->type == JSON_NULL) {  /* deleted since: restore is safe */
            json_append(restore, json_string(rel));
        } else if (jeq(cur, recorded)) {
            json_append(restore, json_string(rel));
        } else {
            json_append(skipped, json_string(rel));
        }
    }
    json_set(out, "success", json_bool(true));
    json_set(out, "restore", restore);
    json_set(out, "skipped", skipped);
    return out;
}

json_t *w28_manager_87eef2_p3_diff_result(const json_t *req) {
    json_t *out = json_object();
    json_t *hash_err;
    const char *commit = jstr_or(req, "commit", "");

    hash_err = w28_manager_87eef2_p1_validate_commit_hash(commit);
    if (hash_err && hash_err->type == JSON_STRING) {
        json_set(out, "success", json_bool(false));
        json_set(out, "error", hash_err);
        return out;
    }
    json_free(hash_err);

    if (!jbool_or(req, "has_head", 1)) {
        json_set(out, "success", json_bool(false));
        json_set(out, "error", json_string("No checkpoints exist for this directory"));
        return out;
    }
    if (!jbool_or(req, "cat_ok", 1)) {
        sb_t b;
        sb_init(&b);
        sb_printf(&b, "Checkpoint '%s' not found", commit);
        json_set(out, "success", json_bool(false));
        json_set(out, "error", json_string(b.p ? b.p : ""));
        sb_drop(&b);
        return out;
    }
    {
        int stat_ok = jbool_or(req, "stat_ok", 1);
        int diff_ok = jbool_or(req, "diff_ok", 1);
        if (!stat_ok && !diff_ok) {
            json_set(out, "success", json_bool(false));
            json_set(out, "error", json_string("Could not generate diff"));
            return out;
        }
        json_set(out, "success", json_bool(true));
        json_set(out, "stat", json_string(stat_ok ? jstr_or(req, "stat", "") : ""));
        json_set(out, "diff", json_string(diff_ok ? jstr_or(req, "diff", "") : ""));
    }
    return out;
}

json_t *w28_manager_87eef2_p3_session_diff(const json_t *req) {
    const json_t *cps = json_obj_get(req, "checkpoints");
    const json_t *diff = json_obj_get(req, "diff");
    size_t n = (cps && cps->type == JSON_ARRAY) ? json_len(cps) : 0;
    json_t *out;

    if (n == 0) {
        out = json_object();
        json_set(out, "success", json_bool(true));
        json_set(out, "stat", json_string(""));
        json_set(out, "diff", json_string(""));
        json_set(out, "empty", json_bool(true));
        return out;
    }
    out = (diff && diff->type == JSON_OBJECT) ? json_copy(diff) : json_object();
    if (jtruth(json_obj_get(out, "success"))) {
        const json_t *baseline = json_obj_get(json_get(cps, n - 1), "hash");
        if (!json_obj_get(out, "baseline"))
            json_set(out, "baseline", json_string(jstr(baseline)));
        if (!jtruth(json_obj_get(out, "stat")) && !jtruth(json_obj_get(out, "diff")))
            json_set(out, "empty", json_bool(true));
    }
    return out;
}

json_t *w28_manager_87eef2_p3_exceeds_size_cap(const json_t *req) {
    const json_t *v = json_obj_get(req, "cap_mb");
    double cap_mb = 0.0, cap, size = 0.0;
    if (v && v->type == JSON_NUMBER) cap_mb = v->num_val;
    cap = cap_mb * 1024.0 * 1024.0;
    if (cap <= 0) return json_bool(false);
    if (!jbool_or(req, "stat_ok", 1)) return json_bool(false);
    if (!jnumber(json_obj_get(req, "size"), &size)) return json_bool(false);
    return json_bool(size > cap ? true : false);
}

/* PoP: agent_checkpoints_manager_u_ref_name @ agent/checkpoints/manager.py:_ref_name */
/* f"{_REFS_PREFIX}/{dir_hash}" with _REFS_PREFIX == "refs/hermes". */
char *agent_checkpoints_manager_u_ref_name(const char *arg) {
    const char *h = arg ? arg : "";
    size_t need = strlen("refs/hermes/") + strlen(h) + 1;
    char *r = malloc(need);
    snprintf(r, need, "refs/hermes/%s", h);
    return r;
}
