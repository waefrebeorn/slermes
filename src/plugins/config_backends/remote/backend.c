/*
 * backend.c - C11 port of upstream plugins/config_backends/remote/backend.py
 * (gates w29_backend_010a82_p1, w29_backend_010a82_p2, w29_backend_010a82_p3).
 *
 * PoP: w29_backend_010a82_constants @ plugins/config_backends/remote/backend.py:POLL_SECONDS,MIN_POLL_SECONDS,BOOT_RETRY_DELAYS,MAX_RETRY_WAIT,_MAX_PREPARE,_MAX_POSTPROCESS,_VALUE_REFUSALS
 * PoP: w29_backend_010a82_poll_interval @ plugins/config_backends/remote/backend.py:poll_interval
 * PoP: w29_backend_010a82_latest_config_version @ plugins/config_backends/remote/backend.py:_latest_config_version
 * PoP: w29_backend_010a82_effective_ok @ plugins/config_backends/remote/backend.py:_effective_ok
 * PoP: w29_backend_010a82_profile_writer @ plugins/config_backends/remote/backend.py:_profile_writer
 * PoP: w29_backend_010a82_value_refusal @ plugins/config_backends/remote/backend.py:_VALUE_REFUSALS
 * PoP: w29_backend_010a82_write_error @ plugins/config_backends/remote/backend.py:RemoteBackend._write_error
 * PoP: w29_backend_010a82_note_level @ plugins/config_backends/remote/backend.py:_log_notes
 * PoP: w29_backend_010a82_capabilities @ plugins/config_backends/remote/backend.py:RemoteBackend.supports_file_tooling,honors_managed_config,protected_env_names,deployment_env_names,bootstrap_env_names
 * PoP: w29_backend_010a82_doctor_warnings @ plugins/config_backends/remote/backend.py:RemoteBackend.doctor_warnings
 * PoP: w29_backend_010a82_destination @ plugins/config_backends/remote/backend.py:RemoteBackend.destination
 * PoP: w29_backend_010a82_install @ plugins/config_backends/remote/backend.py:RemoteBackend._install
 * PoP: w29_backend_010a82_diff_base @ plugins/config_backends/remote/backend.py:RemoteBackend._diff_base
 * PoP: w29_backend_010a82_version_of @ plugins/config_backends/remote/backend.py:RemoteBackend._version_of
 * PoP: w29_backend_010a82_cache_version @ plugins/config_backends/remote/backend.py:RemoteBackend.cache_version
 * PoP: w29_backend_010a82_exists @ plugins/config_backends/remote/backend.py:RemoteBackend.exists
 * PoP: w29_backend_010a82_locked @ plugins/config_backends/remote/backend.py:RemoteBackend.locked
 * PoP: w29_backend_010a82_first_change @ plugins/config_backends/remote/backend.py:_first_change
 * PoP: w29_backend_010a82_inherited_notes @ plugins/config_backends/remote/backend.py:RemoteBackend._inherited_notes
 * PoP: w29_backend_010a82_migration_check_input @ plugins/config_backends/remote/backend.py:RemoteBackend._migration_check_input
 * PoP: w29_backend_010a82_describe @ plugins/config_backends/remote/backend.py:RemoteBackend.describe
 * PoP: w29_backend_010a82_intent @ plugins/config_backends/remote/backend.py:RemoteBackend._intent
 * PoP: w29_backend_010a82_patch_body @ plugins/config_backends/remote/backend.py:RemoteBackend._patch_body
 *
 * What is ported is the pure decision core of the remote config backend: the
 * poll/retry budget, response acceptance and refusal mapping, the install
 * derivation (R6 oldest writer, decoded locks, etag), the write diff base and
 * version tuple, lock lookup, the R5 advisory notes, the migration pre-check,
 * and the whole write intent / PATCH body computation (§15.2 steps 1-5) with
 * its lock and secret-literal refusals.
 *
 * Seams: every HTTP request (client.request), the credential, the poller
 * thread, the clocks (time.time / time_ns / monotonic, random jitter), logging,
 * and the migration runner all stay in the caller and reach this file as
 * fixture inputs (latest/floor/now/etag/bodies).  Nothing here opens a socket,
 * a config file or a managed directory.
 *
 * Harness convention: a config path is a JSON array of segments; a lock list is
 * an array of [path, level]; a set is an array of [path, value] pairs (a Python
 * dict's key order is not part of its value, so the gates compare them as
 * mappings).  JSON text carries no int/float tag, so an integral literal is a
 * Python int here, as Python's own json.loads makes it when written without a
 * decimal point.
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w29_backend_010a82_p1.h"
#include "noc_w29_backend_010a82_p2.h"
#include "noc_w29_backend_010a82_p3.h"

#define BK_POLL_SECONDS 300.0
#define BK_MIN_POLL_SECONDS 30.0
#define BK_MAX_RETRY_WAIT 20.0
#define BK_MAX_PREPARE 3
#define BK_MAX_POSTPROCESS 8
#define BK_MAX_SAFE_INTEGER 9007199254740991.0
#define BK_MAX_DEPTH 64

/* ------------------------------------------------------------------ strings */

static char *bk_strdup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (s) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

typedef struct {
    char *p;
    size_t len, cap;
} bbuf;

static void bb_init(bbuf *b) {
    b->cap = 64;
    b->len = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void bb_putc(bbuf *b, char c) {
    if (!b->p) return;
    if (b->len + 2 > b->cap) {
        size_t nc = b->cap * 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np;
        b->cap = nc;
    }
    b->p[b->len++] = c;
    b->p[b->len] = '\0';
}

static void bb_puts(bbuf *b, const char *s) {
    while (s && *s) bb_putc(b, *s++);
}

static char *fmt_alloc(const char *fmt, ...) {
    va_list ap, ap2;
    int need;
    char *out;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) { va_end(ap2); return NULL; }
    out = (char *)malloc((size_t)need + 1);
    if (out) vsnprintf(out, (size_t)need + 1, fmt, ap2);
    va_end(ap2);
    return out;
}

/* Python repr() of a str: quote choice and escapes. */
static char *py_repr_str(const char *s) {
    int has_single = 0, has_double = 0;
    char quote;
    bbuf b;
    const unsigned char *p;

    for (p = (const unsigned char *)(s ? s : ""); *p; p++) {
        if (*p == '\'') has_single = 1;
        else if (*p == '"') has_double = 1;
    }
    quote = (has_single && !has_double) ? '"' : '\'';
    bb_init(&b);
    bb_putc(&b, quote);
    for (p = (const unsigned char *)(s ? s : ""); *p; p++) {
        unsigned char c = *p;
        if (c == '\\' || c == (unsigned char)quote) { bb_putc(&b, '\\'); bb_putc(&b, (char)c); }
        else if (c == '\n') { bb_putc(&b, '\\'); bb_putc(&b, 'n'); }
        else if (c == '\r') { bb_putc(&b, '\\'); bb_putc(&b, 'r'); }
        else if (c == '\t') { bb_putc(&b, '\\'); bb_putc(&b, 't'); }
        else if (c < 0x20 || c == 0x7f) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "\\x%02x", (unsigned)c);
            bb_puts(&b, tmp);
        } else {
            bb_putc(&b, (char)c);
        }
    }
    bb_putc(&b, quote);
    return b.p;
}

/* ------------------------------------------------------------------- typing */

/* An integral JSON number stands for a Python int (the wire has no tag). */
static int bk_num_is_int(double v) {
    return v == (double)(long long)v && v > -9.3e18 && v < 9.3e18;
}

/* isinstance(v, int), which Python also answers for True/False. */
static int bk_is_int(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) return 1;
    return v->type == JSON_NUMBER && bk_num_is_int(v->num_val);
}

static double bk_int_value(const json_t *v) {
    if (!v) return 0.0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return v->num_val;
}

static int bk_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static const char *py_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return bk_num_is_int(v->num_val) ? "int" : "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* Python repr() for the JSON shapes an entry can carry. */
static char *py_repr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return bk_strdup("None");
    switch (v->type) {
    case JSON_BOOL: return bk_strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (bk_num_is_int(v->num_val)) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            return bk_strdup(buf);
        }
        snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return bk_strdup(buf);
    case JSON_STRING:
        return py_repr_str(v->str_val ? v->str_val : "");
    case JSON_ARRAY: {
        bbuf b;
        size_t i, n = json_len(v);
        bb_init(&b);
        bb_putc(&b, '[');
        for (i = 0; i < n; i++) {
            char *r = py_repr(json_get(v, i));
            if (i) bb_puts(&b, ", ");
            bb_puts(&b, r ? r : "");
            free(r);
        }
        bb_putc(&b, ']');
        return b.p;
    }
    case JSON_OBJECT: {
        bbuf b;
        size_t i, n = json_object_size(v);
        bb_init(&b);
        bb_putc(&b, '{');
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(v, i);
            char *kr = py_repr_str(k ? k : "");
            char *vr = py_repr(json_object_get_at(v, i));
            if (i) bb_puts(&b, ", ");
            bb_puts(&b, kr ? kr : "");
            bb_puts(&b, ": ");
            bb_puts(&b, vr ? vr : "");
            free(kr);
            free(vr);
        }
        bb_putc(&b, '}');
        return b.p;
    }
    }
    return bk_strdup("None");
}

/* Python str() for the values that are stringified on the wire. */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return bk_strdup("None");
    switch (v->type) {
    case JSON_BOOL: return bk_strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (bk_num_is_int(v->num_val)) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            return bk_strdup(buf);
        }
        snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return bk_strdup(buf);
    case JSON_STRING: return bk_strdup(v->str_val ? v->str_val : "");
    default: return py_repr(v);
    }
}

