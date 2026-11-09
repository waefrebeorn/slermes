/*
 * port_hosted_rooms_common.c
 *
 * Pure helpers ported from gateway/hosted_rooms_common.py (14 defs, previously
 * 0 ported). The module is the shared validation / canonical-serialisation
 * layer for the hosted-room gateway.
 *
 *   hrc_identifier   <- hosted_rooms_common.py:identifier
 *   hrc_exact_fields <- hosted_rooms_common.py:exact_fields
 *   hrc_text         <- hosted_rooms_common.py:text
 *   hrc_compact_json <- hosted_rooms_common.py:compact_json
 *   hrc_utf8_len     <- hosted_rooms_common.py:utf8_len
 *
 * Error handling: the Python raises a caller-supplied exception type. The C
 * port returns 0 on success and -1 on the equivalent error, with the exact
 * message the Python would have raised in *out_err (caller frees). That keeps
 * the messages byte-identical without inventing an exception type.
 */

#include "port_hosted_rooms_common.h"

#include <stdlib.h>
#include <string.h>

#include "../lib/libjson/json.h"
#include "../lib/libregex/hermes_regex.h"
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

typedef struct { const char *key; const json_t *val; } hrc_kv_t;

/* ===========================================================================
 *  Small internal helpers
 * =========================================================================== */

static int hrc_isspace(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* a malloc'd sprintf-style message, sized from the format */
static char *hrc_fmt(const char *fmt, ...)
{
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return NULL; }
    char *out = (char *)malloc((size_t)n + 1);
    if (out) vsnprintf(out, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return out;
}

/* ", ".join(sorted) into a malloc'd string */
static char *hrc_join(const char **items, int n)
{
    size_t cap = 1;
    for (int i = 0; i < n; i++) cap += strlen(items[i]) + 2;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (i) strcat(out, ", ");
        strcat(out, items[i]);
    }
    return out;
}



/* hosted_rooms_common.py:IDENTIFIER_RE */
#define HRC_IDENTIFIER_RE "^[A-Za-z0-9][A-Za-z0-9._:-]*$"

#define HRC_MAX_ERR 256

/* ===========================================================================
 *  identifier  (hosted_rooms_common.py:identifier)
 *
 *      if not isinstance(value, str): raise error(f"{label} must be a string")
 *      value = value.strip()
 *      if not value or len(value) > max_chars
 *         or (pattern is not None and not pattern.fullmatch(value)):
 *          raise error(invalid or f"invalid {label}")
 *      return value
 *
 *  `kind` carries the Python type: 0 = not a str, 2 = str. `use_pattern` 0
 *  selects `pattern=None` (skip the shape check). `out` receives the stripped
 *  value; `err` receives the message. Returns 0 / -1.
 * =========================================================================== */

/* PoP: identifier @ gateway/hosted_rooms_common.py:identifier */
int hrc_identifier(int kind, const char *value, const char *label,
                   int max_chars, int use_pattern,
                   const char *invalid_msg, char **out, char **err)
{
    if (out) *out = NULL;
    if (err) *err = NULL;
    if (kind != 2 || !value) {
        if (err) *err = hrc_fmt("%s must be a string", label);
        return -1;
    }

    const char *s = value, *e = value + strlen(value);
    while (*s && hrc_isspace((unsigned char)*s)) s++;
    while (e > s && hrc_isspace((unsigned char)e[-1])) e--;
    size_t len = (size_t)(e - s);

    if (len == 0 || len > (size_t)max_chars) goto invalid;

    if (use_pattern) {
        static hregex_t *re = NULL;
        if (!re) re = regex_compile(HRC_IDENTIFIER_RE, 0);
        if (re) {
            char *tmp = (char *)malloc(len + 1);
            if (!tmp) return -1;
            memcpy(tmp, s, len);
            tmp[len] = '\0';
            regex_match_t *m = regex_match(re, tmp);   /* fullmatch */
            int ok = (m && m->matched && m->groups[0] && strlen(m->groups[0]) == len);
            if (m) regex_match_free(m);
            free(tmp);
            if (!ok) goto invalid;
        }
    }

    if (out) {
        *out = (char *)malloc(len + 1);
        if (!*out) return -1;
        memcpy(*out, s, len);
        (*out)[len] = '\0';
    }
    return 0;

invalid:
    if (err) *err = (invalid_msg && invalid_msg[0])
                        ? hrc_fmt("%s", invalid_msg)
                        : hrc_fmt("invalid %s", label);
    return -1;
}

