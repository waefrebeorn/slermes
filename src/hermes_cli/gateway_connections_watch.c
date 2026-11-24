/*
 * gateway_connections_watch.c -- real C11 port of the pure decisions of
 * hermes_cli/gateway_connections_watch.py.
 *
 * PoP: w22_gatewaycon_d39075_dial_fields @ hermes_cli/gateway_connections_watch.py:_DIAL_FIELDS
 * PoP: w22_gatewaycon_d39075_poll_interval @ hermes_cli/gateway_connections_watch.py:POLL_INTERVAL_S
 * PoP: w22_gatewaycon_d39075_fingerprints @ hermes_cli/gateway_connections_watch.py:fingerprints
 * PoP: w22_gatewaycon_d39075_check @ hermes_cli/gateway_connections_watch.py:StoreWatch.check
 * PoP: w22_gatewaycon_d39075_teardown_plan @ hermes_cli/gateway_connections_watch.py:_teardown
 *
 * `_stamp` (os.stat), `store.read_registry`, `_poll`/`watch_store` (asyncio)
 * and the four teardown functions stay upstream: the first two are the
 * filesystem seam, the last are live authority/socket handles.
 */
#include "hermes_json.h"
#include "noc_w22_gatewaycon_d39075.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GW_DIAL_FIELD_COUNT 12

/* Source order, verbatim from _DIAL_FIELDS. */
static const char *const GW_FIELDS[GW_DIAL_FIELD_COUNT] = {
    "kind", "url", "authMode", "org", "host", "user", "port", "keyPath",
    "remoteHermesPath", "remoteProfile", "token", "headers"
};

/* --- tiny growable text buffer ------------------------------------------- */

typedef struct { char *p; size_t n, cap; int oom; } gw_buf;

static void gw_buf_grow(gw_buf *b, size_t need) {
    size_t cap;
    char *p;
    if (b->oom) return;
    if (b->n + need + 1 <= b->cap) return;
    cap = b->cap ? b->cap : 64;
    while (cap < b->n + need + 1) cap *= 2;
    p = (char *)realloc(b->p, cap);
    if (!p) { b->oom = 1; return; }
    b->p = p;
    b->cap = cap;
}

static void gw_buf_putn(gw_buf *b, const char *s, size_t n) {
    gw_buf_grow(b, n);
    if (b->oom) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void gw_buf_puts(gw_buf *b, const char *s) { gw_buf_putn(b, s, strlen(s)); }

static void gw_buf_putc(gw_buf *b, char c) { gw_buf_putn(b, &c, 1); }

static void gw_buf_putf(gw_buf *b, long long v) {
    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "%lld", v);
    if (n > 0) gw_buf_putn(b, tmp, (size_t)n);
}

/* --- Python json.dumps(value, sort_keys=True, default=str) --------------- */

static int gw_is_py_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* json.dumps(..., ensure_ascii=True): every non-ASCII code point is \uXXXX
 * (surrogate pair above the BMP); ASCII control characters are escaped. */
