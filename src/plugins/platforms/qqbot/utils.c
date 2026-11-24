/*
 * utils.c - C11 port of upstream plugins/platforms/qqbot/utils.py
 * (gate w24_utils_e39793).
 *
 * PoP: w24_utils_e39793_coerce_list @ plugins/platforms/qqbot/utils.py:coerce_list
 * PoP: w24_utils_e39793_build_user_agent @ plugins/platforms/qqbot/utils.py:build_user_agent
 * PoP: w24_utils_e39793_api_headers @ plugins/platforms/qqbot/utils.py:get_api_headers
 * PoP: w24_utils_e39793_constants @ plugins/platforms/qqbot/utils.py:<module>
 *
 * Seams: ``sys.version_info``, ``platform.system()`` and the installed
 * hermes-agent version (``importlib.metadata``) arrive as values, and no HTTP
 * client exists here.  Ported: the f-string User-Agent, the two json headers,
 * and ``coerce_list`` -- including Python ``str()``/``repr()`` rendering, since
 * the coercion appends whatever scalar it was handed (``None`` -> "None",
 * ``{}`` -> "{}").
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_utils_e39793.h"

#define QQBOT_VERSION "1.1.0"
#define API_BASE "https://api.sgroup.qq.com"
#define TOKEN_URL "https://bots.qq.com/app/getAppAccessToken"
#define CT_JSON "application/json"

/* ---- Python str()/repr() rendering -------------------------------------- */

/* Shortest %g that round-trips, the way Python 3 renders a float. */
static void qb_float_repr(double v, char *buf, size_t n) {
    int prec;
    char tmp[64];
    if (!isfinite(v)) {
        snprintf(buf, n, "%s", v != v ? "nan" : (v < 0 ? "-inf" : "inf"));
        return;
    }
    if (v == (double)(long long)v && fabs(v) < 1e16) {
        snprintf(buf, n, "%.0f", v);          /* int-valued -> integer text */
        return;
    }
    for (prec = 1; prec <= 17; prec++) {
        snprintf(tmp, sizeof(tmp), "%.*g", prec, v);
        if (strtod(tmp, NULL) == v) { snprintf(buf, n, "%s", tmp); return; }
    }
    snprintf(buf, n, "%.17g", v);
}

typedef struct {
    char *p;
    size_t n, cap;
} qb_buf;

static int qb_add(qb_buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *np;
        while (cap < b->n + n + 1) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return 0;
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
    return 1;
}

static int qb_puts(qb_buf *b, const char *s) { return qb_add(b, s, strlen(s)); }

static int qb_repr_into(qb_buf *b, const json_t *v);

static int qb_repr_string(qb_buf *b, const char *s) {
    const char *p;
    if (!qb_add(b, "'", 1)) return 0;
    for (p = s ? s : ""; *p; p++) {
        switch (*p) {
        case '\\': if (!qb_add(b, "\\\\", 2)) return 0; break;
        case '\'': if (!qb_add(b, "\\'", 2)) return 0; break;
        case '\n': if (!qb_add(b, "\\n", 2)) return 0; break;
        case '\r': if (!qb_add(b, "\\r", 2)) return 0; break;
        case '\t': if (!qb_add(b, "\\t", 2)) return 0; break;
        default:   if (!qb_add(b, p, 1)) return 0; break;
        }
    }
    return qb_add(b, "'", 1);
}

static int qb_repr_into(qb_buf *b, const json_t *v) {
    char num[64];
    if (!v) return qb_puts(b, "None");
    switch (v->type) {
    case JSON_NULL:   return qb_puts(b, "None");
    case JSON_BOOL:   return qb_puts(b, v->bool_val ? "True" : "False");
    case JSON_NUMBER: qb_float_repr(v->num_val, num, sizeof(num)); return qb_puts(b, num);
    case JSON_STRING: return qb_repr_string(b, v->str_val);
    case JSON_ARRAY: {
        size_t i, n = json_len(v);
        if (!qb_add(b, "[", 1)) return 0;
        for (i = 0; i < n; i++) {
            if (i && !qb_add(b, ", ", 2)) return 0;
            if (!qb_repr_into(b, json_get(v, i))) return 0;
        }
        return qb_add(b, "]", 1);
    }
    case JSON_OBJECT: {
        size_t i, n = json_object_size(v);
        if (!qb_add(b, "{", 1)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(v, i);
            if (i && !qb_add(b, ", ", 2)) return 0;
            if (!qb_repr_string(b, k ? k : "")) return 0;
            if (!qb_add(b, ": ", 2)) return 0;
            if (!qb_repr_into(b, json_object_get_at(v, i))) return 0;
        }
        return qb_add(b, "}", 1);
    }
    }
    return 1;
}