static int bk_json_equal(const json_t *a, const json_t *b) {
    size_t i, n;
    if (!a || !b) return a == b;
    if (a->type == JSON_BOOL || b->type == JSON_BOOL)
        return a->type == b->type && (a->type != JSON_BOOL || a->bool_val == b->bool_val);
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER)
        return a->num_val == b->num_val;
    switch (a->type) {
    case JSON_NULL: return b->type == JSON_NULL;
    case JSON_STRING:
        return b->type == JSON_STRING &&
               strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    case JSON_ARRAY:
        if (b->type != JSON_ARRAY) return 0;
        if (json_len(a) != json_len(b)) return 0;
        for (i = 0; i < json_len(a); i++)
            if (!bk_json_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    case JSON_OBJECT:
        if (b->type != JSON_OBJECT) return 0;
        if (json_object_size(a) != json_object_size(b)) return 0;
        n = json_object_size(a);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            const json_t *bv = json_obj_get(b, k ? k : "");
            if (!bv) return 0;
            if (!bk_json_equal(json_object_get_at(a, i), bv)) return 0;
        }
        return 1;
    default:
        return 0;
    }
}

/* -------------------------------------------------------------------- paths */

typedef struct {
    char **seg;
    size_t n;
} bpath;

static bpath bp_make(void) {
    bpath p;
    p.seg = NULL;
    p.n = 0;
    return p;
}

static void bp_free(bpath *p) {
    size_t i;
    for (i = 0; i < p->n; i++) free(p->seg[i]);
    free(p->seg);
    p->seg = NULL;
    p->n = 0;
}

static void bp_push(bpath *p, const char *s) {
    char **ns = (char **)realloc(p->seg, (p->n + 1) * sizeof(char *));
    if (!ns) return;
    p->seg = ns;
    p->seg[p->n] = bk_strdup(s ? s : "");
    if (p->seg[p->n]) p->n++;
}

static bpath bp_copy(const bpath *p) {
    bpath out = bp_make();
    size_t i;
    for (i = 0; i < p->n; i++) bp_push(&out, p->seg[i]);
    return out;
}

static bpath bp_cat(const bpath *pre, const char *seg) {
    bpath out = bp_copy(pre);
    bp_push(&out, seg);
    return out;
}

static int bp_eq(const bpath *a, const bpath *b) {
    size_t i;
    if (a->n != b->n) return 0;
    for (i = 0; i < a->n; i++)
        if (strcmp(a->seg[i] ? a->seg[i] : "", b->seg[i] ? b->seg[i] : "")) return 0;
    return 1;
}

static int bp_covers(const bpath *lock, const bpath *path) {
    size_t i;
    if (lock->n > path->n) return 0;
    for (i = 0; i < lock->n; i++)
        if (strcmp(lock->seg[i] ? lock->seg[i] : "", path->seg[i] ? path->seg[i] : "")) return 0;
    return 1;
}

static json_t *bp_json(const bpath *p) {
    json_t *a = json_array();
    size_t i;
    for (i = 0; i < p->n; i++) json_append(a, json_string(p->seg[i] ? p->seg[i] : ""));
    return a;
}

static bpath bp_from_json(const json_t *arr) {
    bpath p = bp_make();
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return p;
    for (i = 0; i < json_len(arr); i++) {
        const json_t *s = json_get(arr, i);
        bp_push(&p, (s && s->type == JSON_STRING && s->str_val) ? s->str_val : "");
    }
    return p;
}

/* encode(): each segment escapes \ and ., joined with ".". */
static char *bp_encode(const bpath *p) {
    bbuf b;
    size_t i;
    bb_init(&b);
    for (i = 0; i < p->n; i++) {
        const char *s = p->seg[i] ? p->seg[i] : "";
        if (i) bb_putc(&b, '.');
        for (; *s; s++) {
            if (*s == '\\' || *s == '.') bb_putc(&b, '\\');
            bb_putc(&b, *s);
        }
    }
    return b.p;
}

/* §3.1 limits, as validate() checks them. */
static int bp_validate(const bpath *p, const char *encoded) {
    size_t i, total;
    if (p->n < 1 || p->n > 32) return 0;
    total = strlen(encoded ? encoded : "");
    if (total > 2048) return 0;
    for (i = 0; i < p->n; i++) {
        const unsigned char *s = (const unsigned char *)(p->seg[i] ? p->seg[i] : "");
        size_t n = strlen((const char *)s), j;
        if (n < 1 || n > 256) return 0;
        for (j = 0; j < n; j++)
            if (s[j] < 0x20 || s[j] == 0x7f) return 0;
        if (!strcmp((const char *)s, "__proto__")) return 0;
    }
    return 1;
}

static int bp_decode(const char *encoded, bpath *out) {
    bbuf cur;
    size_t i = 0, n = strlen(encoded ? encoded : "");
    int ok = 1;

    bb_init(&cur);
    if (!cur.p) return 0;
    while (i < n) {
        char ch = encoded[i];
        if (ch == '\\') {
            if (i + 1 >= n || (encoded[i + 1] != '\\' && encoded[i + 1] != '.')) { ok = 0; break; }
            bb_putc(&cur, encoded[i + 1]);
            i += 2;
            continue;
        }
        if (ch == '.') {
            if (cur.len == 0) { ok = 0; break; }
            bp_push(out, cur.p);
            cur.len = 0;
            if (cur.p) cur.p[0] = '\0';
        } else {
            bb_putc(&cur, ch);
        }
        i++;
    }
    if (ok) {
        if (cur.len == 0) ok = 0;
        else bp_push(out, cur.p);
    }
    free(cur.p);
    if (ok && !bp_validate(out, encoded)) ok = 0;
    return ok;
}

/* from_dotted(): the encoded grammar, else a split on every ".". */
static bpath bp_from_dotted(const char *dotted) {
    bpath p = bp_make();
    if (bp_decode(dotted ? dotted : "", &p)) return p;
    bp_free(&p);
    p = bp_make();
    {
        const char *start = dotted ? dotted : "";
        for (;;) {
            const char *dot = strchr(start, '.');
            if (!dot) { bp_push(&p, start); break; }
            {
                size_t len = (size_t)(dot - start);
                char *piece = (char *)malloc(len + 1);
                if (piece) {
                    memcpy(piece, start, len);
                    piece[len] = '\0';
                    bp_push(&p, piece);
                    free(piece);
                }
            }
            start = dot + 1;
        }
    }
    return p;
}

/* --------------------------------------------------------- ordered maps/sets */

typedef struct {
    bpath path;
    json_t *value;
} bset;

typedef struct {
    bset *it;
    size_t n, cap;
} bsets;

static long bs_find(const bsets *s, const bpath *p) {
    size_t i;
    for (i = 0; i < s->n; i++)
        if (bp_eq(&s->it[i].path, p)) return (long)i;
    return -1;
}

static void bs_put(bsets *s, const bpath *p, json_t *value) {
    long idx = bs_find(s, p);
    if (idx >= 0) {
        json_free(s->it[idx].value);
        s->it[idx].value = value;
        return;
    }
    if (s->n == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 8;
        bset *ni = (bset *)realloc(s->it, nc * sizeof(bset));
        if (!ni) { json_free(value); return; }
        s->it = ni;
        s->cap = nc;
    }
    s->it[s->n].path = bp_copy(p);
    s->it[s->n].value = value;
    s->n++;
}

static void bs_setdefault(bsets *s, const bpath *p, json_t *value) {
    if (bs_find(s, p) >= 0) { json_free(value); return; }
    bs_put(s, p, value);
}

static void bs_free(bsets *s) {
    size_t i;
    for (i = 0; i < s->n; i++) {
        bp_free(&s->it[i].path);
        json_free(s->it[i].value);
    }
    free(s->it);
    s->it = NULL;
    s->n = s->cap = 0;
}

typedef struct {
    bpath *it;
    size_t n, cap;
} bpaths;

static void bps_push(bpaths *s, const bpath *p) {
    if (s->n == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 8;
        bpath *ni = (bpath *)realloc(s->it, nc * sizeof(bpath));
        if (!ni) return;
        s->it = ni;
        s->cap = nc;
    }
    s->it[s->n++] = bp_copy(p);
}

static void bps_free(bpaths *s) {
    size_t i;
    for (i = 0; i < s->n; i++) bp_free(&s->it[i]);
    free(s->it);
    s->it = NULL;
    s->n = s->cap = 0;
}

typedef struct {
    bpath path;
    json_t *level;   /* borrowed from the request */
} block;

typedef struct {
    block *it;
    size_t n, cap;
} blocks;

static blocks blocks_from_json(const json_t *arr) {
    blocks l;
    size_t i;
    memset(&l, 0, sizeof(l));
    if (!arr || arr->type != JSON_ARRAY) return l;
    for (i = 0; i < json_len(arr); i++) {
        const json_t *pair = json_get(arr, i);
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
        if (l.n == l.cap) {
            size_t nc = l.cap ? l.cap * 2 : 8;
            block *ni = (block *)realloc(l.it, nc * sizeof(block));
            if (!ni) break;
            l.it = ni;
            l.cap = nc;
        }
        l.it[l.n].path = bp_from_json(json_get(pair, 0));
        l.it[l.n].level = (json_t *)json_get(pair, 1);
        l.n++;
    }
    return l;
}

static void blocks_free(blocks *l) {
    size_t i;
    for (i = 0; i < l->n; i++) bp_free(&l->it[i].path);
    free(l->it);
    l->it = NULL;
    l->n = l->cap = 0;
}

/* ------------------------------------------------------------ doc primitives */

static int bk_get_path(const json_t *doc, const bpath *p, const json_t **out) {
    const json_t *node = doc;
    size_t i;
    for (i = 0; i < p->n; i++) {
        if (!node || node->type != JSON_OBJECT) return 0;
        node = json_obj_get(node, p->seg[i] ? p->seg[i] : "");
        if (!node) return 0;
    }
    *out = node;
    return 1;
}

