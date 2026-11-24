/*
 * session_hosted_members.c — real C11 port of upstream Hermes
 *   gateway/session_hosted_members.py (which local profiles a hosted group on
 *   one authority may seat).
 *
 * PoP: w18_sessionhos_a3f189_member_roster @ gateway/session_hosted_members.py:member_roster
 * PoP: w18_sessionhos_a3f189_served        @ gateway/session_hosted_members.py:_served
 * PoP: w18_sessionhos_a3f189_configured    @ gateway/session_hosted_members.py:_configured
 * PoP: w18_sessionhos_a3f189_config_entry  @ gateway/session_hosted_members.py:_config_entry
 * PoP: w18_sessionhos_a3f189_report        @ gateway/session_hosted_members.py:_report
 * PoP: w18_sessionhos_a3f189_refuse_unseatable @ gateway/session_hosted_members.py:refuse_unseatable
 *
 * Faithfulness notes (deliberate, documented seams):
 *  - local_bots() / _load_gateway_config() / owner_scope() are live-I/O
 *    wrappers: the caller hands in their results (the bots snapshot and the
 *    loaded gateway config), so every rule below them is ported exactly.
 *  - hermes_home_key / profile_name_for_home are mirrored for absolute home
 *    paths: lexical resolve ('..' collapsed, '.' dropped) plus normcase.
 *    Symlinks, 8.3 short names and the marker-file default root are not
 *    probed (they need the filesystem); the shape rule profiles/<name> -> name
 *    and everything else -> 'default' is what served_profile_name answers.
 *  - dict order is Python insertion order: smap/rmap keep it, so the emitted
 *    homes/refused objects carry the same key order as the live dicts.
 */
#include "noc_w18_sessionhos_a3f189.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ basics */

static char *dupstr(const char *s)
{
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *dupstr_n(const char *s, size_t n)
{
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *fmtstr(const char *fmt, ...)
{
    va_list ap;
    int need;
    char *out;

    va_start(ap, fmt);
    need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) return dupstr("");
    out = (char *)malloc((size_t)need + 1);
    if (!out) return NULL;
    va_start(ap, fmt);
    vsnprintf(out, (size_t)need + 1, fmt, ap);
    va_end(ap);
    return out;
}

/* -------------------------------------------------------------------- utf8 */

/* Number of codepoints (Python len() for a str). */
static size_t u8_count(const char *s)
{
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if ((*p & 0xC0) != 0x80) n++;
        p++;
    }
    return n;
}

/* Leading `chars` codepoints, never splitting a multibyte sequence. */
static char *u8_prefix(const char *s, size_t chars)
{
    const char *p = s;
    size_t n = 0;

    while (*p && n < chars) {
        unsigned char c = (unsigned char)*p;
        size_t w = 1;
        size_t i;
        if (c >= 0xF0) w = 4;
        else if (c >= 0xE0) w = 3;
        else if (c >= 0xC0) w = 2;
        for (i = 1; i < w && p[i]; i++) {
            if (((unsigned char)p[i] & 0xC0) != 0x80) { w = i; break; }
        }
        p += w;
        n++;
    }
    return dupstr_n(s, (size_t)(p - s));
}

static int u8_is_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.isprintable() for the byte at the head of a codepoint. */
static int u8_printable(const unsigned char *p)
{
    unsigned char c = *p;
    if (c >= 0x80) return 1;          /* letters/symbols: printable; Cf/Cc not modelled */
    if (c == ' ') return 1;
    if (c < 0x20 || c == 0x7F) return 0;
    return 1;
}

/* persistence.core.errors._one_line: whitespace to single spaces, drop
 * non-printables, bound the length; "" becomes NULL. `limit` is codepoints. */
static char *one_line(const char *value, size_t limit)
{
    char *joined;
    char *out;
    size_t n = 0, cap;
    int pending_space = 0;
    int started = 0;

    if (!value) return NULL;
    cap = strlen(value) + 1;
    joined = (char *)malloc(cap);
    if (!joined) return NULL;

    {
        const unsigned char *p = (const unsigned char *)value;
        while (*p) {
            unsigned char c = *p;
            size_t w = 1;
            if (u8_is_space(c)) {
                if (started) pending_space = 1;
                p++;
                continue;
            }
            if (pending_space) { joined[n++] = ' '; pending_space = 0; }
            started = 1;
            if (c >= 0xF0) w = 4;
            else if (c >= 0xE0) w = 3;
            else if (c >= 0xC0) w = 2;
            if (w == 1) {
                if (u8_printable(p)) joined[n++] = (char)c;
                p++;
            } else {
                size_t i;
                joined[n++] = (char)c;
                for (i = 1; i < w && p[i]; i++) joined[n++] = (char)p[i];
                p += w;
            }
        }
    }
    joined[n] = '\0';

    if (u8_count(joined) > limit) {
        out = u8_prefix(joined, limit);
        free(joined);
    } else {
        out = joined;
    }
    if (!out || !*out) { free(out); return NULL; }
    return out;
}