/* str(v): like repr() except a string is itself, not quoted. */
static char *qb_str(const json_t *v) {
    qb_buf b = {0};
    if (v && v->type == JSON_STRING) {
        return v->str_val ? strdup(v->str_val) : strdup("");
    }
    if (!qb_repr_into(&b, v)) { free(b.p); return NULL; }
    return b.p ? b.p : strdup("");
}

/* Python str.strip() over ASCII whitespace. */
static void qb_strip_inplace(char *s) {
    size_t n, i = 0;
    if (!s) return;
    n = strlen(s);
    while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' ||
                     s[i] == '\r' || s[i] == '\f' || s[i] == '\v')) i++;
    while (n > i && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' ||
                     s[n - 1] == '\r' || s[n - 1] == '\f' || s[n - 1] == '\v')) n--;
    if (i > 0) memmove(s, s + i, n - i);
    s[n - i] = '\0';
}

/* str(value).split(",") */
static json_t *qb_split_commas(const char *s) {
    json_t *out = json_array();
    const char *p = s ? s : "";
    const char *start = p;
    for (;;) {
        if (*p == ',' || *p == '\0') {
            char *piece = (char *)malloc((size_t)(p - start) + 1);
            if (!piece) break;
            memcpy(piece, start, (size_t)(p - start));
            piece[p - start] = '\0';
            json_append(out, json_string(piece));
            free(piece);
            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }
    return out;
}

json_t *w24_utils_e39793_coerce_list(const json_t *value) {
    json_t *items, *out;
    size_t i, n;
    int owned = 0;

    if (!value || value->type == JSON_NULL) return json_array();

    if (value->type == JSON_STRING) {
        items = qb_split_commas(value->str_val);
        owned = 1;
    } else if (value->type == JSON_ARRAY) {
        items = (json_t *)value;                   /* list/tuple/set */
    } else {
        items = json_array();
        json_append(items, json_copy(value));      /* scalar -> [value] */
        owned = 1;
    }
    if (!items) return json_array();

    out = json_array();
    n = json_len(items);
    for (i = 0; i < n; i++) {
        char *s = qb_str(json_get(items, i));
        if (!s) continue;
        qb_strip_inplace(s);
        if (s[0]) json_append(out, json_string(s));
        free(s);
    }
    if (owned) json_free(items);
    return out;
}

json_t *w24_utils_e39793_build_user_agent(const json_t *req) {
    const char *qqbot, *os_name, *hermes;
    long long major = 0, minor = 0, micro = 0;
    char *buf;
    size_t need;
    json_t *out;

    if (!req || req->type != JSON_OBJECT) return json_null();
    qqbot = json_get_str(req, "qqbot_version", QQBOT_VERSION);
    os_name = json_get_str(req, "os_name", "");
    hermes = json_get_str(req, "hermes_version", "");
    if (json_obj_get(req, "py_major")) major = (long long)json_get_num(req, "py_major", 0);
    if (json_obj_get(req, "py_minor")) minor = (long long)json_get_num(req, "py_minor", 0);
    if (json_obj_get(req, "py_micro")) micro = (long long)json_get_num(req, "py_micro", 0);

    need = strlen(qqbot) + strlen(os_name) + strlen(hermes) + 96;
    buf = (char *)malloc(need);
    if (!buf) return json_null();
    {
        char lowered[256];
        size_t i, ln = strlen(os_name);
        if (ln >= sizeof(lowered)) ln = sizeof(lowered) - 1;
        for (i = 0; i < ln; i++) {
            char c = os_name[i];
            lowered[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
        }
        lowered[ln] = '\0';
        snprintf(buf, need, "QQBotAdapter/%s (Python/%lld.%lld.%lld; %s; Hermes/%s)",
                 qqbot, major, minor, micro, lowered, hermes);
    }
    out = json_string(buf);
    free(buf);
    return out;
}

json_t *w24_utils_e39793_api_headers(const json_t *req) {
    const char *ua = "";
    json_t *o;
    if (req && req->type == JSON_OBJECT) ua = json_get_str(req, "user_agent", "");
    o = json_object();
    json_set(o, "Content-Type", json_string(CT_JSON));
    json_set(o, "Accept", json_string(CT_JSON));
    json_set(o, "User-Agent", json_string(ua));
    return o;
}

json_t *w24_utils_e39793_constants(void) {
    json_t *o = json_object();
    json_set(o, "qqbot_version", json_string(QQBOT_VERSION));
    json_set(o, "api_base", json_string(API_BASE));
    json_set(o, "token_url", json_string(TOKEN_URL));
    json_set(o, "content_type", json_string(CT_JSON));
    json_set(o, "accept", json_string(CT_JSON));
    return o;
}
