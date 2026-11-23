/*
 * auth_xai.c — faithful C11 port of hermes_cli/auth_xai.py
 * (gate w7_authxai_4eac9a), deterministic helper subset.
 *
 * PoP: w7_authxai_clean @ hermes_cli/auth_xai.py:_clean
 * PoP: w7_authxai_token_pair @ hermes_cli/auth_xai.py:_token_pair
 * PoP: w7_authxai_state_has_usable_tokens @ hermes_cli/auth_xai.py:_xai_oauth_state_has_usable_tokens
 * PoP: w7_authxai_is_xai_origin_host @ hermes_cli/auth_xai.py:_is_xai_origin_host
 * PoP: w7_authxai_url_problem @ hermes_cli/auth_xai.py:_xai_url_problem
 * PoP: w7_authxai_jwt_exp @ hermes_cli/auth_xai.py:_xai_jwt_exp
 * PoP: w7_authxai_tokens_from_payload @ hermes_cli/auth_xai.py:_xai_tokens_from_payload
 * PoP: w7_authxai_validate_inference_base_url @ hermes_cli/auth_xai.py:_xai_validate_inference_base_url
 *
 * Network I/O, auth-store reads/writes and time-dependent refresh policy stay
 * in Python; the harness injects already-resolved facts for the pure logic.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- growable string buffer ---------------- */

typedef struct { char *p; size_t n, cap; } w7_buf;

static void w7_buf_reserve(w7_buf *b, size_t extra) {
    if (b->n + extra + 1 <= b->cap) return;
    size_t nc = (b->n + extra + 1) * 2;
    char *np = (char *)realloc(b->p, nc);
    if (!np) return;
    b->p = np; b->cap = nc;
}
static void w7_buf_add(w7_buf *b, const char *s) {
    if (!s) return;
    size_t m = strlen(s);
    w7_buf_reserve(b, m);
    if (!b->p || b->n + m + 1 > b->cap) return;
    memcpy(b->p + b->n, s, m);
    b->n += m;
    b->p[b->n] = '\0';
}
static void w7_buf_ch(w7_buf *b, char c) {
    w7_buf_reserve(b, 1);
    if (!b->p || b->n + 2 > b->cap) return;
    b->p[b->n++] = c;
    b->p[b->n] = '\0';
}
static char *w7_buf_take(w7_buf *b) {
    if (!b->p) { char *e = (char *)malloc(1); if (e) e[0] = '\0'; return e; }
    return b->p;
}

static int w7_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:  return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count != 0;
        case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

/* Python repr() of a JSON value (single-quoted strings, True/False/None). */
static void w7_repr_into(const json_t *v, w7_buf *b) {
    if (!v || v->type == JSON_NULL) { w7_buf_add(b, "None"); return; }
    switch (v->type) {
        case JSON_BOOL:
            w7_buf_add(b, v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER: {
            char tmp[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(tmp, sizeof(tmp), "%.0f", v->num_val);
            else
                snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
            w7_buf_add(b, tmp); return;
        }
        case JSON_STRING: {
            const char *s = v->str_val ? v->str_val : "";
            int has_single = strchr(s, '\'') != NULL;
            int has_double = strchr(s, '"') != NULL;
            char q = (has_single && !has_double) ? '"' : '\'';
            w7_buf_ch(b, q);
            for (const char *p = s; *p; p++) {
                if (*p == '\\' || *p == q) w7_buf_ch(b, '\\');
                if (*p == '\n') { w7_buf_add(b, "\\n"); continue; }
                if (*p == '\r') { w7_buf_add(b, "\\r"); continue; }
                if (*p == '\t') { w7_buf_add(b, "\\t"); continue; }
                w7_buf_ch(b, *p);
            }
            w7_buf_ch(b, q); return;
        }
        case JSON_ARRAY: {
            w7_buf_ch(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) w7_buf_add(b, ", ");
                w7_repr_into(v->c.items[i], b);
            }
            w7_buf_ch(b, ']'); return;
        }
        case JSON_OBJECT: {
            w7_buf_ch(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) w7_buf_add(b, ", ");
                const char *k = v->c.keys ? v->c.keys[i] : "";
                w7_buf_ch(b, '\'');
                for (const char *p = k; p && *p; p++) {
                    if (*p == '\\' || *p == '\'') w7_buf_ch(b, '\\');
                    w7_buf_ch(b, *p);
                }
                w7_buf_ch(b, '\'');
                w7_buf_add(b, ": ");
                w7_repr_into(v->c.items[i], b);
            }
            w7_buf_ch(b, '}'); return;
        }
        default: return;
    }
}

