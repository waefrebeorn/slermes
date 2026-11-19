/*
 * port_hermes_auth_nous.c — Port of the pure core of hermes_cli/auth_nous.py.
 */

#include "port_hermes_auth_nous.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "crypto.h"

static char *dup_(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

/* PoP: authn_token_fingerprint @ hermes_cli/auth_nous.py:_token_fingerprint */
char *authn_token_fingerprint(const char *token)
{
    if (!token)
        return NULL;
    /* strip() then sha256 hexdigest[:12]; empty -> None */
    size_t len = strlen(token), s = 0, e = len;
    while (s < e && isspace((unsigned char)token[s])) s++;
    while (e > s && isspace((unsigned char)token[e - 1])) e--;
    if (s == e)
        return NULL;
    unsigned char hash[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)token + s, e - s, hash);
    char *out = (char *)malloc(13);
    if (!out) return NULL;
    /* hexdigest()[:12] = first 12 hex chars = first 6 bytes */
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 6; i++) {
        out[2 * i] = hx[hash[i] >> 4];
        out[2 * i + 1] = hx[hash[i] & 0xF];
    }
    out[12] = '\0';
    return out;
}

/* PoP: authn_iso_after @ hermes_cli/auth_nous.py:_iso_after */
char *authn_iso_after(double now_epoch, long long ttl_seconds)
{
    double at = now_epoch + (double)ttl_seconds;
    time_t secs = (time_t)(long long)at;
    struct tm tc;
    gmtime_s(&tc, &secs);
    double frac = at - floor(at);
    int micros = (int)(frac * 1000000.0 + 0.5);
    if (micros >= 1000000) { micros -= 1000000; secs += 1; gmtime_s(&tc, &secs); }
    char *out = (char *)malloc(40);
    if (!out) return NULL;
    /* datetime.isoformat() omits the fraction when it is zero */
    if (micros != 0) {
        snprintf(out, 40, "%04d-%02d-%02dT%02d:%02d:%02d.%06d+00:00",
                 tc.tm_year + 1900, tc.tm_mon + 1, tc.tm_mday, tc.tm_hour, tc.tm_min,
                 tc.tm_sec, micros);
    } else {
        snprintf(out, 40, "%04d-%02d-%02dT%02d:%02d:%02d+00:00",
                 tc.tm_year + 1900, tc.tm_mon + 1, tc.tm_mday, tc.tm_hour, tc.tm_min,
                 tc.tm_sec);
    }
    return out;
}

static int is_allowed_host(const char *host)
{
    static const char *const ALLOWED[] = {
        "inference-api.nousresearch.com", "welcome-api.nousresearch.com",
    };
    for (size_t i = 0; i < sizeof(ALLOWED) / sizeof(ALLOWED[0]); i++)
        if (strcmp(host, ALLOWED[i]) == 0)
            return 1;
    return 0;
}

/* urlparse(url).hostname — scheme-stripped, lowercased, port dropped. malloc'd
 * (caller frees); returns "" for no host. */
static char *hostname_of(const char *url)
{
    if (!url) return dup_("");
    const char *p = url;
    const char *scheme = strstr(p, "://");
    if (scheme)
        p = scheme + 3;
    size_t n = 0;
    while (p[n] && p[n] != '/' && p[n] != '?' && p[n] != '#' && p[n] != ':') n++;
    /* the ':' port is excluded above (hostname stops at ':') */
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)p[i]);
    o[n] = '\0';
    return o;
}

/* PoP: authn_inference_host_allowed @ hermes_cli/auth_nous.py:_nous_inference_host_allowed */
int authn_inference_host_allowed(const char *hostname, const char *override_url)
{
    if (hostname && is_allowed_host(hostname))
        return 1;
    if (!hostname || !hostname[0])
        return 0;
    if (override_url) {
        char *ovh = hostname_of(override_url);
        int eq = ovh && strcmp(ovh, hostname) == 0;
        free(ovh);
        return eq;
    }
    return 0;
}

static int json_value_is_array_or_has_items(const json_t *v) { return 0; }

static void scope_add(json_t *arr, char *tok)
{
    /* dedupe (Python set) keeping first-seen order */
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *e = arr->c.items[i];
        if (e && e->type == JSON_STRING && strcmp(e->str_val, tok) == 0)
            return;
    }
    json_append(arr, json_string(tok));
}

