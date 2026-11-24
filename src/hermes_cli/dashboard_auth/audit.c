/*
 * audit.c -- real C11 port of hermes_cli/dashboard_auth/audit.py.
 *
 * PoP: w22_audit_b943f2_bounded @ hermes_cli/dashboard_auth/audit.py:_bounded_value
 * PoP: w22_audit_b943f2_event_value @ hermes_cli/dashboard_auth/audit.py:AuditEvent
 * PoP: w22_audit_b943f2_is_redacted @ hermes_cli/dashboard_auth/audit.py:_REDACTED_FIELDS
 * PoP: w22_audit_b943f2_line @ hermes_cli/dashboard_auth/audit.py:audit_log
 * PoP: w22_audit_b943f2_rotation @ hermes_cli/dashboard_auth/audit.py:_rotate_if_needed
 * PoP: w22_audit_b943f2_limits @ hermes_cli/dashboard_auth/audit.py:_MAX_FIELD_LENGTH
 *
 * The open handle, the log directory and the clock stay upstream (the `ts`
 * value is injected); every decision the write path makes is here.
 */
#include "hermes_json.h"
#include "noc_w22_audit_b943f2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUD_TRUNCATION_MARKER "...[truncated]"
#define AUD_TRUNCATION_LEN 14 /* strlen("...[truncated]") */

/* --- AuditEvent: member name -> literal `event` value --------------------- */

typedef struct { const char *name; const char *value; } aud_event_row;

static const aud_event_row EVENT_TABLE[] = {
    {"LOGIN_START",             "login_start"},
    {"LOGIN_SUCCESS",           "login_success"},
    {"LOGIN_FAILURE",           "login_failure"},
    {"LOGOUT",                  "logout"},
    {"REFRESH_SUCCESS",         "refresh_success"},
    {"REFRESH_FAILURE",         "refresh_failure"},
    {"REVOKE",                  "revoke"},
    {"SESSION_VERIFY_FAILURE",  "session_verify_failure"},
    {"SESSION_REJECTED",        "session_rejected"},
    {"WS_TICKET_MINTED",        "ws_ticket_minted"},
    {"WS_TICKET_REJECTED",      "ws_ticket_rejected"},
    {"TOKEN_AUTH_SUCCESS",      "token_auth_success"},
    {"TOKEN_AUTH_FAILURE",      "token_auth_failure"},
    {"PEER_CONTROL",            "peer_control"},
    {"NATIVE_AUTHORIZE_START",  "native_authorize_start"},
    {"NATIVE_CODE_ISSUED",      "native_code_issued"},
    {"NATIVE_TOKEN_SUCCESS",    "native_token_success"},
    {"NATIVE_TOKEN_FAILURE",    "native_token_failure"},
    {NULL, NULL}
};

const char *w22_audit_b943f2_event_value(const char *name) {
    int i;
    if (!name) return NULL;
    for (i = 0; EVENT_TABLE[i].name; i++) {
        if (!strcmp(name, EVENT_TABLE[i].name)) return EVENT_TABLE[i].value;
    }
    return NULL;
}

/* --- _REDACTED_FIELDS ---------------------------------------------------- */

static const char *const REDACTED_FIELDS[] = {
    "access_token", "refresh_token", "code", "code_verifier",
    "state", "ticket", "cookie", "Authorization", "authorization",
    NULL
};

int w22_audit_b943f2_is_redacted(const char *key) {
    int i;
    if (!key) return 0;
    for (i = 0; REDACTED_FIELDS[i]; i++) {
        if (!strcmp(key, REDACTED_FIELDS[i])) return 1;
    }
    return 0;
}

/* --- _bounded_value ------------------------------------------------------ */

/* Characters, the way len() counts them: UTF-8 lead bytes, not raw bytes. */
static size_t aud_char_count(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (p && *p) {
        if ((*p & 0xC0) != 0x80) n++;
        p++;
    }
    return n;
}

/* Byte offset just past `chars` characters (or the NUL). */
static size_t aud_byte_prefix(const char *s, size_t chars) {
    size_t seen = 0, off = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (p && p[off]) {
        if ((p[off] & 0xC0) != 0x80) {
            if (seen == chars) break;
            seen++;
        }
        off++;
    }
    return off;
}