/* -------------------------------------------------------------------- path */

static int is_sep(char c) { return c == '/' || c == '\\'; }

static int path_is_absolute(const char *p)
{
    if (!p || !*p) return 0;
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':')
        return p[2] != '\0' && is_sep(p[2]);
    if (is_sep(p[0]) && is_sep(p[1]) && p[2] && !is_sep(p[2])) return 1;   /* UNC */
    return 0;
}

/* str(PurePath(value)) on this host: one separator, '.' parts dropped. */
static char *path_str(const char *value)
{
    const char *p = value ? value : "";
    char *out;
    size_t n = 0;

    out = (char *)malloc(strlen(p) + 4);
    if (!out) return NULL;

    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':') {
        out[n++] = p[0];
        out[n++] = ':';
        p += 2;
        if (is_sep(*p)) {
            out[n++] = '\\';
            while (is_sep(*p)) p++;
        }
    } else if (is_sep(p[0]) && is_sep(p[1])) {
        /* UNC: the drive is \\server\share and the root is the separator after it. */
        const char *srv, *shr;
        size_t srvlen = 0, shrlen = 0;
        int sep_after_srv = 0;
        while (is_sep(*p)) p++;
        srv = p;
        while (*p && !is_sep(*p)) p++;
        srvlen = (size_t)(p - srv);
        shr = srv;
        if (*p && srvlen) {
            sep_after_srv = 1;
            while (is_sep(*p)) p++;
            shr = p;
            while (*p && !is_sep(*p)) p++;
            shrlen = (size_t)(p - shr);
        }
        out[n++] = '\\';
        out[n++] = '\\';
        memcpy(out + n, srv, srvlen);
        n += srvlen;
        if (shrlen) {
            out[n++] = '\\';
            memcpy(out + n, shr, shrlen);
            n += shrlen;
            out[n++] = '\\';            /* root: an anchor always ends in a separator */
            while (is_sep(*p)) p++;
        } else if (sep_after_srv) {
            /* \\server\ with no share: the drive keeps the separator it was given. */
            out[n++] = '\\';
            while (is_sep(*p)) p++;
        }
    } else if (is_sep(*p)) {
        out[n++] = '\\';
        while (is_sep(*p)) p++;
    }

    while (*p) {
        const char *start;
        size_t len;
        while (is_sep(*p)) p++;
        if (!*p) break;
        start = p;
        while (*p && !is_sep(*p)) p++;
        len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;
        if (n > 0 && out[n - 1] != '\\') out[n++] = '\\';
        memcpy(out + n, start, len);
        n += len;
    }
    if (n == 0) out[n++] = '.';
    out[n] = '\0';
    return out;
}

/* resolve(strict=False) + normcase for an absolute home path. */
static char *home_key(const char *value)
{
    char *norm = path_str(value);
    char *out;
    size_t n = 0;
    const char *p;
    char prefix[4];
    size_t plen = 0;
    size_t offs[64];
    size_t depth = 0;

    if (!norm) return NULL;
    p = norm;
    if (p[0] && p[1] == ':' && p[2] == '\\') {
        prefix[0] = p[0]; prefix[1] = ':'; prefix[2] = '\\'; plen = 3; p += 3;
    } else if (p[0] == '\\' && p[1] == '\\') {
        prefix[0] = '\\'; prefix[1] = '\\'; plen = 2; p += 2;
    } else if (p[0] == '\\') {
        prefix[0] = '\\'; plen = 1; p += 1;
    }

    out = (char *)malloc(strlen(norm) + 2);
    if (!out) { free(norm); return NULL; }
    memcpy(out, prefix, plen);
    n = plen;

    while (*p) {
        const char *start;
        size_t len;
        while (*p == '\\') p++;
        if (!*p) break;
        start = p;
        while (*p && *p != '\\') p++;
        len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (depth > 0) n = offs[--depth];
            continue;
        }
        if (depth < 64) offs[depth++] = n;
        if (n > plen && out[n - 1] != '\\') out[n++] = '\\';
        memcpy(out + n, start, len);
        n += len;
    }
    if (n == 0) out[n++] = '\\';
    out[n] = '\0';
    free(norm);
    {
        size_t i;
        for (i = 0; out[i]; i++) out[i] = (char)tolower((unsigned char)out[i]);
    }
    return out;
}