/* _set_path: takes ownership of value. */
static void bk_set_path(json_t *doc, const bpath *p, json_t *value) {
    json_t *node = doc;
    size_t i;
    if (!doc || doc->type != JSON_OBJECT || p->n == 0) { json_free(value); return; }
    for (i = 0; i + 1 < p->n; i++) {
        const char *seg = p->seg[i] ? p->seg[i] : "";
        json_t *next = json_obj_get(node, seg);
        if (!next || next->type != JSON_OBJECT) {
            next = json_object();
            json_set(node, seg, next);
        }
        node = next;
    }
    json_set(node, p->seg[p->n - 1] ? p->seg[p->n - 1] : "", value);
}

static int bk_pop_path(json_t *doc, const bpath *p) {
    json_t *node = doc;
    size_t i;
    if (p->n == 0) return 0;
    for (i = 0; i + 1 < p->n; i++) {
        if (!node || node->type != JSON_OBJECT) return 0;
        node = json_obj_get(node, p->seg[i] ? p->seg[i] : "");
        if (!node || node->type != JSON_OBJECT) return 0;
    }
    if (node && node->type == JSON_OBJECT)
        return json_obj_del(node, p->seg[p->n - 1] ? p->seg[p->n - 1] : "") ? 1 : 0;
    return 0;
}

/* ---------------------------------------------------------------- diff core */

static void bk_diff(const json_t *base, const json_t *newv, const bpath *prefix,
                    bsets *sets, bpaths *unsets) {
    size_t i, n = json_object_size(newv);
    for (i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(newv, i);
        const json_t *nv = json_object_get_at(newv, i);
        bpath p = bp_cat(prefix, key ? key : "");
        const json_t *bv = json_obj_get(base, key ? key : "");
        if (!bv) {
            bs_put(sets, &p, json_copy(nv));
        } else if (bv->type == JSON_OBJECT && nv && nv->type == JSON_OBJECT) {
            bk_diff(bv, nv, &p, sets, unsets);
        } else if (!bk_json_equal(bv, nv)) {
            bs_put(sets, &p, json_copy(nv));
        }
        bp_free(&p);
    }
    n = json_object_size(base);
    for (i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(base, i);
        if (!json_obj_get(newv, key ? key : "")) {
            bpath p = bp_cat(prefix, key ? key : "");
            bps_push(unsets, &p);
            bp_free(&p);
        }
    }
}

static void bk_intent_diff(const json_t *base, const json_t *newv, const bpath *prefix,
                           bsets *sets, bpaths *unsets) {
    size_t i, n = json_object_size(newv);
    for (i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(newv, i);
        const json_t *nv = json_object_get_at(newv, i);
        bpath p = bp_cat(prefix, key ? key : "");
        const json_t *old = json_obj_get(base, key ? key : "");
        if (nv && nv->type == JSON_OBJECT && json_object_size(nv) > 0 &&
            (!old || old->type != JSON_OBJECT)) {
            json_t *empty = json_object();
            bpaths discard;
            memset(&discard, 0, sizeof(discard));
            bk_intent_diff(empty, nv, &p, sets, &discard);
            bps_free(&discard);
            json_free(empty);
        } else if (!old) {
            bs_put(sets, &p, json_copy(nv));
        } else if (old->type == JSON_OBJECT && nv && nv->type == JSON_OBJECT) {
            bk_intent_diff(old, nv, &p, sets, unsets);
        } else if (!bk_json_equal(old, nv)) {
            bs_put(sets, &p, json_copy(nv));
        }
        bp_free(&p);
    }
    n = json_object_size(base);
    for (i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(base, i);
        if (!json_obj_get(newv, key ? key : "")) {
            bpath p = bp_cat(prefix, key ? key : "");
            bps_push(unsets, &p);
            bp_free(&p);
        }
    }
}

static void bk_apply_intent(json_t *doc, const bsets *sets, const bpaths *unsets) {
    size_t i;
    for (i = 0; i < sets->n; i++) {
        const json_t *v = sets->it[i].value;
        if (v && v->type == JSON_OBJECT && json_object_size(v) == 0) {
            const json_t *cur = NULL;
            if (bk_get_path(doc, &sets->it[i].path, &cur) && cur && cur->type == JSON_OBJECT)
                continue;   /* "make this a mapping": a mapping is already there */
        }
        bk_set_path(doc, &sets->it[i].path, json_copy(v));
    }
    for (i = 0; i < unsets->n; i++) bk_pop_path(doc, &unsets->it[i]);
}

static void bk_strip_locked(json_t *base, json_t *newv, const blocks *locks, bpaths *dropped) {
    size_t i;
    for (i = 0; i < locks->n; i++) {
        const json_t *bv = NULL, *nv = NULL;
        int in_base = bk_get_path(base, &locks->it[i].path, &bv);
        int in_new = bk_get_path(newv, &locks->it[i].path, &nv);
        if (in_base != in_new || (in_new && !bk_json_equal(bv, nv)))
            bps_push(dropped, &locks->it[i].path);
        bk_pop_path(base, &locks->it[i].path);
        bk_pop_path(newv, &locks->it[i].path);
    }
}

/* §6.4 rule 2: the path itself and every path inside value, objects only. */
static int bk_check_touched(const bpath *p, const json_t *v, const blocks *locks, size_t *li) {
    size_t i;
    for (i = 0; i < locks->n; i++) {
        if (bp_covers(&locks->it[i].path, p)) { *li = i; return 1; }
        if (p->n < locks->it[i].path.n && bp_covers(p, &locks->it[i].path) &&
            !(v && v->type == JSON_OBJECT)) { *li = i; return 1; }
    }
    if (v && v->type == JSON_OBJECT) {
        size_t k, n = json_object_size(v);
        for (k = 0; k < n; k++) {
            const char *key = json_object_get_key_at(v, k);
            bpath child = bp_cat(p, key ? key : "");
            int hit = bk_check_touched(&child, json_object_get_at(v, k), locks, li);
            bp_free(&child);
            if (hit) return 1;
        }
    }
    return 0;
}

static int bk_write_check(const bsets *sets, const bpaths *unsets, const blocks *locks,
                          size_t *out_lock) {
    size_t i;
    for (i = 0; i < unsets->n; i++) {
        size_t j;
        for (j = 0; j < locks->n; j++)
            if (bp_covers(&locks->it[j].path, &unsets->it[i])) { *out_lock = j; return 1; }
    }
    for (i = 0; i < sets->n; i++)
        if (bk_check_touched(&sets->it[i].path, sets->it[i].value, locks, out_lock)) return 1;
    return 0;
}

/* ------------------------------------------------------------- secret rules */

static const char *const BK_SECRET_KEYS[] = {
    "api_key", "apikey", "key", "token", "access_token", "refresh_token", "id_token",
    "secret", "client_secret", "password", "passwd", "authorization", "private_key",
    "bearer", "jwt",
};
static const char *const BK_SECRET_SUFFIXES[] = {
    "_api_key", "_token", "_secret", "_password", "_key", "_access_key",
};
static const char *const BK_NON_SECRET_SUFFIXES[] = {
    "_url", "_host", "_user", "_id", "_domain", "_scheme",
};
static const char *const BK_ENV_KEYS[] = {
    "OPENROUTER_API_KEY", "OPENAI_API_KEY", "ANTHROPIC_API_KEY", "VOICE_TOOLS_OPENAI_KEY",
    "EXA_API_KEY", "PARALLEL_API_KEY", "FIRECRAWL_API_KEY", "FIRECRAWL_API_URL",
    "FIRECRAWL_GATEWAY_URL", "TOOL_GATEWAY_URL", "CONNECTOR_GATEWAY_URL", "TOOL_GATEWAY_DOMAIN",
    "TOOL_GATEWAY_SCHEME", "TOOL_GATEWAY_USER_TOKEN", "TAVILY_API_KEY", "PERPLEXITY_API_KEY",
    "API_SERVER_KEY", "BROWSERBASE_API_KEY", "BROWSERBASE_PROJECT_ID", "BROWSER_USE_API_KEY",
    "FAL_KEY", "TELEGRAM_BOT_TOKEN", "DISCORD_BOT_TOKEN", "TERMINAL_SSH_HOST",
    "TERMINAL_SSH_USER", "TERMINAL_SSH_KEY", "SUDO_PASSWORD", "SLACK_BOT_TOKEN",
    "SLACK_APP_TOKEN", "GITHUB_TOKEN", "HONCHO_API_KEY",
};
static const char *const BK_ENV_SUFFIXES[] = { "_API_KEY", "_TOKEN", "_SECRET" };
static const char *const BK_ENV_PREFIXES[] = { "TERMINAL_SSH" };
static const char *const BK_NON_SECRET_PATHS[][3] = {
    { "voice", "record_key", NULL },
    { "terminal", "docker_shared_container_key", NULL },
    { "browser", "camofox", "session_key" },
};
static const size_t BK_NON_SECRET_PATH_LEN[] = { 2, 2, 3 };

#define BK_COUNT(a) (sizeof(a) / sizeof((a)[0]))

static char *bk_leaf(const char *s) {
    size_t i, n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (c == '-') c = '_';
        else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out[i] = c;
    }
    out[n] = '\0';
    return out;
}

static int bk_ends_with(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return b <= a && strcmp(s + a - b, suffix) == 0;
}

static int bk_in_list(const char *s, const char *const *items, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (!strcmp(s, items[i])) return 1;
    return 0;
}

static int bk_ends_any(const char *s, const char *const *items, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (bk_ends_with(s, items[i])) return 1;
    return 0;
}

static int bk_starts_any(const char *s, const char *const *items, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (strncmp(s, items[i], strlen(items[i])) == 0) return 1;
    return 0;
}