static json_t *aud_bounded(const json_t *value) {
    size_t i;
    if (!value) return json_null();
    switch (value->type) {
    case JSON_OBJECT: {
        json_t *out = json_object();
        for (i = 0; i < json_object_size(value); i++) {
            const char *k = json_object_get_key_at(value, i);
            if (!k) continue;
            json_set(out, k, aud_bounded(json_object_get_at(value, i)));
        }
        return out;
    }
    case JSON_ARRAY: {
        json_t *out = json_array();
        for (i = 0; i < (size_t)json_len(value); i++) {
            json_append(out, aud_bounded(json_get(value, i)));
        }
        return out;
    }
    case JSON_STRING: {
        size_t n = aud_char_count(value->str_val);
        if (n > (size_t)W22_AUDIT_B943F2_MAX_FIELD_LENGTH) {
            size_t keep = (size_t)W22_AUDIT_B943F2_MAX_FIELD_LENGTH - AUD_TRUNCATION_LEN;
            size_t off = aud_byte_prefix(value->str_val, keep);
            char *buf = (char *)malloc(off + AUD_TRUNCATION_LEN + 1);
            json_t *out;
            if (!buf) return json_null();
            memcpy(buf, value->str_val, off);
            memcpy(buf + off, AUD_TRUNCATION_MARKER, AUD_TRUNCATION_LEN);
            buf[off + AUD_TRUNCATION_LEN] = '\0';
            out = json_string(buf);
            free(buf);
            return out;
        }
        return json_string(value->str_val);
    }
    default:
        return json_copy(value);
    }
}

json_t *w22_audit_b943f2_bounded(const json_t *value) {
    return aud_bounded(value);
}

/* --- the entry line ------------------------------------------------------ */

/* --- a compact JSON writer that matches json.dumps(separators=(",",":")) ---
 *
 * libjson's serializer passes bytes >= 0x80 through, but Python's json.dumps
 * defaults to ensure_ascii=True and escapes everything outside 0x20..0x7e as
 * \uXXXX (astral code points as a surrogate pair). The audit line is compared
 * byte for byte against that, so the writer is spelled out here. */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    int oom;
} aud_sb;

static void aud_sb_reserve(aud_sb *b, size_t extra) {
    size_t need;
    char *nb;
    if (b->oom) return;
    need = b->len + extra + 1;
    if (need <= b->cap) return;
    while (b->cap < need) b->cap = b->cap ? b->cap * 2 : 128;
    nb = (char *)realloc(b->buf, b->cap);
    if (!nb) { b->oom = 1; return; }
    b->buf = nb;
}

static void aud_sb_putc(aud_sb *b, char c) {
    aud_sb_reserve(b, 1);
    if (b->oom) return;
    b->buf[b->len++] = c;
    b->buf[b->len] = '\0';
}

static void aud_sb_put(aud_sb *b, const char *s) {
    size_t n;
    if (!s) return;
    n = strlen(s);
    aud_sb_reserve(b, n);
    if (b->oom) return;
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static char *aud_sb_take(aud_sb *b) {
    if (b->oom) { free(b->buf); return NULL; }
    if (!b->buf) { b->buf = (char *)malloc(1); if (b->buf) b->buf[0] = '\0'; }
    return b->buf;
}

/* One code point from UTF-8; *used gets the byte width (1 on a bad byte). */
static unsigned aud_utf8_next(const unsigned char *p, size_t *used) {
    if (p[0] < 0x80) { *used = 1; return p[0]; }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *used = 2;
        return ((unsigned)(p[0] & 0x1F) << 6) | (unsigned)(p[1] & 0x3F);
    }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *used = 3;
        return ((unsigned)(p[0] & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6) |
               (unsigned)(p[2] & 0x3F);
    }
    if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
        (p[3] & 0xC0) == 0x80) {
        *used = 4;
        return ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3F) << 12) |
               ((unsigned)(p[2] & 0x3F) << 6) | (unsigned)(p[3] & 0x3F);
    }
    *used = 1;
    return p[0];
}