/* `Path(value) == Path(value).resolve()` for absolute, canonical homes. */
static int path_is_resolved(const char *value)
{
    const char *p = value ? value : "";
    while (*p) {
        const char *start;
        size_t len;
        while (is_sep(*p)) p++;
        if (!*p) break;
        start = p;
        while (*p && !is_sep(*p)) p++;
        len = (size_t)(p - start);
        if (len == 2 && start[0] == '.' && start[1] == '.') return 0;
    }
    return 1;
}

/* ---------------------------------------------------------------- identity */

/* IDENTIFIER_RE = ^[A-Za-z0-9][A-Za-z0-9._:-]*$ .fullmatch() */
static int ident_ok(const char *s)
{
    const char *p;
    unsigned char c;
    if (!s || !*s) return 0;
    c = (unsigned char)s[0];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return 0;
    for (p = s; *p; p++) {
        c = (unsigned char)*p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) continue;
        if (c == '.' || c == '_' || c == ':' || c == '-') continue;
        return 0;
    }
    return 1;
}

/* gateway.session_authorities.served_profile_name(home): the directory name
 * under profiles/, else 'default'. */
char *w18_sessionhos_a3f189_served_profile_name(const char *home)
{
    char *norm;
    size_t len, s, pe, ps, leaf_len;
    char *leaf = NULL;
    char *parent = NULL;
    char *out;

    if (!home || !*home) return dupstr("default");
    norm = path_str(home);
    if (!norm) return dupstr("default");
    len = strlen(norm);
    while (len > 0 && norm[len - 1] == '\\') len--;
    if (len == 0 || norm[len - 1] == ':') {          /* the drive root itself */
        free(norm);
        return dupstr("default");
    }
    s = len;
    while (s > 0 && norm[s - 1] != '\\') s--;
    leaf_len = len - s;
    leaf = dupstr_n(norm + s, leaf_len);
    if (s > 0) {
        pe = s - 1;
        ps = pe;
        while (ps > 0 && norm[ps - 1] != '\\') ps--;
        parent = dupstr_n(norm + ps, pe - ps);
    }
    out = dupstr("default");
    if (leaf && leaf[0] && leaf[0] != '.' && parent && !strcmp(parent, "profiles")) {
        free(out);
        out = dupstr(leaf);
    }
    free(norm);
    free(leaf);
    free(parent);
    return out;
}

/* ------------------------------------------------------------- string maps */

typedef struct {
    char **keys;
    char **vals;
    size_t count;
    size_t cap;
} smap;

static int smap_index(const smap *m, const char *key)
{
    size_t i;
    if (!m || !key) return -1;
    for (i = 0; i < m->count; i++)
        if (m->keys[i] && !strcmp(m->keys[i], key)) return (int)i;
    return -1;
}

static void smap_set(smap *m, const char *key, const char *val)
{
    int i = smap_index(m, key);
    if (i >= 0) {
        char *copy = dupstr(val);
        free(m->vals[i]);
        m->vals[i] = copy;
        return;
    }
    if (m->count == m->cap) {
        size_t cap = m->cap ? m->cap * 2 : 8;
        char **k = (char **)realloc(m->keys, cap * sizeof(*k));
        char **v;
        if (!k) return;
        m->keys = k;
        v = (char **)realloc(m->vals, cap * sizeof(*v));
        if (!v) return;
        m->vals = v;
        m->cap = cap;
    }
    m->keys[m->count] = dupstr(key);
    m->vals[m->count] = dupstr(val);
    m->count++;
}

static void smap_setdefault(smap *m, const char *key, const char *val)
{
    if (smap_index(m, key) < 0) smap_set(m, key, val);
}

static const char *smap_get(const smap *m, const char *key)
{
    int i = smap_index(m, key);
    return i >= 0 ? m->vals[i] : NULL;
}

static int smap_has(const smap *m, const char *key) { return smap_index(m, key) >= 0; }

static int smap_del(smap *m, const char *key)
{
    int i = smap_index(m, key);
    if (i < 0) return 0;
    free(m->keys[i]);
    free(m->vals[i]);
    memmove(m->keys + i, m->keys + i + 1, (m->count - (size_t)i - 1) * sizeof(*m->keys));
    memmove(m->vals + i, m->vals + i + 1, (m->count - (size_t)i - 1) * sizeof(*m->vals));
    m->count--;
    return 1;
}