static void gw_dump_string(gw_buf *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    gw_buf_putc(b, '"');
    while (*p) {
        unsigned char c = *p;
        if (c >= 0x80) {
            unsigned long cp = 0;
            int extra;
            if ((c & 0xe0) == 0xc0) { cp = c & 0x1fUL; extra = 1; }
            else if ((c & 0xf0) == 0xe0) { cp = c & 0x0fUL; extra = 2; }
            else if ((c & 0xf8) == 0xf0) { cp = c & 0x07UL; extra = 3; }
            else { cp = 0xfffdUL; extra = 0; }
            p++;
            while (extra-- > 0 && (*p & 0xc0) == 0x80) {
                cp = (cp << 6) | (*p & 0x3fUL);
                p++;
            }
            if (cp > 0xffffUL) {
                unsigned long v = cp - 0x10000UL;
                char esc[16];
                snprintf(esc, sizeof(esc), "\\u%04lx\\u%04lx",
                         0xd800UL + (v >> 10), 0xdc00UL + (v & 0x3ffUL));
                gw_buf_puts(b, esc);
            } else {
                char esc[16];
                snprintf(esc, sizeof(esc), "\\u%04lx", cp);
                gw_buf_puts(b, esc);
            }
            continue;
        }
        switch (c) {
        case '"':  gw_buf_puts(b, "\\\""); break;
        case '\\': gw_buf_puts(b, "\\\\"); break;
        case '\b': gw_buf_puts(b, "\\b"); break;
        case '\f': gw_buf_puts(b, "\\f"); break;
        case '\n': gw_buf_puts(b, "\\n"); break;
        case '\r': gw_buf_puts(b, "\\r"); break;
        case '\t': gw_buf_puts(b, "\\t"); break;
        default:
            if (c < 0x20 || c == 0x7f) {
                char esc[8];
                snprintf(esc, sizeof(esc), "\\u%04x", (unsigned)c);
                gw_buf_puts(b, esc);
            } else {
                gw_buf_putc(b, (char)c);
            }
        }
        p++;
    }
    gw_buf_putc(b, '"');
}

static void gw_dump_value(gw_buf *b, const json_t *v);

static int gw_cmp_key(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void gw_dump_object(gw_buf *b, const json_t *v) {
    size_t n = json_object_size(v), i;
    const char **keys;
    gw_buf_putc(b, '{');
    if (n == 0) { gw_buf_putc(b, '}'); return; }
    keys = (const char **)malloc(n * sizeof(*keys));
    if (!keys) { b->oom = 1; return; }
    for (i = 0; i < n; i++) keys[i] = json_object_get_key_at(v, i);
    qsort(keys, n, sizeof(*keys), gw_cmp_key);
    for (i = 0; i < n; i++) {
        if (i) gw_buf_puts(b, ", ");
        gw_dump_string(b, keys[i]);
        gw_buf_puts(b, ": ");
        gw_dump_value(b, json_obj_get(v, keys[i]));
    }
    free(keys);
    gw_buf_putc(b, '}');
}

static void gw_dump_value(gw_buf *b, const json_t *v) {
    size_t i, n;
    if (!v) { gw_buf_puts(b, "null"); return; }
    switch (v->type) {
    case JSON_NULL: gw_buf_puts(b, "null"); return;
    case JSON_BOOL: gw_buf_puts(b, v->bool_val ? "true" : "false"); return;
    case JSON_NUMBER:
        if (gw_is_py_int(v)) {
            gw_buf_putf(b, (long long)v->num_val);
        } else {
            char tmp[40];
            snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
            gw_buf_puts(b, tmp);
        }
        return;
    case JSON_STRING: gw_dump_string(b, v->str_val); return;
    case JSON_ARRAY:
        gw_buf_putc(b, '[');
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) gw_buf_puts(b, ", ");
            gw_dump_value(b, json_get(v, i));
        }
        gw_buf_putc(b, ']');
        return;
    case JSON_OBJECT: gw_dump_object(b, v); return;
    }
    gw_buf_puts(b, "null");
}

/* Caller free()s. */
static char *gw_dumps(const json_t *v) {
    gw_buf b;
    memset(&b, 0, sizeof(b));
    gw_buf_grow(&b, 16);
    gw_dump_value(&b, v);
    if (b.oom) { free(b.p); return NULL; }
    return b.p;
}

/* --- dial fields --------------------------------------------------------- */

double w22_gatewaycon_d39075_poll_interval(void) { return 1.5; }

json_t *w22_gatewaycon_d39075_dial_fields(void) {
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < GW_DIAL_FIELD_COUNT; i++) json_append(arr, json_string(GW_FIELDS[i]));
    return arr;
}

/* --- fingerprints -------------------------------------------------------- */

static json_t *gw_failed(const char *error) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(0));
    json_set(out, "error", json_string(error));
    return out;
}