static char *w7_py_str(const json_t *v) {
    if (!w7_truthy(v)) return w7_buf_take(&(w7_buf){0});
    w7_buf b = {0};
    if (v->type == JSON_STRING) w7_buf_add(&b, v->str_val);
    else w7_repr_into(v, &b);
    return w7_buf_take(&b);
}

static char *w7_strip(const char *s) {
    if (!s) return (char *)calloc(1, 1);
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *p = (char *)malloc(n + 1);
    if (!p) return (char *)calloc(1, 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

json_t *w7_authxai_clean(const json_t *value) {
    char *raw = w7_py_str(value);
    char *s = w7_strip(raw);
    json_t *out = json_string(s);
    free(raw); free(s);
    return out;
}

/* ---------------- token pair / state ---------------- */

json_t *w7_authxai_token_pair(const json_t *tokens) {
    json_t *out = json_array();
    const char *fields[] = {"access_token", "refresh_token"};
    if (!tokens || tokens->type != JSON_OBJECT) {
        json_append(out, json_string(""));
        json_append(out, json_string(""));
        return out;
    }
    for (int i = 0; i < 2; i++) {
        json_t *v = json_obj_get(tokens, fields[i]);
        char *raw = w7_py_str(v);
        char *s = w7_strip(raw);
        json_append(out, json_string(s));
        free(raw); free(s);
    }
    return out;
}

json_t *w7_authxai_state_has_usable_tokens(const json_t *state) {
    if (!state || state->type != JSON_OBJECT) return json_bool(false);
    json_t *pair = w7_authxai_token_pair(json_obj_get(state, "tokens"));
    int ok = 1;
    for (size_t i = 0; i < pair->c.count; i++) {
        json_t *v = pair->c.items[i];
        if (!v->str_val || !v->str_val[0]) { ok = 0; break; }
    }
    json_free(pair);
    return json_bool(ok ? true : false);
}

/* ---------------- host / URL logic ---------------- */

json_t *w7_authxai_is_xai_origin_host(const char *host) {
    if (!host) return json_bool(false);
    size_t n = strlen(host);
    int ok = !strcmp(host, "x.ai") ||
             (n > 5 && !strcmp(host + n - 5, ".x.ai"));
    return json_bool(ok ? true : false);
}

static char *w7_lower_n(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    for (size_t i = 0; i < n; i++) p[i] = (char)tolower((unsigned char)s[i]);
    p[n] = '\0';
    return p;
}

/* urlparse subset: scheme + hostname, matching urllib.parse for http(s) URLs. */
static void w7_parse_url(const char *url, char **scheme_out, char **host_out) {
    *scheme_out = (char *)calloc(1, 1);
    *host_out = (char *)calloc(1, 1);
    if (!url || !*url) return;
    const char *p = url;
    /* scheme: leading [A-Za-z][A-Za-z0-9+.-]* followed by ':' */
    if (isalpha((unsigned char)*p)) {
        size_t i = 0;
        while (p[i] && (isalnum((unsigned char)p[i]) || p[i] == '+' || p[i] == '-' || p[i] == '.')) i++;
        if (p[i] == ':') {
            char *sc = w7_lower_n(p, i);
            free(*scheme_out);
            *scheme_out = sc;
            p += i + 1;
        }
    }
    /* netloc only after "//" */
    if (p[0] == '/' && p[1] == '/') {
        p += 2;
        const char *end = p;
        while (*end && *end != '/' && *end != '?' && *end != '#') end++;
        const char *auth = p;
        const char *at = NULL;
        for (const char *q = p; q < end; q++) if (*q == '@') at = q;
        if (at) auth = at + 1;
        const char *hb = auth, *he = end;
        if (*hb == '[') {
            hb++;
            const char *close = memchr(hb, ']', (size_t)(he - hb));
            if (close) he = close;
        } else {
            for (const char *q = auth; q < end; q++) {
                if (*q == ':') { he = q; break; }
            }
        }
        if (he > hb) {
            free(*host_out);
            *host_out = w7_lower_n(hb, (size_t)(he - hb));
        }
    }
}

json_t *w7_authxai_url_problem(const char *url) {
    json_t *out = json_array();
    char *scheme = NULL, *host = NULL;
    w7_parse_url(url, &scheme, &host);
    const char *problem = NULL;
    if (!scheme || strcmp(scheme, "https")) problem = "scheme";
    else if (!host || !host[0]) problem = "host";
    else {
        json_t *ok = w7_authxai_is_xai_origin_host(host);
        if (!ok->bool_val) problem = "origin";
        json_free(ok);
    }
    json_append(out, problem ? json_string(problem) : json_null());
    json_append(out, json_string(host ? host : ""));
    free(scheme); free(host);
    return out;
}

/* ---------------- JWT exp ---------------- */

static int w7_b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

/* base64.urlsafe_b64decode with the default non-validating alphabet. */
static char *w7_b64url_decode(const char *s, size_t *len_out) {
    size_t cap = strlen(s) * 3 / 4 + 4;
    char *out = (char *)malloc(cap + 1);
    if (!out) return NULL;
    size_t n = 0;
    int acc = 0, bits = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '=') break;
        int v = w7_b64val(*p);
        if (v < 0) continue;   /* Python b64decode(validate=False) skips junk */
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[n++] = (char)((acc >> bits) & 0xFF);
        }
    }
    out[n] = '\0';
    if (len_out) *len_out = n;
    return out;
}