static void smap_copy(smap *dst, const smap *src)
{
    size_t i;
    memset(dst, 0, sizeof(*dst));
    if (!src) return;
    for (i = 0; i < src->count; i++) smap_set(dst, src->keys[i], src->vals[i]);
}

static void smap_free(smap *m)
{
    size_t i;
    if (!m) return;
    for (i = 0; i < m->count; i++) { free(m->keys[i]); free(m->vals[i]); }
    free(m->keys);
    free(m->vals);
    m->keys = NULL;
    m->vals = NULL;
    m->count = m->cap = 0;
}

static json_t *smap_to_json(const smap *m)
{
    json_t *obj = json_object();
    size_t i;
    if (!obj) return NULL;
    for (i = 0; m && i < m->count; i++)
        json_set(obj, m->keys[i], json_string(m->vals[i] ? m->vals[i] : ""));
    return obj;
}

/* ------------------------------------------------------------ refusal maps */

typedef struct {
    char *name;
    char *reason;
    char *field;
    char *detail;   /* may be NULL */
} rentry;

typedef struct {
    rentry *e;
    size_t count;
    size_t cap;
} rmap;

static int rmap_index(const rmap *m, const char *name)
{
    size_t i;
    if (!m || !name) return -1;
    for (i = 0; i < m->count; i++)
        if (m->e[i].name && !strcmp(m->e[i].name, name)) return (int)i;
    return -1;
}

static void rmap_set(rmap *m, const char *name, const char *reason,
                     const char *field, const char *detail)
{
    int i = rmap_index(m, name);
    if (i >= 0) {
        free(m->e[i].reason); free(m->e[i].field); free(m->e[i].detail);
        m->e[i].reason = dupstr(reason);
        m->e[i].field = dupstr(field);
        m->e[i].detail = detail ? dupstr(detail) : NULL;
        return;
    }
    if (m->count == m->cap) {
        size_t cap = m->cap ? m->cap * 2 : 8;
        rentry *e = (rentry *)realloc(m->e, cap * sizeof(*e));
        if (!e) return;
        m->e = e;
        m->cap = cap;
    }
    memset(&m->e[m->count], 0, sizeof(rentry));
    m->e[m->count].name = dupstr(name);
    m->e[m->count].reason = dupstr(reason);
    m->e[m->count].field = dupstr(field);
    m->e[m->count].detail = detail ? dupstr(detail) : NULL;
    m->count++;
}

static int rmap_has(const rmap *m, const char *name) { return rmap_index(m, name) >= 0; }

static int rmap_del(rmap *m, const char *name)
{
    int i = rmap_index(m, name);
    if (i < 0) return 0;
    free(m->e[i].name); free(m->e[i].reason); free(m->e[i].field); free(m->e[i].detail);
    memmove(m->e + i, m->e + i + 1, (m->count - (size_t)i - 1) * sizeof(*m->e));
    m->count--;
    return 1;
}

static void rmap_free(rmap *m)
{
    size_t i;
    if (!m) return;
    for (i = 0; i < m->count; i++) {
        free(m->e[i].name); free(m->e[i].reason); free(m->e[i].field); free(m->e[i].detail);
    }
    free(m->e);
    m->e = NULL;
    m->count = m->cap = 0;
}

static json_t *rmap_to_json(const rmap *m)
{
    json_t *obj = json_object();
    size_t i;
    if (!obj) return NULL;
    for (i = 0; m && i < m->count; i++) {
        json_t *triple = json_array();
        json_append(triple, json_string(m->e[i].reason ? m->e[i].reason : ""));
        json_append(triple, json_string(m->e[i].field ? m->e[i].field : ""));
        json_append(triple, m->e[i].detail ? json_string(m->e[i].detail) : json_null());
        json_set(obj, m->e[i].name, triple);
    }
    return obj;
}

/* ---------------------------------------------------------------- logging */

typedef struct {
    char *field;
    char *detail;
} rlog_ent;

typedef struct {
    rlog_ent *e;
    size_t count;
    size_t cap;
} rlog;

static void rlog_add(rlog *log, const char *field, const char *detail)
{
    if (!log) return;
    if (log->count == log->cap) {
        size_t cap = log->cap ? log->cap * 2 : 4;
        rlog_ent *e = (rlog_ent *)realloc(log->e, cap * sizeof(*e));
        if (!e) return;
        log->e = e;
        log->cap = cap;
    }
    log->e[log->count].field = dupstr(field);
    log->e[log->count].detail = dupstr(detail);
    log->count++;
}

