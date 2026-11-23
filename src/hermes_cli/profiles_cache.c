/*
 * profiles_cache.c — C home for upstream Hermes hermes_cli/profiles_cache.py.
 * PoP: w12_profilesca_d12947_cached_profile_read @ hermes_cli/profiles_cache.py:_cached_profile_read
 * PoP: w12_profilesca_d12947_yaml_scalar_to_json @ hermes_cli/profiles_cache.py:yaml_scalar_to_json
 * PoP: w12_profilesca_d12947_json_safe_dumps @ hermes_cli/profiles_cache.py:json_safe_dumps
 *
 * Only DERIVED values are cached, exactly as upstream: the cache is keyed by
 * (str(path), kind) and a missing file (NULL signature) is never cached.
 */
#include "hermes_json.h"
#include "noc_w12_profilesca_d12947.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ cache */
typedef struct {
    char   *path;
    char   *kind;
    json_t *sig;
    json_t *val;
} pc_entry;

static pc_entry *g_pc_cache = NULL;
static size_t    g_pc_count = 0;
static size_t    g_pc_cap   = 0;

static void pc_cache_clear(void)
{
    for (size_t i = 0; i < g_pc_count; i++) {
        free(g_pc_cache[i].path);
        free(g_pc_cache[i].kind);
        json_free(g_pc_cache[i].sig);
        json_free(g_pc_cache[i].val);
    }
    g_pc_count = 0;
}

void w12_profilesca_d12947_cache_free(void)
{
    pc_cache_clear();
    free(g_pc_cache);
    g_pc_cache = NULL;
    g_pc_cap = 0;
}

