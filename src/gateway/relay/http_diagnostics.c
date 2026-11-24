/*
 * http_diagnostics.c - C11 port of upstream gateway/relay/http_diagnostics.py
 * (gate w24_httpdiagno_5c2dc9).
 *
 * PoP: w24_httpdiagno_5c2dc9_connector_reason @ gateway/relay/http_diagnostics.py:connector_reason
 * PoP: w24_httpdiagno_5c2dc9_answered_by_proxy @ gateway/relay/http_diagnostics.py:answered_by_proxy
 * PoP: w24_httpdiagno_5c2dc9_scrub @ gateway/relay/http_diagnostics.py:_scrub
 * PoP: w24_httpdiagno_5c2dc9_constants @ gateway/relay/http_diagnostics.py:<header tuples, BODY_EXCERPT_CHARS>
 *
 * Ported: who answered a non-2xx (connector refusal vs proxy page), read from
 * the response body and headers alone.  The seams are the urllib.error.HTTPError
 * object, exc.read() and describe_http_error's generic redactor
 * (agent.redact.redact_sensitive_text, ~2k lines of vendor patterns): the body
 * arrives here already read, and the exact-credential scrub -- the part whose
 * result a value decides -- is ported.  So read_error_body and
 * describe_http_error are not ported; connector_reason, answered_by_proxy and
 * _scrub are.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "noc_w24_httpdiagno_5c2dc9.h"

/* Response headers that name the hop that answered. */
static const char *const HD_ORIGIN_HEADERS[W24_HTTPDIAGNO_5C2DC9_ORIGIN_HEADERS] = {
    "Server", "Via", "Proxy-Status", "X-Squid-Error",
};
/* Headers only an intermediary sets (the connector sets none of them). */
static const char *const HD_PROXY_ONLY_HEADERS[W24_HTTPDIAGNO_5C2DC9_PROXY_ONLY_HEADERS] = {
    "Via", "Proxy-Status", "X-Squid-Error",
};
/* Server values naming an egress/CDN/mesh proxy that can answer on its own
 * with a JSON {"error"} body.  A hosting edge's own name is not here. */
static const char *const HD_PROXY_SERVER_NAMES[W24_HTTPDIAGNO_5C2DC9_PROXY_SERVER_NAMES] = {
    "squid", "envoy", "cloudflare", "zscaler", "bluecoat", "proxysg", "netskope", "forcepoint",
};
#define HD_BODY_EXCERPT_CHARS 200
#define HD_BODY_READ_LIMIT 4096

static int hd_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() over the whitespace a body can carry. */
static char *hd_strip(const char *s) {
    size_t a = 0, b, n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    b = n;
    while (a < b && hd_is_space(s[a])) a++;
    while (b > a && hd_is_space(s[b - 1])) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* HTTPMessage.get is case-insensitive; a plain mapping is not.  Match the
 * first key equal to ``name`` ignoring ASCII case, and report whether it was
 * present at all (presence != truthiness, as in the live any(...get(...))). */
static const json_t *hd_header(const json_t *headers, const char *name, int *present) {
    size_t i, n;
    *present = 0;
    if (!headers || headers->type != JSON_OBJECT) return NULL;
    n = json_object_size(headers);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(headers, i);
        const char *a = k, *b = name;
        int same = 1;
        if (!k) continue;
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a - 'A' + 'a') : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b - 'A' + 'a') : *b;
            if (ca != cb) { same = 0; break; }
            a++; b++;
        }
        if (!same || *a || *b) continue;
        *present = 1;
        return json_object_get_at(headers, i);
    }
    return NULL;
}

static int hd_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str(v) for a header value. */
static void hd_text(const json_t *v, char *buf, size_t cap) {
    if (cap == 0) return;
    buf[0] = '\0';
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
    case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
    case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val &&
            v->num_val > -1e16 && v->num_val < 1e16)
            snprintf(buf, cap, "%lld", (long long)v->num_val);
        else
            snprintf(buf, cap, "%.17g", v->num_val);
        return;
    default: {
        char *s = json_dumps(v, 0);
        snprintf(buf, cap, "%s", s ? s : "");
        free(s);
        return;
    }
    }
}

json_t *w24_httpdiagno_5c2dc9_connector_reason(const json_t *body) {
    const char *text = (body && body->type == JSON_STRING && body->str_val) ? body->str_val : "";
    json_t *parsed = json_parse(text, NULL);
    const json_t *reason;
    json_t *out;
    char *stripped;

    if (!parsed) return json_string("");
    if (parsed->type != JSON_OBJECT) { json_free(parsed); return json_string(""); }
    reason = json_obj_get(parsed, "error");
    if (!reason || reason->type != JSON_STRING) { json_free(parsed); return json_string(""); }
    stripped = hd_strip(reason->str_val);
    out = json_string(stripped ? stripped : "");
    free(stripped);
    json_free(parsed);
    return out;
}