/* ===========================================================================
 *  exact_fields  (hosted_rooms_common.py:exact_fields)
 *
 *      if not isinstance(value, Mapping): raise error(not_object or f"{label} must be an object")
 *      keys = frozenset(value)
 *      missing = required - keys
 *      unknown = keys - required - optional
 *      if missing: raise error(missing_fmt.format(label=label, fields=", ".join(sorted(missing))))
 *      if unknown: raise error(unknown_fmt.format(label=label, fields=", ".join(sorted(unknown))))
 *      return value
 *
 *  `keys` is the object's keys; `required` / `optional` the allowed sets.
 * =========================================================================== */

static int hrc_cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* PoP: exact_fields @ gateway/hosted_rooms_common.py:exact_fields */
int hrc_exact_fields(const char *const *keys, int nkeys,
                     const char *const *required, int nrequired,
                     const char *const *optional, int noptional,
                     const char *label, const char *not_object,
                     char **err)
{
    if (err) *err = NULL;

    const char **missing = (const char **)calloc((size_t)nrequired + 1, sizeof(char *));
    const char **unknown = (const char **)calloc((size_t)nkeys + 1, sizeof(char *));
    if (!missing || !unknown) { free(missing); free(unknown); return -1; }

    int nmissing = 0, nunknown = 0;
    for (int i = 0; i < nrequired; i++) {
        int found = 0;
        for (int j = 0; j < nkeys; j++) if (strcmp(required[i], keys[j]) == 0) { found = 1; break; }
        if (!found) missing[nmissing++] = required[i];
    }
    for (int i = 0; i < nkeys; i++) {
        int is_req = 0, is_opt = 0;
        for (int j = 0; j < nrequired; j++) if (strcmp(keys[i], required[j]) == 0) { is_req = 1; break; }
        if (!is_req)
            for (int j = 0; j < noptional; j++) if (strcmp(keys[i], optional[j]) == 0) { is_opt = 1; break; }
        if (!is_req && !is_opt) unknown[nunknown++] = keys[i];
    }

    int rc = 0;
    if (nmissing) {
        qsort(missing, (size_t)nmissing, sizeof(char *), hrc_cmp_str);
        if (err) *err = hrc_fmt("%s is missing fields: %s", label, hrc_join(missing, nmissing));
        rc = -1;
    } else if (nunknown) {
        qsort(unknown, (size_t)nunknown, sizeof(char *), hrc_cmp_str);
        if (err) *err = hrc_fmt("%s has unknown fields: %s", label, hrc_join(unknown, nunknown));
        rc = -1;
    }
    free(missing); free(unknown);
    return rc;
}

/* ===========================================================================
 *  text  (hosted_rooms_common.py:text)
 *
 *      if not isinstance(value, str): raise error(f"{label} must be a string")
 *      if not value.strip(): raise error(f"{label} must not be empty")
 *      if strip: value = value.strip()
 *      if len(value.encode("utf-8")) > max_bytes: raise error(f"{label} is too large")
 *      return value
 *
 *  Note the order: the BLANK check runs on the UNSTRIPPED value, and the size
 *  check runs on the STRIPPED one.
 * =========================================================================== */

/* PoP: text @ gateway/hosted_rooms_common.py:text */
int hrc_text(int kind, const char *value, const char *label,
             int max_bytes, int strip, char **out, char **err)
{
    if (out) *out = NULL;
    if (err) *err = NULL;
    if (kind != 2 || !value) {
        if (err) *err = hrc_fmt("%s must be a string", label);
        return -1;
    }
    /* value.strip() non-empty? */
    const char *s = value, *e = value + strlen(value);
    while (*s && hrc_isspace((unsigned char)*s)) s++;
    while (e > s && hrc_isspace((unsigned char)e[-1])) e--;
    if (s == e) {
        if (err) *err = hrc_fmt("%s must not be empty", label);
        return -1;
    }
    if (strip) value = s;   /* the length check below uses the stripped span */

    size_t len = strip ? (size_t)(e - s) : strlen(value);
    if (len > (size_t)max_bytes) {
        if (err) *err = hrc_fmt("%s is too large", label);
        return -1;
    }
    if (out) {
        *out = (char *)malloc(len + 1);
        if (!*out) return -1;
        memcpy(*out, value, len);
        (*out)[len] = '\0';
    }
    return 0;
}