json_t *w7_authxai_jwt_exp(const char *access_token) {
    if (!access_token || !strchr(access_token, '.')) return json_null();
    const char *first = strchr(access_token, '.');
    const char *second = strchr(first + 1, '.');
    size_t plen = second ? (size_t)(second - (first + 1)) : strlen(first + 1);
    char *b64 = (char *)malloc(plen + 5);
    if (!b64) return json_null();
    memcpy(b64, first + 1, plen);
    size_t pad = (4 - (plen % 4)) % 4;
    for (size_t i = 0; i < pad; i++) b64[plen + i] = '=';
    b64[plen + pad] = '\0';
    size_t rawlen = 0;
    char *raw = w7_b64url_decode(b64, &rawlen);
    free(b64);
    if (!raw) return json_null();
    json_t *payload = json_parse(raw, NULL);
    free(raw);
    if (!payload || payload->type != JSON_OBJECT) { json_free(payload); return json_null(); }
    json_t *exp = json_obj_get(payload, "exp");
    json_t *out = (exp && exp->type == JSON_NUMBER) ? json_number(exp->num_val) : json_null();
    json_free(payload);
    return out;
}

/* ---------------- token payload ---------------- */

json_t *w7_authxai_tokens_from_payload(const json_t *payload,
        const json_t *access_token, const char *fallback_refresh) {
    json_t *out = json_object();
    json_set(out, "access_token", json_copy(access_token));

    json_t *refresh = payload ? json_obj_get(payload, "refresh_token") : NULL;
    if (w7_truthy(refresh)) {
        char *raw = w7_py_str(refresh);
        char *s = w7_strip(raw);
        json_set(out, "refresh_token", json_string(s));
        free(raw); free(s);
    } else {
        json_set(out, "refresh_token", json_string(fallback_refresh ? fallback_refresh : ""));
    }

    json_t *idtok = payload ? json_obj_get(payload, "id_token") : NULL;
    json_t *clean_id = w7_authxai_clean(idtok);
    json_set(out, "id_token", json_copy(clean_id));
    json_free(clean_id);

    json_t *expires = payload ? json_obj_get(payload, "expires_in") : NULL;
    json_set(out, "expires_in", expires ? json_copy(expires) : json_null());

    json_t *tt = payload ? json_obj_get(payload, "token_type") : NULL;
    json_t *clean_tt = w7_authxai_clean(w7_truthy(tt) ? tt : json_string("Bearer"));
    if (!clean_tt->str_val || !clean_tt->str_val[0]) {
        json_free(clean_tt);
        clean_tt = json_string("Bearer");
    }
    json_set(out, "token_type", json_copy(clean_tt));
    json_free(clean_tt);
    return out;
}

json_t *w7_authxai_validate_inference_base_url(const char *value, const char *fallback) {
    const char *src = value ? value : "";
    char *stripped = w7_strip(src);
    size_t n = strlen(stripped);
    while (n > 0 && stripped[n - 1] == '/') stripped[--n] = '\0';
    if (n == 0) { free(stripped); return json_string(fallback ? fallback : ""); }
    json_t *problem = w7_authxai_url_problem(stripped);
    int bad = problem->c.items[0]->type != JSON_NULL;
    json_free(problem);
    json_t *out = json_string(bad ? (fallback ? fallback : "") : stripped);
    free(stripped);
    return out;
}