static json_t *gw_fingerprints(const json_t *registry, const char **error) {
    json_t *conns, *out;
    size_t i, k;

    if (!registry || registry->type != JSON_OBJECT) { *error = "TypeError"; return NULL; }
    conns = json_obj_get(registry, "connections");
    if (!conns) { *error = "KeyError"; return NULL; }
    if (conns->type != JSON_ARRAY) { *error = "TypeError"; return NULL; }
    out = json_object();
    for (i = 0; i < json_len(conns); i++) {
        json_t *c = json_get(conns, i);
        json_t *id, *picked;
        char *id_text, *text;
        if (!c || c->type != JSON_OBJECT) { json_free(out); *error = "TypeError"; return NULL; }
        id = json_obj_get(c, "id");
        if (!id) { json_free(out); *error = "KeyError"; return NULL; }
        id_text = id->type == JSON_STRING ? NULL : gw_dumps(id);
        picked = json_array();
        for (k = 0; k < GW_DIAL_FIELD_COUNT; k++) {
            json_t *v = json_obj_get(c, GW_FIELDS[k]);
            json_append(picked, v ? json_copy(v) : json_null());
        }
        text = gw_dumps(picked);
        json_free(picked);
        if (text) {
            json_set(out, id->type == JSON_STRING ? id->str_val : (id_text ? id_text : ""),
                     json_string(text));
            free(text);
        }
        free(id_text);
    }
    return out;
}

json_t *w22_gatewaycon_d39075_fingerprints(const json_t *registry) {
    const char *error = NULL;
    json_t *prints = gw_fingerprints(registry, &error);
    json_t *out;
    if (!prints) return gw_failed(error ? error : "TypeError");
    out = json_object();
    json_set(out, "ok", json_bool(1));
    json_set(out, "value", prints);
    return out;
}

/* --- equality ------------------------------------------------------------ */

static int gw_eq(const json_t *a, const json_t *b) {
    size_t i, n;
    if (!a || !b) return a == b;
    if (a->type != b->type) return 0;
    switch (a->type) {
    case JSON_NULL: return 1;
    case JSON_BOOL: return (a->bool_val ? 1 : 0) == (b->bool_val ? 1 : 0);
    case JSON_NUMBER: return a->num_val == b->num_val;
    case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
    case JSON_ARRAY:
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++) if (!gw_eq(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    case JSON_OBJECT:
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *key = json_object_get_key_at(a, i);
            if (!key) continue;
            if (!gw_eq(json_object_get_at(a, i), json_obj_get(b, key))) return 0;
        }
        return 1;
    }
    return 0;
}

/* The three stat fields of a stamp, as text, so int-vs-string stays distinct. */
static char *gw_stamp_text(const json_t *stamp, const char *field) {
    const json_t *v;
    if (!stamp || stamp->type == JSON_NULL) return NULL;
    if (stamp->type != JSON_OBJECT) return NULL;
    v = json_obj_get(stamp, field);
    return gw_dumps(v ? v : NULL);
}

static int gw_stamp_eq(const json_t *a, const json_t *b) {
    static const char *const FIELDS[3] = {"mtime_ns", "size", "ino"};
    int a_null = (!a || a->type == JSON_NULL);
    int b_null = (!b || b->type == JSON_NULL);
    int i, same = 1;
    if (a_null || b_null) return a_null && b_null;
    for (i = 0; i < 3 && same; i++) {
        char *ta = gw_stamp_text(a, FIELDS[i]);
        char *tb = gw_stamp_text(b, FIELDS[i]);
        if (!ta || !tb || strcmp(ta, tb)) same = 0;
        free(ta);
        free(tb);
    }
    return same;
}

/* --- check --------------------------------------------------------------- */

static json_t *gw_check_result(const json_t *changed, const json_t *removed) {
    json_t *out = json_object();
    json_set(out, "changed", changed ? changed : json_array());
    json_set(out, "removed", removed ? removed : json_array());
    return out;
}