/* ===========================================================================
 *  compact_json  (hosted_rooms_common.py:compact_json)
 *
 *      json.dumps(value, ensure_ascii=ensure_ascii, sort_keys=True,
 *                 separators=(",", ":"))
 *
 *  Sorted-key, separator-free JSON - the digest/storage canonical form.
 *  libjson has no sorted-key serializer, so the object's keys are collected
 *  and sorted here. Caller frees.
 * =========================================================================== */

static int hrc_cmp_kv(const void *a, const void *b)
{
    const hrc_kv_t *ka = (const hrc_kv_t *)a;
    const hrc_kv_t *kb = (const hrc_kv_t *)b;
    return strcmp(ka->key, kb->key);
}

/* PoP: compact_json @ gateway/hosted_rooms_common.py:compact_json */
char *hrc_compact_json(const json_t *value)
{
    if (!value) return NULL;
    switch (value->type) {
    case JSON_NULL:   return strdup("null");
    case JSON_BOOL:   return strdup(value->bool_val ? "true" : "false");
    case JSON_NUMBER: {
        char buf[64];
        if (value->num_val == (double)(long long)value->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)value->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", value->num_val);
        return strdup(buf);
    }
    case JSON_STRING: {
        size_t n = strlen(value->str_val);
        char *out = (char *)malloc(n * 6 + 3);
        if (!out) return NULL;
        char *p = out;
        *p++ = '"';
        for (size_t i = 0; i < n; i++) {
            unsigned char c = (unsigned char)value->str_val[i];
            switch (c) {
            case '"':  *p++ = '\\'; *p++ = '"';  break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            default:
                if (c < 0x20) { p += sprintf(p, "\\u%04x", c); }
                else { *p++ = (char)c; }
            }
        }
        *p++ = '"';
        *p = '\0';
        return out;
    }
    case JSON_ARRAY: {
        size_t cap = 3;
        for (size_t i = 0; i < value->c.count; i++) {
            char *s = hrc_compact_json(value->c.items[i]);
            if (s) { cap += strlen(s) + 2; free(s); }
        }
        char *out = (char *)malloc(cap);
        if (!out) return NULL;
        char *p = out;
        *p++ = '[';
        for (size_t i = 0; i < value->c.count; i++) {
            if (i) *p++ = ',';
            char *s = hrc_compact_json(value->c.items[i]);
            if (s) { size_t l = strlen(s); memcpy(p, s, l); p += l; free(s); }
        }
        *p++ = ']';
        *p = '\0';
        return out;
    }
    case JSON_OBJECT: {
        size_t n = value->c.count;
        hrc_kv_t *kvs = (hrc_kv_t *)calloc(n ? n : 1, sizeof(hrc_kv_t));
        if (!kvs) return NULL;
        for (size_t i = 0; i < n; i++) {
            kvs[i].key = value->c.keys[i];
            kvs[i].val = value->c.items[i];
        }
        qsort(kvs, n, sizeof(hrc_kv_t), hrc_cmp_kv);

        size_t cap = 3;
        for (size_t i = 0; i < n; i++) {
            char *s = hrc_compact_json(kvs[i].val);
            if (s) { cap += strlen(kvs[i].key) + strlen(s) + 4; free(s); }
        }
        char *out = (char *)malloc(cap);
        if (!out) { free(kvs); return NULL; }
        char *p = out;
        *p++ = '{';
        for (size_t i = 0; i < n; i++) {
            if (i) *p++ = ',';
            *p++ = '"';
            size_t kl = strlen(kvs[i].key);
            memcpy(p, kvs[i].key, kl); p += kl;
            *p++ = '"'; *p++ = ':';
            char *s = hrc_compact_json(kvs[i].val);
            if (s) { size_t l = strlen(s); memcpy(p, s, l); p += l; free(s); }
        }
        *p++ = '}';
        *p = '\0';
        free(kvs);
        return out;
    }
    default:
        return NULL;
    }
}

/* ===========================================================================
 *  utf8_len  (hosted_rooms_common.py:utf8_len)
 *
 *      return len("".join(parts).encode("utf-8"))
 *
 *  The parts are concatenated FIRST, then measured - for valid UTF-8 that is
 *  the same as the sum of the parts, and this implementation measures each
 *  part's bytes (identical result).
 * =========================================================================== */

/* PoP: utf8_len @ gateway/hosted_rooms_common.py:utf8_len */
size_t hrc_utf8_len(const char *const *parts, int nparts)
{
    size_t total = 0;
    for (int i = 0; i < nparts; i++) if (parts[i]) total += strlen(parts[i]);
    return total;
}