static int bk_is_env_config_key(const char *key) {
    char *up;
    int hit;
    size_t i, n;
    if (strchr(key, '.')) return 0;
    n = strlen(key);
    up = (char *)malloc(n + 1);
    if (!up) return 0;
    for (i = 0; i < n; i++) {
        char c = key[i];
        up[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }
    up[n] = '\0';
    hit = bk_in_list(up, BK_ENV_KEYS, BK_COUNT(BK_ENV_KEYS)) ||
          bk_ends_any(up, BK_ENV_SUFFIXES, BK_COUNT(BK_ENV_SUFFIXES)) ||
          bk_starts_any(up, BK_ENV_PREFIXES, BK_COUNT(BK_ENV_PREFIXES));
    free(up);
    return hit;
}

static int bk_is_secret_path(const bpath *p) {
    char *leaf;
    int secret;
    size_t i, j;

    if (p->n == 0) return 0;
    for (i = 0; i < BK_COUNT(BK_NON_SECRET_PATHS); i++) {
        int same = 1;
        if (BK_NON_SECRET_PATH_LEN[i] != p->n) continue;
        for (j = 0; j < p->n; j++)
            if (strcmp(p->seg[j] ? p->seg[j] : "", BK_NON_SECRET_PATHS[i][j])) { same = 0; break; }
        if (same) return 0;
    }
    leaf = bk_leaf(p->seg[p->n - 1]);
    if (!leaf) return 0;
    if (p->n == 1 && bk_is_env_config_key(p->seg[0] ? p->seg[0] : "")) {
        secret = !bk_ends_any(leaf, BK_NON_SECRET_SUFFIXES, BK_COUNT(BK_NON_SECRET_SUFFIXES));
        free(leaf);
        return secret;
    }
    secret = bk_in_list(leaf, BK_SECRET_KEYS, BK_COUNT(BK_SECRET_KEYS)) ||
             bk_ends_any(leaf, BK_SECRET_SUFFIXES, BK_COUNT(BK_SECRET_SUFFIXES));
    free(leaf);
    return secret;
}

/* ^\$\{[A-Za-z_][A-Za-z0-9_]*\}$, fullmatch. */
static int bk_placeholder(const char *s) {
    size_t len = strlen(s), i;
    char c;
    if (len < 4) return 0;
    if (!(s[0] == '$' && s[1] == '{' && s[len - 1] == '}')) return 0;
    c = s[2];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')) return 0;
    for (i = 3; i + 1 < len; i++) {
        c = s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

static int bk_is_literal(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL || v->type == JSON_NULL) return 0;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        return s[0] != '\0' && !bk_placeholder(s);
    }
    return v->type == JSON_NUMBER;
}

/* secret_literal_path: first (path, literal) walking objects and lists. */
static int bk_secret_walk(const json_t *value, const bpath *p, bpath *hit) {
    size_t i, count;
    if (p->n > 0 && bk_is_secret_path(p) && bk_is_literal(value)) {
        *hit = bp_copy(p);
        return 1;
    }
    if (!value || p->n + 1 >= BK_MAX_DEPTH) return 0;
    if (value->type == JSON_OBJECT) {
        count = json_object_size(value);
        for (i = 0; i < count; i++) {
            const char *k = json_object_get_key_at(value, i);
            bpath child = bp_cat(p, k ? k : "");
            int got = bk_secret_walk(json_object_get_at(value, i), &child, hit);
            bp_free(&child);
            if (got) return 1;
        }
    } else if (value->type == JSON_ARRAY) {
        count = json_len(value);
        for (i = 0; i < count; i++)   /* list elements inherit the list's path */
            if (bk_secret_walk(json_get(value, i), p, hit)) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ to_wire */

static json_t *bk_to_wire(const json_t *value, const bpath *p, char **err) {
    size_t i, count;
    if (!value || value->type == JSON_NULL) return json_null();
    switch (value->type) {
    case JSON_NULL:   return json_null();
    case JSON_STRING: return json_string(value->str_val ? value->str_val : "");
    case JSON_BOOL:   return json_bool(value->bool_val);
    case JSON_NUMBER:
        if (bk_num_is_int(value->num_val)) {
            if (value->num_val >= -BK_MAX_SAFE_INTEGER && value->num_val <= BK_MAX_SAFE_INTEGER)
                return json_number(value->num_val);
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%lld", (long long)value->num_val);
                return json_string(buf);
            }
        }
        if (!isfinite(value->num_val)) {
            char *label = bp_encode(p);
            *err = fmt_alloc("%s: NaN/Infinity cannot be stored in Remote Config",
                             (label && label[0]) ? label : "<root>");
            free(label);
            return NULL;
        }
        return json_number(value->num_val);
    case JSON_ARRAY:
        count = json_len(value);
        {
            json_t *out = json_array();
            for (i = 0; i < count; i++) {
                json_t *conv = bk_to_wire(json_get(value, i), p, err);
                if (!conv) { json_free(out); return NULL; }
                json_append(out, conv);
            }
            return out;
        }
    case JSON_OBJECT:
        count = json_object_size(value);
        {
            json_t *out = json_object();
            for (i = 0; i < count; i++) {
                const char *k = json_object_get_key_at(value, i);
                bpath child = bp_cat(p, k ? k : "");
                json_t *conv = bk_to_wire(json_object_get_at(value, i), &child, err);
                bp_free(&child);
                if (!conv) { json_free(out); return NULL; }
                json_set(out, k ? k : "", conv);
            }
            return out;
        }
    }
    return json_null();
}

/* --------------------------------------------------------------- responses */

/* A fresh copy of doc without the reserved _config_version. */
static json_t *bk_without_version(const json_t *doc) {
    json_t *copy = doc ? json_copy(doc) : json_object();
    if (copy && copy->type != JSON_OBJECT) { json_free(copy); return json_object(); }
    json_obj_del(copy, "_config_version");
    return copy;
}

/* ==================================================================== P1 === */

json_t *w29_backend_010a82_constants(void) {
    json_t *o = json_object();
    json_t *delays = json_array();
    static const struct { int status; const char *error; } refusals[] = {
        { 400, "config_path_invalid" }, { 400, "config_path_reserved" },
        { 400, "config_secret_literal" }, { 400, "config_value_invalid" },
        { 413, "config_body_too_large" }, { 413, "config_level_too_large" },
    };
    json_t *ref;
    size_t i;

    json_set(o, "poll_seconds", json_number(BK_POLL_SECONDS));
    json_set(o, "min_poll_seconds", json_number(BK_MIN_POLL_SECONDS));
    json_append(delays, json_number(10.0));
    json_append(delays, json_number(20.0));
    json_set(o, "boot_retry_delays", delays);
    json_set(o, "max_retry_wait", json_number(BK_MAX_RETRY_WAIT));
    json_set(o, "max_prepare", json_number((double)BK_MAX_PREPARE));
    json_set(o, "max_postprocess", json_number((double)BK_MAX_POSTPROCESS));
    ref = json_array();
    for (i = 0; i < sizeof(refusals) / sizeof(refusals[0]); i++) {
        json_t *pair = json_array();
        json_append(pair, json_number((double)refusals[i].status));
        json_append(pair, json_string(refusals[i].error));
        json_append(ref, pair);
    }
    json_set(o, "value_refusals", ref);
    return o;
}

json_t *w29_backend_010a82_poll_interval(void) {
    return json_number(BK_POLL_SECONDS > BK_MIN_POLL_SECONDS ? BK_POLL_SECONDS : BK_MIN_POLL_SECONDS);
}

json_t *w29_backend_010a82_latest_config_version(const json_t *req) {
    const json_t *cfg = req && req->type == JSON_OBJECT ? json_obj_get(req, "default_config") : NULL;
    const json_t *v = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "_config_version") : NULL;
    if (bk_is_int(v)) return json_number(bk_int_value(v));
    return json_number(1.0);
}

json_t *w29_backend_010a82_effective_ok(const json_t *body) {
    const json_t *cfg, *pv, *locks;
    if (!body || body->type != JSON_OBJECT) return json_bool(false);
    cfg = json_obj_get(body, "config");
    pv = json_obj_get(body, "profileVersion");
    locks = json_obj_get(body, "locks");
    return json_bool(cfg && cfg->type == JSON_OBJECT && bk_is_int(pv) &&
                     locks && locks->type == JSON_ARRAY);
}

json_t *w29_backend_010a82_profile_writer(const json_t *body) {
    json_t *out = json_object();
    const json_t *levels = body && body->type == JSON_OBJECT ? json_obj_get(body, "levels") : NULL;
    size_t i;
    json_set(out, "known", json_bool(false));
    json_set(out, "value", json_null());
    if (levels && levels->type == JSON_ARRAY) {
        for (i = 0; i < json_len(levels); i++) {
            const json_t *lv = json_get(levels, i);
            const json_t *kind, *w;
            if (!lv || lv->type != JSON_OBJECT) continue;
            kind = json_obj_get(lv, "kind");
            if (!kind || kind->type != JSON_STRING || !kind->str_val ||
                strcmp(kind->str_val, "profile")) continue;
            w = json_obj_get(lv, "writerConfigVersion");
            if (!w || w->type == JSON_NULL) {
                json_set(out, "known", json_bool(true));
                json_set(out, "value", json_null());
                return out;
            }
            if (bk_is_int(w)) {
                json_set(out, "known", json_bool(true));
                json_set(out, "value", json_number(bk_int_value(w)));
                return out;
            }
            return out;   /* an int-typed slot holding something else: _UNKNOWN */
        }
    }
    return out;
}

json_t *w29_backend_010a82_value_refusal(const json_t *req) {
    static const int statuses[] = { 400, 400, 400, 400, 413, 413 };
    static const char *const errors[] = {
        "config_path_invalid", "config_path_reserved", "config_secret_literal",
        "config_value_invalid", "config_body_too_large", "config_level_too_large",
    };
    const json_t *st = req && req->type == JSON_OBJECT ? json_obj_get(req, "status") : NULL;
    const json_t *er = req && req->type == JSON_OBJECT ? json_obj_get(req, "error") : NULL;
    size_t i;
    if (!bk_is_int(st) || !er || er->type != JSON_STRING) return json_bool(false);
    for (i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++)
        if ((int)bk_int_value(st) == statuses[i] && er->str_val &&
            !strcmp(er->str_val, errors[i])) return json_bool(true);
    return json_bool(false);
}

json_t *w29_backend_010a82_note_level(const json_t *req) {
    const json_t *note = req && req->type == JSON_OBJECT ? json_obj_get(req, "note") : NULL;
    const char *s = note && note->type == JSON_STRING ? note->str_val : "";
    json_t *out = json_object();
    if (s && !strncmp(s, "debug:", 6)) {
        json_set(out, "level", json_string("debug"));
        json_set(out, "message", json_string(s + 6));
    } else {
        json_set(out, "level", json_string("warning"));
        json_set(out, "message", json_string(s ? s : ""));
    }
    return out;
}

json_t *w29_backend_010a82_write_error(const json_t *resp) {
    static const int ref_status[] = { 400, 400, 400, 400, 413, 413 };
    static const char *const ref_error[] = {
        "config_path_invalid", "config_path_reserved", "config_secret_literal",
        "config_value_invalid", "config_body_too_large", "config_level_too_large",
    };
    const json_t *body = resp && resp->type == JSON_OBJECT ? json_obj_get(resp, "body") : NULL;
    const json_t *stv = resp && resp->type == JSON_OBJECT ? json_obj_get(resp, "status") : NULL;
    int status = (int)bk_int_value(stv);
    const json_t *ev, *mv;
    char *error, *message, *detail = NULL, *msg = NULL;
    json_t *out = json_object();
    size_t i;

    if (!body || body->type != JSON_OBJECT) body = NULL;
    ev = body ? json_obj_get(body, "error") : NULL;
    mv = body ? json_obj_get(body, "message") : NULL;
    error = (ev && bk_truthy(ev)) ? py_str(ev) : fmt_alloc("http_%d", status);
    message = (mv && bk_truthy(mv)) ? py_str(mv) : fmt_alloc("HTTP %d", status);

    if (status == 403 && error && !strcmp(error, "config_key_locked")) {
        const json_t *pv = body ? json_obj_get(body, "path") : NULL;
        const json_t *lv = body ? json_obj_get(body, "lockedBy") : NULL;
        char *path = (pv && bk_truthy(pv)) ? py_str(pv) : bk_strdup("");
        char *level = (lv && bk_truthy(lv)) ? py_str(lv) : bk_strdup("upper");
        msg = fmt_alloc("Remote Config refused the write: %s", message ? message : "");
        json_set(out, "error", json_string("ConfigLockedError"));
        json_set(out, "code", json_string("config_key_locked"));
        json_set(out, "message", json_string(msg ? msg : ""));
        json_set(out, "path", json_string(path ? path : ""));
        json_set(out, "locked_by", json_string(level ? level : ""));
        free(path); free(level); free(msg); free(error); free(message);
        return out;
    }
    for (i = 0; i < sizeof(ref_status) / sizeof(ref_status[0]); i++) {
        bbuf extras;
        int any = 0;
        const json_t *pv, *rv;
        if (status != ref_status[i] || !error || strcmp(error, ref_error[i])) continue;
        detail = bk_strdup((message && message[0]) ? message : error);
        pv = body ? json_obj_get(body, "path") : NULL;
        rv = body ? json_obj_get(body, "reason") : NULL;
        bb_init(&extras);
        if (pv && pv->type == JSON_STRING && pv->str_val && pv->str_val[0] &&
            !strstr(detail ? detail : "", pv->str_val)) {
            bb_puts(&extras, "path ");
            bb_puts(&extras, pv->str_val);
            any = 1;
        }
        if (rv && rv->type == JSON_STRING && rv->str_val && rv->str_val[0] &&
            !strstr(detail ? detail : "", rv->str_val)) {
            if (any) bb_puts(&extras, ", ");
            bb_puts(&extras, "reason ");
            bb_puts(&extras, rv->str_val);
            any = 1;
        }
        {
            char *joined = any
                ? fmt_alloc("Remote Config refused the write: %s (%s)",
                            detail ? detail : "", extras.p ? extras.p : "")
                : fmt_alloc("Remote Config refused the write: %s", detail ? detail : "");
            free(detail);
            detail = joined;
        }
        free(extras.p);
        json_set(out, "error", json_string("ConfigValueError"));
        json_set(out, "code", json_string(error));
        json_set(out, "message", json_string(detail ? detail : ""));
        json_set(out, "path", json_null());
        json_set(out, "locked_by", json_null());
        free(detail); free(error); free(message);
        return out;
    }
    msg = fmt_alloc("Remote Config refused the write (%d %s): %s",
                    status, error ? error : "", message ? message : "");
    json_set(out, "error", json_string("ConfigWriteError"));
    json_set(out, "code", json_string(error ? error : ""));
    json_set(out, "message", json_string(msg ? msg : ""));
    json_set(out, "path", json_null());
    json_set(out, "locked_by", json_null());
    free(msg); free(error); free(message);
    return out;
}

json_t *w29_backend_010a82_capabilities(void) {
    static const char *const protected_names[] = {
        "GATEWAY_RELAY_IDP_CLIENT_ID", "GATEWAY_RELAY_IDP_CLIENT_SECRET",
        "GATEWAY_RELAY_IDP_SCOPE", "GATEWAY_RELAY_IDP_TOKEN_URL",
        "HERMES_CONFIG_BACKEND", "HERMES_CONFIG_INSTANCE_ID", "HERMES_CONFIG_REMOTE_URL",
        "HERMES_PORTAL_BASE_URL", "HERMES_SHARED_AUTH_DIR", "NOUS_PORTAL_BASE_URL",
    };
    static const char *const deployment_names[] = {
        "HERMES_CONFIG_BACKEND", "HERMES_CONFIG_INSTANCE_ID", "HERMES_CONFIG_REMOTE_URL",
    };
    json_t *o = json_object();
    json_t *prot = json_array();
    json_t *dep = json_array();
    json_t *boot;
    size_t i;

    for (i = 0; i < BK_COUNT(protected_names); i++) json_append(prot, json_string(protected_names[i]));
    for (i = 0; i < BK_COUNT(deployment_names); i++) json_append(dep, json_string(deployment_names[i]));
    json_set(o, "supports_file_tooling", json_bool(false));
    json_set(o, "honors_managed_config", json_bool(false));
    json_set(o, "protected_env_names", prot);
    json_set(o, "deployment_env_names", dep);
    /* bootstrap_env_names() is PLANE_CREDENTIAL_ENV_NAMES itself. */
    boot = json_copy(prot);
    json_set(o, "bootstrap_env_names", boot);
    return o;
}

json_t *w29_backend_010a82_doctor_warnings(const json_t *req) {
    const json_t *m = req && req->type == JSON_OBJECT ? json_obj_get(req, "managed") : NULL;
    json_t *out = json_array();
    char *text, *remove;
    if (!m || m->type != JSON_STRING || !m->str_val || !m->str_val[0]) return out;
    text = fmt_alloc("%s exists but is ignored in remote config mode", m->str_val);
    remove = fmt_alloc("Remove %s: remote config mode takes config and locks only from Remote Config",
                       m->str_val);
    {
        json_t *triple = json_array();
        json_append(triple, json_string(text ? text : ""));
        json_append(triple, json_string("(remove it)"));
        json_append(triple, json_string(remove ? remove : ""));
        json_append(out, triple);
    }
    free(text);
    free(remove);
    return out;
}

json_t *w29_backend_010a82_destination(const json_t *req) {
    const json_t *p = req && req->type == JSON_OBJECT ? json_obj_get(req, "profile") : NULL;
    char *rep = py_repr_str((p && p->type == JSON_STRING && p->str_val) ? p->str_val : "");
    char *line = fmt_alloc("Remote Config (profile %s level)", rep ? rep : "''");
    json_t *out = json_string(line ? line : "");
    free(rep);
    free(line);
    return out;
}

/* ==================================================================== P2 === */

json_t *w29_backend_010a82_install(const json_t *req) {
    const json_t *body = req && req->type == JSON_OBJECT ? json_obj_get(req, "body") : NULL;
    const json_t *etag_in = req && req->type == JSON_OBJECT ? json_obj_get(req, "etag") : NULL;
    const json_t *latestv = req && req->type == JSON_OBJECT ? json_obj_get(req, "latest") : NULL;
    const json_t *config = body && body->type == JSON_OBJECT ? json_obj_get(body, "config") : NULL;
    const json_t *levels = body && body->type == JSON_OBJECT ? json_obj_get(body, "levels") : NULL;
    const json_t *body_etag = body && body->type == JSON_OBJECT ? json_obj_get(body, "etag") : NULL;
    const json_t *pv = body && body->type == JSON_OBJECT ? json_obj_get(body, "profileVersion") : NULL;
    const json_t *prov = body && body->type == JSON_OBJECT ? json_obj_get(body, "provenance") : NULL;
    const json_t *locks = body && body->type == JSON_OBJECT ? json_obj_get(body, "locks") : NULL;
    double latest = bk_is_int(latestv) ? bk_int_value(latestv) : 1.0;
    json_t *out = json_object();
    json_t *doc, *notes, *locks_out, *others;
    int have_version = 0, version_is_bool = 0;
    double min_version = 0.0;
    size_t i;

    if (!config || config->type != JSON_OBJECT) config = json_object();
    doc = json_copy(config);
    json_obj_del(doc, "_config_version");

    notes = json_array();
    locks_out = json_array();
    others = json_array();

    if (levels && levels->type == JSON_ARRAY) {
        for (i = 0; i < json_len(levels); i++) {
            const json_t *lv = json_get(levels, i);
            const json_t *w, *kind;
            if (!lv || lv->type != JSON_OBJECT) continue;
            w = json_obj_get(lv, "writerConfigVersion");
            if (bk_is_int(w)) {
                double v = bk_int_value(w);
                if (!have_version || v < min_version) {
                    have_version = 1;
                    min_version = v;
                    version_is_bool = (w->type == JSON_BOOL);
                }
            }
            kind = json_obj_get(lv, "kind");
            if (!(kind && kind->type == JSON_STRING && kind->str_val &&
                  !strcmp(kind->str_val, "profile")) && bk_is_int(w))
                json_append(others, json_number(bk_int_value(w)));
        }
    }
    {
        double doc_version = have_version ? min_version : latest;
        json_t *dv;
        if (have_version && version_is_bool) {
            dv = json_bool(min_version != 0.0);
            json_set(doc, "_config_version", json_bool(min_version != 0.0));
        } else {
            dv = json_number(doc_version);
            json_set(doc, "_config_version", json_number(doc_version));
        }
        json_set(out, "doc_version", dv);
    }
    json_set(out, "doc", doc);

    if (locks && locks->type == JSON_ARRAY) {
        for (i = 0; i < json_len(locks); i++) {
            const json_t *entry = json_get(locks, i);
            const json_t *pathv = (entry && entry->type == JSON_OBJECT)
                                      ? json_obj_get(entry, "path") : NULL;
            const json_t *levelv = (entry && entry->type == JSON_OBJECT)
                                       ? json_obj_get(entry, "level") : NULL;
            bpath p = bp_make();
            int ok = 0;
            if (entry && entry->type == JSON_OBJECT && pathv && levelv &&
                pathv->type == JSON_STRING)
                ok = bp_decode(pathv->str_val ? pathv->str_val : "", &p);
            if (!ok) {
                char *rep = py_repr(entry);
                char *note = fmt_alloc("Remote Config: ignoring malformed lock entry %s",
                                       rep ? rep : "None");
                json_append(notes, json_string(note ? note : ""));
                free(note);
                free(rep);
                bp_free(&p);
                continue;
            }
            {
                json_t *pair = json_array();
                char *ls = py_str(levelv);
                json_append(pair, bp_json(&p));
                json_append(pair, json_string(ls ? ls : ""));
                json_append(locks_out, pair);
                free(ls);
            }
            bp_free(&p);
        }
    }

    json_set(out, "profile_version",
             (pv && bk_is_int(pv)) ? json_number(bk_int_value(pv)) : json_null());
    json_set(out, "profile_writer", w29_backend_010a82_profile_writer(body));
    json_set(out, "other_writers", others);
    json_set(out, "locks", locks_out);
    if (etag_in && etag_in->type == JSON_STRING && etag_in->str_val && etag_in->str_val[0]) {
        json_set(out, "etag", json_string(etag_in->str_val));
    } else if (body_etag && bk_truthy(body_etag)) {
        char *s = py_str(body_etag);
        json_set(out, "etag", json_string(s ? s : ""));
        free(s);
    } else {
        json_set(out, "etag", json_string(""));
    }
    json_set(out, "provenance", (prov && prov->type == JSON_OBJECT) ? json_copy(prov) : json_object());
    json_set(out, "notes", notes);
    return out;
}

json_t *w29_backend_010a82_diff_base(const json_t *state) {
    const json_t *base = state && state->type == JSON_OBJECT ? json_obj_get(state, "base") : NULL;
    const json_t *server = state && state->type == JSON_OBJECT
                               ? json_obj_get(state, "server_config") : NULL;
    if (base && base->type != JSON_NULL) return bk_without_version(base);
    return bk_without_version(server);
}

json_t *w29_backend_010a82_version_of(const json_t *state) {
    const json_t *priv = state && state->type == JSON_OBJECT ? json_obj_get(state, "private") : NULL;
    json_t *out = json_array();
    const json_t *changed = state && state->type == JSON_OBJECT
                                ? json_obj_get(state, "changed_ns") : NULL;
    const json_t *etag = state && state->type == JSON_OBJECT ? json_obj_get(state, "etag") : NULL;
    const json_t *gen = state && state->type == JSON_OBJECT ? json_obj_get(state, "gen") : NULL;
    if (priv && priv->type == JSON_OBJECT) {
        const json_t *serial = json_obj_get(priv, "serial");
        const json_t *mgen = json_obj_get(priv, "gen");
        json_append(out, json_number(-bk_int_value(serial)));
        json_append(out, json_number(bk_int_value(mgen)));
        json_append(out, json_number(bk_int_value(changed)));
        json_append(out, (etag && etag->type == JSON_STRING) ? json_string(etag->str_val)
                                                             : json_string(""));
        json_append(out, json_number(bk_int_value(gen)));
    } else {
        json_append(out, json_number(bk_int_value(changed)));
        json_append(out, (etag && etag->type == JSON_STRING) ? json_string(etag->str_val)
                                                             : json_string(""));
        json_append(out, json_number(bk_int_value(gen)));
    }
    return out;
}

json_t *w29_backend_010a82_cache_version(const json_t *version) {
    return version ? json_copy(version) : json_null();
}

json_t *w29_backend_010a82_exists(void) {
    return json_bool(true);
}

json_t *w29_backend_010a82_locked(const json_t *req) {
    blocks locks;
    const json_t *dotted = req && req->type == JSON_OBJECT ? json_obj_get(req, "dotted") : NULL;
    bpath path;
    json_t *out = json_object();
    size_t i;

    if (!dotted || dotted->type != JSON_STRING) {
        json_set(out, "found", json_bool(false));
        json_set(out, "level", json_null());
        return out;
    }
    locks = blocks_from_json(req && req->type == JSON_OBJECT ? json_obj_get(req, "locks") : NULL);
    path = bp_from_dotted(dotted->str_val ? dotted->str_val : "");
    json_set(out, "found", json_bool(false));
    json_set(out, "level", json_null());
    for (i = 0; i < locks.n; i++) {
        if (bp_covers(&locks.it[i].path, &path)) {
            json_set(out, "found", json_bool(true));
            json_set(out, "level", locks.it[i].level ? json_copy(locks.it[i].level) : json_null());
            break;
        }
    }
    bp_free(&path);
    blocks_free(&locks);
    return out;
}

static json_t *bk_first_change(const bpath *path, const json_t *sent, int found,
                               const json_t *got) {
    char *err = NULL;
    if (!found) return bp_json(path);
    if (sent && sent->type == JSON_OBJECT && got && got->type == JSON_OBJECT) {
        size_t i, n = json_object_size(sent);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(sent, i);
            bpath child = bp_cat(path, k ? k : "");
            json_t *res = bk_first_change(&child,
                                          json_object_get_at(sent, i),
                                          json_obj_get(got, k ? k : "") ? 1 : 0,
                                          json_obj_get(got, k ? k : ""));
            bp_free(&child);
            if (res) return res;
        }
        return NULL;
    }
    {
        json_t *wire = bk_to_wire(got, path, &err);
        if (!wire || err) {
            free(err);
            json_free(wire);
            return bp_json(path);
        }
        if (bk_json_equal(wire, sent)) { json_free(wire); return NULL; }
        json_free(wire);
        return bp_json(path);
    }
}