/* recurse one value's tokens into arr */
static void scope_collect(json_t *arr, const json_t *value)
{
    if (!value)
        return;
    if (value->type == JSON_STRING) {
        const char *raw = value->str_val ? value->str_val : "";
        size_t len = strlen(raw);
        char *buf = (char *)malloc(len + 1);
        if (!buf) return;
        memcpy(buf, raw, len + 1);
        /* replace "," with " " then whitespace-tokenize, keep non-stripped-empty */
        char *it = buf;
        while (*it) {
            if (*it == ',') *it = ' ';
            it++;
        }
        it = buf;
        while (*it) {
            while (*it && isspace((unsigned char)*it)) it++;
            if (!*it) break;
            char *start = it;
            while (*it && !isspace((unsigned char)*it)) it++;
            size_t tlen = (size_t)(it - start);
            /* include only if the token strips to non-empty */
            size_t a = 0, b = tlen;
            while (a < b && isspace((unsigned char)start[a])) a++;
            while (b > a && isspace((unsigned char)start[b - 1])) b--;
            if (b == 0) continue;
            char *tok = (char *)malloc(tlen + 1);
            if (!tok) continue;
            memcpy(tok, start, tlen);
            tok[tlen] = '\0';
            scope_add(arr, tok);
            free(tok);
        }
        free(buf);
    } else if (value->type == JSON_ARRAY) {
        for (size_t i = 0; i < value->c.count; i++) {
            json_t *item = value->c.items[i];
            if (item && item->type == JSON_STRING)
                scope_collect(arr, item);
        }
    }
    (void)json_value_is_array_or_has_items;
}

/* PoP: authn_scope_values @ hermes_cli/auth_nous.py:_scope_values */
json_t *authn_scope_values(const char *value_json)
{
    json_t *value = value_json ? json_parse(value_json, NULL) : NULL;
    json_t *arr = json_array();
    if (!value || (value->type != JSON_STRING && value->type != JSON_ARRAY)) {
        if (value) json_free(value);
        return arr;
    }
    scope_collect(arr, value);
    json_free(value);
    return arr;
}

/* PoP: authn_unusable_invoke_jwt_error @ hermes_cli/auth_nous.py:_unusable_invoke_jwt_error */
char *authn_unusable_invoke_jwt_error(const char *reason, int no_refresh_token,
                                      const char **code_out)
{
    static const char *RELOGIN = "Re-authenticate with: hermes auth add nous";
    if (code_out)
        *code_out = no_refresh_token ? "nous_auth_missing_refresh_token" : (reason ? reason : "");
    const char *detail = no_refresh_token ? " and no refresh token is available" : "";
    const char *r = reason ? reason : "";
    size_t n = strlen("Nous Portal access token is not a usable inference JWT (")
               + strlen(r) + strlen(detail) + strlen("). ") + strlen(RELOGIN) + 1;
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "Nous Portal access token is not a usable inference JWT (%s)%s. %s",
             r, detail, RELOGIN);
    return out;
}

/* PoP: authn_migrate_stale_portal_url @ hermes_cli/auth_nous.py:_migrate_stale_nous_portal_url */
json_t *authn_migrate_stale_portal_url(const char *providers_json)
{
    json_t *providers = providers_json ? json_parse(providers_json, NULL) : NULL;
    if (!providers || providers->type != JSON_OBJECT) {
        if (providers) json_free(providers);
        return providers ? providers : json_object();
    }
    json_t *nous = json_obj_get(providers, "nous");
    if (nous && nous->type == JSON_OBJECT) {
        const char *stored = json_get_str(nous, "portal_base_url", NULL);
        if (stored && *stored) {
            size_t len = strlen(stored), e2 = len;
            while (e2 > 0 && isspace((unsigned char)stored[e2 - 1])) e2--;
            /* we do not need to re-strip leading; hostname_of lowercases */
            char *sub = (char *)malloc(e2 + 1);
            if (sub) {
                memcpy(sub, stored, e2);
                sub[e2] = '\0';
                char *host = hostname_of(sub);
                free(sub);
                if (host && strcmp(host, "api.nousresearch.com") == 0)
                    json_set(nous, "portal_base_url", json_string(NOUS_DEFAULT_PORTAL_URL));
                free(host);
            }
        }
    }
    return providers;
}

/* PoP: authn_effective_provider_state @ hermes_cli/auth_nous.py:_nous_effective_provider_state */
json_t *authn_effective_provider_state(const char *state_json)
{
    json_t *state = state_json ? json_parse(state_json, NULL) : NULL;
    json_t *out = json_object();
    if (!state || state->type != JSON_OBJECT) {
        if (state) json_free(state);
        return out;
    }
    static const char *const IGNORED[] = { "expires_in", "agent_key_expires_in" };
    for (size_t i = 0; i < state->c.count; i++) {
        const char *k = state->c.keys[i];
        int skip = 0;
        for (size_t j = 0; j < sizeof(IGNORED) / sizeof(IGNORED[0]); j++)
            if (strcmp(k, IGNORED[j]) == 0) { skip = 1; break; }
        if (skip) continue;
        char *ser = json_serialize(state->c.items[i]);
        if (ser) {
            json_t *v = json_parse(ser, NULL);
            free(ser);
            if (v) json_set(out, k, v);
        }
    }
    json_free(state);
    return out;
}

/* PoP: authn_portal_host @ hermes_cli/auth_nous.py:_portal_host */
char *authn_portal_host(const char *portal_url)
{
    const char *url = (portal_url && portal_url[0]) ? portal_url : NOUS_DEFAULT_PORTAL_URL;
    char *h = hostname_of(url);
    if (h && h[0]) return h;
    free(h);
    return dup_("portal.nousresearch.com");
}
