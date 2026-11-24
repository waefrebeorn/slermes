/*
 * paths.c - C11 port of upstream plugins/config_backends/remote/paths.py
 * (gate w24_paths_616c0c).
 *
 * PoP: w24_paths_616c0c_encode @ plugins/config_backends/remote/paths.py:encode
 * PoP: w24_paths_616c0c_decode @ plugins/config_backends/remote/paths.py:decode
 * PoP: w24_paths_616c0c_validate @ plugins/config_backends/remote/paths.py:validate
 * PoP: w24_paths_616c0c_covers @ plugins/config_backends/remote/paths.py:covers
 * PoP: w24_paths_616c0c_covering_lock @ plugins/config_backends/remote/paths.py:covering_lock
 * PoP: w24_paths_616c0c_from_dotted @ plugins/config_backends/remote/paths.py:from_dotted
 * PoP: w24_paths_616c0c_constants @ plugins/config_backends/remote/paths.py:<module constants>
 *
 * What is ported is the whole grammar: the escape rules, the segment-wise
 * coverage test and the limit checks with their exact PathError texts.  A
 * refusal is returned as {"ok": false, "error": ...} so callers see the same
 * string the plane would put in a config_path_invalid payload; no plane, no
 * socket and no config file is touched here.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_paths_616c0c.h"

/* ------------------------------------------------------------------ buffers */

typedef struct {
    char *p;
    size_t len, cap;
} pbuf;

static void pb_init(pbuf *b) {
    b->cap = 64;
    b->len = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void pb_putc(pbuf *b, char c) {
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

static void pb_puts(pbuf *b, const char *s) {
    while (s && *s) pb_putc(b, *s++);
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

/* Python's type names for the shapes a JSON value can carry here. */
static const char *py_typename(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* Python repr() of a str: quote choice, backslash escapes, \xNN controls. */
static char *py_repr(const char *s) {
    int has_single = 0, has_double = 0;
    char quote;
    pbuf b;
    const unsigned char *p = (const unsigned char *)(s ? s : "");

    for (; *p; p++) {
        if (*p == '\'') has_single = 1;
        else if (*p == '"') has_double = 1;
    }
    quote = (has_single && !has_double) ? '"' : '\'';
    pb_init(&b);
    pb_putc(&b, quote);
    for (p = (const unsigned char *)(s ? s : ""); *p; p++) {
        unsigned char c = *p;
        if (c == '\\') { pb_putc(&b, '\\'); pb_putc(&b, '\\'); }
        else if (c == (unsigned char)quote) { pb_putc(&b, '\\'); pb_putc(&b, quote); }
        else if (c == '\n') { pb_putc(&b, '\\'); pb_putc(&b, 'n'); }
        else if (c == '\r') { pb_putc(&b, '\\'); pb_putc(&b, 'r'); }
        else if (c == '\t') { pb_putc(&b, '\\'); pb_putc(&b, 't'); }
        else if (c < 0x20 || c == 0x7f) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "\\x%02x", (unsigned)c);
            pb_puts(&b, tmp);
        } else {
            pb_putc(&b, (char)c);
        }
    }
    pb_putc(&b, quote);
    if (!b.p) return NULL;
    return b.p;
}

/* --------------------------------------------------------------- segments */

typedef struct {
    char *seg[W24_PATHS_616C0C_MAX_SEGMENTS * 8];
    size_t n;
} plist;

static void pl_free(plist *l) {
    size_t i;
    for (i = 0; i < l->n; i++) free(l->seg[i]);
    l->n = 0;
}

static void pl_push(plist *l, const char *s) {
    if (l->n >= sizeof(l->seg) / sizeof(l->seg[0])) return;
    l->seg[l->n] = strdup(s ? s : "");
    if (l->seg[l->n]) l->n++;
}

/* ".".join(s.replace("\\", "\\\\").replace(".", "\\.") for s in segments) */
static char *p_encode_list(const plist *l, size_t n) {
    pbuf b;
    size_t i;
    pb_init(&b);
    for (i = 0; i < n; i++) {
        const char *s;
        if (i) pb_putc(&b, '.');
        for (s = l->seg[i]; s && *s; s++) {
            if (*s == '\\') { pb_putc(&b, '\\'); pb_putc(&b, '\\'); }
            else if (*s == '.') { pb_putc(&b, '\\'); pb_putc(&b, '.'); }
            else pb_putc(&b, *s);
        }
    }
    return b.p;
}

static json_t *ok_value(json_t *v) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", v);
    return o;
}

static json_t *err_value(char *msg) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    if (msg) {
        json_set(o, "error", json_string(msg));
        free(msg);
    } else {
        json_set(o, "error", json_string("PathError"));
    }
    return o;
}

/* ------------------------------------------------------------- validation */