json_t *w29_backend_010a82_first_change(const json_t *req) {
    bpath path;
    json_t *out;
    const json_t *pathv = req && req->type == JSON_OBJECT ? json_obj_get(req, "path") : NULL;
    const json_t *sent = req && req->type == JSON_OBJECT ? json_obj_get(req, "sent") : NULL;
    const json_t *got = req && req->type == JSON_OBJECT ? json_obj_get(req, "got") : NULL;
    int found = (req && req->type == JSON_OBJECT) ? json_get_bool(req, "found", false) : 0;
    path = bp_from_json(pathv);
    out = bk_first_change(&path, sent, found, got);
    bp_free(&path);
    return out ? out : json_null();
}

json_t *w29_backend_010a82_inherited_notes(const json_t *req) {
    const json_t *server = req && req->type == JSON_OBJECT
                               ? json_obj_get(req, "server_config") : NULL;
    const json_t *unsets = req && req->type == JSON_OBJECT ? json_obj_get(req, "unsets") : NULL;
    json_t *out = json_array();
    size_t i;

    if (!unsets || unsets->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(unsets); i++) {
        bpath p = bp_from_json(json_get(unsets, i));
        const json_t *node = server;
        size_t j;
        int present = 1;
        for (j = 0; j < p.n; j++) {
            if (!node || node->type != JSON_OBJECT) { present = 0; break; }
            node = json_obj_get(node, p.seg[j] ? p.seg[j] : "");
            if (!node) { present = 0; break; }
        }
        if (present) {
            char *enc = bp_encode(&p);
            char *note = fmt_alloc(
                "Remote Config: %s is still set by an upper level; removing it from this "
                "profile cannot remove an inherited value (set it to null for 'no value')",
                enc ? enc : "");
            json_append(out, json_string(note ? note : ""));
            free(note);
            free(enc);
        }
        bp_free(&p);
    }
    return out;
}