static char *pc_strdup(const char *s)
{
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void pc_cache_add(const char *path, const char *kind,
                         const json_t *sig, const json_t *val)
{
    pc_entry *e;
    if (g_pc_count == g_pc_cap) {
        size_t nc = g_pc_cap ? g_pc_cap * 2 : 8;
        pc_entry *np = (pc_entry *)realloc(g_pc_cache, nc * sizeof(*np));
        if (!np) return;
        g_pc_cache = np;
        g_pc_cap = nc;
    }
    e = &g_pc_cache[g_pc_count];
    e->path = pc_strdup(path);
    e->kind = pc_strdup(kind);
    e->sig  = sig ? json_copy(sig) : json_null();
    e->val  = val ? json_copy(val) : json_null();
    g_pc_count++;
}

/* Signature equality: a signature is a small scalar/array token; serializing
 * both sides is a faithful, allocation-safe structural comparison. */
static int pc_sig_eq(const json_t *a, const json_t *b)
{
    char *sa, *sb;
    int eq;
    if (a == b) return 1;
    if (!a || !b) return 0;
    sa = json_dumps(a, 0);
    sb = json_dumps(b, 0);
    eq = (sa && sb && !strcmp(sa, sb));
    free(sa);
    free(sb);
    return eq;
}

struct json_t *w12_profilesca_d12947_cached_profile_read(
    const char *path, const char *kind,
    const json_t *signature, const json_t *compute_value,
    long long cache_max, int reset)
{
    const char *p = path ? path : "";
    const char *k = kind ? kind : "";
    int present = (signature && signature->type != JSON_NULL) ? 1 : 0;
    int computed = 1, cleared = 0;
    json_t *value;
    json_t *out;

    if (reset) pc_cache_clear();
    if (cache_max < 0) cache_max = 0;

    if (!present) {
        /* Missing file: compute() runs, nothing is stored (upstream contract). */
        value = compute_value ? json_copy(compute_value) : json_null();
    } else {
        int idx = -1;
        for (size_t i = 0; i < g_pc_count; i++) {
            if (!strcmp(g_pc_cache[i].path, p) && !strcmp(g_pc_cache[i].kind, k)) {
                idx = (int)i;
                break;
            }
        }
        if (idx >= 0 && pc_sig_eq(g_pc_cache[idx].sig, signature)) {
            value = json_copy(g_pc_cache[idx].val);
            computed = 0;
        } else {
            if ((long long)g_pc_count >= cache_max) {
                pc_cache_clear();
                cleared = 1;
            }
            pc_cache_add(p, k, signature, compute_value);
            value = compute_value ? json_copy(compute_value) : json_null();
        }
    }

    out = json_object();
    json_set(out, "value", value);
    json_set(out, "computed", json_bool(computed ? true : false));
    json_set(out, "cleared", json_bool(cleared ? true : false));
    return out;
}

/* -------------------------------------------------------- str() coercion */
void w12_profilesca_d12947_yaml_scalar_to_json(const struct json_t *value,
                                               char *out, size_t cap)
{
    if (!out || !cap) return;
    out[0] = '\0';
    if (!value || value->type == JSON_NULL) {
        snprintf(out, cap, "%s", "None");
        return;
    }
    switch (value->type) {
    case JSON_STRING:
        snprintf(out, cap, "%s", value->str_val ? value->str_val : "");
        break;
    case JSON_BOOL:
        snprintf(out, cap, "%s", value->bool_val ? "True" : "False");
        break;
    case JSON_NUMBER:
        if (value->num_val == (double)(long long)value->num_val)
            snprintf(out, cap, "%.0f", value->num_val);
        else
            snprintf(out, cap, "%.17g", value->num_val);
        break;
    default:
        {
            char *s = json_dumps(value, 0);
            snprintf(out, cap, "%s", s ? s : "");
            free(s);
        }
        break;
    }
}

/* ------------------------------------------------------------- py dumps */
typedef struct {
    char  *buf;
    size_t len, cap;
} pc_sb;

static void pc_sb_putc(pc_sb *s, char c)
{
    if (s->len + 1 >= s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 128;
        char *nb = (char *)realloc(s->buf, nc);
        if (!nb) return;
        s->buf = nb;
        s->cap = nc;
    }
    s->buf[s->len++] = c;
    s->buf[s->len] = '\0';
}

static void pc_sb_puts(pc_sb *s, const char *t)
{
    if (!t) return;
    while (*t) pc_sb_putc(s, *t++);
}

/* Python json.dumps escaping with ensure_ascii=True. */
static void pc_py_quote(pc_sb *s, const char *v)
{
    const unsigned char *p = (const unsigned char *)(v ? v : "");
    pc_sb_putc(s, '"');
    for (; *p; p++) {
        unsigned char c = *p;
        switch (c) {
        case '"':  pc_sb_puts(s, "\\\""); break;
        case '\\': pc_sb_puts(s, "\\\\"); break;
        case '\b': pc_sb_puts(s, "\\b");  break;
        case '\f': pc_sb_puts(s, "\\f");  break;
        case '\n': pc_sb_puts(s, "\\n");  break;
        case '\r': pc_sb_puts(s, "\\r");  break;
        case '\t': pc_sb_puts(s, "\\t");  break;
        default:
            if (c < 0x20) {
                char esc[8];
                snprintf(esc, sizeof(esc), "\\u%04x", c);
                pc_sb_puts(s, esc);
            } else {
                pc_sb_putc(s, (char)c); /* ASCII text; UTF-8 emitted verbatim */
            }
            break;
        }
    }
    pc_sb_putc(s, '"');
}

static void pc_py_node(const json_t *n, pc_sb *s, int *bad)
{
    if (!n || *bad) return;
    switch (n->type) {
    case JSON_NULL:  pc_sb_puts(s, "null"); break;
    case JSON_BOOL:  pc_sb_puts(s, n->bool_val ? "true" : "false"); break;
    case JSON_NUMBER:
        if (!isfinite(n->num_val)) { *bad = 1; break; }
        {
            char tmp[64];
            if (n->num_val == (double)(long long)n->num_val)
                snprintf(tmp, sizeof(tmp), "%.0f", n->num_val);
            else
                snprintf(tmp, sizeof(tmp), "%.17g", n->num_val);
            pc_sb_puts(s, tmp);
        }
        break;
    case JSON_STRING:
        if (n->str_val && !strcmp(n->str_val, "__W12_NAN__")) { *bad = 1; break; }
        pc_py_quote(s, n->str_val);
        break;
    case JSON_ARRAY:
        pc_sb_putc(s, '[');
        for (size_t i = 0; i < n->c.count; i++) {
            if (i) pc_sb_puts(s, ", ");
            pc_py_node(n->c.items[i], s, bad);
        }
        pc_sb_putc(s, ']');
        break;
    case JSON_OBJECT:
        pc_sb_putc(s, '{');
        for (size_t i = 0; i < n->c.count; i++) {
            if (i) pc_sb_puts(s, ", ");
            pc_py_quote(s, n->c.keys[i]);
            pc_sb_puts(s, ": ");
            pc_py_node(n->c.items[i], s, bad);
        }
        pc_sb_putc(s, '}');
        break;
    }
}

char *w12_profilesca_d12947_json_safe_dumps(const struct json_t *value)
{
    pc_sb s;
    int bad = 0;
    memset(&s, 0, sizeof(s));
    pc_py_node(value, &s, &bad);
    if (bad || !s.buf) {
        free(s.buf);
        return pc_strdup("{}");
    }
    return s.buf;
}
