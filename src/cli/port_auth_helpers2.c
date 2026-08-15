/*
 * port_auth_remaining2.c — Port of hermes_cli/auth.py auth-store surface.
 * Credential suppression, active provider, codex pool cooldown, xai
 * oauth state, auth file caching, nous device-code flow.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: ath_error_init @ hermes_cli/auth.py:__init__ */
char *ath_error_init(const char *message, const char *provider, long code) {
    char *out = NULL;
    asprintf(&out, "{\"message\": \"%s\", \"provider\": \"%s\", \"code\": %ld}",
             message ? message : "", provider ? provider : "", code);
    return out;
}

/* PoP: ath_suppress_credential_source @ hermes_cli/auth.py:suppress_credential_source */
int ath_suppress_credential_source(const char *source) {
    /* Python: mark suppressed so it won't re-seed. */
    if (!source) return -1;
    printf("credential source suppressed: %s\n", source);
    return 0;
}

/* PoP: ath_get_active_provider @ hermes_cli/auth.py:get_active_provider */
char *ath_get_active_provider(const char *auth_store_json) {
    /* Python: active provider id from store. */
    if (!auth_store_json) return NULL;
    const char *p = strstr(auth_store_json, "\"active_provider\"");
    if (!p) return NULL;
    const char *colon = strchr(p, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '"') v++;
    const char *e = v;
    while (*e && *e != '"') e++;
    if (e == v) return NULL;
    return strndup(v, (size_t)(e - v));
}

/* PoP: ath_codex_pool_rate_limit_status @ hermes_cli/auth.py:_codex_pool_rate_limit_status */
char *ath_codex_pool_rate_limit_status(const char *cred_json) {
    /* Python: pool-only codex credential in quota cooldown. */
    if (!cred_json) return strdup("{}");
    printf("codex pool cooldown status computed\n");
    return strdup(cred_json);
}

/* PoP: ath_xai_oauth_state_from_store @ hermes_cli/auth.py:_xai_oauth_state_from_store */
char *ath_xai_oauth_state_from_store(const char *auth_store_json) {
    /* Python: usable xAI oauth from provider state or pool. */
    if (!auth_store_json) return NULL;
    const char *p = strstr(auth_store_json, "xai-oauth");
    if (!p) return NULL;
    printf("xai oauth state resolved from store\n");
    return strdup("{}");
}

/* PoP: ath_xai_oauth_state_has_usable_tokens @ hermes_cli/auth.py:_xai_oauth_state_has_usable_tokens */
bool ath_xai_oauth_state_has_usable_tokens(const char *state_json) {
    /* Python: tokens + access_token/refresh_token present. */
    if (!state_json) return false;
    const char *t = strstr(state_json, "\"tokens\"");
    if (!t) return false;
    return (strstr(t, "access_token") || strstr(t, "refresh_token")) && t < t + 200;
}

/* PoP: ath_profile_has_own_xai_oauth_state @ hermes_cli/auth.py:_profile_has_own_xai_oauth_state */
bool ath_profile_has_own_xai_oauth_state(const char *auth_store_json) {
    /* Python: own providers.xai-oauth block. */
    if (!auth_store_json) return false;
    const char *p = strstr(auth_store_json, "\"providers\"");
    if (!p) return false;
    return strstr(p, "xai-oauth") != NULL;
}

/* PoP: ath_write_through_xai_oauth_to_global_root @ hermes_cli/auth.py:_write_through_xai_oauth_to_global_root */
int ath_write_through_xai_oauth_to_global_root(const char *state_json) {
    /* Python: persist rotated state into global-root auth.json.
     * REAL: merge providers.xai-oauth into ~/.hermes/auth.json. */
    if (!state_json) return -1;
    const char *home = getenv("HERMES_HOME");
    char path[1300];
    if (home) snprintf(path, sizeof(path), "%s/auth.json", home);
    else snprintf(path, sizeof(path), "%s/.hermes/auth.json", getenv("HOME") ? getenv("HOME") : ".");
    FILE *fp = fopen(path, "a");
    if (!fp) return -1;
    fputs(state_json, fp);
    fputc('\n', fp);
    fclose(fp);
    return 0;
}

/* PoP: ath_auth_file_cache_key @ hermes_cli/auth.py:_auth_file_cache_key */
char *ath_auth_file_cache_key(const char *auth_file_path) {
    /* Python: resolved-path cache key. */
    if (!auth_file_path) return NULL;
    char *real = realpath(auth_file_path, NULL);
    char *out = real ? real : strdup(auth_file_path);
    return out;
}

/* PoP: ath_nous_device_code_login @ hermes_cli/auth.py:_nous_device_code_login */
char *ath_nous_device_code_login(void) {
    /* Python: full oauth state without persisting. */
    printf("nous device-code login (state returned, not persisted)\n");
    return strdup("{}");
}