json_t *w29_backend_010a82_migration_check_input(const json_t *req) {
    const json_t *server = req && req->type == JSON_OBJECT
                               ? json_obj_get(req, "server_config") : NULL;
    const json_t *others = req && req->type == JSON_OBJECT
                               ? json_obj_get(req, "other_writers") : NULL;
    const json_t *pw = req && req->type == JSON_OBJECT ? json_obj_get(req, "profile_writer") : NULL;
    const json_t *sets_in = req && req->type == JSON_OBJECT ? json_obj_get(req, "sets") : NULL;
    const json_t *unsets_in = req && req->type == JSON_OBJECT ? json_obj_get(req, "unsets") : NULL;
    int has_writer = (req && req->type == JSON_OBJECT) ? json_get_bool(req, "has_writer", false) : 0;
    double latest = bk_int_value(req && req->type == JSON_OBJECT ? json_obj_get(req, "latest") : NULL);
    double floor_v = bk_int_value(req && req->type == JSON_OBJECT ? json_obj_get(req, "floor") : NULL);
    int have = 0;
    double after = 0.0;
    size_t i;

    if (!sets_in || sets_in->type != JSON_ARRAY || json_len(sets_in) == 0) return json_null();

    if (others && others->type == JSON_ARRAY) {
        for (i = 0; i < json_len(others); i++) {
            const json_t *v = json_get(others, i);
            if (!bk_is_int(v)) continue;
            if (!have || bk_int_value(v) < after) { after = bk_int_value(v); have = 1; }
        }
    }
    if (has_writer) {
        if (!have || latest < after) { after = latest; have = 1; }
    } else if (pw && pw->type == JSON_OBJECT) {
        const json_t *kind = json_obj_get(pw, "kind");
        if (kind && kind->type == JSON_STRING && kind->str_val && !strcmp(kind->str_val, "int")) {
            double v = bk_int_value(json_obj_get(pw, "value"));
            if (!have || v < after) { after = v; have = 1; }
        }
    }
    if (!have) after = latest;
    if (!(after >= floor_v && after < latest)) return json_null();

    {
        json_t *stored = bk_without_version(server);
        json_t *out = json_object();
        for (i = 0; i < json_len(sets_in); i++) {
            const json_t *pair = json_get(sets_in, i);
            bpath p;
            if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
            p = bp_from_json(json_get(pair, 0));
            bk_set_path(stored, &p, json_copy(json_get(pair, 1)));
            bp_free(&p);
        }
        if (unsets_in && unsets_in->type == JSON_ARRAY) {
            for (i = 0; i < json_len(unsets_in); i++) {
                bpath p = bp_from_json(json_get(unsets_in, i));
                bk_pop_path(stored, &p);
                bp_free(&p);
            }
        }
        json_set(out, "stored", stored);
        json_set(out, "after", json_number(after));
        return out;
    }
}

