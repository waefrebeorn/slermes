/*
 * git_credentials.c — C port of upstream Hermes hermes_cli/git_credentials.py.
 *
 * PoP: w13_gitcredent_677b0e_https_origin @ hermes_cli/git_credentials.py:_https_origin
 * PoP: w13_gitcredent_677b0e_token_pair @ hermes_cli/git_credentials.py:_token_pair
 * PoP: w13_gitcredent_677b0e_auth_headers @ hermes_cli/git_credentials.py:_auth_headers
 * PoP: w13_gitcredent_677b0e_credential_required @ hermes_cli/git_credentials.py:is_credential_required_error
 * PoP: w13_gitcredent_677b0e_with_git_auth @ hermes_cli/git_credentials.py:with_git_auth
 *
 * Pure, subprocess-free seams. Every exported symbol is prefixed with the gate name.
 */
#include "noc_w13_gitcredent_677b0e.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static long long count_arg(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING && v->str_val && v->str_val[0])
        return strtoll(v->str_val, NULL, 10);
    return 0;
}

char *w13_gitcredent_677b0e_https_origin(const char *url) {
    if (!url) return NULL;
    const char *p = strstr(url, "://");
    if (!p || (size_t)(p - url) != 5 || strncasecmp(url, "https", 5) != 0) return NULL;
    const char *rest = p + 3;
    const char *end = rest;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    char auth[512];
    size_t n = (size_t)(end - rest);
    if (n >= sizeof(auth)) n = sizeof(auth) - 1;
    memcpy(auth, rest, n);
    auth[n] = '\0';
    char *at = strrchr(auth, '@');
    char *hostport = at ? at + 1 : auth;
    char host[256] = {0}, port[32] = {0};
    if (*hostport == '[') {
        char *rb = strchr(hostport, ']');
        if (!rb) return NULL;
        size_t hl = (size_t)(rb - (hostport + 1));
        if (hl >= sizeof(host)) hl = sizeof(host) - 1;
        memcpy(host, hostport + 1, hl);
        host[hl] = '\0';
        char *after = rb + 1;
        if (*after == ':') {
            strncpy(port, after + 1, sizeof(port) - 1);
            port[sizeof(port) - 1] = '\0';
        }
    } else {
        char *colon = strrchr(hostport, ':');
        if (colon) {
            size_t hl = (size_t)(colon - hostport);
            if (hl >= sizeof(host)) hl = sizeof(host) - 1;
            memcpy(host, hostport, hl);
            host[hl] = '\0';
            strncpy(port, colon + 1, sizeof(port) - 1);
            port[sizeof(port) - 1] = '\0';
        } else {
            strncpy(host, hostport, sizeof(host) - 1);
            host[sizeof(host) - 1] = '\0';
        }
    }
    if (!host[0]) return NULL;
    for (char *q = host; *q; q++) *q = (char)tolower((unsigned char)*q);
    char out[640];
    if (port[0]) {
        char *e = NULL;
        long pv = strtol(port, &e, 10);
        if (e && *e) return NULL;
        if (strchr(host, ':'))
            snprintf(out, sizeof(out), "https://[%s]:%ld", host, pv);
        else
            snprintf(out, sizeof(out), "https://%s:%ld", host, pv);
    } else {
        if (strchr(host, ':'))
            snprintf(out, sizeof(out), "https://[%s]", host);
        else
            snprintf(out, sizeof(out), "https://%s", host);
    }
    return strdup(out);
}

json_t *w13_gitcredent_677b0e_token_pair(const json_t *token) {
    if (!json_truthy(token)) return json_null();
    const char *s = jstr(token);
    if (!s) return json_null();
    json_t *a = json_array();
    json_append(a, json_string("x-access-token"));
    json_append(a, json_string(s));
    return a;
}

static int cmp_strptr(const void *a, const void *b) {
    const char *const *x = (const char *const *)a;
    const char *const *y = (const char *const *)b;
    return strcmp(*x, *y);
}

json_t *w13_gitcredent_677b0e_auth_headers(const json_t *env) {
    json_t *out = json_array();
    if (!env || env->type != JSON_OBJECT) return out;
    char **vals = NULL;
    size_t n = 0, cap = 0;
    for (size_t i = 0; i < json_object_size(env); i++) {
        const char *k = json_object_get_key_at(env, i);
        json_t *v = json_object_get_at(env, i);
        const char *s = jstr(v);
        if (!k || !s) continue;
        if (strncmp(k, "GIT_CONFIG_VALUE_", 17) != 0) continue;
        if (strncmp(s, "Authorization:", 13) != 0) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 8;
            char **tmp = (char **)realloc(vals, cap * sizeof(char *));
            if (!tmp) break;
            vals = tmp;
        }
        vals[n++] = strdup(s);
    }
    if (n > 1) qsort(vals, n, sizeof(char *), cmp_strptr);
    for (size_t i = 0; i < n; i++) {
        json_append(out, json_string(vals[i]));
        free(vals[i]);
    }
    free(vals);
    return out;
}