static json_t *gw_raised(const char *type) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    return out;
}

/* StoreWatch._read(): `registry["revision"]` then fingerprints(registry). */
static json_t *gw_read(const json_t *registry, json_t **prints_out, const char **error) {
    json_t *rev, *prints;
    if (!registry || registry->type != JSON_OBJECT) { *error = "TypeError"; return NULL; }
    rev = json_obj_get(registry, "revision");
    if (!rev) { *error = "KeyError"; return NULL; }
    prints = gw_fingerprints(registry, error);
    if (!prints) return NULL;
    *prints_out = prints;
    return rev;
}

json_t *w22_gatewaycon_d39075_check(const json_t *prev_registry, const json_t *prev_stamp,
                                    const json_t *new_stamp, const json_t *new_registry) {
    const char *error = NULL;
    json_t *prev_prints = NULL, *new_prints = NULL, *changed, *removed;
    json_t *prev_rev, *new_rev;
    size_t i;

    /* `if stamp == self.stamp: return [], []` -- the file did not move. */
    if (gw_stamp_eq(prev_stamp, new_stamp)) {
        return gw_check_result(json_array(), json_array());
    }

    new_rev = gw_read(new_registry, &new_prints, &error);
    if (!new_rev) return gw_raised(error ? error : "TypeError");
    prev_rev = gw_read(prev_registry, &prev_prints, &error);
    if (!prev_rev) {
        json_free(new_prints);
        return gw_raised(error ? error : "TypeError");
    }
    {
        if (gw_eq(prev_rev, new_rev) && gw_eq(prev_prints, new_prints)) {
            json_free(prev_prints);
            json_free(new_prints);
            return gw_check_result(json_array(), json_array());
        }
    }

    removed = json_array();
    changed = json_array();
    for (i = 0; i < json_object_size(prev_prints); i++) {
        const char *key = json_object_get_key_at(prev_prints, i);
        json_t *then = json_object_get_at(prev_prints, i);
        json_t *now;
        if (!key) continue;
        now = json_obj_get(new_prints, key);
        if (!now) {
            json_append(removed, json_string(key));
        } else if (!gw_eq(then, now)) {
            json_append(changed, json_string(key));
        }
    }
    json_free(prev_prints);
    json_free(new_prints);
    return gw_check_result(changed, removed);
}

/* --- teardown plan ------------------------------------------------------- */

static void gw_append_step(json_t *plan, const char *step, const json_t *id) {
    json_t *pair = json_array();
    json_append(pair, json_string(step));
    json_append(pair, id ? json_copy(id) : json_null());
    json_append(plan, pair);
}

json_t *w22_gatewaycon_d39075_teardown_plan(const json_t *changed, const json_t *removed) {
    json_t *plan = json_array();
    size_t i;
    if (!changed || changed->type != JSON_ARRAY) changed = NULL;
    if (!removed || removed->type != JSON_ARRAY) removed = NULL;

    /* [(close_gateway_sockets, i) for i in [*changed, *removed]] */
    if (changed) for (i = 0; i < json_len(changed); i++)
        gw_append_step(plan, "close_gateway_sockets", json_get(changed, i));
    if (removed) for (i = 0; i < json_len(removed); i++)
        gw_append_step(plan, "close_gateway_sockets", json_get(removed, i));
    /* + [(reach.forget, i) for i in removed] */
    if (removed) for (i = 0; i < json_len(removed); i++)
        gw_append_step(plan, "forget", json_get(removed, i));
    /* + [(ssh.disconnect, i) for i in changed] + [(oauth.cancel_sign_in, i) ...] */
    if (changed) for (i = 0; i < json_len(changed); i++)
        gw_append_step(plan, "disconnect", json_get(changed, i));
    if (changed) for (i = 0; i < json_len(changed); i++)
        gw_append_step(plan, "cancel_sign_in", json_get(changed, i));
    return plan;
}