json_t *w24_httpdiagno_5c2dc9_answered_by_proxy(const json_t *headers) {
    size_t i;
    char server[1024];
    const json_t *v;
    int present;

    if (!headers || headers->type != JSON_OBJECT) return json_bool(false);
    for (i = 0; i < W24_HTTPDIAGNO_5C2DC9_PROXY_ONLY_HEADERS; i++) {
        v = hd_header(headers, HD_PROXY_ONLY_HEADERS[i], &present);
        if (present && hd_truthy(v)) return json_bool(true);
    }
    v = hd_header(headers, "Server", &present);
    if (!present) return json_bool(false);
    /* str(headers.get("Server") or "").lower() */
    if (!hd_truthy(v)) server[0] = '\0';
    else hd_text(v, server, sizeof(server));
    for (i = 0; server[i]; i++)
        if (server[i] >= 'A' && server[i] <= 'Z') server[i] = (char)(server[i] - 'A' + 'a');
    for (i = 0; i < W24_HTTPDIAGNO_5C2DC9_PROXY_SERVER_NAMES; i++)
        if (strstr(server, HD_PROXY_SERVER_NAMES[i])) return json_bool(true);
    return json_bool(false);
}

/* text.replace(secret, "***") over every occurrence. */
static char *hd_replace_all(const char *text, const char *secret) {
    size_t tl = strlen(text), sl = strlen(secret), cap, o = 0;
    const char *p = text, *hit;
    char *out;

    if (sl == 0) {
        out = (char *)malloc(tl + 1);
        if (out) memcpy(out, text, tl + 1);
        return out;
    }
    cap = tl * 3 + 4;
    out = (char *)malloc(cap);
    if (!out) return NULL;
    while ((hit = strstr(p, secret)) != NULL) {
        size_t seg = (size_t)(hit - p), need = o + seg + 4;
        if (need > cap) {
            char *grown;
            cap = need * 2;
            grown = (char *)realloc(out, cap);
            if (!grown) { free(out); return NULL; }
            out = grown;
        }
        memcpy(out + o, p, seg);
        o += seg;
        memcpy(out + o, "***", 3);
        o += 3;
        p = hit + sl;
    }
    {
        size_t seg = strlen(p), need = o + seg + 1;
        if (need > cap) {
            char *grown = (char *)realloc(out, need);
            if (!grown) { free(out); return NULL; }
            out = grown;
        }
        memcpy(out + o, p, seg);
        o += seg;
        out[o] = '\0';
    }
    return out;
}

json_t *w24_httpdiagno_5c2dc9_scrub(const json_t *text, const json_t *secrets) {
    const char *cur = (text && text->type == JSON_STRING && text->str_val) ? text->str_val : "";
    char *work = (char *)malloc(strlen(cur) + 1);
    size_t i, n = (secrets && secrets->type == JSON_ARRAY) ? json_len(secrets) : 0;
    json_t *out;

    if (!work) return json_string("");
    strcpy(work, cur);
    for (i = 0; i < n; i++) {
        const json_t *s = json_get(secrets, i);
        char *next;
        if (!s || s->type != JSON_STRING || !s->str_val || s->str_val[0] == '\0') continue;
        next = hd_replace_all(work, s->str_val);
        if (!next) continue;
        free(work);
        work = next;
    }
    out = json_string(work);
    free(work);
    return out;
}

static json_t *hd_str_array(const char *const *items, size_t n) {
    json_t *a = json_array();
    size_t i;
    for (i = 0; i < n; i++) json_append(a, json_string(items[i]));
    return a;
}

json_t *w24_httpdiagno_5c2dc9_constants(void) {
    json_t *o = json_object();
    json_set(o, "origin_headers",
             hd_str_array(HD_ORIGIN_HEADERS, W24_HTTPDIAGNO_5C2DC9_ORIGIN_HEADERS));
    json_set(o, "proxy_only_headers",
             hd_str_array(HD_PROXY_ONLY_HEADERS, W24_HTTPDIAGNO_5C2DC9_PROXY_ONLY_HEADERS));
    json_set(o, "proxy_server_names",
             hd_str_array(HD_PROXY_SERVER_NAMES, W24_HTTPDIAGNO_5C2DC9_PROXY_SERVER_NAMES));
    json_set(o, "body_excerpt_chars", json_number((double)HD_BODY_EXCERPT_CHARS));
    json_set(o, "body_read_limit", json_number((double)HD_BODY_READ_LIMIT));
    return o;
}