static int ci_has(const char *hay, const char *needle) {
    size_t hl = strlen(hay), nl = strlen(needle);
    if (nl > hl) return 0;
    for (size_t i = 0; i + nl <= hl; i++) {
        size_t j = 0;
        while (j < nl && tolower((unsigned char)hay[i + j]) == tolower((unsigned char)needle[j])) j++;
        if (j == nl) return 1;
    }
    return 0;
}

/* (?:error|status|http)[: ]+40[13]\b */
static int http_401_403(const char *text) {
    const char *words[] = {"error", "status", "http"};
    size_t tl = strlen(text);
    for (size_t i = 0; text[i]; i++) {
        for (size_t w = 0; w < 3; w++) {
            size_t wl = strlen(words[w]);
            if (i + wl > tl) continue;
            int ok = 1;
            for (size_t j = 0; j < wl; j++)
                if (tolower((unsigned char)text[i + j]) != words[w][j]) { ok = 0; break; }
            if (!ok) continue;
            size_t k = i + wl;
            size_t seps = 0;
            while (text[k] == ':' || text[k] == ' ') { k++; seps++; }
            if (!seps) continue;
            if (text[k] != '4' || text[k + 1] != '0' || (text[k + 2] != '1' && text[k + 2] != '3')) continue;
            char nb = text[k + 3];
            if (!(nb == '\0' || !(isalnum((unsigned char)nb) || nb == '_'))) continue;
            return 1;
        }
    }
    return 0;
}

json_t *w13_gitcredent_677b0e_credential_required(const struct json_t *err, const struct json_t *out) {
    const char *a = jstr(err);
    const char *b = jstr(out);
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    char *buf = (char *)malloc(la + lb + 2);
    if (!buf) return json_bool(0);
    memcpy(buf, a, la);
    buf[la] = '\n';
    memcpy(buf + la + 1, b, lb + 1);
    int hit = ci_has(buf, "could not read username") ||
              ci_has(buf, "could not read password") ||
              ci_has(buf, "terminal prompts disabled") ||
              ci_has(buf, "authentication failed") ||
              http_401_403(buf);
    free(buf);
    return json_bool(hit ? 1 : 0);
}

/* RFC 4648 base64 (Python base64.b64encode). */
static char *b64_encode(const char *in) {
    static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t n = strlen(in);
    size_t out_n = ((n + 2) / 3) * 4;
    char *out = (char *)malloc(out_n + 1);
    if (!out) return NULL;
    size_t i = 0, j = 0;
    while (i + 2 < n) {
        unsigned v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8) | (unsigned char)in[i + 2];
        out[j++] = T[(v >> 18) & 63]; out[j++] = T[(v >> 12) & 63];
        out[j++] = T[(v >> 6) & 63];  out[j++] = T[v & 63];
        i += 3;
    }
    if (n - i == 1) {
        unsigned v = (unsigned char)in[i] << 16;
        out[j++] = T[(v >> 18) & 63]; out[j++] = T[(v >> 12) & 63];
        out[j++] = '='; out[j++] = '=';
    } else if (n - i == 2) {
        unsigned v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8);
        out[j++] = T[(v >> 18) & 63]; out[j++] = T[(v >> 12) & 63];
        out[j++] = T[(v >> 6) & 63];  out[j++] = '=';
    }
    out[j] = '\0';
    return out;
}

json_t *w13_gitcredent_677b0e_with_git_auth(const struct json_t *env,
                                            const struct json_t *url,
                                            const struct json_t *auth) {
    json_t *res = json_object();
    if (env && env->type == JSON_OBJECT)
        for (size_t i = 0; i < json_object_size(env); i++)
            json_set(res, json_object_get_key_at(env, i), json_copy(json_object_get_at(env, i)));
    const char *u = jstr(url);
    char *origin = w13_gitcredent_677b0e_https_origin(u);
    if (!origin) return res;
    if (!auth || auth->type != JSON_ARRAY || auth->c.count < 2) { free(origin); return res; }
    const char *user = jstr(json_get(auth, 0));
    const char *pass = jstr(json_get(auth, 1));
    if (!user || !pass) { free(origin); return res; }
    size_t ulen = strlen(user), plen = strlen(pass);
    char *cred = (char *)malloc(ulen + plen + 2);
    if (!cred) { free(origin); return res; }
    memcpy(cred, user, ulen);
    cred[ulen] = ':';
    memcpy(cred + ulen + 1, pass, plen + 1);
    char *encoded = b64_encode(cred);
    free(cred);
    if (!encoded) { free(origin); return res; }
    long long idx = count_arg(json_obj_get(env, "GIT_CONFIG_COUNT"));
    char kk[64], kv[64], cnt[32], hdr[640], val[1024];
    snprintf(kk, sizeof(kk), "GIT_CONFIG_KEY_%lld", idx);
    snprintf(kv, sizeof(kv), "GIT_CONFIG_VALUE_%lld", idx);
    snprintf(cnt, sizeof(cnt), "%lld", idx + 1);
    snprintf(hdr, sizeof(hdr), "http.%s/.extraheader", origin);
    snprintf(val, sizeof(val), "Authorization: basic %s", encoded);
    free(encoded);
    json_set(res, kk, json_string(hdr));
    json_set(res, kv, json_string(val));
    json_set(res, "GIT_CONFIG_COUNT", json_string(cnt));
    free(origin);
    return res;
}