static void aud_sb_string(aud_sb *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char esc[16];
    aud_sb_putc(b, '"');
    while (*p) {
        size_t used = 1;
        unsigned cp = aud_utf8_next(p, &used);
        if (cp == '"') aud_sb_put(b, "\\\"");
        else if (cp == '\\') aud_sb_put(b, "\\\\");
        else if (cp == '\n') aud_sb_put(b, "\\n");
        else if (cp == '\r') aud_sb_put(b, "\\r");
        else if (cp == '\t') aud_sb_put(b, "\\t");
        else if (cp == '\b') aud_sb_put(b, "\\b");
        else if (cp == '\f') aud_sb_put(b, "\\f");
        else if (cp >= 0x20 && cp <= 0x7e) aud_sb_putc(b, (char)cp);
        else if (cp < 0x10000) {
            snprintf(esc, sizeof(esc), "\\u%04x", cp);
            aud_sb_put(b, esc);
        } else {
            unsigned v = cp - 0x10000;
            snprintf(esc, sizeof(esc), "\\u%04x\\u%04x",
                     0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF));
            aud_sb_put(b, esc);
        }
        p += used;
    }
    aud_sb_putc(b, '"');
}

static void aud_sb_number(aud_sb *b, double d) {
    char tmp[64];
    if (d == (double)(long long)d) snprintf(tmp, sizeof(tmp), "%.0f", d);
    else snprintf(tmp, sizeof(tmp), "%.17g", d);
    aud_sb_put(b, tmp);
}

/* Compact: no whitespace at all, like separators=(",", ":"). */
static void aud_dump(aud_sb *b, const json_t *n) {
    size_t i;
    if (!n) { aud_sb_put(b, "null"); return; }
    switch (n->type) {
    case JSON_NULL: aud_sb_put(b, "null"); break;
    case JSON_BOOL: aud_sb_put(b, n->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: aud_sb_number(b, n->num_val); break;
    case JSON_STRING: aud_sb_string(b, n->str_val); break;
    case JSON_ARRAY:
        aud_sb_putc(b, '[');
        for (i = 0; i < (size_t)json_len(n); i++) {
            if (i) aud_sb_putc(b, ',');
            aud_dump(b, json_get(n, i));
        }
        aud_sb_putc(b, ']');
        break;
    case JSON_OBJECT:
        aud_sb_putc(b, '{');
        for (i = 0; i < json_object_size(n); i++) {
            if (i) aud_sb_putc(b, ',');
            aud_sb_string(b, json_object_get_key_at(n, i));
            aud_sb_putc(b, ':');
            aud_dump(b, json_object_get_at(n, i));
        }
        aud_sb_putc(b, '}');
        break;
    }
}

char *w22_audit_b943f2_line(const char *ts, const char *event_name, const json_t *fields) {
    const char *event = w22_audit_b943f2_event_value(event_name);
    json_t *entry;
    size_t i;
    aud_sb b;

    if (!event) return NULL;
    entry = json_object();
    json_set(entry, "ts", json_string(ts ? ts : ""));
    json_set(entry, "event", json_string(event));
    for (i = 0; i < json_object_size(fields); i++) {
        const char *k = json_object_get_key_at(fields, i);
        if (!k || w22_audit_b943f2_is_redacted(k)) continue;
        json_set(entry, k, aud_bounded(json_object_get_at(fields, i)));
    }
    memset(&b, 0, sizeof(b));
    aud_dump(&b, entry);
    json_free(entry);
    return aud_sb_take(&b);
}

/* --- _rotate_if_needed --------------------------------------------------- */

json_t *w22_audit_b943f2_rotation(const char *name, const char *size, const char *pending,
                                  const json_t *existing) {
    json_t *out = json_array();
    long long current, added;
    int i;

    if (!size) return out; /* stat raised OSError: the append surfaces the error */
    current = strtoll(size, NULL, 10);
    added = pending ? strtoll(pending, NULL, 10) : 0;
    if (current + added < W22_AUDIT_B943F2_MAX_BYTES) return out;

    for (i = W22_AUDIT_B943F2_BACKUP_COUNT - 1; i > 0; i--) {
        size_t j;
        int present = 0;
        for (j = 0; j < (size_t)json_len(existing); j++) {
            json_t *v = json_get((json_t *)existing, j);
            if (v && v->type == JSON_NUMBER && (int)v->num_val == i) present = 1;
        }
        if (present) {
            char op[300];
            snprintf(op, sizeof(op), "%s.%d -> %s.%d", name ? name : "(unset)", i,
                     name ? name : "(unset)", i + 1);
            json_append(out, json_string(op));
        }
    }
    {
        char op[300];
        snprintf(op, sizeof(op), "%s -> %s.1", name ? name : "(unset)", name ? name : "(unset)");
        json_append(out, json_string(op));
    }
    return out;
}