json_t *w29_backend_010a82_describe(const json_t *req) {
    const json_t *url = req && req->type == JSON_OBJECT ? json_obj_get(req, "url") : NULL;
    const json_t *inst = req && req->type == JSON_OBJECT ? json_obj_get(req, "instance_id") : NULL;
    const json_t *nowv = req && req->type == JSON_OBJECT ? json_obj_get(req, "now") : NULL;
    const json_t *state = req && req->type == JSON_OBJECT ? json_obj_get(req, "state") : NULL;
    const char *u = (url && url->type == JSON_STRING && url->str_val) ? url->str_val : "";
    char *head, *line;
    json_t *out;

    head = fmt_alloc("Remote Config %s (instance %s)", u,
                     (inst && inst->type == JSON_STRING && inst->str_val && inst->str_val[0])
                         ? inst->str_val : "<unset>");
    if (!state || state->type == JSON_NULL) {
        line = fmt_alloc("%s: profile not fetched yet", head ? head : "");
        out = json_string(line ? line : "");
        free(head);
        free(line);
        return out;
    }
    {
        const json_t *profile = json_obj_get(state, "profile");
        const json_t *pv = json_obj_get(state, "profile_version");
        const json_t *lk = json_obj_get(state, "locks_count");
        const json_t *fetched = json_obj_get(state, "fetched_at");
        const json_t *last_error = json_obj_get(state, "last_error");
        char *rep = py_repr(profile);
        char *tail = "";
        char *tail_alloc = NULL;
        long long age = -1;
        if (fetched && bk_truthy(fetched))
            age = (long long)(bk_int_value(nowv) - bk_int_value(fetched));
        if (last_error && last_error->type == JSON_STRING && last_error->str_val &&
            last_error->str_val[0]) {
            tail_alloc = fmt_alloc(", last poll failed: %s", last_error->str_val);
            tail = tail_alloc ? tail_alloc : "";
        }
        line = fmt_alloc("%s: profile %s, profile level v%lld, %lld lock(s), fetched %llds ago%s",
                         head ? head : "", rep ? rep : "None",
                         (long long)bk_int_value(pv), (long long)bk_int_value(lk), age, tail);
        out = json_string(line ? line : "");
        free(rep);
        free(tail_alloc);
        free(head);
        free(line);
    }
    return out;
}

/* ==================================================================== P3 === */

static json_t *bk_err(const char *type, const char *code, char *msg) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string(type));
    json_set(o, "code", json_string(code));
    json_set(o, "message", json_string(msg ? msg : ""));
    free(msg);
    return o;
}

static json_t *bk_ok(void) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    return o;
}

static json_t *bk_sets_json(const bsets *sets) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < sets->n; i++) {
        json_t *pair = json_array();
        json_append(pair, bp_json(&sets->it[i].path));
        json_append(pair, json_copy(sets->it[i].value));
        json_append(out, pair);
    }
    return out;
}

static json_t *bk_paths_json(const bpaths *paths) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < paths->n; i++) json_append(out, bp_json(&paths->it[i]));
    return out;
}

static void bk_sets_from_json(const json_t *arr, bsets *out) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return;
    for (i = 0; i < json_len(arr); i++) {
        const json_t *pair = json_get(arr, i);
        bpath p;
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
        p = bp_from_json(json_get(pair, 0));
        bs_put(out, &p, json_copy(json_get(pair, 1)));
        bp_free(&p);
    }
}

static void bk_paths_from_json(const json_t *arr, bpaths *out) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return;
    for (i = 0; i < json_len(arr); i++) {
        bpath p = bp_from_json(json_get(arr, i));
        bps_push(out, &p);
        bp_free(&p);
    }
}

static int cmp_cstr(const void *a, const void *b) {
    const char *const *sa = (const char *const *)a;
    const char *const *sb = (const char *const *)b;
    return strcmp(*sa, *sb);
}

/* The pinned leaves a document save / a keyed set names, added with setdefault. */
static void bk_add_pinned(const json_t *newv, const bpath *path, bsets *sets, bsets *pinned) {
    const json_t *wire = NULL;
    if (!bk_get_path(newv, path, &wire)) return;
    if (wire && wire->type == JSON_OBJECT && json_object_size(wire) > 0) {
        bsets leaves;
        bpaths discard;
        json_t *empty = json_object();
        size_t li;
        memset(&leaves, 0, sizeof(leaves));
        memset(&discard, 0, sizeof(discard));
        bk_intent_diff(empty, wire, path, &leaves, &discard);
        for (li = 0; li < leaves.n; li++) {
            bs_setdefault(sets, &leaves.it[li].path, json_copy(leaves.it[li].value));
            if (bs_find(pinned, &leaves.it[li].path) < 0)
                bs_put(pinned, &leaves.it[li].path, NULL);
        }
        bs_free(&leaves);
        bps_free(&discard);
        json_free(empty);
    } else {
        bs_setdefault(sets, path, json_copy(wire));
        if (bs_find(pinned, path) < 0) bs_put(pinned, path, NULL);
    }
}

