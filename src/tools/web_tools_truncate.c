/*
 * web_tools_truncate.c - faithful C11 port of selected pure helpers from
 * upstream Hermes tools/web_tools_truncate.py (gate w11_webtoolst_dae278).
 *
 * PoP: w11_webtoolst_dae278_clamp_char_limit @ tools/web_tools_truncate.py:_clamp_char_limit
 * PoP: w11_webtoolst_dae278_clamp_or_default @ tools/web_tools_truncate.py:_clamp_or_default
 * PoP: w11_webtoolst_dae278_effective_char_limit @ tools/web_tools_truncate.py:_effective_char_limit
 * PoP: w11_webtoolst_dae278_convert_base64_images @ tools/web_tools_truncate.py:convert_base64_images_to_links
 * PoP: w11_webtoolst_dae278_trim_results @ tools/web_tools_truncate.py:_trim_results
 *
 * Filesystem/network helpers (_store_full_text, _binary_payload_kind) and the
 * disk-writing truncation path are intentionally not ported: they are
 * process-bound and non-deterministic.
 */
#include "noc_w11_webtoolst_dae278.h"

#include <stdlib.h>
#include <string.h>

#define DEFAULT_EXTRACT_CHAR_LIMIT 15000LL
#define CHAR_LIMIT_FLOOR 2000LL
#define CHAR_LIMIT_CEILING 500000LL

/* ---------- small string builder ---------- */
typedef struct { char *s; size_t n, cap; } SB;

static void sb_init(SB *b) {
    b->cap = 64; b->n = 0;
    b->s = (char *)malloc(b->cap);
    if (b->s) b->s[0] = '\0';
}
static void sb_addn(SB *b, const char *p, size_t n) {
    if (!p || !n || !b->s) return;
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap * 2;
        while (nc < b->n + n + 1) nc *= 2;
        char *q = (char *)realloc(b->s, nc);
        if (!q) return;
        b->s = q; b->cap = nc;
    }
    memcpy(b->s + b->n, p, n);
    b->n += n;
    b->s[b->n] = '\0';
}
static void sb_add(SB *b, const char *p) { if (p) sb_addn(b, p, strlen(p)); }
static void sb_addc(SB *b, char c) { sb_addn(b, &c, 1); }
static char *sb_take(SB *b) {
    char *s = b->s ? b->s : (char *)malloc(1);
    if (s && !b->s) s[0] = '\0';
    b->s = NULL; b->n = b->cap = 0;
    return s;
}
static void sb_free(SB *b) { free(b->s); b->s = NULL; b->n = b->cap = 0; }

static int py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python int(value) for a JSON scalar. Returns 1 on success, 0 on TypeError/ValueError. */
static int py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        while (*s && py_isspace((unsigned char)*s)) s++;
        if (!*s) return 0;
        char *end = NULL;
        long long x = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end && py_isspace((unsigned char)*end)) end++;
        if (*end) return 0;
        *out = x;
        return 1;
    }
    return 0; /* null / array / object -> TypeError */
}

static json_t *clamped(long long x) {
    if (x < CHAR_LIMIT_FLOOR) x = CHAR_LIMIT_FLOOR;
    if (x > CHAR_LIMIT_CEILING) x = CHAR_LIMIT_CEILING;
    return json_number((double)x);
}

json_t *w11_webtoolst_dae278_clamp_char_limit(const json_t *value) {
    long long x = 0;
    if (!py_int(value, &x)) return json_string("__ERR__");
    return clamped(x);
}

json_t *w11_webtoolst_dae278_clamp_or_default(const json_t *value) {
    long long x = 0;
    if (!value || value->type == JSON_NULL || !py_int(value, &x))
        return json_number((double)DEFAULT_EXTRACT_CHAR_LIMIT);
    return clamped(x);
}

json_t *w11_webtoolst_dae278_effective_char_limit(const json_t *char_limit,
                                                  const json_t *config_value) {
    if (!char_limit || char_limit->type == JSON_NULL)
        return w11_webtoolst_dae278_clamp_or_default(config_value);
    return w11_webtoolst_dae278_clamp_or_default(char_limit);
}

/* ---------- regex-equivalent base64 image replacement ---------- */
static const char DATA_PREFIX[] = "data:image/";
#define DATA_PREFIX_LEN ((sizeof(DATA_PREFIX) - 1))

static int b64_char(char c, int allow_ws) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=') return 1;
    return allow_ws && py_isspace((unsigned char)c);
}