static void rlog_free(rlog *log)
{
    size_t i;
    if (!log) return;
    for (i = 0; i < log->count; i++) { free(log->e[i].field); free(log->e[i].detail); }
    free(log->e);
    log->e = NULL;
    log->count = log->cap = 0;
}

static json_t *rlog_to_json(const rlog *log)
{
    json_t *arr = json_array();
    size_t i;
    if (!arr) return NULL;
    for (i = 0; log && i < log->count; i++) {
        json_t *item = json_array();
        json_append(item, json_string(log->e[i].field ? log->e[i].field : ""));
        json_append(item, json_string(log->e[i].detail ? log->e[i].detail : ""));
        json_append(arr, item);
    }
    return arr;
}

/* repr(value)[:200] — enough of CPython's repr to key the once-only log. */
static char *value_repr(const json_t *value)
{
    char *out;
    if (!value || value->type == JSON_NULL) return dupstr("None");
    switch (value->type) {
    case JSON_BOOL:
        return dupstr(value->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        double d = value->num_val;
        if (d == (double)(long long)d) return fmtstr("%lld", (long long)d);
        return fmtstr("%g", d);
    }
    case JSON_STRING: {
        const char *s = value->str_val ? value->str_val : "";
        int has_single = strchr(s, '\'') != NULL;
        int has_double = strchr(s, '"') != NULL;
        char quote = (has_single && !has_double) ? '"' : '\'';
        size_t n = 0;
        char *buf = (char *)malloc(strlen(s) * 4 + 3);
        if (!buf) return dupstr("");
        buf[n++] = quote;
        for (; *s; s++) {
            unsigned char c = (unsigned char)*s;
            if (c == '\\' || c == (unsigned char)quote) { buf[n++] = '\\'; buf[n++] = (char)c; }
            else if (c == '\n') { buf[n++] = '\\'; buf[n++] = 'n'; }
            else if (c == '\r') { buf[n++] = '\\'; buf[n++] = 'r'; }
            else if (c == '\t') { buf[n++] = '\\'; buf[n++] = 't'; }
            else if (c < 0x20 || c == 0x7F) { n += (size_t)sprintf(buf + n, "\\x%02x", c); }
            else buf[n++] = (char)c;
        }
        buf[n++] = quote;
        buf[n] = '\0';
        if (u8_count(buf) > 200) { out = u8_prefix(buf, 200); free(buf); }
        else out = buf;
        return out;
    }
    default: {
        char *s = json_dumps(value, 0);
        out = s ? dupstr(s) : dupstr("");
        free(s);
        return out;
    }
    }
}

/* _REPORTED: (own, field, repr(value)[:200], detail) already logged. */
static char **g_reported;
static size_t g_reported_count;
static size_t g_reported_cap;

void w18_sessionhos_a3f189_report_reset(void)
{
    size_t i;
    for (i = 0; i < g_reported_count; i++) free(g_reported[i]);
    free(g_reported);
    g_reported = NULL;
    g_reported_count = g_reported_cap = 0;
}

static int report_impl(rlog *log, const char *own, const char *field,
                       const json_t *value, const char *detail)
{
    char *repr = value_repr(value);
    char *key = fmtstr("%s\x1f%s\x1f%s\x1f%s", own ? own : "", field ? field : "",
                       repr ? repr : "", detail ? detail : "");
    size_t i;
    int seen = 0;

    free(repr);
    if (!key) return 0;
    for (i = 0; i < g_reported_count; i++) {
        if (!strcmp(g_reported[i], key)) { seen = 1; break; }
    }
    if (seen) { free(key); return 0; }
    if (g_reported_count == g_reported_cap) {
        size_t cap = g_reported_cap ? g_reported_cap * 2 : 16;
        char **grown = (char **)realloc(g_reported, cap * sizeof(*grown));
        if (!grown) { free(key); return 0; }
        g_reported = grown;
        g_reported_cap = cap;
    }
    g_reported[g_reported_count++] = key;
    rlog_add(log, field, detail);
    return 1;
}

int w18_sessionhos_a3f189_report(const char *own, const char *field,
                                 const json_t *value, const char *detail)
{
    return report_impl(NULL, own, field, value, detail);
}

/* ------------------------------------------------------------ config entry */

static json_t *jget(const json_t *obj, const char *key)
{
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}

static int jis_str(const json_t *v) { return v && v->type == JSON_STRING; }