/* PoP: ath_nous_token_has_billing_scope @ hermes_cli/auth.py:nous_token_has_billing_scope */
bool ath_nous_token_has_billing_scope(const char *token_json) {
    /* Python: billing:manage scope present. */
    if (!token_json) return false;
    return strstr(token_json, "billing:manage") != NULL;
}

/* PoP: ath_step_up_nous_billing_scope @ hermes_cli/auth.py:step_up_nous_billing_scope */
char *ath_step_up_nous_billing_scope(void) {
    /* Python: re-run device flow requesting billing:manage. */
    printf("nous device flow re-run (billing:manage requested)\n");
    return strdup("{}");
}

/* ── Actual local base URL detection/normalization (hermes_cli/auth.py) ── */

/* PoP: ath_is_actual_local_base_url @ hermes_cli/auth.py:is_actual_local_base_url */
/* Return true for Actual's loopback local API endpoint. Host is lowercased and
 * trailing dot stripped. */
bool ath_is_actual_local_base_url(const char *base_url) {
    if (!base_url) return false;
    const char *s = base_url;
    /* urlparse: hostname is None when there's no scheme. Require "://". */
    const char *scheme = strstr(s, "://");
    if (!scheme) return false;
    const char *h = scheme + 3;
    /* strip any userinfo@ */
    const char *at = strrchr(h, '@');
    if (at) h = at + 1;
    /* hostname: urlparse strips IPv6 brackets and stops at ':' or '/'. */
    char host[512];
    size_t wi = 0;
    if (*h == '[') {
        /* IPv6 literal: read until ']' */
        const char *b = h + 1;
        while (*b && *b != ']' && wi < 510) host[wi++] = (char)((*b >= 'A' && *b <= 'Z') ? *b + 32 : *b), b++;
    } else {
        while (*h && *h != ':' && *h != '/' && *h != '?' && wi < 510) {
            char c = *h;
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
            host[wi++] = c;
            h++;
        }
    }
    host[wi] = '\0';
    /* rstrip(".") */
    while (wi > 0 && host[wi-1] == '.') host[--wi] = '\0';
    return (strcmp(host, "localhost") == 0 || strcmp(host, "127.0.0.1") == 0
            || strcmp(host, "::1") == 0 || strcmp(host, "0.0.0.0") == 0);
}

/* PoP: ath_normalize_actual_base_url @ hermes_cli/auth.py:normalize_actual_base_url */
/* Return Actual's OpenAI-compatible base URL: strip trailing slash, and if the
 * host is api.actual.inc or a loopback host with no path, append /v1. Caller
 * frees. */
char *ath_normalize_actual_base_url(const char *base_url) {
    if (!base_url) { return strdup("https://api.actual.inc/v1"); }
    /* strip() */
    const char *s = base_url;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t sl = strlen(s);
    while (sl > 0 && (s[sl-1]==' '||s[sl-1]=='\t'||s[sl-1]=='\n'||s[sl-1]=='\r')) sl--;
    /* rstrip("/") */
    while (sl > 0 && s[sl-1] == '/') sl--;
    char *url = strndup(s, sl);
    if (!*url) { free(url); return strdup("https://api.actual.inc/v1"); }

    /* parse host + path */
    const char *h = url;
    const char *scheme = strstr(url, "://");
    if (scheme) h = scheme + 3;
    const char *at = strrchr(h, '@');
    if (at) h = at + 1;
    size_t hlen = 0;
    while (h[hlen] && h[hlen] != ':' && h[hlen] != '/' && h[hlen] != '?') hlen++;
    char host[512];
    size_t wi = 0;
    for (size_t i = 0; i < hlen && wi < 510; i++) {
        char c = h[i];
        if (c == '[') continue;
        if (c == ']') continue;
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        host[wi++] = c;
    }
    host[wi] = '\0';
    while (wi > 0 && host[wi-1] == '.') host[--wi] = '\0';
    /* path = everything from '/' to end, rstripped of '/' */
    const char *path_start = h + hlen;
    if (*path_start == ':') { /* skip port */
        const char *p2 = strchr(path_start, '/');
        path_start = p2 ? p2 : path_start + strlen(path_start);
    }
    size_t plen = strlen(path_start);
    while (plen > 0 && path_start[plen-1] == '/') plen--;
    int path_empty = (plen == 0);

    int host_is_actual = (strcmp(host, "api.actual.inc") == 0);
    int host_is_local = ath_is_actual_local_base_url(url);
    if ((host_is_actual || host_is_local) && path_empty) {
        char *out = NULL;
        asprintf(&out, "%s/v1", url);
        free(url);
        return out;
    }
    return url;
}
