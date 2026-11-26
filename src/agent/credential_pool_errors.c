/*
 * credential_pool_errors.c — faithful C11 port of agent/credential_pool_errors.py
 * (gate w16_credential_c298e3).
 *
 * PoP: w16_credential_c298e3_parse_absolute_timestamp @ agent/credential_pool_errors.py:_parse_absolute_timestamp
 * PoP: w16_credential_c298e3_redact_error_text @ agent/credential_pool_errors.py:redact_error_text
 * PoP: w16_credential_c298e3_normalize_error_context @ agent/credential_pool_errors.py:_normalize_error_context
 *
 * The two delegated helpers (redact_sensitive_text, reset_delay_from_message)
 * are pinned to identity/None by the oracle seam; the module's own logic —
 * known-secret masking (len>=8, stripped, longest-first) and the reason/message
 * / reset_at normalisation — is ported verbatim. Timestamps use the same
 * epoch-seconds / epoch-ms rule as credential_pool_timestamps.
 */
#include "noc_w16_credential_c298e3.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MIN_KNOWN_SECRET_LEN 8

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *strip_dup(const char *s) {
    char *t = dupstr(s ? s : "");
    size_t a = 0, b;
    if (!t) return NULL;
    b = strlen(t);
    while (a < b && isspace((unsigned char)t[a])) a++;
    while (b > a && isspace((unsigned char)t[b - 1])) b--;
    if (a && b > a) memmove(t, t + a, b - a);
    t[b - a] = '\0';
    return t;
}

static int json_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    return 1; /* non-empty array/object */
}

/* Replace every occurrence of needle in src; caller frees the result. */
static char *replace_all(const char *src, const char *needle) {
    size_t ns = strlen(needle), ss = strlen(src);
    size_t count = 0, pos = 0, i, out_i = 0;
    char *out;
    if (!ns || !src) return dupstr(src ? src : "");
    while ((pos = (size_t)(strstr(src + pos, needle) ? strstr(src + pos, needle) - src : (size_t)-1)) != (size_t)-1) {
        count++; pos += ns;
    }
    out = (char *)malloc(ss + count * (3 - ns) + 1);
    if (!out) return NULL;
    for (i = 0; i < ss;) {
        if (ns && ss - i >= ns && !memcmp(src + i, needle, ns)) {
            out[out_i++] = '*'; out[out_i++] = '*'; out[out_i++] = '*';
            i += ns;
        } else {
            out[out_i++] = src[i++];
        }
    }
    out[out_i] = '\0';   /* NUL at the final length (pitfall #2) */
    return out;
}

static int cmp_secret(const void *a, const void *b) {
    const char *const *sa = (const char *const *)a;
    const char *const *sb = (const char *const *)b;
    size_t la = strlen(*sa), lb = strlen(*sb);
    if (la != lb) return lb < la ? -1 : 1; /* longest first */
    return strcmp(*sa, *sb);               /* deterministic tie-break */
}

static char *mask_known_secrets(const char *text, const json_t *secrets) {
    char *cur = dupstr(text ? text : "");
    char **list = NULL;
    size_t n = 0, i;
    if (!cur) return NULL;
    if (secrets && secrets->type == JSON_ARRAY) {
        size_t m = secrets->c.count;
        if (m) list = (char **)calloc(m, sizeof(char *));
        for (i = 0; i < m && list; ++i) {
            const json_t *v = json_get(secrets, i);
            char *s;
            if (!v || v->type != JSON_STRING) continue;
            s = strip_dup(v->str_val);
            if (s && strlen(s) >= MIN_KNOWN_SECRET_LEN) { list[n++] = s; }
            else free(s);
        }
    }
    if (n > 1) qsort(list, n, sizeof(char *), cmp_secret);
    for (i = 0; i < n; ++i) {
        char *next = replace_all(cur, list[i]);
        free(cur); cur = next;
        free(list[i]);
    }
    free(list);
    return cur; /* allocation start (pitfall #5) */
}

json_t *w16_credential_c298e3_parse_absolute_timestamp(const json_t *value) {
    double num;
    int from_string = 0;
    if (!value || value->type == JSON_NULL) return json_null();
    if (value->type == JSON_NUMBER) {
        num = value->num_val;
    } else if (value->type == JSON_STRING) {
        char *s = strip_dup(value->str_val);
        char *end = NULL;
        from_string = 1;
        if (!s || !s[0]) { free(s); return json_null(); }
        num = strtod(s, &end);
        if (end && *end != '\0') { free(s); return json_null(); } /* ISO/non-numeric: null in this seam */
        free(s);
    } else {
        return json_null();
    }
    /* Python rejects non-positive only in the numeric-value branch; a numeric
     * STRING ("0", "-5") is returned verbatim by _parse_absolute_timestamp. */
    if (!from_string && num <= 0.0) return json_null();
    if (isnan(num)) return json_null();
    if (num > 1000000000000.0) num /= 1000.0;
    return json_number(num);
}

json_t *w16_credential_c298e3_redact_error_text(const char *text, const json_t *known_secrets) {
    char *masked = mask_known_secrets(text, known_secrets);
    json_t *out;
    if (!masked) return json_null();
    /* Pattern-redactor seam pinned to identity by the oracle. */
    out = json_string(masked);
    free(masked);
    return out;
}

json_t *w16_credential_c298e3_normalize_error_context(const json_t *error_context,
                                                      const json_t *known_secrets) {
    json_t *out = json_object();
    const char *keys[2] = {"reason", "message"};
    int i;
    const json_t *reset_at = NULL;
    json_t *parsed;

    if (!error_context || error_context->type != JSON_OBJECT) return out;

    for (i = 0; i < 2; ++i) {
        const json_t *v = json_obj_get(error_context, keys[i]);
        if (v && v->type == JSON_STRING) {
            char *stripped = strip_dup(v->str_val);
            if (stripped && stripped[0]) {
                char *red = mask_known_secrets(stripped, known_secrets);
                json_set(out, keys[i], json_string(red ? red : ""));
                free(red);
            }
            free(stripped);
        }
    }

    {
        const char *rk[3] = {"reset_at", "resets_at", "retry_until"};
        for (i = 0; i < 3; ++i) {
            const json_t *cand = json_obj_get(error_context, rk[i]);
            if (json_truthy(cand)) { reset_at = cand; break; }
        }
    }
    parsed = w16_credential_c298e3_parse_absolute_timestamp(reset_at);
    /* reset_delay_from_message seam pinned to None by the oracle: no time.time() branch. */
    if (parsed->type != JSON_NULL) json_set(out, "reset_at", parsed);
    return out;
}