/* _config_entry -> home / reason / detail (all owned copies). */
static void entry_impl(const char *name, const json_t *value,
                       const smap *served, const smap *seated,
                       char **out_home, char **out_reason, char **out_detail)
{
    const char *target;
    char *tstr;
    char *key;
    const char *served_home;
    const char *seat;

    *out_home = NULL;
    *out_reason = NULL;
    *out_detail = NULL;

    if (!name || !ident_ok(name) || !jis_str(value)) {
        *out_reason = dupstr("invalid_params");
        *out_detail = dupstr("must map a profile name to a home");
        return;
    }
    target = value->str_val ? value->str_val : "";
    if (!path_is_absolute(target) || !path_is_resolved(target)) {
        *out_reason = dupstr("invalid_params");
        *out_detail = dupstr("must be an absolute, resolved home");
        return;
    }
    tstr = path_str(target);
    key = home_key(target);

    served_home = smap_get(served, name);
    if (served_home) {
        char *sk = home_key(served_home);
        int differs = !sk || !key || strcmp(sk, key) != 0;
        free(sk);
        if (differs) {
            *out_reason = dupstr("permission_denied");
            *out_detail = dupstr("this gateway serves that profile from another home");
            free(tstr);
            free(key);
            return;
        }
    }
    seat = smap_get(seated, key ? key : "");
    if (seat && strcmp(seat, name) != 0) {
        *out_home = tstr;
        *out_reason = dupstr("invalid_params");
        *out_detail = fmtstr("names the home already seated as '%s'", seat);
        free(key);
        return;
    }
    *out_home = served_home ? path_str(served_home) : tstr;
    free(key);
}

json_t *w18_sessionhos_a3f189_config_entry(const json_t *name, const json_t *value,
                                           const json_t *served, const json_t *seated)
{
    smap s, d;
    char *home = NULL, *reason = NULL, *detail = NULL;
    json_t *out;

    memset(&s, 0, sizeof(s));
    memset(&d, 0, sizeof(d));
    if (served && served->type == JSON_OBJECT) {
        size_t i;
        for (i = 0; i < json_object_size(served); i++) {
            const char *k = json_object_get_key_at(served, i);
            json_t *v = json_object_get_at(served, i);
            if (k && jis_str(v)) smap_set(&s, k, v->str_val);
        }
    }
    if (seated && seated->type == JSON_OBJECT) {
        size_t i;
        for (i = 0; i < json_object_size(seated); i++) {
            const char *k = json_object_get_key_at(seated, i);
            json_t *v = json_object_get_at(seated, i);
            if (k && jis_str(v)) smap_set(&d, k, v->str_val);
        }
    }
    entry_impl(jis_str(name) ? name->str_val : NULL, value, &s, &d,
               &home, &reason, &detail);
    out = json_object();
    json_set(out, "home", home ? json_string(home) : json_null());
    json_set(out, "reason", reason ? json_string(reason) : json_null());
    json_set(out, "detail", detail ? json_string(detail) : json_null());
    free(home);
    free(reason);
    free(detail);
    smap_free(&s);
    smap_free(&d);
    return out;
}

/* --------------------------------------------------------------- configured */

/* The hosted_rooms.profiles mapping, borrowed from *config (NULL -> {}). */
static const json_t *configured_node(const char *own, const json_t *config, rlog *log)
{
    json_t *section = jget(config, "hosted_rooms");
    const json_t *profiles;

    if (section && section->type == JSON_OBJECT) profiles = jget(section, "profiles");
    else profiles = section;

    if (!profiles || profiles->type == JSON_NULL) return NULL;
    if (profiles->type == JSON_OBJECT) return profiles;
    report_impl(log, own, "hosted_rooms.profiles", profiles,
                "must map profile names to profile homes");
    return NULL;
}

json_t *w18_sessionhos_a3f189_configured(const char *own, const json_t *config,
                                         json_t **out_reports)
{
    rlog log;
    const json_t *profiles;
    memset(&log, 0, sizeof(log));
    profiles = configured_node(own, config, &log);
    if (out_reports) *out_reports = rlog_to_json(&log);
    rlog_free(&log);
    return profiles ? json_copy(profiles) : json_object();
}

/* ------------------------------------------------------------------- served */