/* The ported validate(): NULL on success, an allocated message on refusal. */
static char *p_validate(const plist *l, const char *encoded) {
    char *shown = NULL, *rep;
    size_t i;

    if (!encoded) {
        shown = p_encode_list(l, l->n);
        if (!shown) return NULL;
    }
    rep = py_repr(encoded ? encoded : shown);
    if (!rep) { free(shown); return NULL; }

    if (l->n < 1 || l->n > W24_PATHS_616C0C_MAX_SEGMENTS) {
        char *m = fmt_alloc("config path %s must have 1-%d segments", rep,
                            W24_PATHS_616C0C_MAX_SEGMENTS);
        free(rep); free(shown);
        return m;
    }
    if (strlen(encoded ? encoded : shown) > W24_PATHS_616C0C_MAX_ENCODED_BYTES) {
        char *m = fmt_alloc("config path is longer than %d bytes",
                            W24_PATHS_616C0C_MAX_ENCODED_BYTES);
        free(rep); free(shown);
        return m;
    }
    for (i = 0; i < l->n; i++) {
        const unsigned char *p = (const unsigned char *)l->seg[i];
        size_t blen = strlen(l->seg[i]);
        int bad = 0;
        if (blen < 1 || blen > W24_PATHS_616C0C_MAX_SEGMENT_BYTES) {
            char *m = fmt_alloc("config path %s: a segment must be 1-%d bytes", rep,
                                W24_PATHS_616C0C_MAX_SEGMENT_BYTES);
            free(rep); free(shown);
            return m;
        }
        for (; *p; p++) {
            if (*p < 0x20 || *p == 0x7f) { bad = 1; break; }
        }
        if (bad) {
            char *m = fmt_alloc("config path %s: control character in a segment", rep);
            free(rep); free(shown);
            return m;
        }
        if (!strcmp(l->seg[i], "__proto__")) {
            char *m = fmt_alloc("reserved segment '__proto__' in config path %s", rep);
            free(rep); free(shown);
            return m;
        }
    }
    free(rep);
    free(shown);
    return NULL;
}

/* decode(): NULL on success (fills *out), an allocated message on refusal. */
static char *p_decode(const char *encoded, plist *out) {
    size_t i = 0, n = strlen(encoded);
    pbuf cur;
    char *err = NULL;

    pb_init(&cur);
    if (!cur.p) return NULL;
    while (i < n) {
        char ch = encoded[i];
        if (ch == '\\') {
            if (i + 1 >= n || (encoded[i + 1] != '\\' && encoded[i + 1] != '.')) {
                char *rep = py_repr(encoded);
                err = fmt_alloc("invalid escape in config path %s", rep ? rep : "''");
                free(rep);
                free(cur.p);
                return err;
            }
            pb_putc(&cur, encoded[i + 1]);
            i += 2;
            continue;
        }
        if (ch == '.') {
            if (cur.len == 0) {
                char *rep = py_repr(encoded);
                err = fmt_alloc("empty segment in config path %s", rep ? rep : "''");
                free(rep);
                free(cur.p);
                return err;
            }
            pl_push(out, cur.p);
            cur.len = 0;
            if (cur.p) cur.p[0] = '\0';
        } else {
            pb_putc(&cur, ch);
        }
        i++;
    }
    if (cur.len == 0) {
        char *rep = py_repr(encoded);
        err = fmt_alloc("empty segment in config path %s", rep ? rep : "''");
        free(rep);
        free(cur.p);
        return err;
    }
    pl_push(out, cur.p);
    free(cur.p);
    err = p_validate(out, encoded);
    return err;
}

/* ---------------------------------------------------------------- surface */

json_t *w24_paths_616c0c_encode(const json_t *segments) {
    json_t *out;
    plist l;
    char *s;
    size_t i;

    memset(&l, 0, sizeof(l));
    if (!segments || segments->type != JSON_ARRAY) {
        return err_value(fmt_alloc("'%s' object is not iterable",
                                   py_typename(segments)));
    }
    for (i = 0; i < json_len(segments); i++) {
        const json_t *item = json_get(segments, i);
        if (!item || item->type != JSON_STRING) {
            const char *tn = py_typename(item);
            pl_free(&l);
            return err_value(fmt_alloc("'%s' object has no attribute 'replace'", tn));
        }
        pl_push(&l, item->str_val ? item->str_val : "");
    }
    s = p_encode_list(&l, l.n);
    pl_free(&l);
    if (!s) return err_value(NULL);
    out = ok_value(json_string(s));
    free(s);
    return out;
}

json_t *w24_paths_616c0c_decode(const json_t *encoded) {
    plist l;
    char *err;
    json_t *path, *out;
    size_t i;

    memset(&l, 0, sizeof(l));
    if (!encoded || encoded->type != JSON_STRING) {
        return err_value(fmt_alloc("object of type '%s' has no len()",
                                   py_typename(encoded)));
    }
    err = p_decode(encoded->str_val ? encoded->str_val : "", &l);
    if (err) {
        pl_free(&l);
        return err_value(err);
    }
    path = json_array();
    for (i = 0; i < l.n; i++) json_append(path, json_string(l.seg[i]));
    pl_free(&l);
    out = ok_value(path);
    return out;
}