/* Match `data:image/[^;]+;base64,[A-Za-z0-9+/=]+` at i; allow_ws adds \s to payload. */
static int match_data_uri(const char *s, size_t n, size_t i, int allow_ws, size_t *end) {
    size_t p = i;
    if (n - p < DATA_PREFIX_LEN || memcmp(s + p, DATA_PREFIX, DATA_PREFIX_LEN) != 0) return 0;
    p += DATA_PREFIX_LEN;
    size_t mstart = p;
    while (p < n && s[p] != ';') p++;
    if (p == mstart || p >= n) return 0;              /* [^;]+ needs >=1 char + ';' */
    if (n - p < 8 || memcmp(s + p, ";base64,", 8) != 0) return 0;
    p += 8;
    size_t qstart = p;
    while (p < n && b64_char(s[p], allow_ws)) p++;
    if (p == qstart) return 0;
    *end = p;
    return 1;
}

/* mode 1: ![alt]( data-uri ) ; mode 2: ( data-uri ) ; mode 3: bare data-uri */
static char *replace_pass(const char *s, int mode) {
    size_t n = strlen(s);
    SB out;
    sb_init(&out);
    size_t i = 0;
    while (i < n) {
        if (mode == 1 && s[i] == '!' && i + 1 < n && s[i + 1] == '[') {
            size_t p = i + 2, astart = p;
            while (p < n && s[p] != ']') p++;
            if (p < n) {
                size_t aend = p;
                p++;                                   /* consume ']' */
                if (p < n && s[p] == '(') {
                    p++;
                    while (p < n && py_isspace((unsigned char)s[p])) p++;
                    size_t ue;
                    if (match_data_uri(s, n, p, 1, &ue) && ue < n && s[ue] == ')') {
                        size_t a = astart, b = aend;
                        while (a < b && py_isspace((unsigned char)s[a])) a++;
                        while (b > a && py_isspace((unsigned char)s[b - 1])) b--;
                        if (a == b) sb_add(&out, "[IMAGE]");
                        else { sb_add(&out, "[IMAGE: "); sb_addn(&out, s + a, b - a); sb_addc(&out, ']'); }
                        i = ue + 1;
                        continue;
                    }
                }
            }
        } else if (mode == 2 && s[i] == '(') {
            size_t p = i + 1;
            while (p < n && py_isspace((unsigned char)s[p])) p++;
            size_t ue;
            if (match_data_uri(s, n, p, 1, &ue) && ue < n && s[ue] == ')') {
                sb_add(&out, "[IMAGE]");
                i = ue + 1;
                continue;
            }
        } else if (mode == 3 && n - i >= DATA_PREFIX_LEN &&
                   memcmp(s + i, DATA_PREFIX, DATA_PREFIX_LEN) == 0) {
            size_t ue;
            if (match_data_uri(s, n, i, 0, &ue)) {
                sb_add(&out, "[IMAGE]");
                i = ue;
                continue;
            }
        }
        sb_addc(&out, s[i]);
        i++;
    }
    char *r = sb_take(&out);
    sb_free(&out);
    return r;
}

json_t *w11_webtoolst_dae278_convert_base64_images(const char *text) {
    if (!text) return json_string("");
    char *p1 = replace_pass(text, 1);
    char *p2 = replace_pass(p1 ? p1 : "", 2);
    char *p3 = replace_pass(p2 ? p2 : "", 3);
    json_t *res = json_string(p3 ? p3 : "");
    free(p1); free(p2); free(p3);
    return res;
}

json_t *w11_webtoolst_dae278_trim_results(const json_t *results) {
    json_t *out = json_array();
    if (!json_is_array(results)) return out;
    for (size_t i = 0; i < json_len(results); i++) {
        json_t *e = json_get(results, i);
        if (!json_is_object(e)) { json_append(out, json_null()); continue; }
        json_t *o = json_object();
        const char *defaults[3] = {"url", "title", "content"};
        for (int k = 0; k < 3; k++) {
            json_t *v = json_obj_get(e, defaults[k]);
            json_set(o, defaults[k], v ? json_copy(v) : json_string(""));
        }
        json_t *v = json_obj_get(e, "error");
        json_set(o, "error", v ? json_copy(v) : json_null());
        v = json_obj_get(e, "blocked_by_policy");
        if (v) json_set(o, "blocked_by_policy", json_copy(v));
        json_append(out, o);
    }
    return out;
}