static void bots_load(const json_t *bots, smap *served, rmap *refused,
                      const char **first_self)
{
    size_t i, n = 0;

    *first_self = NULL;
    if (!bots || bots->type != JSON_ARRAY) return;
    n = json_len(bots);

    for (i = 0; i < n; i++) {
        json_t *bot = json_get(bots, i);
        json_t *self = jget(bot, "is_self");
        if (self && self->type == JSON_BOOL && self->bool_val) {
            json_t *nm = jget(bot, "name");
            if (jis_str(nm)) *first_self = nm->str_val;
            break;
        }
    }
    for (i = 0; i < n; i++) {
        json_t *bot = json_get(bots, i);
        const char *name, *home, *problem;
        json_t *pn = jget(bot, "problem");
        json_t *rt = jget(bot, "retiring");
        json_t *self = jget(bot, "is_self");
        int is_self = self && self->type == JSON_BOOL && self->bool_val;
        int retiring = rt && rt->type == JSON_BOOL && rt->bool_val;

        if (!bot || bot->type != JSON_OBJECT) continue;
        name = jis_str(jget(bot, "name")) ? jget(bot, "name")->str_val : "";
        home = jis_str(jget(bot, "home")) ? jget(bot, "home")->str_val : "";
        problem = (pn && pn->type != JSON_NULL) ? (jis_str(pn) ? pn->str_val : "") : NULL;

        if (problem) {
            /* bot.problem.replace(' yet', ' in groups yet') — every occurrence. */
            const char *from = " yet", *to = " in groups yet";
            size_t flen = strlen(from), tlen = strlen(to), hits = 0, i;
            const char *scan = problem;
            char *detail;
            size_t cap;
            while ((scan = strstr(scan, from)) != NULL) { hits++; scan += flen; }
            cap = strlen(problem) + hits * (tlen - flen) + 1;
            detail = (char *)malloc(cap);
            if (detail) {
                size_t n = 0;
                const char *p = problem;
                const char *at;
                while ((at = strstr(p, from)) != NULL) {
                    size_t chunk = (size_t)(at - p);
                    memcpy(detail + n, p, chunk);
                    n += chunk;
                    memcpy(detail + n, to, tlen);
                    n += tlen;
                    p = at + flen;
                }
                for (i = 0; p[i]; i++) detail[n + i] = p[i];
                detail[n + i] = '\0';
            }
            rmap_set(refused, name, "invalid_params", "members", detail ? detail : problem);
            free(detail);
        } else if (retiring && !is_self) {
            continue;
        } else {
            /* A bot's home is Path(authority.profile_id): str() folds separators. */
            char *h = path_str(home);
            smap_set(served, name, h ? h : "");
            free(h);
        }
    }
}

int w18_sessionhos_a3f189_served(const char *profile_id, const json_t *bots,
                                 char **out_own, json_t **out_homes,
                                 json_t **out_refused)
{
    smap served;
    rmap refused;
    const char *first_self = NULL;
    char *own;

    memset(&served, 0, sizeof(served));
    memset(&refused, 0, sizeof(refused));
    bots_load(bots, &served, &refused, &first_self);
    own = first_self ? dupstr(first_self) : w18_sessionhos_a3f189_served_profile_name(profile_id);
    if (!own) own = dupstr("default");
    if (!rmap_has(&refused, own)) {
        char *home = path_str(profile_id ? profile_id : "");
        smap_setdefault(&served, own, home ? home : "");
        free(home);
    }
    if (out_own) *out_own = dupstr(own);
    if (out_homes) *out_homes = smap_to_json(&served);
    if (out_refused) *out_refused = rmap_to_json(&refused);
    free(own);
    smap_free(&served);
    rmap_free(&refused);
    return 0;
}

/* ------------------------------------------------------------ member_roster */

