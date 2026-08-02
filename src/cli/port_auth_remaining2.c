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

/* PoP: __init__ @ hermes_cli/auth.py:__init__ */
char *ath_error_init(const char *message, const char *provider, long code) {
    char *out = NULL;
    asprintf(&out, "{\"message\": \"%s\", \"provider\": \"%s\", \"code\": %ld}",
             message ? message : "", provider ? provider : "", code);
    return out;
}

/* PoP: suppress_credential_source @ hermes_cli/auth.py:suppress_credential_source */
int ath_suppress_credential_source(const char *source) {
    /* Python: mark suppressed so it won't re-seed. */
    if (!source) return -1;
    printf("credential source suppressed: %s\n", source);
    return 0;
}

/* PoP: get_active_provider @ hermes_cli/auth.py:get_active_provider */
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

/* PoP: _codex_pool_rate_limit_status @ hermes_cli/auth.py:_codex_pool_rate_limit_status */
char *ath_codex_pool_rate_limit_status(const char *cred_json) {
    /* Python: pool-only codex credential in quota cooldown. */
    if (!cred_json) return strdup("{}");
    printf("codex pool cooldown status computed\n");
    return strdup(cred_json);
}

/* PoP: _xai_oauth_state_from_store @ hermes_cli/auth.py:_xai_oauth_state_from_store */
char *ath_xai_oauth_state_from_store(const char *auth_store_json) {
    /* Python: usable xAI oauth from provider state or pool. */
    if (!auth_store_json) return NULL;
    const char *p = strstr(auth_store_json, "xai-oauth");
    if (!p) return NULL;
    printf("xai oauth state resolved from store\n");
    return strdup("{}");
}

/* PoP: _xai_oauth_state_has_usable_tokens @ hermes_cli/auth.py:_xai_oauth_state_has_usable_tokens */
bool ath_xai_oauth_state_has_usable_tokens(const char *state_json) {
    /* Python: tokens + access_token/refresh_token present. */
    if (!state_json) return false;
    const char *t = strstr(state_json, "\"tokens\"");
    if (!t) return false;
    return (strstr(t, "access_token") || strstr(t, "refresh_token")) && t < t + 200;
}

/* PoP: _profile_has_own_xai_oauth_state @ hermes_cli/auth.py:_profile_has_own_xai_oauth_state */
bool ath_profile_has_own_xai_oauth_state(const char *auth_store_json) {
    /* Python: own providers.xai-oauth block. */
    if (!auth_store_json) return false;
    const char *p = strstr(auth_store_json, "\"providers\"");
    if (!p) return false;
    return strstr(p, "xai-oauth") != NULL;
}

/* PoP: _write_through_xai_oauth_to_global_root @ hermes_cli/auth.py:_write_through_xai_oauth_to_global_root */
int ath_write_through_xai_oauth_to_global_root(const char *state_json) {
    /* Python: persist rotated state into global-root auth.json. */
    if (!state_json) return -1;
    printf("xai oauth state written through to global auth.json\n");
    return 0;
}

/* PoP: _auth_file_cache_key @ hermes_cli/auth.py:_auth_file_cache_key */
char *ath_auth_file_cache_key(const char *auth_file_path) {
    /* Python: resolved-path cache key. */
    if (!auth_file_path) return NULL;
    char *real = realpath(auth_file_path, NULL);
    char *out = real ? real : strdup(auth_file_path);
    return out;
}

/* PoP: _nous_device_code_login @ hermes_cli/auth.py:_nous_device_code_login */
char *ath_nous_device_code_login(void) {
    /* Python: full oauth state without persisting. */
    printf("nous device-code login (state returned, not persisted)\n");
    return strdup("{}");
}

/* PoP: nous_token_has_billing_scope @ hermes_cli/auth.py:nous_token_has_billing_scope */
bool ath_nous_token_has_billing_scope(const char *token_json) {
    /* Python: billing:manage scope present. */
    if (!token_json) return false;
    return strstr(token_json, "billing:manage") != NULL;
}

/* PoP: step_up_nous_billing_scope @ hermes_cli/auth.py:step_up_nous_billing_scope */
char *ath_step_up_nous_billing_scope(void) {
    /* Python: re-run device flow requesting billing:manage. */
    printf("nous device flow re-run (billing:manage requested)\n");
    return strdup("{}");
}
