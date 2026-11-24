/*
 * api_content.c -- real C11 port of the two api_content sidecar writers of
 * upstream Hermes persistence/sqlite/conversation/_store/api_content.py.
 *
 * PoP: w20_apicontent_974acd_latest_sql @ persistence/sqlite/conversation/_store/api_content.py:set_latest_user_api_content
 * PoP: w20_apicontent_974acd_row_sql @ persistence/sqlite/conversation/_store/api_content.py:set_message_api_content
 * PoP: w20_apicontent_974acd_encode_content @ persistence/sqlite/conversation/_store/messages.py:_encode_content
 * PoP: w20_apicontent_974acd_plan_latest @ persistence/sqlite/conversation/_store/api_content.py:set_latest_user_api_content
 * PoP: w20_apicontent_974acd_plan_row @ persistence/sqlite/conversation/_store/api_content.py:set_message_api_content
 *
 * The holder (a live StoreHolder bound to one sqlite connection) stays
 * upstream: these functions shape the exact UPDATE and its parameter tuple,
 * and report the rowcount the caller's write returned.
 */
#include "hermes_json.h"
#include "noc_w20_apicontent_974acd.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- tiny growable byte buffer ------------------------------------------- */

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} ac974_sb;

static void ac974_sb_init(ac974_sb *b) {
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
}

static int ac974_sb_grow(ac974_sb *b, size_t extra) {
    size_t need = b->n + extra + 1;
    size_t cap;
    char *q;
    if (need <= b->cap) return 1;
    cap = b->cap ? b->cap : 64;
    while (cap < need) cap *= 2;
    q = (char *)realloc(b->p, cap);
    if (!q) return 0;
    b->p = q;
    b->cap = cap;
    return 1;
}

static void ac974_sb_puts(ac974_sb *b, const char *s) {
    size_t l;
    if (!s) return;
    l = strlen(s);
    if (!ac974_sb_grow(b, l)) return;
    memcpy(b->p + b->n, s, l);
    b->n += l;
    b->p[b->n] = '\0';
}

static void ac974_sb_putn(ac974_sb *b, const char *s, size_t n) {
    if (!ac974_sb_grow(b, n)) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void ac974_sb_putc(ac974_sb *b, char c) {
    if (!ac974_sb_grow(b, 1)) return;
    b->p[b->n++] = c;
    b->p[b->n] = '\0';
}

static char *ac974_sb_take(ac974_sb *b) {
    char *r = b->p;
    if (!r) {
        r = (char *)malloc(1);
        if (r) r[0] = '\0';
    }
    b->p = NULL;
    b->n = b->cap = 0;
    return r;
}

/* Python json.dumps(value) with its default ensure_ascii=True and the default
 * ", " / ": " separators -- the encoding _encode_content prefixes. */
static void ac974_put_escaped(ac974_sb *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char buf[16];
    ac974_sb_putc(b, '"');
    while (*p) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') {
            ac974_sb_putc(b, '\\');
            ac974_sb_putc(b, (char)c);
            p++;
        } else if (c == '\n') { ac974_sb_puts(b, "\\n"); p++; }
        else if (c == '\r') { ac974_sb_puts(b, "\\r"); p++; }
        else if (c == '\t') { ac974_sb_puts(b, "\\t"); p++; }
        else if (c == '\b') { ac974_sb_puts(b, "\\b"); p++; }
        else if (c == '\f') { ac974_sb_puts(b, "\\f"); p++; }
        else if (c < 0x20) {
            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
            ac974_sb_puts(b, buf);
            p++;
        } else if (c < 0x80) {
            ac974_sb_putc(b, (char)c);
            p++;
        } else {
            /* UTF-8 sequence -> \uXXXX, surrogate pair above the BMP. */
            unsigned cp = 0;
            int extra = 0, i;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; extra = 1; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; extra = 2; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; extra = 3; }
            else { cp = c; extra = 0; }
            p++;
            for (i = 0; i < extra && *p; i++, p++) cp = (cp << 6) | (unsigned)(*p & 0x3Fu);
            if (cp > 0xFFFFu) {
                unsigned v = cp - 0x10000u;
                snprintf(buf, sizeof(buf), "\\u%04x\\u%04x",
                         0xD800u + (v >> 10), 0xDC00u + (v & 0x3FFu));
            } else {
                snprintf(buf, sizeof(buf), "\\u%04x", cp);
            }
            ac974_sb_puts(b, buf);
        }
    }
    ac974_sb_putc(b, '"');
}

static void ac974_put_number(ac974_sb *b, double d) {
    char buf[40];
    if (!isfinite(d)) { ac974_sb_puts(b, "NaN"); return; }
    if (floor(d) == d && d >= -9.007199254740992e15 && d <= 9.007199254740992e15) {
        snprintf(buf, sizeof(buf), "%lld", (long long)d);
    } else {
        snprintf(buf, sizeof(buf), "%.17g", d);
    }
    ac974_sb_puts(b, buf);
}