json_t *w24_paths_616c0c_validate(const json_t *req) {
    const json_t *segments = json_obj_get(req, "segments");
    const json_t *encoded = json_obj_get(req, "encoded");
    const char *enc = NULL;
    plist l;
    char *err;
    size_t i;

    memset(&l, 0, sizeof(l));
    if (!segments || segments->type != JSON_ARRAY) {
        return err_value(fmt_alloc("'%s' object is not iterable",
                                   py_typename(segments)));
    }
    if (encoded && encoded->type == JSON_STRING) enc = encoded->str_val;
    for (i = 0; i < json_len(segments); i++) {
        const json_t *item = json_get(segments, i);
        if (!item || item->type != JSON_STRING) {
            const char *tn = py_typename(item);
            pl_free(&l);
            /* Without an explicit encoded path, encode(segments) runs first and
             * fails on .replace; with one, the segment's .encode fails. */
            if (!enc) return err_value(fmt_alloc("'%s' object has no attribute 'replace'", tn));
            return err_value(fmt_alloc("'%s' object has no attribute 'encode'", tn));
        }
        pl_push(&l, item->str_val ? item->str_val : "");
    }
    err = p_validate(&l, enc);
    pl_free(&l);
    if (err) return err_value(err);
    return ok_value(json_bool(true));
}

json_t *w24_paths_616c0c_covers(const json_t *lock, const json_t *path) {
    size_t nlock, np, i;

    if (!lock || lock->type != JSON_ARRAY) return json_bool(false);
    if (!path || path->type != JSON_ARRAY) return json_bool(false);
    nlock = json_len(lock);
    np = json_len(path);
    if (nlock > np) return json_bool(false);
    for (i = 0; i < nlock; i++) {
        const json_t *a = json_get(lock, i), *b = json_get(path, i);
        const char *sa = a && a->type == JSON_STRING ? a->str_val : NULL;
        const char *sb = b && b->type == JSON_STRING ? b->str_val : NULL;
        if (!sa || !sb || strcmp(sa, sb)) return json_bool(false);
    }
    return json_bool(true);
}

json_t *w24_paths_616c0c_covering_lock(const json_t *locks, const json_t *path) {
    size_t i;

    if (!locks || locks->type != JSON_ARRAY) return json_null();
    for (i = 0; i < json_len(locks); i++) {
        const json_t *pair = json_get(locks, i);
        const json_t *lock, *level;
        json_t *covered;
        int hit;
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
        lock = json_get(pair, 0);
        level = json_get(pair, 1);
        covered = w24_paths_616c0c_covers(lock, path);
        hit = covered && covered->type == JSON_BOOL && covered->bool_val;
        if (covered) json_free(covered);
        if (hit) {
            json_t *o = json_object();
            json_set(o, "lock", lock ? json_copy(lock) : json_array());
            json_set(o, "level", level ? json_copy(level) : json_null());
            return o;
        }
    }
    return json_null();
}

json_t *w24_paths_616c0c_from_dotted(const json_t *dotted) {
    plist l;
    char *err;
    json_t *path;
    size_t i;

    memset(&l, 0, sizeof(l));
    if (!dotted || dotted->type != JSON_STRING) {
        return err_value(fmt_alloc("object of type '%s' has no len()",
                                   py_typename(dotted)));
    }
    err = p_decode(dotted->str_val ? dotted->str_val : "", &l);
    if (err) {
        /* Not a well-formed encoded path: the CLI spelling splits on every dot. */
        const char *start = dotted->str_val ? dotted->str_val : "";
        free(err);
        pl_free(&l);
        for (;;) {
            const char *dot = strchr(start, '.');
            if (!dot) {
                pl_push(&l, start);
                break;
            }
            {
                size_t len = (size_t)(dot - start);
                char *piece = (char *)malloc(len + 1);
                if (piece) {
                    memcpy(piece, start, len);
                    piece[len] = '\0';
                    pl_push(&l, piece);
                    free(piece);
                }
            }
            start = dot + 1;
        }
    }
    path = json_array();
    for (i = 0; i < l.n; i++) json_append(path, json_string(l.seg[i]));
    pl_free(&l);
    return ok_value(path);
}

json_t *w24_paths_616c0c_constants(void) {
    json_t *o = json_object();
    json_set(o, "max_segments", json_number((double)W24_PATHS_616C0C_MAX_SEGMENTS));
    json_set(o, "max_segment_bytes", json_number((double)W24_PATHS_616C0C_MAX_SEGMENT_BYTES));
    json_set(o, "max_encoded_bytes", json_number((double)W24_PATHS_616C0C_MAX_ENCODED_BYTES));
    return o;
}