json_t *w29_backend_010a82_intent(const json_t *req) {
    const json_t *server = req && req->type == JSON_OBJECT
                               ? json_obj_get(req, "server_config") : NULL;
    const json_t *st_base = req && req->type == JSON_OBJECT ? json_obj_get(req, "st_base") : NULL;
    const json_t *read_base = req && req->type == JSON_OBJECT
                                  ? json_obj_get(req, "read_base") : NULL;
    const json_t *document = req && req->type == JSON_OBJECT ? json_obj_get(req, "document") : NULL;
    const json_t *set_in = req && req->type == JSON_OBJECT ? json_obj_get(req, "set") : NULL;
    const json_t *unset_in = req && req->type == JSON_OBJECT ? json_obj_get(req, "unset") : NULL;
    const json_t *pinned_in = req && req->type == JSON_OBJECT
                                  ? json_obj_get(req, "pinned_keys") : NULL;
    const json_t *refused = req && req->type == JSON_OBJECT
                                ? json_obj_get(req, "refused_prefixes") : NULL;
    int private_ = (req && req->type == JSON_OBJECT) ? json_get_bool(req, "private", false) : 0;
    int is_doc = document && document->type != JSON_NULL;
    blocks locks;
    json_t *base, *newv, *out;
    bsets sets, pinned;   /* pinned values stay NULL: only paths matter */
    bpaths unsets, dropped;
    size_t i;

    memset(&sets, 0, sizeof(sets));
    memset(&pinned, 0, sizeof(pinned));
    memset(&unsets, 0, sizeof(unsets));
    memset(&dropped, 0, sizeof(dropped));
    locks = blocks_from_json(req && req->type == JSON_OBJECT ? json_obj_get(req, "locks") : NULL);

    base = (st_base && st_base->type == JSON_OBJECT) ? bk_without_version(st_base)
                                                     : bk_without_version(server);
    if (is_doc) {
        char *err = NULL;
        bpath p = bp_make();
        if (!private_ && read_base && read_base->type == JSON_OBJECT) {
            json_free(base);
            base = bk_without_version(read_base);
        }
        newv = bk_to_wire(document, &p, &err);
        bp_free(&p);
        if (!newv || err) {
            free(err);
            json_free(newv);
            json_free(base);
            blocks_free(&locks);
            return bk_err("ConfigValueError", "config_value_invalid", NULL);
        }
        if (newv->type != JSON_OBJECT) {
            json_free(newv);
            json_free(base);
            blocks_free(&locks);
            return bk_err("ConfigValueError", "config_value_invalid",
                          bk_strdup("the config document must be a mapping"));
        }
    } else {
        newv = json_copy(base);
    }

    if (set_in && set_in->type == JSON_OBJECT) {
        size_t n = json_object_size(set_in);
        for (i = 0; i < n; i++) {
            const char *key = json_object_get_key_at(set_in, i);
            bpath p = bp_from_dotted(key ? key : "");
            char *err = NULL;
            json_t *wire = bk_to_wire(json_object_get_at(set_in, i), &p, &err);
            if (!wire || err) {
                free(err);
                json_free(wire);
                bp_free(&p);
                bs_free(&sets);
                bps_free(&unsets);
                bps_free(&dropped);
                blocks_free(&locks);
                json_free(base);
                json_free(newv);
                return bk_err("ConfigValueError", "config_value_invalid", NULL);
            }
            bk_set_path(newv, &p, wire);
            bp_free(&p);
        }
    }
    if (unset_in && unset_in->type == JSON_ARRAY) {
        for (i = 0; i < json_len(unset_in); i++) {
            const json_t *k = json_get(unset_in, i);
            if (k && k->type == JSON_STRING) {
                bpath p = bp_from_dotted(k->str_val ? k->str_val : "");
                bk_pop_path(newv, &p);
                bp_free(&p);
            }
        }
    }
    json_obj_del(newv, "_config_version");

    if (is_doc) {
        bk_strip_locked(base, newv, &locks, &dropped);
        if (refused && refused->type == JSON_ARRAY) {
            for (i = 0; i < json_len(refused); i++) {
                const json_t *pre = json_get(refused, i);
                bpath whole;
                size_t d;
                if (!pre || pre->type != JSON_STRING) continue;
                whole = bp_from_dotted(pre->str_val ? pre->str_val : "");
                for (d = 0; d < dropped.n; d++) {
                    if (bp_covers(&whole, &dropped.it[d]) || bp_covers(&dropped.it[d], &whole)) {
                        char *enc = bp_encode(&dropped.it[d]);
                        char *level = bk_strdup("upper");
                        char *msg;
                        size_t li;
                        for (li = 0; li < locks.n; li++)
                            if (bp_eq(&locks.it[li].path, &dropped.it[d])) {
                                free(level);
                                level = py_str(locks.it[li].level);
                                break;
                            }
                        msg = fmt_alloc("%s is locked by the %s level of Remote Config",
                                        enc ? enc : "", level ? level : "upper");
                        free(enc);
                        free(level);
                        bp_free(&whole);
                        json_free(base);
                        json_free(newv);
                        bs_free(&sets);
                        bps_free(&unsets);
                        bps_free(&dropped);
                        blocks_free(&locks);
                        return bk_err("ConfigLockedError", "config_key_locked", msg);
                    }
                }
                bp_free(&whole);
            }
        }
    }

    {
        bpath empty = bp_make();
        bk_intent_diff(base, newv, &empty, &sets, &unsets);
        bp_free(&empty);
    }

    /* named(): the keyed sets, then (a document save) the pinned keys. */
    if (set_in && set_in->type == JSON_OBJECT) {
        size_t n = json_object_size(set_in);
        for (i = 0; i < n; i++) {
            const char *key = json_object_get_key_at(set_in, i);
            bpath path = bp_from_dotted(key ? key : "");
            bk_add_pinned(newv, &path, &sets, &pinned);
            bp_free(&path);
        }
    }
    if (is_doc && pinned_in && pinned_in->type == JSON_ARRAY) {
        for (i = 0; i < json_len(pinned_in); i++) {
            const json_t *k = json_get(pinned_in, i);
            bpath path;
            if (!k || k->type != JSON_STRING) continue;
            path = bp_from_dotted(k->str_val ? k->str_val : "");
            bk_add_pinned(newv, &path, &sets, &pinned);
            bp_free(&path);
        }
    }

    out = bk_ok();
    json_set(out, "sets", bk_sets_json(&sets));
    json_set(out, "unsets", bk_paths_json(&unsets));
    {
        json_t *pin = json_array();
        size_t k, n = pinned.n;
        char **enc = (char **)calloc(n ? n : 1, sizeof(char *));
        for (k = 0; k < n; k++) enc[k] = bp_encode(&pinned.it[k].path);
        qsort(enc, n, sizeof(char *), cmp_cstr);
        for (k = 0; k < n; k++) {
            json_append(pin, json_string(enc[k] ? enc[k] : ""));
            free(enc[k]);
        }
        free(enc);
        json_set(out, "pinned", pin);
    }

    bs_free(&sets);
    bs_free(&pinned);
    bps_free(&unsets);
    bps_free(&dropped);
    blocks_free(&locks);
    json_free(base);
    json_free(newv);
    return out;
}

json_t *w29_backend_010a82_patch_body(const json_t *req) {
    const json_t *server = req && req->type == JSON_OBJECT
                               ? json_obj_get(req, "server_config") : NULL;
    const json_t *st_base = req && req->type == JSON_OBJECT ? json_obj_get(req, "st_base") : NULL;
    const json_t *sets_in = req && req->type == JSON_OBJECT ? json_obj_get(req, "sets") : NULL;
    const json_t *unsets_in = req && req->type == JSON_OBJECT
                                  ? json_obj_get(req, "unsets") : NULL;
    const json_t *pinned_in = req && req->type == JSON_OBJECT ? json_obj_get(req, "pinned") : NULL;
    const json_t *pw = req && req->type == JSON_OBJECT ? json_obj_get(req, "profile_writer") : NULL;
    double latest = bk_int_value(req && req->type == JSON_OBJECT ? json_obj_get(req, "latest") : NULL);
    const json_t *pv = req && req->type == JSON_OBJECT ? json_obj_get(req, "profile_version") : NULL;
    blocks locks;
    json_t *base, *newv, *out;
    bsets intent_sets, sets;
    bpaths intent_unsets, unsets;
    size_t i;

    memset(&intent_sets, 0, sizeof(intent_sets));
    memset(&sets, 0, sizeof(sets));
    memset(&intent_unsets, 0, sizeof(intent_unsets));
    memset(&unsets, 0, sizeof(unsets));
    locks = blocks_from_json(req && req->type == JSON_OBJECT ? json_obj_get(req, "locks") : NULL);
    bk_sets_from_json(sets_in, &intent_sets);
    bk_paths_from_json(unsets_in, &intent_unsets);

    base = (st_base && st_base->type == JSON_OBJECT) ? bk_without_version(st_base)
                                                     : bk_without_version(server);
    newv = json_copy(base);
    bk_apply_intent(newv, &intent_sets, &intent_unsets);
    {
        bpath empty = bp_make();
        bk_diff(base, newv, &empty, &sets, &unsets);
        bp_free(&empty);
    }

    if (pinned_in && pinned_in->type == JSON_ARRAY) {
        for (i = 0; i < json_len(pinned_in); i++) {
            bpath p = bp_from_json(json_get(pinned_in, i));
            long idx = bs_find(&intent_sets, &p);
            const json_t *value = idx >= 0 ? intent_sets.it[idx].value : NULL;
            int covered = 0;
            size_t li;
            if (value && value->type == JSON_OBJECT && json_object_size(value) == 0) {
                bp_free(&p);
                continue;
            }
            if (bs_find(&sets, &p) >= 0) { bp_free(&p); continue; }
            for (li = 0; li < locks.n; li++)
                if (bp_covers(&locks.it[li].path, &p)) { covered = 1; break; }
            if (!covered)
                for (li = 0; li < sets.n; li++)
                    if (bp_covers(&sets.it[li].path, &p)) { covered = 1; break; }
            if (!covered) bs_put(&sets, &p, json_copy(value));
            bp_free(&p);
        }
    }

    {
        size_t li;
        if (bk_write_check(&sets, &unsets, &locks, &li)) {
            char *enc = bp_encode(&locks.it[li].path);
            char *level = py_str(locks.it[li].level);
            char *msg = fmt_alloc("%s is locked by the %s level of Remote Config",
                                  enc ? enc : "", level ? level : "upper");
            free(enc);
            free(level);
            bs_free(&sets);
            bps_free(&unsets);
            bs_free(&intent_sets);
            bps_free(&intent_unsets);
            blocks_free(&locks);
            json_free(base);
            json_free(newv);
            return bk_err("ConfigLockedError", "config_key_locked", msg);
        }
    }
    for (i = 0; i < sets.n; i++) {
        bpath bad = bp_make();
        if (bk_secret_walk(sets.it[i].value, &sets.it[i].path, &bad)) {
            char *enc = bp_encode(&bad);
            char *msg = fmt_alloc(
                "%s is a secret-shaped key: Remote Config stores only a ${VAR} reference "
                "there, never the secret itself. Put the value in .env and set a ${VAR} reference.",
                enc ? enc : "");
            free(enc);
            bp_free(&bad);
            bs_free(&sets);
            bps_free(&unsets);
            bs_free(&intent_sets);
            bps_free(&intent_unsets);
            blocks_free(&locks);
            json_free(base);
            json_free(newv);
            return bk_err("ConfigValueError", "config_secret_literal", msg);
        }
        bp_free(&bad);
    }

    out = bk_ok();
    if (sets.n == 0 && unsets.n == 0) {
        json_set(out, "body", json_null());
    } else {
        json_t *body = json_object();
        json_t *set_body = json_object();
        json_t *unset_body = json_array();
        int stamp = 0;
        for (i = 0; i < sets.n; i++) {
            char *enc = bp_encode(&sets.it[i].path);
            json_set(set_body, enc ? enc : "", json_copy(sets.it[i].value));
            free(enc);
        }
        for (i = 0; i < unsets.n; i++) {
            char *enc = bp_encode(&unsets.it[i]);
            json_append(unset_body, json_string(enc ? enc : ""));
            free(enc);
        }
        json_set(body, "expectedVersion",
                 (pv && bk_is_int(pv)) ? json_number(bk_int_value(pv)) : json_null());
        if (pw && pw->type == JSON_OBJECT) {
            const json_t *kind = json_obj_get(pw, "kind");
            const char *k = (kind && kind->type == JSON_STRING) ? kind->str_val : "";
            if (k && !strcmp(k, "null")) stamp = 1;
            else if (k && !strcmp(k, "int") &&
                     bk_int_value(json_obj_get(pw, "value")) == latest) stamp = 1;
        }
        if (stamp) json_set(body, "writerConfigVersion", json_number(latest));
        if (json_object_size(set_body) > 0) json_set(body, "set", set_body);
        else json_free(set_body);
        if (json_len(unset_body) > 0) json_set(body, "unset", unset_body);
        else json_free(unset_body);
        json_set(out, "body", body);
    }
    json_set(out, "sets", bk_sets_json(&sets));
    json_set(out, "unsets", bk_paths_json(&unsets));

    bs_free(&sets);
    bps_free(&unsets);
    bs_free(&intent_sets);
    bps_free(&intent_unsets);
    blocks_free(&locks);
    json_free(base);
    json_free(newv);
    return out;
}