int w18_sessionhos_a3f189_member_roster(const char *profile_id, const json_t *bots,
                                        int siblings, const json_t *config,
                                        json_t **out_homes, json_t **out_refused,
                                        json_t **out_reports)
{
    smap served, homes, seated, blocked;
    rmap refused;
    rlog log;
    const char *first_self = NULL;
    char *own;
    const json_t *profiles;
    size_t i;

    memset(&served, 0, sizeof(served));
    memset(&homes, 0, sizeof(homes));
    memset(&seated, 0, sizeof(seated));
    memset(&blocked, 0, sizeof(blocked));
    memset(&refused, 0, sizeof(refused));
    memset(&log, 0, sizeof(log));

    bots_load(bots, &served, &refused, &first_self);
    own = first_self ? dupstr(first_self) : w18_sessionhos_a3f189_served_profile_name(profile_id);
    if (!own) own = dupstr("default");
    if (!rmap_has(&refused, own)) {
        char *home = path_str(profile_id ? profile_id : "");
        smap_setdefault(&served, own, home ? home : "");
        free(home);
    }

    /* blocked = set(refused) — a snapshot: the loop below mutates refused. */
    for (i = 0; i < refused.count; i++) smap_set(&blocked, refused.e[i].name, "1");

    smap_copy(&homes, &served);

    if (!siblings) {
        smap snapshot;
        size_t j;
        memset(&snapshot, 0, sizeof(snapshot));
        smap_copy(&snapshot, &homes);
        for (j = 0; j < snapshot.count; j++) {
            const char *name = snapshot.keys[j];
            if (!strcmp(name, own)) continue;
            smap_del(&homes, name);
            rmap_set(&refused, name, "permission_denied", "members",
                     "only a host-scoped connection seats a served profile without an entry");
        }
        smap_free(&snapshot);
    }

    /* seated = {hermes_home_key(home): name for name, home in served.items()} */
    for (i = 0; i < served.count; i++) {
        char *key = home_key(served.vals[i]);
        if (key) { smap_set(&seated, key, served.keys[i]); free(key); }
    }

    profiles = configured_node(own, config, &log);
    for (i = 0; profiles && i < json_object_size(profiles); i++) {
        const char *name = json_object_get_key_at(profiles, i);
        json_t *value = json_object_get_at(profiles, i);
        char *field;
        char *home = NULL, *reason = NULL, *detail = NULL;

        if (!name) continue;
        if (smap_has(&blocked, name)) continue;
        field = fmtstr("hosted_rooms.profiles.%s", name);
        entry_impl(name, value, &served, &seated, &home, &reason, &detail);

        if (siblings && smap_has(&served, name)) {
            /* Served: needs no entry, and the served home wins over this one. */
            if (reason) report_impl(&log, own, field, value, detail);
        } else if (!reason) {
            smap_set(&homes, name, home ? home : "");
            {
                char *key = home_key(home ? home : "");
                if (key) { smap_setdefault(&seated, key, name); free(key); }
            }
            rmap_del(&refused, name);
        } else {
            report_impl(&log, own, field, value, detail);
            rmap_set(&refused, name, reason, field, detail);
            if (!home) smap_del(&homes, name);
            else smap_set(&homes, name, home);
        }
        free(field);
        free(home);
        free(reason);
        free(detail);
    }

    if (out_homes) *out_homes = smap_to_json(&homes);
    if (out_refused) *out_refused = rmap_to_json(&refused);
    if (out_reports) *out_reports = rlog_to_json(&log);

    free(own);
    smap_free(&served);
    smap_free(&homes);
    smap_free(&seated);
    smap_free(&blocked);
    rmap_free(&refused);
    rlog_free(&log);
    return 0;
}

/* -------------------------------------------------------- refuse_unseatable */

json_t *w18_sessionhos_a3f189_refuse_unseatable(const json_t *members,
                                                const json_t *refused)
{
    json_t *out = json_object();
    size_t i, n = (members && members->type == JSON_ARRAY) ? json_len(members) : 0;

    json_set(out, "raised", json_bool(0));
    for (i = 0; i < n; i++) {
        json_t *raw = json_get(members, i);
        json_t *target, *profile;
        if (!raw || raw->type != JSON_OBJECT) continue;
        target = jget(raw, "target");
        profile = jget(raw, "profile");
        if (target && target->type == JSON_OBJECT) {
            json_t *kind = jget(target, "kind");
            if (kind) {
                /* target.get('kind', 'local') != 'local' */
                if (!jis_str(kind) || strcmp(kind->str_val, "local") != 0) continue;
            }
        }
        if (jis_str(profile)) {
            json_t *entry = jget(refused, profile->str_val);
            if (entry && entry->type == JSON_ARRAY) {
                json_t *rn = json_get(entry, 0);
                json_t *fi = json_get(entry, 1);
                json_t *de = json_get(entry, 2);
                char *raw_detail = fmtstr("member %zu profile '%s': %s", i,
                                          profile->str_val,
                                          jis_str(de) ? de->str_val : "None");
                char *detail = one_line(raw_detail, 160);
                char *field = one_line(jis_str(fi) ? fi->str_val : NULL, 80);
                json_set(out, "raised", json_bool(1));
                json_set(out, "reason", jis_str(rn) ? json_string(rn->str_val) : json_null());
                json_set(out, "field", field ? json_string(field) : json_null());
                json_set(out, "detail", detail ? json_string(detail) : json_null());
                free(raw_detail);
                free(detail);
                free(field);
                return out;
            }
        }
    }
    return out;
}

/* ------------------------------------------------------------------ helpers */

char *w18_sessionhos_a3f189_path_str(const char *value) { return path_str(value); }
char *w18_sessionhos_a3f189_home_key(const char *value) { return home_key(value); }
int w18_sessionhos_a3f189_identifier_ok(const char *name) { return ident_ok(name); }