static void ac974_dumps(ac974_sb *b, const json_t *v) {
    size_t i;
    if (!v) { ac974_sb_puts(b, "null"); return; }
    switch (v->type) {
    case JSON_NULL:   ac974_sb_puts(b, "null"); break;
    case JSON_BOOL:   ac974_sb_puts(b, v->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: ac974_put_number(b, v->num_val); break;
    case JSON_STRING: ac974_put_escaped(b, v->str_val); break;
    case JSON_ARRAY:
        ac974_sb_putc(b, '[');
        for (i = 0; i < v->c.count; i++) {
            if (i) ac974_sb_puts(b, ", ");
            ac974_dumps(b, v->c.items[i]);
        }
        ac974_sb_putc(b, ']');
        break;
    case JSON_OBJECT:
        ac974_sb_putc(b, '{');
        for (i = 0; i < v->c.count; i++) {
            if (i) ac974_sb_puts(b, ", ");
            ac974_put_escaped(b, v->c.keys[i]);
            ac974_sb_puts(b, ": ");
            ac974_dumps(b, v->c.items[i]);
        }
        ac974_sb_putc(b, '}');
        break;
    }
}

/* --- statements ----------------------------------------------------------- */

const char *w20_apicontent_974acd_latest_sql(void) {
    return "UPDATE messages SET api_content = ? WHERE id = (SELECT id FROM messages "
           "WHERE session_id = ? AND role = 'user' AND active = 1 ORDER BY id DESC LIMIT 1"
           ") AND content IS ?";
}

const char *w20_apicontent_974acd_row_sql(void) {
    return "UPDATE messages SET api_content = ? WHERE id = ? AND session_id = ? "
           "AND role = 'user' AND active = 1 AND content IS ?";
}

/* --- _encode_content ------------------------------------------------------ */

/* _CONTENT_JSON_PREFIX is a NUL byte followed by "json:". A NUL cannot ride in
 * a C string, so the JSON channel carries the value as {"__prefixed__":"json:..."}
 * (the rest of the string after the reserved NUL) -- the established
 * representation contract for this sentinel across the C ports. */
static const char AC974_CONTENT_JSON_PREFIX[] = "\x00json:";
#define AC974_CONTENT_JSON_PREFIX_LEN 6

static json_t *ac974_prefixed(const char *rest, size_t rest_len) {
    json_t *out = json_object();
    json_t *text;
    char *copy = (char *)malloc(rest_len + 1);
    if (!copy) return out;
    memcpy(copy, rest, rest_len);
    copy[rest_len] = '\0';
    text = json_string(copy);
    free(copy);
    json_set(out, "__prefixed__", text ? text : json_null());
    return out;
}

json_t *w20_apicontent_974acd_encode_content(const json_t *content) {
    if (!content) return json_null();
    if (content->type == JSON_STRING) {
        const char *s = content->str_val ? content->str_val : "";
        if (strncmp(s, AC974_CONTENT_JSON_PREFIX, AC974_CONTENT_JSON_PREFIX_LEN) != 0) {
            return json_string(s);              /* plain text round-trips verbatim */
        }
        /* A literal string carrying the reserved prefix: double-encode it. */
        {
            ac974_sb b;
            char *text;
            json_t *out;
            ac974_sb_init(&b);
            ac974_dumps(&b, content);
            text = ac974_sb_take(&b);
            out = ac974_prefixed(text ? text : "", text ? strlen(text) : 0);
            free(text);
            return out;
        }
    } else if (content->type == JSON_NULL || content->type == JSON_NUMBER ||
               content->type == JSON_BOOL) {
        return json_copy(content);              /* sqlite binds these as they are */
    }
    {
        ac974_sb b;
        char *text;
        json_t *out;
        ac974_sb_init(&b);
        ac974_sb_puts(&b, "json:");             /* the rest of the reserved prefix */
        ac974_dumps(&b, content);
        text = ac974_sb_take(&b);
        out = ac974_prefixed(text ? text : "", text ? strlen(text) : 0);
        free(text);
        return out;
    }
}

/* --- plans ---------------------------------------------------------------- */

/* _scrub_surrogates(api_content): a decoded JSON string cannot carry a lone
 * surrogate, so the bind value is the text itself; None stays None. */
static json_t *ac974_api_param(const char *api_content) {
    return api_content ? json_string(api_content) : json_null();
}

json_t *w20_apicontent_974acd_plan_latest(const char *session_id, const json_t *content,
                                          const char *api_content, int rowcount) {
    json_t *out = json_object();
    json_t *params = json_array();
    json_append(params, ac974_api_param(api_content));
    json_append(params, session_id ? json_string(session_id) : json_null());
    json_append(params, w20_apicontent_974acd_encode_content(content));
    json_set(out, "sql", json_string(w20_apicontent_974acd_latest_sql()));
    json_set(out, "params", params);
    json_set(out, "rowcount", json_number((double)rowcount));
    return out;
}

/* `isinstance(row_id, int)` and not a bool, and strictly positive. */
static int ac974_row_id_ok(const json_t *row_id) {
    double d;
    if (!row_id || row_id->type != JSON_NUMBER) return 0;
    d = row_id->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    return d > 0.0;
}

json_t *w20_apicontent_974acd_plan_row(const char *session_id, const json_t *row_id,
                                       const json_t *content, const char *api_content,
                                       int rowcount) {
    json_t *out;
    json_t *params;
    if (!session_id || !session_id[0] || !ac974_row_id_ok(row_id)) {
        /* The guard returns 0 before the UPDATE is ever built. */
        out = json_object();
        json_set(out, "rowcount", json_number(0.0));
        return out;
    }
    params = json_array();
    json_append(params, ac974_api_param(api_content));
    json_append(params, json_number(row_id->num_val));
    json_append(params, json_string(session_id));
    json_append(params, w20_apicontent_974acd_encode_content(content));
    out = json_object();
    json_set(out, "sql", json_string(w20_apicontent_974acd_row_sql()));
    json_set(out, "params", params);
    json_set(out, "rowcount", json_number((double)rowcount));
    return out;
}
