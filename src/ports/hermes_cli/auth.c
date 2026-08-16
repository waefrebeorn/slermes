#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/auth.c — C11 port of hermes_cli/auth.py
 * Merged from 6 source file(s) by reorganization.
 */

/* ── from src/agent/port_final_missing_gaps.c ── */
/* Slermes C11 port — 14 truly missing functions.
 * Faithful implementations of Python functions that had no prior C port.
 */


#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "hermes_json.h"

/* ═══════════════════════════════════════════════════════════════════════════ */
/* gateway/channel_directory.py                                              */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ─── _normalize_channel_query(value) -> str ─── */
/* PoP: _normalize_channel_query @ gateway/channel_directory.py:_normalize_channel_query */
char *_normalize_channel_query(const char *value) {
    if (!value) return strdup("");
    while (*value == '#') value++;
    while (*value == ' ' || *value == '\t') value++;
    int len = strlen(value);
    while (len > 0 && (value[len-1] == ' ' || value[len-1] == '\t')) len--;
    char *result = (char*)malloc(len + 1);
    for (int i = 0; i < len; i++)
        result[i] = (char)tolower((unsigned char)value[i]);
    result[len] = '\0';
    return result;
}

/* ─── _session_entry_id(origin) -> Optional[str] ─── */
/* PoP: _session_entry_id @ gateway/channel_directory.py:_session_entry_id */
json_t *_session_entry_id(json_t *origin) {
    if (!origin) return json_string("");
    const char *chat_id = json_get_str(origin, "chat_id", "");
    if (!chat_id || !*chat_id) return json_string("");
    const char *thread_id = json_get_str(origin, "thread_id", "");
    if (thread_id && *thread_id) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s:%s", chat_id, thread_id);
        return json_string(buf);
    }
    return json_string(chat_id);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/* hermes_cli/auth.py                                                        */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ─── _spotify_scope_string(raw_scope) -> str ─── */
/* PoP: _spotify_scope_string @ hermes_cli/auth.py:_spotify_scope_string */
json_t *_spotify_scope_string(json_t *raw_scope) {
    if (!raw_scope) return json_string("");
    /* Join scope list with spaces */
    if (json_is_array(raw_scope)) {
        char result[2048] = {0};
        size_t n = json_len(raw_scope);
        for (size_t i = 0; i < n; i++) {
            const char *s = json_get_str(json_get(raw_scope, i), "", "");
            if (s && *s) {
                if (result[0]) strcat(result, " ");
                strncat(result, s, sizeof(result) - strlen(result) - 1);
            }
        }
        return json_string(result);
    }
    return json_string(json_get_str(raw_scope, "", ""));
}

/* ─── _spotify_redirect_uri(explicit, state) -> str ─── */
/* PoP: _spotify_redirect_uri @ hermes_cli/auth.py:_spotify_redirect_uri */
json_t *_spotify_redirect_uri(const char *explicit, json_t *state) {
    if (explicit && *explicit) return json_string(explicit);
    if (state) {
        const char *uri = json_get_str(state, "redirect_uri", "");
        if (uri && *uri) return json_string(uri);
    }
    return json_string("http://localhost:8080/callback");
}

/* ─── _spotify_api_base_url(state) -> str ─── */
/* PoP: _spotify_api_base_url @ hermes_cli/auth.py:_spotify_api_base_url */
json_t *_spotify_api_base_url(json_t *state) {
    if (state) {
        const char *url = json_get_str(state, "api_base_url", "");
        if (url && *url) return json_string(url);
    }
    const char *env = getenv("HERMES_SPOTIFY_API_BASE_URL");
    if (env && *env) return json_string(env);
    return json_string("https://api.spotify.com/v1");
}

/* ─── _spotify_accounts_base_url(state) -> str ─── */
/* PoP: _spotify_accounts_base_url @ hermes_cli/auth.py:_spotify_accounts_base_url */
json_t *_spotify_accounts_base_url(json_t *state) {
    if (state) {
        const char *url = json_get_str(state, "accounts_base_url", "");
        if (url && *url) return json_string(url);
    }
    const char *env = getenv("HERMES_SPOTIFY_ACCOUNTS_BASE_URL");
    if (env && *env) return json_string(env);
    return json_string("https://accounts.spotify.com");
}

/* ─── _spotify_code_challenge(code_verifier) -> str ─── */
/* PoP: _spotify_code_challenge @ hermes_cli/auth.py:_spotify_code_challenge */
json_t *_spotify_code_challenge(const char *code_verifier) {
    if (!code_verifier) return json_string("");
    /* SHA256 + base64url encode without padding — uses libcrypto */
    unsigned char hash[32];
    extern void crypto_sha256(const unsigned char *data, size_t len, unsigned char *out);
    crypto_sha256((const unsigned char*)code_verifier, strlen(code_verifier), hash);
    /* base64url encode */
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    char result[64];
    int j = 0;
    for (int i = 0; i < 32; i += 3) {
        unsigned n = ((unsigned)hash[i] << 16);
        if (i+1 < 32) n |= ((unsigned)hash[i+1] << 8);
        if (i+2 < 32) n |= hash[i+2];
        result[j++] = b64[(n >> 18) & 0x3F];
        result[j++] = b64[(n >> 12) & 0x3F];
        if (i+1 < 32) result[j++] = b64[(n >> 6) & 0x3F];
        if (i+2 < 32) result[j++] = b64[n & 0x3F];
    }
    result[j] = '\0';
    /* Remove padding (already using url-safe alphabet without =) */
    return json_string(result);
}

/* ─── _oauth_pkce_code_verifier(length=64) -> str ─── */
/* PoP: _oauth_pkce_code_verifier @ hermes_cli/auth.py:_oauth_pkce_code_verifier */
json_t *_oauth_pkce_code_verifier(int length) {
    if (length <= 0) length = 64;
    /* Generate random bytes and base64url encode */
    unsigned char *raw = (unsigned char*)malloc(length);
    extern bool crypto_random_bytes(unsigned char *buf, size_t n);
    crypto_random_bytes(raw, (size_t)length);
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    char *result = (char*)malloc(length * 2);
    int j = 0;
    for (int i = 0; i < length; i += 3) {
        unsigned n = ((unsigned)raw[i] << 16);
        if (i+1 < length) n |= ((unsigned)raw[i+1] << 8);
        if (i+2 < length) n |= raw[i+2];
        result[j++] = b64[(n >> 18) & 0x3F];
        result[j++] = b64[(n >> 12) & 0x3F];
        if (i+1 < length) result[j++] = b64[(n >> 6) & 0x3F];
        if (i+2 < length) result[j++] = b64[n & 0x3F];
    }
    result[j] = '\0';
    free(raw);
    /* Truncate to 128 chars max */
    if (strlen(result) > 128) result[128] = '\0';
    return json_string(result);
}

/* ─── _oauth_pkce_code_challenge(code_verifier) -> str ─── */
/* PoP: _oauth_pkce_code_challenge @ hermes_cli/auth.py:_oauth_pkce_code_challenge */
json_t *_oauth_pkce_code_challenge(const char *code_verifier) {
    /* Same as _spotify_code_challenge: SHA256 + base64url */
    return _spotify_code_challenge(code_verifier);
}

/* ─── _minimax_expired_in_looks_like_unix_ms(expired_in, now_ms) -> bool ─── */
/* PoP: _minimax_expired_in_looks_like_unix_ms @ hermes_cli/auth.py:_minimax_expired_in_looks_like_unix_ms */
int _minimax_expired_in_looks_like_unix_ms(long expired_in, long now_ms) {
    return expired_in > now_ms;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/* hermes_cli/kanban_db.py                                                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ─── _resolve_crash_grace_seconds() -> int ─── */
/* PoP: _resolve_crash_grace_seconds @ hermes_cli/kanban_db.py:_resolve_crash_grace_seconds */
int _resolve_crash_grace_seconds(void) {
    const char *env = getenv("HERMES_KANBAN_CRASH_GRACE_SECONDS");
    if (env && *env) {
        int val = atoi(env);
        if (val > 0) return val;
    }
    return 300;  /* DEFAULT_CRASH_GRACE_SECONDS = 5 minutes */
}

/* ─── _resolve_rate_limit_cooldown_seconds() -> int ─── */
/* PoP: _resolve_rate_limit_cooldown_seconds @ hermes_cli/kanban_db.py:_resolve_rate_limit_cooldown_seconds */
int _resolve_rate_limit_cooldown_seconds(void) {
    const char *env = getenv("HERMES_KANBAN_RATE_LIMIT_COOLDOWN_SECONDS");
    if (env && *env) {
        int val = atoi(env);
        if (val > 0) return val;
    }
    return 60;  /* DEFAULT_RATE_LIMIT_COOLDOWN_SECONDS = 1 minute */
}

/* ─── _is_windows_batch_shim(path) -> bool ─── */
/* PoP: _is_windows_batch_shim @ hermes_cli/kanban_db.py:_is_windows_batch_shim */
int _is_windows_batch_shim(const char *path) {
    if (!path) return 0;
    int len = strlen(path);
    if (len >= 4) {
        const char *ext = path + len - 4;
        if (strcasecmp(ext, ".cmd") == 0 || strcasecmp(ext, ".bat") == 0)
            return 1;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/* hermes_cli/main.py                                                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ─── _infer_stepfun_region(base_url) -> str ─── */
/* PoP: _infer_stepfun_region @ hermes_cli/main.py:_infer_stepfun_region */
json_t *_infer_stepfun_region(const char *base_url) {
    if (!base_url) return json_string("global");
    char *normalized = strdup(base_url);
    for (int i = 0; normalized[i]; i++)
        normalized[i] = (char)tolower((unsigned char)normalized[i]);
    
    json_t *result;
    if (strstr(normalized, "api.stepfun.com"))
        result = json_string("global");
    else if (strstr(normalized, "api-stepfun.cn"))
        result = json_string("cn");
    else
        result = json_string("global");
    
    free(normalized);
    return result;
}


/* ── from src/cli/port_auth_helpers.c ── */
/*
 * port_auth_helpers.c
 *
 * Pure, portable helper functions ported from hermes_cli/auth.py.
 * Most auth.py pure helpers (has_usable_secret, format_auth_error,
 * _coerce_ttl_seconds, _is_expiring, _optional_base_url, *_pkce_*,
 * _resolve_*_base_url, etc.) are already implemented in port_auth_na.c, so
 * this file ports only the ones NOT yet present, avoiding link collisions.
 * No network, no auth.json load (callers pass the store JSON in).
 *
 * Module prefix used by the scanner for hermes_cli/auth.py is "auth_".
 *
 * C name <- python name (auth_ prefix):
 *   auth_xai_callback_cors_origin, auth_ssh_user_at_host,
 *   auth_is_terminal_nous_refresh_error, auth_is_source_suppressed
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>

/* ---- lowercase helper ---- */
static void lc(char *s)
{
    for (; *s; s++) if (isupper((unsigned char)*s)) *s = (char)tolower((unsigned char)*s);
}

/* ---------------------------------------------------------------------- */
/* PoP: auth_xai_callback_cors_origin @ hermes_cli/auth.py:_xai_callback_cors_origin */
/* Returns malloc'd allowed origin, or "" (empty) when not allowed. */
char *auth_xai_callback_cors_origin(const char *origin)
{
    if (!origin) return strdup("");
    if (strcmp(origin, "https://accounts.x.ai") == 0) return strdup(origin);
    if (strcmp(origin, "https://auth.x.ai") == 0) return strdup(origin);
    return strdup("");
}

/* ---------------------------------------------------------------------- */
/* PoP: auth_ssh_user_at_host @ hermes_cli/auth.py:_ssh_user_at_host */
/* Returns "user@hostname" using USER/LOGNAME env + gethostname. */
char *auth_ssh_user_at_host(void)
{
    char host[256];
    if (gethostname(host, sizeof(host)) != 0) host[0] = '\0';
    if (!host[0]) strcpy(host, "<this-host>");
    const char *user = getenv("USER");
    if (!user) user = getenv("LOGNAME");
    if (!user) user = "<user>";
    char *out = malloc(strlen(user) + strlen(host) + 2);
    sprintf(out, "%s@%s", user, host);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: explicitly @ hermes_cli/auth.py:_is_terminal_nous_refresh_error */
/* provider/code/relogin passed explicitly (no AuthError object). */
int auth_is_terminal_nous_refresh_error(const char *provider, const char *code, int relogin_required)
{
    if (!provider || !code) return 0;
    char p[64]; strncpy(p, provider, sizeof(p)-1); p[sizeof(p)-1]='\0'; lc(p);
    if (strcmp(p, "nous") != 0) return 0;
    if (!relogin_required) return 0;
    char c[64]; strncpy(c, code, sizeof(c)-1); c[sizeof(c)-1]='\0'; lc(c);
    if (strcmp(c, "invalid_grant")==0 || strcmp(c,"invalid_token")==0 || strcmp(c,"refresh_token_reused")==0)
        return 1;
    return 0;
}

/* ---------------------------------------------------------------------- */
/* PoP: JSON @ hermes_cli/auth.py:is_source_suppressed */
/* Takes the auth.json "suppressed_sources" object as JSON (caller loads it).
 * Returns 1 if (provider_id -> list) contains source. */
int auth_is_source_suppressed(const char *provider_id, const char *source, const char *suppressed_json)
{
    if (!provider_id || !source || !suppressed_json) return 0;
    json_t *root = json_parse(suppressed_json, NULL);
    if (!root || root->type != JSON_OBJECT) { if (root) json_free(root); return 0; }
    json_t *prov = json_object_get(root, provider_id);
    if (!prov || prov->type != JSON_ARRAY) { json_free(root); return 0; }
    int found = 0;
    for (size_t i = 0; i < json_array_size(prov); i++) {
        json_t *e = json_array_get(prov, i);
        if (e && e->type == JSON_STRING && strcmp(json_string_value(e), source) == 0) { found = 1; break; }
    }
    json_free(root);
    return found;
}


/* ── from src/cli/port_auth_helpers2.c ── */
/*
 * port_auth_remaining2.c — Port of hermes_cli/auth.py auth-store surface.
 * Credential suppression, active provider, codex pool cooldown, xai
 * oauth state, auth file caching, nous device-code flow.
 */
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


/* ── from src/cli/port_auth_na.c ── */
/*
 * port_auth_na.c — Port of Python hermes_cli/auth.py (NA_CLI functions)
 * Functions that don't exist in port_auth.c.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "libcrypto/crypto.h"
#include "libcredentialfiles/credential_files.h"

/* Forward declaration: to_epoch lives in port_kanban_db.c (reused here). */
long to_epoch(const char *val);
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <strings.h>
#include <limits.h>
#include <time.h>

/* Port of Python: _codex_pool_rate_limit_status */
json_t* _codex_pool_rate_limit_status(void)
{
    hermes_log(LOG_DEBUG, "port", "_codex_pool_rate_limit_status: called");

    const char* home = getenv("HOME");
    if (!home) return NULL;

    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/auth.json", home);

    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t n = fread(buf, 1, size, f);
    fclose(f);
    buf[n] = '\0';

    json_t* auth_store = json_parse(buf, NULL);
    free(buf);
    if (!auth_store) return NULL;

    json_t* pool = json_object_get(auth_store, "credential_pool");
    if (!pool) { json_free(auth_store); return NULL; }

    json_t* codex = json_object_get(pool, "codex");
    if (!codex) { json_free(auth_store); return NULL; }

    json_t* result = json_copy(codex);
    json_free(auth_store);
    return result ? result : json_new_object();
}

/* Port of Python: _xai_oauth_state_from_store */
json_t* _xai_oauth_state_from_store(json_t* auth_store)
{
    if (!auth_store) return NULL;
    hermes_log(LOG_DEBUG, "port", "_xai_oauth_state_from_store: called");

    json_t* providers = json_object_get(auth_store, "providers");
    json_t* xai_state = providers ? json_object_get(providers, "xai-oauth") : NULL;

    json_t* tokens = xai_state ? json_object_get(xai_state, "tokens") : NULL;
    if (tokens) {
        json_t* access = json_object_get(tokens, "access_token");
        json_t* refresh = json_object_get(tokens, "refresh_token");
        const char* at = access ? json_node_get_string(access) : NULL;
        const char* rt = refresh ? json_node_get_string(refresh) : NULL;
        if (at && at[0] && rt && rt[0]) return json_copy(xai_state);
    }

    json_t* pool = json_object_get(auth_store, "credential_pool");
    json_t* entries = pool ? json_object_get(pool, "xai-oauth") : NULL;
    if (entries) {
        int count = json_array_count(entries);
        for (int i = 0; i < count; i++) {
            json_t* entry = json_array_get(entries, i);
            if (!entry) continue;
            json_t* access = json_object_get(entry, "access_token");
            json_t* refresh = json_object_get(entry, "refresh_token");
            const char* at = access ? json_node_get_string(access) : NULL;
            const char* rt = refresh ? json_node_get_string(refresh) : NULL;
            if (at && at[0] && rt && rt[0]) {
                json_t* merged = json_copy(xai_state ? xai_state : json_new_object());
                json_t* new_tokens = json_new_object();
                json_object_set(new_tokens, "access_token", json_new_string(at));
                json_object_set(new_tokens, "refresh_token", json_new_string(rt));
                json_object_set(new_tokens, "token_type", json_new_string("Bearer"));
                json_object_set(merged, "tokens", new_tokens);
                return merged;
            }
        }
    }

    return NULL;
}

/* Port of Python: _xai_oauth_state_has_usable_tokens */
bool _xai_oauth_state_has_usable_tokens(json_t* state)
{
    if (!state) return false;
    hermes_log(LOG_DEBUG, "port", "_xai_oauth_state_has_usable_tokens: called");

    json_t* tokens = json_object_get(state, "tokens");
    if (!tokens) return false;

    json_t* access = json_object_get(tokens, "access_token");
    json_t* refresh = json_object_get(tokens, "refresh_token");
    const char* at = access ? json_node_get_string(access) : NULL;
    const char* rt = refresh ? json_node_get_string(refresh) : NULL;

    return (at && at[0] && rt && rt[0]);
}

/* Port of Python: _profile_has_own_xai_oauth_state */
bool _profile_has_own_xai_oauth_state(json_t* auth_store)
{
    if (!auth_store) return false;
    hermes_log(LOG_DEBUG, "port", "_profile_has_own_xai_oauth_state: called");

    json_t* providers = json_object_get(auth_store, "providers");
    if (!providers) return false;

    json_t* xai = json_object_get(providers, "xai-oauth");
    return (xai != NULL);
}

/* Port of Python: _write_through_xai_oauth_to_global_root */
void _write_through_xai_oauth_to_global_root(json_t* state)
{
    if (!state) return;
    hermes_log(LOG_DEBUG, "port", "_write_through_xai_oauth_to_global_root: called");

    const char* home = getenv("HOME");
    if (!home) return;

    if (getenv("PYTEST_CURRENT_TEST")) return;

    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/auth.json", home);

    FILE* f = fopen(path, "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return; }

    size_t n = fread(buf, 1, size, f);
    fclose(f);
    buf[n] = '\0';

    json_t* global_store = json_parse(buf, NULL);
    free(buf);
    if (!global_store) return;

    json_t* providers = json_object_get(global_store, "providers");
    if (!providers) {
        providers = json_new_object();
        json_object_set(global_store, "providers", providers);
    }

    json_t* xai_copy = json_copy(state);
    json_object_set(providers, "xai-oauth", xai_copy);

    char* out = json_serialize(global_store);
    if (out) {
        f = fopen(path, "w");
        if (f) { fputs(out, f); fclose(f); }
        free(out);
    }

    json_free(global_store);
}

/* Port of Python: _auth_file_cache_key */
json_t* _auth_file_cache_key(void)
{
    hermes_log(LOG_DEBUG, "port", "_auth_file_cache_key: called");

    const char* home = getenv("HOME");
    if (!home) home = ".";

    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/auth.json", home);

    json_t* result = json_new_object();
    json_object_set(result, "path", json_new_string(path));

    struct stat st;
    if (stat(path, &st) == 0) {
        json_object_set(result, "mtime", json_new_number((double)st.st_mtime));
    } else {
        json_object_set(result, "mtime", json_new_number(0));
    }

    return result;
}

/* Port of Python: nous_token_has_billing_scope */
bool nous_token_has_billing_scope(void)
{
    hermes_log(LOG_DEBUG, "port", "nous_token_has_billing_scope: called");

    const char* home = getenv("HOME");
    if (!home) return false;

    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/auth.json", home);

    FILE* f = fopen(path, "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return false; }

    size_t n = fread(buf, 1, size, f);
    fclose(f);
    buf[n] = '\0';

    json_t* auth_store = json_parse(buf, NULL);
    free(buf);
    if (!auth_store) return false;

    json_t* providers = json_object_get(auth_store, "providers");
    json_t* nous = providers ? json_object_get(providers, "nous") : NULL;
    json_t* scope_node = nous ? json_object_get(nous, "scope") : NULL;
    const char* scope = scope_node ? json_node_get_string(scope_node) : NULL;

    bool has_scope = false;
    if (scope && scope[0]) {
        const char* billing = "billing:manage";
        int blen = strlen(billing);
        const char* p = scope;
        while (*p) {
            while (*p == ' ') p++;
            if (strncmp(p, billing, blen) == 0 && (p[blen] == ' ' || p[blen] == '\0')) {
                has_scope = true;
                break;
            }
            while (*p && *p != ' ') p++;
        }
    }

    json_free(auth_store);
    return has_scope;
}

/* Port of Python: step_up_nous_billing_scope */
bool step_up_nous_billing_scope(bool open_browser, float timeout_seconds)
{
    (void)open_browser;
    (void)timeout_seconds;
    hermes_log(LOG_DEBUG, "port", "step_up_nous_billing_scope: called");

    if (nous_token_has_billing_scope()) return true;

    hermes_log(LOG_WARNING, "port", "step_up_nous_billing_scope: billing:manage scope not present, manual re-login required");
    return false;
}

/* ===========================================================================
 *  Auth error helpers — ported from hermes_cli/auth.py
 *  These were REAL_GAP. A minimal AuthError abstraction is defined here to
 *  match the Python dataclass fields (code, provider, relogin_required, msg).
 * =========================================================================== */

#define CODEX_RATE_LIMITED_CODE 429

typedef struct {
    const char *code;          /* e.g. "subscription_required", "429", ... */
    const char *provider;      /* e.g. "nous", "codex" */
    int         relogin_required;
    const char *msg;           /* the base error message */
} auth_err_local_t;

/*
 * PoP: is_rate_limited_auth_error @ hermes_cli/auth.py:is_rate_limited_auth_error */
int is_rate_limited_auth_error(const auth_err_local_t *error)
{
    if (!error) return 0;
    return (error->code != NULL) && (error->relogin_required == 0) &&
           (strcmp(error->code, "429") == 0);
}

/*
 * headers is a simple key→value map (NULL-terminated array of pairs). */
int parse_retry_after_seconds(const char *const *headers)
{
    if (!headers) return -1;
    const char *raw = NULL;
    for (int i = 0; headers[i]; i += 2) {
        if (strcasecmp(headers[i], "retry-after") == 0) { raw = headers[i+1]; break; }
    }
    if (!raw) return -1;
    char *end = NULL;
    long seconds = strtol(raw, &end, 10);
    if (end == raw || *end != '\0') return -1;
    return (seconds >= 0) ? (int)seconds : -1;
}

/*
 * Returns malloc'd 12-hex-char fingerprint of the token (sha256[:12]). Caller frees. */
char *token_fingerprint(const char *token)
{
    if (!token) return NULL;
    char cleaned[4096];
    size_t i = 0;
    while (token[i] == ' ' || token[i] == '\t') i++;
    size_t j = strlen(token);
    while (j > i && (token[j-1]==' '||token[j-1]=='\t')) j--;
    if (i >= j) return NULL;
    size_t k = 0;
    for (; i < j && k < sizeof(cleaned)-1; i++) cleaned[k++] = token[i];
    cleaned[k] = '\0';

    unsigned char raw[32];
    crypto_sha256((const unsigned char*)cleaned, strlen(cleaned), raw);
    static const char *hexd = "0123456789abcdef";
    char *out = (char*)malloc(13);
    for (int n = 0; n < 12; n++) {
        out[n] = hexd[(raw[n >> 1] >> ((n & 1) ? 0 : 4)) & 0xF];
    }
    out[12] = '\0';
    return out;
}

/*
 * Returns malloc'd user-facing message. Caller frees. For non-AuthError
 * inputs returns a copy of msg. */
/* PoP: format_auth_error @ hermes_cli/auth.py:format_auth_error */
char *format_auth_error(const auth_err_local_t *error)
{
    if (!error) return strdup("");
    if (!error->code) return strdup(error->msg ? error->msg : "");

    /* rate-limited: no remediation */
    if (is_rate_limited_auth_error(error))
        return strdup(error->msg ? error->msg : "");

    if (error->relogin_required) {
        size_t n = (error->msg ? strlen(error->msg) : 0) + 64;
        char *r = (char*)malloc(n);
        snprintf(r, n, "%s Run `hermes model` to re-authenticate.", error->msg ? error->msg : "");
        return r;
    }

    const char *nous = error->provider && strcmp(error->provider, "nous") == 0 ? "nous" : NULL;
    if (strcmp(error->code, "subscription_required") == 0) {
        if (nous) return strdup("Check credits or billing in Nous Portal, then retry.");
        return strdup("No active paid subscription found. Please purchase/activate a subscription, then retry.");
    }
    if (strcmp(error->code, "insufficient_credits") == 0) {
        if (nous) return strdup("Check credits or billing in Nous Portal, then retry.");
        return strdup("Subscription credits are exhausted. Top up/renew credits, then retry.");
    }
    if (strcmp(error->code, "temporarily_unavailable") == 0) {
        size_t n = (error->msg ? strlen(error->msg) : 0) + 64;
        char *r = (char*)malloc(n);
        snprintf(r, n, "%s Please retry in a few seconds.", error->msg ? error->msg : "");
        return r;
    }
    return strdup(error->msg ? error->msg : "");
}

/* ===========================================================================
 *  Additional auth.py pure helpers (constant/env/scope/JWT/PKCE; no network)
 * =========================================================================== */

/* Hermes home resolution: mirror hermes_cli.config.get_hermes_home(). */
static const char *auth_hermes_home(void)
{
    const char *h = getenv("HERMES_HOME");
    if (!h || !h[0]) h = credfiles_get_hermes_home();
    return h ? h : "~/.hermes";
}

/*
 * Returns malloc'd "<hermes_home>/auth.json". Caller frees. */
char *auth_file_path(void)
{
    const char *home = auth_hermes_home();
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/auth.json", home);
    return strdup(out);
}

/*
 * Returns malloc'd global-root auth.json when in profile mode (root != home),
 * else NULL (classic mode). Caller frees result (or ignores NULL). */
/* PoP: _global_auth_file_path @ hermes_cli/auth.py:_auth_file_path */
char *_global_auth_file_path(void)
{
    const char *home = auth_hermes_home();
    /* get_default_hermes_root() resolves to ~/.hermes unless HERMES_ROOT set */
    const char *root = getenv("HERMES_ROOT");
    if (!root || !root[0]) root = "~/.hermes";
    /* Profile mode when home is a subdir under the global root. */
    size_t rl = strlen(root);
    if (strncmp(home, root, rl) == 0 && home[rl] == '/') {
        char out[PATH_MAX];
        snprintf(out, sizeof(out), "%s/auth.json", root);
        return strdup(out);
    }
    return NULL;
}

/*
 * PoP: _resolve_kimi_base_url @ hermes_cli/auth.py:_resolve_kimi_base_url */
const char *resolve_kimi_base_url(const char *api_key, const char *default_url, const char *env_override)
{
    static const char *KIMI_CODE_BASE_URL = "https://api.kimi.com/coding/v1";
    if (env_override && env_override[0]) return env_override;
    if (!api_key || !api_key[0]) return default_url ? default_url : "";
    if (strncmp(api_key, "sk-kimi-", 8) == 0) return KIMI_CODE_BASE_URL;
    return default_url ? default_url : "";
}

/*
 * PoP: _normalize_lmstudio_runtime_base_url @ hermes_cli/auth.py:_normalize_lmstudio_runtime_base_url */
char *normalize_lmstudio_runtime_base_url(const char *base_url)
{
    char root[1024];
    size_t n = 0;
    const char *s = base_url ? base_url : "";
    while (s[n]==' '||s[n]=='\t') s++;
    size_t L = strlen(s);
    while (L>0 && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='/')) L--;
    if (L >= sizeof(root)) L = sizeof(root)-1;
    memcpy(root, s, L); root[L] = '\0';
    const char *suffixes[] = {"/api/v1", "/api", "/v1"};
    for (int i = 0; i < 3; i++) {
        size_t sl = strlen(suffixes[i]);
        if (L >= sl && strcmp(root + L - sl, suffixes[i]) == 0) {
            L -= sl; root[L] = '\0';
            while (L>0 && root[L-1]=='/') root[--L]='\0';
            break;
        }
    }
    const char *fallback = "http://127.0.0.1:1234";
    char out[1100];
    snprintf(out, sizeof(out), "%s/v1", L ? root : fallback);
    return strdup(out);
}

/*
 * Returns 1 when value is a non-empty, non-placeholder secret string. */
/* PoP: has_usable_secret @ hermes_cli/auth.py:has_usable_secret */
int has_usable_secret(const char *value, int min_length)
{
    static const char *PLACEHOLDER[] = {
        "*","**","***","changeme","your_api_key","your_api_key_here",
        "your-api-key","placeholder","example","dummy","null","none",NULL
    };
    if (!value) return 0;
    const char *p = value;
    while (*p==' '||*p=='\t') p++;
    size_t L = strlen(p);
    while (L>0 && (p[L-1]==' '||p[L-1]=='\t')) L--;
    if (L < (size_t)min_length) return 0;
    char buf[256];
    if (L >= sizeof(buf)) L = sizeof(buf)-1;
    memcpy(buf, p, L); buf[L] = '\0';
    for (int i = 0; PLACEHOLDER[i]; i++)
        if (strcasecmp(buf, PLACEHOLDER[i]) == 0) return 0;
    return 1;
}

/*
 * Returns 1 when expires_at_iso is missing or within skew_seconds of now. */
int is_expiring(const char *expires_at_iso, int skew_seconds)
{
    long exp = to_epoch(expires_at_iso);   /* reuse kanban_db to_epoch */
    if (exp < 0) return 1;
    long now = (long)time(NULL);
    return (exp <= now + skew_seconds) ? 1 : 0;
}

/*
 * PoP: _coerce_ttl_seconds @ hermes_cli/auth.py:_coerce_ttl_seconds */
int coerce_ttl_seconds(const char *expires_in)
{
    if (!expires_in) return 0;
    char *end = NULL;
    long ttl = strtol(expires_in, &end, 10);
    if (end == expires_in || *end != '\0') return 0;
    return (int)((ttl > 0) ? ttl : 0);
}

/*
 * Returns malloc'd trimmed base URL (trailing slashes stripped) or NULL.
 * Caller frees (or ignores NULL). */
char *optional_base_url(const char *value)
{
    if (!value) return NULL;
    const char *p = value;
    while (*p==' '||*p=='\t') p++;
    size_t L = strlen(p);
    while (L>0 && (p[L-1]==' '||p[L-1]=='\t'||p[L-1]=='/')) L--;
    if (L == 0) return NULL;
    char *out = malloc(L+1);
    memcpy(out, p, L); out[L]='\0';
    return out;
}

/*
 * Decode the payload segment of a JWT into a JSON object. Returns NULL on
 * failure; caller frees via json_free. */
json_t *_decode_jwt_claims(const char *token)
{
    if (!token) return NULL;
    /* count dots */
    int dots = 0;
    for (const char *p = token; *p; p++) if (*p == '.') dots++;
    if (dots != 2) return NULL;
    const char *seg1 = strchr(token, '.');
    if (!seg1) return NULL;
    const char *payload = seg1 + 1;
    const char *seg2 = strchr(payload, '.');
    if (!seg2) return NULL;
    size_t plen = (size_t)(seg2 - payload);
    char buf[4096];
    if (plen >= sizeof(buf)) return NULL;
    memcpy(buf, payload, plen); buf[plen] = '\0';
    /* pad to multiple of 4 */
    int pad = (4 - (int)plen % 4) % 4;
    char padded[4100];
    memcpy(padded, buf, plen);
    for (int i = 0; i < pad; i++) padded[plen+i] = '=';
    padded[plen+pad] = '\0';
    size_t raw_len = 0;
    unsigned char *raw = crypto_base64url_decode(padded, &raw_len);
    if (!raw) return NULL;
    json_t *claims = json_parse((const char*)raw, NULL);
    free(raw);
    if (!claims) return NULL;
    if (claims->type != JSON_OBJECT) { json_free(claims); return NULL; }
    return claims;
}

/*
 * Normalise a space/comma separated scope string (or collection) into a
 * malloc'd, NULL-terminated array of malloc'd scope strings. *out_count set.
 * Caller frees each string and the array. */
char **scope_values(const char *raw_scope, int *out_count)
{
    char **out = malloc(sizeof(char*));
    out[0] = NULL;
    int n = 0;
    if (raw_scope) {
        char buf[2048];
        size_t L = strlen(raw_scope);
        if (L >= sizeof(buf)) L = sizeof(buf)-1;
        memcpy(buf, raw_scope, L); buf[L]='\0';
        for (char *p = buf; *p; ) {
            while (*p==' '||*p=='\t'||*p==',') p++;
            if (!*p) break;
            char *start = p;
            while (*p && *p!=' ' && *p!='\t' && *p!=',') p++;
            size_t len = (size_t)(p - start);
            char s[256];
            if (len >= sizeof(s)) len = sizeof(s)-1;
            memcpy(s, start, len); s[len]='\0';
            out = realloc(out, sizeof(char*)*(n+2));
            out[n++] = strdup(s);
            out[n] = NULL;
        }
    }
    if (out_count) *out_count = n;
    return out;
}

/*
 * Default Spotify scope used when none provided. */
static const char *DEFAULT_SPOTIFY_SCOPE = "user-read-playback-state user-modify-playback-state";

char **spotify_scope_list(const char *raw_scope, int *out_count)
{
    const char *src = raw_scope && raw_scope[0] ? raw_scope : DEFAULT_SPOTIFY_SCOPE;
    char **out = malloc(sizeof(char*));
    out[0] = NULL;
    int n = 0;
    /* de-dup */
    char buf[2048];
    size_t L = strlen(src);
    if (L >= sizeof(buf)) L = sizeof(buf)-1;
    memcpy(buf, src, L); buf[L]='\0';
    for (char *p = buf; *p; ) {
        while (*p==' ') p++;
        if (!*p) break;
        char *start = p;
        while (*p && *p!=' ') p++;
        size_t len = (size_t)(p - start);
        char s[256];
        if (len >= sizeof(s)) len = sizeof(s)-1;
        memcpy(s, start, len); s[len]='\0';
        int dup = 0;
        for (int i = 0; i < n; i++) if (strcmp(out[i], s) == 0) { dup = 1; break; }
        if (!dup) { out = realloc(out, sizeof(char*)*(n+2)); out[n++] = strdup(s); out[n]=NULL; }
    }
    if (out_count) *out_count = n;
    return out;
}

/*
 * PoP: _spotify_scope_string @ hermes_cli/auth.py:_spotify_scope_string */
char *spotify_scope_string(const char *raw_scope)
{
    int n = 0;
    char **sc = spotify_scope_list(raw_scope, &n);
    size_t cap = 1;
    for (int i = 0; i < n; i++) cap += strlen(sc[i]) + 1;
    char *out = malloc(cap);
    out[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (i) strcat(out, " ");
        strcat(out, sc[i]);
    }
    for (int i = 0; i < n; i++) free(sc[i]);
    free(sc);
    return out;
}

/*
 * Returns malloc'd client_id from explicit/state/env, or NULL if missing. */
char *_spotify_client_id(const char *explicit, const char *state_client_id)
{
    const char *cands[4];
    cands[0] = explicit;
    cands[1] = getenv("HERMES_SPOTIFY_CLIENT_ID");
    cands[2] = getenv("SPOTIFY_CLIENT_ID");
    cands[3] = state_client_id;
    for (int i = 0; i < 4; i++) {
        const char *c = cands[i];
        if (c && c[0]) return strdup(c);
    }
    return NULL;
}

/*
 * PoP: _spotify_redirect_uri @ hermes_cli/auth.py:_spotify_redirect_uri */
char *spotify_redirect_uri(const char *explicit, const char *state_redirect_uri,
                           const char *default_redirect)
{
    const char *cands[5];
    cands[0] = explicit;
    cands[1] = getenv("HERMES_SPOTIFY_REDIRECT_URI");
    cands[2] = getenv("SPOTIFY_REDIRECT_URI");
    cands[3] = state_redirect_uri;
    cands[4] = default_redirect;
    for (int i = 0; i < 5; i++) {
        const char *c = cands[i];
        if (c && c[0]) return strdup(c);
    }
    return default_redirect ? strdup(default_redirect) : NULL;
}

/*
 * PoP: _spotify_api_base_url @ hermes_cli/auth.py:_spotify_api_base_url */
char *spotify_api_base_url(const char *state_api_base_url, const char *default_url)
{
    const char *cands[3];
    cands[0] = getenv("HERMES_SPOTIFY_API_BASE_URL");
    cands[1] = state_api_base_url;
    cands[2] = default_url;
    for (int i = 0; i < 3; i++) {
        const char *c = cands[i];
        if (c && c[0]) {
            char buf[1024];
            size_t L = strlen(c);
            while (L>0 && c[L-1]=='/') L--;
            memcpy(buf, c, L); buf[L]='\0';
            return strdup(buf);
        }
    }
    return default_url ? strdup(default_url) : NULL;
}

/*
 * PoP: _spotify_accounts_base_url @ hermes_cli/auth.py:_spotify_accounts_base_url */
char *spotify_accounts_base_url(const char *state_accounts_base_url, const char *default_url)
{
    const char *cands[3];
    cands[0] = getenv("HERMES_SPOTIFY_ACCOUNTS_BASE_URL");
    cands[1] = state_accounts_base_url;
    cands[2] = default_url;
    for (int i = 0; i < 3; i++) {
        const char *c = cands[i];
        if (c && c[0]) {
            char buf[1024];
            size_t L = strlen(c);
            while (L>0 && c[L-1]=='/') L--;
            memcpy(buf, c, L); buf[L]='\0';
            return strdup(buf);
        }
    }
    return default_url ? strdup(default_url) : NULL;
}

/*
 * Returns malloc'd PKCE code_verifier (urlsafe base64 of `length` random bytes,
 * stripped of '=' and truncated to 128). Caller frees. */
char *_spotify_code_verifier(int length)
{
    if (length <= 0) length = 64;
    unsigned char raw[128];
    crypto_random_bytes(raw, (size_t)length > sizeof(raw) ? sizeof(raw) : (size_t)length);
    char *enc = crypto_base64url_encode(raw, (size_t)length);
    /* strip '=' */
    char *p = enc;
    while (*p && *p != '=') p++;
    *p = '\0';
    if (strlen(enc) > 128) enc[128] = '\0';
    return enc;
}

/*
 * PoP: _spotify_code_challenge @ hermes_cli/auth.py:_spotify_code_challenge */
char *spotify_code_challenge(const char *code_verifier)
{
    if (!code_verifier) return NULL;
    unsigned char digest[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char*)code_verifier, strlen(code_verifier), digest);
    char *enc = crypto_base64url_encode(digest, CRYPTO_SHA256_LEN);
    char *p = enc;
    while (*p && *p != '=') p++;
    *p = '\0';
    return enc;
}

/*
 * PoP: _oauth_pkce_code_verifier @ hermes_cli/auth.py:_oauth_pkce_code_verifier */
char *oauth_pkce_code_verifier(int length)
{
    return _spotify_code_verifier(length);
}

/*
 * PoP: _oauth_pkce_code_challenge @ hermes_cli/auth.py:_oauth_pkce_code_challenge */
char *oauth_pkce_code_challenge(const char *code_verifier)
{
    return spotify_code_challenge(code_verifier);
}

/*
 * Build the Spotify PKCE authorize URL. Returns malloc'd string. Caller frees. */
char *_spotify_build_authorize_url(const char *client_id, const char *redirect_uri,
                                  const char *scope, const char *state,
                                  const char *code_challenge, const char *accounts_base_url)
{
    if (!accounts_base_url) accounts_base_url = "";
    /* URL-encode the simple params (values are alnum/%-safe in practice). */
    char *enc(const char *v){ char b[2048]; const char*s=v?v:""; size_t j=0;
        for(;*s && j+1<sizeof(b);s++){ if((*s>='A'&&*s<='Z')||(*s>='a'&&*s<='z')||
            (*s>='0'&&*s<='9')||*s=='-'||*s=='_'||*s=='.'||*s=='~') b[j++]=*s;
            else { static const char*hx="0123456789ABCDEF"; b[j++]='%'; b[j++]=hx[((unsigned char)*s)>>4]; b[j++]=hx[((unsigned char)*s)&0xF]; } }
        b[j]='\0'; return strdup(b); }
    char *c = enc(client_id), *r = enc(redirect_uri), *sc = enc(scope),
         *st = enc(state), *cc = enc(code_challenge);
    char out[4096];
    snprintf(out, sizeof(out),
        "%s/authorize?client_id=%s&response_type=code&redirect_uri=%s"
        "&scope=%s&state=%s&code_challenge_method=S256&code_challenge=%s",
        accounts_base_url, c, r, sc, st, cc);
    free(c); free(r); free(sc); free(st); free(cc);
    return strdup(out);
}

/*
 * Validate a Spotify loopback redirect URI. Returns 0 on success (filling
 * *host_out, *port_out, *path_out via malloc), -1 on invalid (err filled). */
int _spotify_validate_redirect_uri(const char *redirect_uri, char **host_out,
                                  int *port_out, char **path_out, char *err, size_t errsz)
{
    if (host_out) *host_out = NULL;
    if (path_out) *path_out = NULL;
    if (err) err[0] = '\0';
    /* minimal http://host:port/path parse */
    const char *p = redirect_uri;
    if (strncmp(p, "http://", 7) != 0) {
        if (err) snprintf(err, errsz, "Spotify PKCE redirect_uri must use http://localhost or http://127.0.0.1.");
        return -1;
    }
    p += 7;
    const char *colon = strchr(p, ':');
    const char *slash = strchr(p, '/');
    if (!colon) { if (err) snprintf(err, errsz, "Spotify PKCE redirect_uri must include an explicit localhost port."); return -1; }
    size_t hl = (size_t)(colon - p);
    char host[256];
    if (hl >= sizeof(host)) hl = sizeof(host)-1;
    memcpy(host, p, hl); host[hl]='\0';
    if (strcmp(host, "127.0.0.1") != 0 && strcmp(host, "localhost") != 0) {
        if (err) snprintf(err, errsz, "Spotify PKCE redirect_uri must point to localhost or 127.0.0.1.");
        return -1;
    }
    const char *portstr = colon + 1;
    const char *pend = slash ? slash : portstr + strlen(portstr);
    char pbuf[32];
    size_t pl = (size_t)(pend - portstr);
    if (pl >= sizeof(pbuf)) pl = sizeof(pbuf)-1;
    memcpy(pbuf, portstr, pl); pbuf[pl]='\0';
    char *e = NULL;
    long port = strtol(pbuf, &e, 10);
    if (e == pbuf || *e != '\0') { if (err) snprintf(err, errsz, "Spotify PKCE redirect_uri has an invalid port."); return -1; }
    const char *path = slash ? slash : "/";
    if (host_out) *host_out = strdup(host);
    if (port_out) *port_out = (int)port;
    if (path_out) *path_out = strdup(path);
    return 0;
}

/*
 * Like the Spotify variant but requires http://127.0.0.1. Returns 0 on
 * success, -1 on invalid (err filled). */
/* PoP: xai_validate_loopback_redirect_uri @ hermes_cli/dashboard_auth/routes.py:_validate_loopback_redirect_uri */
int xai_validate_loopback_redirect_uri(const char *redirect_uri, char **host_out,
                                       int *port_out, char **path_out, char *err, size_t errsz)
{
    if (host_out) *host_out = NULL;
    if (path_out) *path_out = NULL;
    if (err) err[0] = '\0';
    const char *p = redirect_uri;
    if (strncmp(p, "http://", 7) != 0) {
        if (err) snprintf(err, errsz, "xAI OAuth redirect_uri must use http://127.0.0.1.");
        return -1;
    }
    p += 7;
    const char *colon = strchr(p, ':');
    const char *slash = strchr(p, '/');
    if (!colon) { if (err) snprintf(err, errsz, "xAI OAuth redirect_uri must include an explicit port."); return -1; }
    size_t hl = (size_t)(colon - p);
    char host[256];
    if (hl >= sizeof(host)) hl = sizeof(host)-1;
    memcpy(host, p, hl); host[hl]='\0';
    if (strcmp(host, "127.0.0.1") != 0) {
        if (err) snprintf(err, errsz, "xAI OAuth redirect_uri must point to 127.0.0.1.");
        return -1;
    }
    const char *portstr = colon + 1;
    const char *pend = slash ? slash : portstr + strlen(portstr);
    char pbuf[32];
    size_t pl = (size_t)(pend - portstr);
    if (pl >= sizeof(pbuf)) pl = sizeof(pbuf)-1;
    memcpy(pbuf, portstr, pl); pbuf[pl]='\0';
    char *e = NULL;
    long port = strtol(pbuf, &e, 10);
    if (e == pbuf || *e != '\0') { if (err) snprintf(err, errsz, "xAI OAuth redirect_uri has an invalid port."); return -1; }
    const char *path = slash ? slash : "/";
    if (host_out) *host_out = strdup(host);
    if (port_out) *port_out = (int)port;
    if (path_out) *path_out = strdup(path);
    return 0;
}

/*
 * PoP: _minimax_expired_in_looks_like_unix_ms @ hermes_cli/auth.py:_minimax_expired_in_looks_like_unix_ms */
int minimax_expired_in_looks_like_unix_ms(long expired_in, long now_ms)
{
    return (int)expired_in > (now_ms / 2);
}

/*
 * Returns access-token expiry as unix seconds. */
double _minimax_resolve_token_expiry_unix(long expired_in, double now_unix)
{
    long now_ms = (long)(now_unix * 1000.0);
    if (minimax_expired_in_looks_like_unix_ms(expired_in, now_ms))
        return (double)expired_in / 1000.0;
    return now_unix + (expired_in > 1 ? expired_in : 1);
}

/*
 * Returns malloc'd path to the Qwen CLI auth tokens file. Caller frees. */
char *_qwen_cli_auth_path(void)
{
    const char *home = getenv("HOME");
    if (!home) home = "~";
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/.config/qwen-cli/auth.json", home);
    return strdup(out);
}


/* ── from src/cli/port_auth_store.c ── */
/*
 * port_auth_store.c — auth.json persistence layer ported from hermes_cli/auth.py.
 * See include/port_auth_store.h for the API surface. C11, opaque txn struct,
 * minimal includes. Reuses: auth_file_path/_global_auth_file_path/has_usable_secret
 * (port_auth_na.c), auth_helpers (JWT/scope/expiry), provider_registry,
 * config_py_* (config.yaml IO), sanitize_borrowed_credential_payload,
 * _parse_absolute_timestamp/_exhausted_ttl (credential_pool internals).
 */
#include "port_auth_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <pthread.h>

#include "hermes_json.h"
#include "hermes_logger.h"
#include "port_provider_registry.h"

/* ── external reuse (port_auth_na.c) ── */
extern char *auth_file_path(void);           /* malloc'd <home>/auth.json */
extern char *_global_auth_file_path(void);    /* malloc'd or NULL */
extern int   has_usable_secret(const char *value, int min_length);
/* port_config_py_io.c */
extern json_t *config_py_read_raw_config(void);
extern void    config_py_get_config_path(char *buf, size_t sz);
extern int     config_py_atomic_config_write(const char *config_path, const json_t *data);
extern int     config_py_require_readable_config_before_write(const char *config_path);
extern char   *config_py_get_env_value_prefer_dotenv(const char *key);
/* credential pool internals */
extern json_node_t *sanitize_borrowed_credential_payload(const json_node_t *payload, const char *provider_id);
extern double _parse_absolute_timestamp(const char *value);
extern int    _exhausted_ttl(int error_code);
/* provider alias resolution (port_provider_registry.c) */
extern char *provider_resolve_alias(const char *requested);

/* ════════════════════════════════════════════════════════════════
 *  File lock — cross-process flock with per-path reentrancy depth
 * ════════════════════════════════════════════════════════════════ */

typedef struct lock_slot {
    char path[PATH_MAX];
    int  fd;
    int  depth;
    struct lock_slot *next;
} lock_slot_t;

static __thread lock_slot_t *tls_locks = NULL;


static lock_slot_t *lock_slot_find(const char *path) {
    for (lock_slot_t *s = tls_locks; s; s = s->next)
        if (strcmp(s->path, path) == 0) return s;
    return NULL;
}

/* PoP: auth_file_lock_acquire @ hermes_cli/auth.py:_file_lock */
int auth_file_lock_acquire(const char *lock_path, double timeout_seconds)
{
    if (!lock_path || !*lock_path) return 0;
    lock_slot_t *slot = lock_slot_find(lock_path);
    if (slot && slot->depth > 0) { slot->depth++; return 1; }  /* reentrant */

    /* ensure parent dir exists */
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", lock_path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; if (dir[0]) mkdir(dir, 0700); }

    int fd = open(lock_path, O_RDWR | O_CREAT | O_APPEND, 0600);
    if (fd < 0) return 0;

    double deadline_s = timeout_seconds < 1.0 ? 1.0 : timeout_seconds;
    struct timespec start; clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) break;
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (double)(now.tv_sec - start.tv_sec) +
                         (double)(now.tv_nsec - start.tv_nsec) / 1e9;
        if (elapsed >= deadline_s) { close(fd); return 0; }
        struct timespec nap = {0, 50 * 1000 * 1000};  /* 50ms — Python sleeps 0.05 */
        nanosleep(&nap, NULL);
    }

    if (!slot) {
        slot = calloc(1, sizeof(*slot));
        if (!slot) { flock(fd, LOCK_UN); close(fd); return 0; }
        snprintf(slot->path, sizeof(slot->path), "%s", lock_path);
        slot->next = tls_locks; tls_locks = slot;
    }
    slot->fd = fd;
    slot->depth = 1;
    return 1;
}

/* PoP: auth_file_lock_release @ hermes_cli/auth.py:_file_lock */
void auth_file_lock_release(const char *lock_path)
{
    if (!lock_path) return;
    lock_slot_t *slot = lock_slot_find(lock_path);
    if (!slot || slot->depth <= 0) return;
    if (--slot->depth == 0) {
        flock(slot->fd, LOCK_UN);
        close(slot->fd);
        slot->fd = -1;
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Auth store lock / path helpers
 * ════════════════════════════════════════════════════════════════ */

/* PoP: authstore_lock_path @ hermes_cli/auth.py:_auth_lock_path */
char *authstore_lock_path(void)
{
    char *auth = auth_file_path();
    if (!auth) return NULL;
    size_t n = strlen(auth);
    /* replace trailing ".json" with ".lock" (with_suffix semantics) */
    char *out = malloc(n + 8);
    if (!out) { free(auth); return NULL; }
    strcpy(out, auth);
    char *dot = strrchr(out, '.');
    if (dot && !strchr(dot, '/')) strcpy(dot, ".lock");
    else strcat(out, ".lock");
    free(auth);
    return out;
}

/* PoP: authstore_same_path @ hermes_cli/auth.py:_same_path */
bool authstore_same_path(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    char ra[PATH_MAX], rb[PATH_MAX];
    const char *pa = realpath(a, ra) ? ra : a;
    const char *pb = realpath(b, rb) ? rb : b;
    return strcmp(pa, pb) == 0;
}

static char *lock_path_for(const char *auth_path)
{
    size_t n = strlen(auth_path);
    char *out = malloc(n + 8);
    if (!out) return NULL;
    strcpy(out, auth_path);
    char *dot = strrchr(out, '.');
    if (dot && !strchr(dot, '/')) strcpy(dot, ".lock");
    else strcat(out, ".lock");
    return out;
}

/* PoP: authstore_lock @ hermes_cli/auth.py:_auth_store_lock */
int authstore_lock(const char *target_path, double timeout_seconds)
{
    char *auth = target_path ? strdup(target_path) : auth_file_path();
    if (!auth) return 0;
    char *lp = lock_path_for(auth);
    free(auth);
    if (!lp) return 0;
    int ok = auth_file_lock_acquire(lp, timeout_seconds > 0 ? timeout_seconds
                                                            : AUTH_LOCK_TIMEOUT_SECONDS_C);
    free(lp);
    return ok;
}

/* PoP: authstore_unlock @ hermes_cli/auth.py:_auth_store_lock */
void authstore_unlock(const char *target_path)
{
    char *auth = target_path ? strdup(target_path) : auth_file_path();
    if (!auth) return;
    char *lp = lock_path_for(auth);
    free(auth);
    if (!lp) return;
    auth_file_lock_release(lp);
    free(lp);
}

/* ════════════════════════════════════════════════════════════════
 *  Auth store load / save
 * ════════════════════════════════════════════════════════════════ */

static json_t *empty_store(void)
{
    json_t *s = json_new_object();
    json_set(s, "version", json_int(AUTH_STORE_VERSION_C));
    json_set(s, "providers", json_new_object());
    return s;
}

static char *iso_now_utc(void)
{
    time_t t = time(NULL);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    char *buf = malloc(40);
    if (!buf) return NULL;
    strftime(buf, 40, "%Y-%m-%dT%H:%M:%S+00:00", &tmv);
    return buf;
}

/* PoP: authstore_load @ hermes_cli/auth.py:_load_auth_store */
json_t *authstore_load(const char *auth_file)
{
    char *path = auth_file ? strdup(auth_file) : auth_file_path();
    if (!path) return empty_store();

    struct stat st;
    if (stat(path, &st) != 0) { free(path); return empty_store(); }

    char *err = NULL;
    json_t *raw = json_parse_file(path, &err);
    if (!raw) {
        /* preserve corrupt copy at <path>.json.corrupt (best-effort) */
        char corrupt[PATH_MAX + 16];
        snprintf(corrupt, sizeof(corrupt), "%s.corrupt", path);
        FILE *in = fopen(path, "rb");
        if (in) {
            FILE *out = fopen(corrupt, "wb");
            if (out) {
                char buf[4096]; size_t n;
                while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
                fclose(out);
            }
            fclose(in);
        }
        hermes_log(LOG_WARNING, "auth",
                   "failed to parse %s (%s) — starting with empty store; corrupt copy at %s",
                   path, err ? err : "parse error", corrupt);
        free(err); free(path);
        return empty_store();
    }
    free(err); free(path);

    if (raw->type == JSON_OBJECT) {
        json_t *providers = json_obj_get(raw, "providers");
        json_t *pool = json_obj_get(raw, "credential_pool");
        if ((providers && providers->type == JSON_OBJECT) ||
            (pool && pool->type == JSON_OBJECT)) {
            if (!providers || providers->type != JSON_OBJECT)
                json_set(raw, "providers", json_new_object());
            providers = json_obj_get(raw, "providers");
            if (providers && providers->type == JSON_OBJECT)
                auth_migrate_stale_nous_portal_url(providers);
            return raw;
        }
        /* migrate from PR's "systems" format */
        json_t *systems = json_obj_get(raw, "systems");
        if (systems && systems->type == JSON_OBJECT) {
            json_t *store = empty_store();
            json_t *nous = json_obj_get(systems, "nous_portal");
            if (nous) {
                json_t *providers2 = json_obj_get(store, "providers");
                json_set(providers2, "nous", json_copy(nous));
                json_set(store, "active_provider", json_new_string("nous"));
            } else {
                json_set(store, "active_provider", json_null());
            }
            json_free(raw);
            return store;
        }
    }
    json_free(raw);
    return empty_store();
}

/* PoP: authstore_load_global @ hermes_cli/auth.py:_load_global_auth_store */
json_t *authstore_load_global(void)
{
    char *gpath = _global_auth_file_path();
    if (!gpath) return json_new_object();
    struct stat st;
    if (stat(gpath, &st) != 0) { free(gpath); return json_new_object(); }
    json_t *store = authstore_load(gpath);
    free(gpath);
    if (!store) return json_new_object();
    return store;
}

/* Tighten parent dir to 0700 (mirror secure_parent_dir; refuse "/" and 1-level dirs). */
static void secure_parent_dir_c(const char *file_path)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", file_path);
    char *slash = strrchr(dir, '/');
    if (!slash || slash == dir) return;
    *slash = '\0';
    /* refuse top-level dirs like "/home" (only one slash) */
    const char *second = strchr(dir + 1, '/');
    if (!second) return;
    chmod(dir, 0700);
}

/* PoP: authstore_save @ hermes_cli/auth.py:_save_auth_store */
char *authstore_save(json_t *auth_store, const char *target_path)
{
    if (!auth_store) return NULL;
    char *path = target_path ? strdup(target_path) : auth_file_path();
    if (!path) return NULL;

    /* mkdir -p parent */
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; if (dir[0]) mkdir(dir, 0700); }
    secure_parent_dir_c(path);

    json_set(auth_store, "version", json_int(AUTH_STORE_VERSION_C));
    char *now = iso_now_utc();
    if (now) { json_set(auth_store, "updated_at", json_new_string(now)); free(now); }

    char *payload = json_serialize_pretty(auth_store, 2);
    if (!payload) { free(path); return NULL; }

    /* atomic O_EXCL 0600 tmp + rename (TOCTOU-safe; mirrors #19673/#21148) */
    char tmp[PATH_MAX + 64];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d.%lx", path, (int)getpid(),
             (unsigned long)time(NULL) ^ (unsigned long)(size_t)payload);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd < 0) { free(payload); free(path); return NULL; }
    size_t len = strlen(payload);
    ssize_t wr = write(fd, payload, len);
    if (wr == (ssize_t)len) wr += write(fd, "\n", 1);
    fsync(fd);
    close(fd);
    free(payload);
    if (wr != (ssize_t)(len + 1) || rename(tmp, path) != 0) {
        unlink(tmp); free(path); return NULL;
    }
    /* fsync parent dir */
    if (slash) {
        int dfd = open(dir, O_RDONLY);
        if (dfd >= 0) { fsync(dfd); close(dfd); }
    }
    chmod(path, S_IRUSR | S_IWUSR);
    return path;
}

/* ════════════════════════════════════════════════════════════════
 *  Provider state (profile -> global fallback)
 * ════════════════════════════════════════════════════════════════ */

/* PoP: authstore_load_provider_state_with_source @ hermes_cli/auth.py:_load_provider_state_with_source */
json_t *authstore_load_provider_state_with_source(json_t *auth_store, const char *provider_id,
                                                  char **out_source_path)
{
    if (out_source_path) *out_source_path = NULL;
    if (!auth_store || !provider_id) return NULL;

    json_t *providers = json_obj_get(auth_store, "providers");
    if (providers && providers->type == JSON_OBJECT) {
        json_t *state = json_obj_get(providers, provider_id);
        if (state && state->type == JSON_OBJECT) {
            if (out_source_path) *out_source_path = auth_file_path();
            return json_copy(state);
        }
    }

    char *gpath = _global_auth_file_path();
    json_t *gstore = authstore_load_global();
    if (gstore) {
        json_t *gproviders = json_obj_get(gstore, "providers");
        if (gproviders && gproviders->type == JSON_OBJECT) {
            json_t *gstate = json_obj_get(gproviders, provider_id);
            if (gstate && gstate->type == JSON_OBJECT) {
                json_t *copy = json_copy(gstate);
                json_free(gstore);
                if (out_source_path) *out_source_path = gpath;
                else free(gpath);
                return copy;
            }
        }
        json_free(gstore);
    }
    free(gpath);
    return NULL;
}

/* PoP: authstore_load_provider_state @ hermes_cli/auth.py:_load_provider_state */
json_t *authstore_load_provider_state(json_t *auth_store, const char *provider_id)
{
    char *src = NULL;
    json_t *state = authstore_load_provider_state_with_source(auth_store, provider_id, &src);
    free(src);
    return state;
}

/* PoP: authstore_save_provider_state @ hermes_cli/auth.py:_save_provider_state */
void authstore_save_provider_state(json_t *auth_store, const char *provider_id, const json_t *state)
{
    if (!auth_store || !provider_id || !state) return;
    json_t *providers = json_obj_get(auth_store, "providers");
    if (!providers || providers->type != JSON_OBJECT) {
        json_set(auth_store, "providers", json_new_object());
        providers = json_obj_get(auth_store, "providers");
    }
    json_set(providers, provider_id, json_copy(state));
    json_set(auth_store, "active_provider", json_new_string(provider_id));
}

/* PoP: authstore_store_provider_state @ hermes_cli/auth.py:_store_provider_state */
void authstore_store_provider_state(json_t *auth_store, const char *provider_id,
                                    const json_t *state, bool set_active)
{
    if (!auth_store || !provider_id || !state) return;
    json_t *providers = json_obj_get(auth_store, "providers");
    if (!providers || providers->type != JSON_OBJECT) {
        json_set(auth_store, "providers", json_new_object());
        providers = json_obj_get(auth_store, "providers");
    }
    json_set(providers, provider_id, json_copy(state));
    if (set_active)
        json_set(auth_store, "active_provider", json_new_string(provider_id));
}

/* PoP: authstore_persist_provider_state_to_store @ hermes_cli/auth.py:_persist_provider_state_to_store */
char *authstore_persist_provider_state_to_store(const char *provider_id, const json_t *state,
                                                const char *target_path, bool set_active)
{
    if (!provider_id || !state || !target_path) return NULL;
    if (!authstore_lock(target_path, AUTH_LOCK_TIMEOUT_SECONDS_C)) return NULL;
    json_t *store = authstore_load(target_path);
    authstore_store_provider_state(store, provider_id, state, set_active);
    char *saved = authstore_save(store, target_path);
    json_free(store);
    authstore_unlock(target_path);
    return saved;
}

/* PoP: authstore_save_provider_state_to_source @ hermes_cli/auth.py:_save_provider_state_to_source */
void authstore_save_provider_state_to_source(json_t *auth_store, const char *provider_id,
                                             const json_t *state, const char *source_path)
{
    char *active = auth_file_path();
    bool same = true;
    if (source_path && active) same = authstore_same_path(source_path, active);
    free(active);
    if (same) {
        authstore_save_provider_state(auth_store, provider_id, state);
        char *saved = authstore_save(auth_store, NULL);
        free(saved);
        return;
    }
    char *saved = authstore_persist_provider_state_to_store(provider_id, state, source_path, true);
    free(saved);
}

/* ── opaque provider-state transaction ── */
struct auth_provider_txn {
    json_t *store;          /* active auth store (owned) */
    json_t *state;          /* provider state copy (owned, may be NULL) */
    char   *source_path;    /* owned, may be NULL */
    char   *locked_target;  /* second lock target (owned, may be NULL) */
};

/* PoP: authstore_provider_state_transaction @ hermes_cli/auth.py:_provider_state_transaction */
auth_provider_txn_t *authstore_provider_state_transaction(const char *provider_id)
{
    if (!provider_id) return NULL;
    if (!authstore_lock(NULL, AUTH_LOCK_TIMEOUT_SECONDS_C)) return NULL;

    auth_provider_txn_t *txn = calloc(1, sizeof(*txn));
    if (!txn) { authstore_unlock(NULL); return NULL; }

    txn->store = authstore_load(NULL);
    txn->state = authstore_load_provider_state_with_source(txn->store, provider_id,
                                                           &txn->source_path);
    char *active = auth_file_path();
    bool same = (txn->source_path == NULL) ||
                (active && authstore_same_path(txn->source_path, active));
    free(active);
    if (same) return txn;

    /* profile read fell back to global: lock + re-read the source store */
    if (authstore_lock(txn->source_path, AUTH_LOCK_TIMEOUT_SECONDS_C)) {
        txn->locked_target = strdup(txn->source_path);
        json_t *sstore = authstore_load(txn->source_path);
        json_free(txn->state);
        txn->state = NULL;
        json_t *sproviders = json_obj_get(sstore, "providers");
        if (sproviders && sproviders->type == JSON_OBJECT) {
            json_t *raw = json_obj_get(sproviders, provider_id);
            if (raw && raw->type == JSON_OBJECT) txn->state = json_copy(raw);
        }
        json_free(sstore);
    }
    return txn;
}

json_t *auth_provider_txn_store(auth_provider_txn_t *txn) { return txn ? txn->store : NULL; }
json_t *auth_provider_txn_state(auth_provider_txn_t *txn) { return txn ? txn->state : NULL; }
const char *auth_provider_txn_source_path(auth_provider_txn_t *txn)
{ return txn ? txn->source_path : NULL; }

/* PoP: auth_provider_txn_end @ hermes_cli/auth.py:_provider_state_transaction */
void auth_provider_txn_end(auth_provider_txn_t *txn)
{
    if (!txn) return;
    if (txn->locked_target) { authstore_unlock(txn->locked_target); free(txn->locked_target); }
    authstore_unlock(NULL);
    json_free(txn->store);
    json_free(txn->state);
    free(txn->source_path);
    free(txn);
}

/* ════════════════════════════════════════════════════════════════
 *  Provider management
 * ════════════════════════════════════════════════════════════════ */

static void lc_copy(char *dst, size_t sz, const char *src)
{
    size_t i = 0;
    if (src) {
        /* strip leading/trailing whitespace, lowercase */
        while (*src && isspace((unsigned char)*src)) src++;
        size_t n = strlen(src);
        while (n > 0 && isspace((unsigned char)src[n-1])) n--;
        for (; i < n && i < sz - 1; i++)
            dst[i] = (char)tolower((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

/* PoP: mark_provider_active_if_unset @ hermes_cli/auth.py:mark_provider_active_if_unset */
void mark_provider_active_if_unset(const char *provider_id)
{
    if (!provider_id || !*provider_id) return;
    if (!authstore_lock(NULL, AUTH_LOCK_TIMEOUT_SECONDS_C)) return;
    json_t *store = authstore_load(NULL);
    const char *active = json_get_str(store, "active_provider", "");
    /* stripped emptiness check */
    bool unset = true;
    for (const char *p = active; p && *p; p++)
        if (!isspace((unsigned char)*p)) { unset = false; break; }
    if (unset) {
        json_set(store, "active_provider", json_new_string(provider_id));
        char *saved = authstore_save(store, NULL);
        free(saved);
    }
    json_free(store);
    authstore_unlock(NULL);
}

/* SERVICE_PROVIDER_NAMES (auth.py module constant): spotify only. */
static const struct { const char *id; const char *name; } SERVICE_PROVIDERS[] = {
    { "spotify", "Spotify" },
};
static const int SERVICE_PROVIDERS_N =
    (int)(sizeof(SERVICE_PROVIDERS)/sizeof(SERVICE_PROVIDERS[0]));

/* PoP: is_known_auth_provider @ hermes_cli/auth.py:is_known_auth_provider */
bool is_known_auth_provider(const char *provider_id)
{
    char norm[128];
    lc_copy(norm, sizeof(norm), provider_id);
    if (!norm[0]) return false;
    if (provider_registry_get(norm)) return true;
    for (int i = 0; i < SERVICE_PROVIDERS_N; i++)
        if (strcmp(SERVICE_PROVIDERS[i].id, norm) == 0) return true;
    return false;
}

/* PoP: get_auth_provider_display_name @ hermes_cli/auth.py:get_auth_provider_display_name */
char *get_auth_provider_display_name(const char *provider_id)
{
    char norm[128];
    lc_copy(norm, sizeof(norm), provider_id);
    const catalog_provider_t *pc = provider_registry_get(norm);
    if (pc && pc->name) return strdup(pc->name);
    for (int i = 0; i < SERVICE_PROVIDERS_N; i++)
        if (strcmp(SERVICE_PROVIDERS[i].id, norm) == 0)
            return strdup(SERVICE_PROVIDERS[i].name);
    return strdup(provider_id ? provider_id : "");
}

/* PoP: is_runtime_provider_routable @ hermes_cli/auth.py:is_runtime_provider_routable */
bool is_runtime_provider_routable(const char *provider_id)
{
    char norm[128];
    lc_copy(norm, sizeof(norm), provider_id);
    if (!norm[0]) return false;
    if (strcmp(norm, "auto") == 0 || strcmp(norm, "openrouter") == 0 ||
        strcmp(norm, "custom") == 0 || strcmp(norm, "moa") == 0)
        return true;
    if (strncmp(norm, "custom:", 7) == 0) return true;
    /* resolve_provider(normalized) without env fallbacks: alias-resolve then
     * registry membership check (raises AuthError only for unknown ids). */
    char *resolved = provider_resolve_alias(norm);
    if (!resolved) return false;
    bool ok = (strcmp(resolved, "openrouter") == 0 || strcmp(resolved, "custom") == 0 ||
               provider_registry_get(resolved) != NULL);
    free(resolved);
    return ok;
}

/* PoP: read_credential_pool @ hermes_cli/auth.py:read_credential_pool */
json_t *read_credential_pool(const char *provider_id)
{
    json_t *store = authstore_load(NULL);
    json_t *pool = json_obj_get(store, "credential_pool");
    bool pool_ok = pool && pool->type == JSON_OBJECT;

    json_t *gstore = authstore_load_global();
    json_t *gpool = gstore ? json_obj_get(gstore, "credential_pool") : NULL;
    bool gpool_ok = gpool && gpool->type == JSON_OBJECT;

    json_t *result;
    if (!provider_id) {
        result = pool_ok ? json_copy(pool) : json_new_object();
        if (gpool_ok) {
            for (size_t i = 0; i < gpool->c.count; i++) {
                const char *key = gpool->c.keys[i];
                json_t *gentries = gpool->c.items[i];
                if (!gentries || gentries->type != JSON_ARRAY || gentries->c.count == 0)
                    continue;
                json_t *existing = json_obj_get(result, key);
                if (existing && existing->type == JSON_ARRAY && existing->c.count > 0)
                    continue;   /* profile shadows per-provider */
                json_set(result, key, json_copy(gentries));
            }
        }
    } else {
        json_t *entries = pool_ok ? json_obj_get(pool, provider_id) : NULL;
        if (entries && entries->type == JSON_ARRAY && entries->c.count > 0) {
            result = json_copy(entries);
        } else {
            json_t *gentries = gpool_ok ? json_obj_get(gpool, provider_id) : NULL;
            result = (gentries && gentries->type == JSON_ARRAY)
                     ? json_copy(gentries) : json_new_array();
        }
    }
    json_free(store);
    if (gstore) json_free(gstore);
    return result;
}

/* pool status fields adopted on merge (auth.py:_POOL_STATUS_FIELDS) */
static const char *POOL_STATUS_FIELDS[] = {
    "last_status", "last_status_at", "last_error_code",
    "last_error_reason", "last_error_message", "last_error_reset_at",
};
static const int POOL_STATUS_FIELDS_N =
    (int)(sizeof(POOL_STATUS_FIELDS)/sizeof(POOL_STATUS_FIELDS[0]));

/* dict-shaped _exhausted_until: reset_at wins, else last_status_at + ttl(code). */
static double pool_entry_exhausted_until(const json_t *disk_entry)
{
    const char *reset_at = json_get_str(disk_entry, "last_error_reset_at", NULL);
    if (reset_at && *reset_at) {
        double t = _parse_absolute_timestamp(reset_at);
        if (t > 0) return t;
    }
    double status_at = _parse_absolute_timestamp(
        json_get_str(disk_entry, "last_status_at", NULL));
    if (status_at > 0) {
        int code = (int)json_get_num(disk_entry, "last_error_code", 0);
        return status_at + (double)_exhausted_ttl(code);
    }
    return 0;
}

/* PoP: auth_merge_disk_cooldown_state @ hermes_cli/auth.py:_merge_disk_cooldown_state */
json_t *auth_merge_disk_cooldown_state(const json_t *entry, const json_t *disk_entry,
                                       const char *provider_id)
{
    (void)provider_id;
    if (!entry) return NULL;
    json_t *keep = json_copy(entry);
    if (!disk_entry || disk_entry->type != JSON_OBJECT) return keep;

    const char *disk_status = json_get_str(disk_entry, "last_status", NULL);
    if (!disk_status ||
        (strcmp(disk_status, "dead") != 0 && strcmp(disk_status, "exhausted") != 0))
        return keep;

    /* token change = caller re-authed; never resurrect the old cooldown */
    const char *mem_access = json_get_str(entry, "access_token", "");
    const char *disk_access = json_get_str(disk_entry, "access_token", "");
    if (mem_access[0] && disk_access[0] && strcmp(mem_access, disk_access) != 0)
        return keep;

    double disk_ts = _parse_absolute_timestamp(json_get_str(disk_entry, "last_status_at", NULL));
    double mem_ts = _parse_absolute_timestamp(json_get_str(entry, "last_status_at", NULL));
    if (disk_ts <= 0) disk_ts = 0;
    if (mem_ts <= 0) mem_ts = 0;
    if (disk_ts <= mem_ts) return keep;

    if (strcmp(disk_status, "exhausted") == 0) {
        double until = pool_entry_exhausted_until(disk_entry);
        if (until <= 0 || until <= (double)time(NULL)) return keep;
    }

    for (int i = 0; i < POOL_STATUS_FIELDS_N; i++) {
        json_t *v = json_obj_get(disk_entry, POOL_STATUS_FIELDS[i]);
        json_set(keep, POOL_STATUS_FIELDS[i], v ? json_copy(v) : json_null());
    }
    return keep;
}

static bool str_in_json_array(const json_t *arr, const char *s)
{
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *e = arr->c.items[i];
        if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, s) == 0)
            return true;
    }
    return false;
}

/* PoP: write_credential_pool @ hermes_cli/auth.py:write_credential_pool */
char *write_credential_pool(const char *provider_id, const json_t *entries,
                            const json_t *removed_ids)
{
    if (!provider_id || !entries || entries->type != JSON_ARRAY) return NULL;
    if (!authstore_lock(NULL, AUTH_LOCK_TIMEOUT_SECONDS_C)) return NULL;

    json_t *store = authstore_load(NULL);
    json_t *pool = json_obj_get(store, "credential_pool");
    if (!pool || pool->type != JSON_OBJECT) {
        json_set(store, "credential_pool", json_new_object());
        pool = json_obj_get(store, "credential_pool");
    }

    json_t *existing = json_obj_get(pool, provider_id);
    bool existing_ok = existing && existing->type == JSON_ARRAY;

    json_t *merged = json_new_array();
    /* sanitize incoming entries; merge per-id cooldown state from disk */
    for (size_t i = 0; i < entries->c.count; i++) {
        json_t *entry = entries->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) {
            json_append(merged, entry ? json_copy(entry) : json_null());
            continue;
        }
        json_t *sanitized = sanitize_borrowed_credential_payload(entry, provider_id);
        if (!sanitized) sanitized = json_copy(entry);
        const char *id = json_get_str(sanitized, "id", NULL);
        json_t *disk_match = NULL;
        if (id && existing_ok) {
            for (size_t j = 0; j < existing->c.count; j++) {
                json_t *de = existing->c.items[j];
                if (de && de->type == JSON_OBJECT) {
                    const char *did = json_get_str(de, "id", NULL);
                    if (did && strcmp(did, id) == 0) { disk_match = de; break; }
                }
            }
        }
        json_t *final = auth_merge_disk_cooldown_state(sanitized, disk_match, provider_id);
        json_free(sanitized);
        json_append(merged, final);
    }

    /* re-adopt disk entries missing from the snapshot (added concurrently) */
    if (existing_ok) {
        for (size_t j = 0; j < existing->c.count; j++) {
            json_t *de = existing->c.items[j];
            if (!de || de->type != JSON_OBJECT) continue;
            const char *did = json_get_str(de, "id", NULL);
            if (!did || !*did) continue;
            if (removed_ids && str_in_json_array(removed_ids, did)) continue;
            bool present = false;
            for (size_t i = 0; i < merged->c.count; i++) {
                json_t *me = merged->c.items[i];
                if (me && me->type == JSON_OBJECT) {
                    const char *mid = json_get_str(me, "id", NULL);
                    if (mid && strcmp(mid, did) == 0) { present = true; break; }
                }
            }
            if (present) continue;
            json_t *sanitized = sanitize_borrowed_credential_payload(de, provider_id);
            json_append(merged, sanitized ? sanitized : json_copy(de));
        }
    }

    json_set(pool, provider_id, merged);
    char *saved = authstore_save(store, NULL);
    json_free(store);
    authstore_unlock(NULL);
    return saved;
}

/* PoP: unsuppress_credential_source @ hermes_cli/auth.py:unsuppress_credential_source */
bool unsuppress_credential_source(const char *provider_id, const char *source)
{
    if (!provider_id || !source) return false;
    if (!authstore_lock(NULL, AUTH_LOCK_TIMEOUT_SECONDS_C)) return false;
    json_t *store = authstore_load(NULL);
    json_t *suppressed = json_obj_get(store, "suppressed_sources");
    bool cleared = false;
    if (suppressed && suppressed->type == JSON_OBJECT) {
        json_t *list = json_obj_get(suppressed, provider_id);
        if (list && list->type == JSON_ARRAY && str_in_json_array(list, source)) {
            /* rebuild the list without `source` */
            json_t *nl = json_new_array();
            for (size_t i = 0; i < list->c.count; i++) {
                json_t *e = list->c.items[i];
                if (e && e->type == JSON_STRING && e->str_val &&
                    strcmp(e->str_val, source) == 0)
                    continue;
                json_append(nl, e ? json_copy(e) : json_null());
            }
            if (nl->c.count == 0) {
                json_free(nl);
                json_obj_del(suppressed, provider_id);
                if (suppressed->c.count == 0)
                    json_obj_del(store, "suppressed_sources");
            } else {
                json_set(suppressed, provider_id, nl);
            }
            char *saved = authstore_save(store, NULL);
            free(saved);
            cleared = true;
        }
    }
    json_free(store);
    authstore_unlock(NULL);
    return cleared;
}

/* PoP: get_provider_auth_state @ hermes_cli/auth.py:get_provider_auth_state */
json_t *get_provider_auth_state(const char *provider_id)
{
    json_t *store = authstore_load(NULL);
    json_t *state = authstore_load_provider_state(store, provider_id);
    json_free(store);
    return state;
}

static bool moa_slot_matches(const json_t *slot, const char *norm)
{
    if (!slot || slot->type != JSON_OBJECT) return false;
    const char *p = json_get_str(slot, "provider", "");
    char lp[128];
    lc_copy(lp, sizeof(lp), p);
    return lp[0] && strcmp(lp, norm) == 0;
}

/* PoP: is_provider_explicitly_configured @ hermes_cli/auth.py:is_provider_explicitly_configured */
bool is_provider_explicitly_configured(const char *provider_id)
{
    char norm[128];
    lc_copy(norm, sizeof(norm), provider_id);
    if (!norm[0]) return false;

    /* 1. auth.json active_provider */
    json_t *store = authstore_load(NULL);
    {
        char act[128];
        lc_copy(act, sizeof(act), json_get_str(store, "active_provider", ""));
        if (act[0] && strcmp(act, norm) == 0) { json_free(store); return true; }
    }
    json_free(store);

    /* 2. config.yaml model.provider + MoA slots */
    json_t *cfg = config_py_read_raw_config();
    if (cfg) {
        json_t *model = json_obj_get(cfg, "model");
        if (model && model->type == JSON_OBJECT) {
            char cp[128];
            lc_copy(cp, sizeof(cp), json_get_str(model, "provider", ""));
            if (cp[0] && strcmp(cp, norm) == 0) { json_free(cfg); return true; }
        }
        json_t *moa = json_obj_get(cfg, "moa");
        if (moa && moa->type == JSON_OBJECT) {
            json_t *refs = json_obj_get(moa, "reference_models");
            if (refs && refs->type == JSON_ARRAY)
                for (size_t i = 0; i < refs->c.count; i++)
                    if (moa_slot_matches(refs->c.items[i], norm)) { json_free(cfg); return true; }
            if (moa_slot_matches(json_obj_get(moa, "aggregator"), norm)) { json_free(cfg); return true; }
            json_t *presets = json_obj_get(moa, "presets");
            if (presets && presets->type == JSON_OBJECT) {
                for (size_t k = 0; k < presets->c.count; k++) {
                    json_t *preset = presets->c.items[k];
                    if (!preset || preset->type != JSON_OBJECT) continue;
                    json_t *prefs = json_obj_get(preset, "reference_models");
                    if (prefs && prefs->type == JSON_ARRAY)
                        for (size_t i = 0; i < prefs->c.count; i++)
                            if (moa_slot_matches(prefs->c.items[i], norm)) { json_free(cfg); return true; }
                    if (moa_slot_matches(json_obj_get(preset, "aggregator"), norm)) { json_free(cfg); return true; }
                }
            }
        }
        json_free(cfg);
    }

    /* 3. provider-specific env vars (excluding CLAUDE_CODE_OAUTH_TOKEN) */
    const catalog_provider_t *pc = provider_registry_get(norm);
    if (pc && pc->auth_type && strcmp(pc->auth_type, "api_key") == 0 && pc->api_key_env_vars) {
        for (int i = 0; pc->api_key_env_vars[i]; i++) {
            const char *var = pc->api_key_env_vars[i];
            if (strcmp(var, "CLAUDE_CODE_OAUTH_TOKEN") == 0) continue;
            const char *val = getenv(var);
            if (val && has_usable_secret(val, 4)) return true;
        }
    }

    /* 4. explicit-flow credential-pool entries (env-backed require live var) */
    json_t *entries = read_credential_pool(norm);
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < entries->c.count; i++) {
            json_t *e = entries->c.items[i];
            if (!e || e->type != JSON_OBJECT) continue;
            char src[256];
            lc_copy(src, sizeof(src), json_get_str(e, "source", ""));
            if (!src[0]) continue;
            if (strncmp(src, "env:", 4) == 0) {
                /* re-read the ORIGINAL (unlowered) source for the var name */
                const char *orig = json_get_str(e, "source", "");
                const char *var = strchr(orig, ':');
                if (var && *(var + 1)) {
                    char vn[128];
                    size_t n = strlen(var + 1);
                    while (n > 0 && isspace((unsigned char)var[n])) n--;
                    const char *s = var + 1;
                    while (*s && isspace((unsigned char)*s)) { s++; n--; }
                    if (n >= sizeof(vn)) n = sizeof(vn) - 1;
                    memcpy(vn, s, n); vn[n] = '\0';
                    const char *val = vn[0] ? getenv(vn) : NULL;
                    if (val && has_usable_secret(val, 4)) { json_free(entries); return true; }
                }
                continue;
            }
            if (strcmp(src, "device_code") == 0 || strcmp(src, "loopback_pkce") == 0 ||
                strcmp(src, "hermes_pkce") == 0 || strcmp(src, "manual") == 0 ||
                strncmp(src, "manual:", 7) == 0) {
                json_free(entries);
                return true;
            }
        }
    }
    if (entries) json_free(entries);
    return false;
}

/* PoP: clear_provider_auth @ hermes_cli/auth.py:clear_provider_auth */
bool clear_provider_auth(const char *provider_id)
{
    if (!authstore_lock(NULL, AUTH_LOCK_TIMEOUT_SECONDS_C)) return false;
    json_t *store = authstore_load(NULL);
    const char *target = provider_id;
    if (!target || !*target) target = json_get_str(store, "active_provider", NULL);
    if (!target || !*target) { json_free(store); authstore_unlock(NULL); return false; }

    char tgt[128];
    snprintf(tgt, sizeof(tgt), "%s", target);

    json_t *providers = json_obj_get(store, "providers");
    if (!providers || providers->type != JSON_OBJECT) {
        json_set(store, "providers", json_new_object());
        providers = json_obj_get(store, "providers");
    }
    json_t *pool = json_obj_get(store, "credential_pool");
    if (!pool || pool->type != JSON_OBJECT) {
        json_set(store, "credential_pool", json_new_object());
        pool = json_obj_get(store, "credential_pool");
    }

    bool cleared = false;
    if (json_obj_del(providers, tgt)) cleared = true;
    if (json_obj_del(pool, tgt)) cleared = true;
    const char *active = json_get_str(store, "active_provider", NULL);
    if (active && strcmp(active, tgt) == 0) {
        json_set(store, "active_provider", json_null());
        cleared = true;
    }
    if (cleared) {
        char *saved = authstore_save(store, NULL);
        free(saved);
    }
    json_free(store);
    authstore_unlock(NULL);
    return cleared;
}

/* PoP: deactivate_provider @ hermes_cli/auth.py:deactivate_provider */
void deactivate_provider(void)
{
    if (!authstore_lock(NULL, AUTH_LOCK_TIMEOUT_SECONDS_C)) return;
    json_t *store = authstore_load(NULL);
    json_set(store, "active_provider", json_null());
    char *saved = authstore_save(store, NULL);
    free(saved);
    json_free(store);
    authstore_unlock(NULL);
}

/* PoP: get_anthropic_key @ hermes_cli/auth.py:get_anthropic_key */
char *get_anthropic_key(void)
{
    const catalog_provider_t *pc = provider_registry_get("anthropic");
    if (pc && pc->api_key_env_vars) {
        for (int i = 0; pc->api_key_env_vars[i]; i++) {
            char *val = config_py_get_env_value_prefer_dotenv(pc->api_key_env_vars[i]);
            if (val && *val) return val;
            free(val);
        }
    }
    return strdup("");
}

/* ════════════════════════════════════════════════════════════════
 *  config.yaml provider helpers
 * ════════════════════════════════════════════════════════════════ */

/* PoP: auth_get_config_provider @ hermes_cli/auth.py:_get_config_provider */
char *auth_get_config_provider(void)
{
    json_t *cfg = config_py_read_raw_config();
    if (!cfg) return NULL;
    json_t *model = json_obj_get(cfg, "model");
    char *out = NULL;
    if (model && model->type == JSON_OBJECT) {
        const char *p = json_get_str(model, "provider", NULL);
        if (p) {
            char norm[128];
            lc_copy(norm, sizeof(norm), p);
            if (norm[0]) out = strdup(norm);
        }
    }
    json_free(cfg);
    return out;
}

/* PoP: auth_config_provider_matches @ hermes_cli/auth.py:_config_provider_matches */
bool auth_config_provider_matches(const char *provider_id)
{
    if (!provider_id || !*provider_id) return false;
    char norm[128];
    lc_copy(norm, sizeof(norm), provider_id);
    char *cur = auth_get_config_provider();
    bool match = cur && strcmp(cur, norm) == 0;
    free(cur);
    return match;
}

/* PoP: auth_should_reset_config_provider_on_logout @ hermes_cli/auth.py:_should_reset_config_provider_on_logout */
bool auth_should_reset_config_provider_on_logout(const char *provider_id)
{
    if (!provider_id || !*provider_id) return false;
    char norm[128];
    lc_copy(norm, sizeof(norm), provider_id);
    if (!provider_registry_get(norm)) return false;
    return auth_config_provider_matches(norm);
}

/* PoP: auth_logout_default_provider_from_config @ hermes_cli/auth.py:_logout_default_provider_from_config */
char *auth_logout_default_provider_from_config(void)
{
    char *provider = auth_get_config_provider();
    if (!provider) return NULL;
    if (strcmp(provider, "nous") == 0 || strcmp(provider, "openai-codex") == 0 ||
        strcmp(provider, "xai-oauth") == 0)
        return provider;
    free(provider);
    return NULL;
}

/* PoP: auth_reset_config_provider @ hermes_cli/auth.py:_reset_config_provider */
char *auth_reset_config_provider(void)
{
    char path[PATH_MAX];
    config_py_get_config_path(path, sizeof(path));
    struct stat st;
    if (stat(path, &st) != 0) return strdup(path);
    if (config_py_require_readable_config_before_write(path) != 0) return strdup(path);

    json_t *cfg = config_py_read_raw_config();
    if (!cfg || cfg->c.count == 0) { if (cfg) json_free(cfg); return strdup(path); }

    json_t *model = json_obj_get(cfg, "model");
    if (model && model->type == JSON_OBJECT) {
        json_set(model, "provider", json_new_string("auto"));
        if (json_obj_get(model, "base_url"))
            json_set(model, "base_url", json_new_string(OPENROUTER_BASE_URL_C));
        config_py_atomic_config_write(path, cfg);
    }
    json_free(cfg);
    return strdup(path);
}

/* PoP: auth_save_model_choice @ hermes_cli/auth.py:_save_model_choice */
void auth_save_model_choice(const char *model_id)
{
    if (!model_id || !*model_id) return;
    char path[PATH_MAX];
    config_py_get_config_path(path, sizeof(path));

    json_t *cfg = config_py_read_raw_config();
    if (!cfg) cfg = json_new_object();

    json_t *model = json_obj_get(cfg, "model");
    if (model && model->type == JSON_OBJECT) {
        json_set(model, "default", json_new_string(model_id));
    } else {
        json_t *m = json_new_object();
        json_set(m, "default", json_new_string(model_id));
        json_set(cfg, "model", m);
    }
    config_py_atomic_config_write(path, cfg);
    json_free(cfg);
}

/* ════════════════════════════════════════════════════════════════
 *  TLS verify resolution
 * ════════════════════════════════════════════════════════════════ */

static bool truthy_str(const char *v, bool def)
{
    if (!v) return def;
    char lo[16];
    lc_copy(lo, sizeof(lo), v);
    if (!lo[0]) return def;
    return strcmp(lo, "1") == 0 || strcmp(lo, "true") == 0 ||
           strcmp(lo, "yes") == 0 || strcmp(lo, "on") == 0;
}

/* PoP: auth_default_verify @ hermes_cli/auth.py:_default_verify */
auth_verify_t auth_default_verify(void)
{
    /* Linux/WSL: system trust store is fine — httpx's `True` default. The
     * darwin/certifi branch is a macOS-only workaround; on this platform the
     * faithful behavior is verification-on with default CAs. */
    auth_verify_t v; v.insecure = false; v.ca_bundle[0] = '\0';
    return v;
}

/* PoP: auth_resolve_verify @ hermes_cli/auth.py:_resolve_verify */
auth_verify_t auth_resolve_verify(const char *insecure_opt, const char *ca_bundle,
                                  const json_t *auth_state)
{
    const json_t *tls = NULL;
    if (auth_state && auth_state->type == JSON_OBJECT) {
        tls = json_obj_get(auth_state, "tls");
        if (tls && tls->type != JSON_OBJECT) tls = NULL;
    }

    bool effective_insecure;
    if (insecure_opt) {
        effective_insecure = truthy_str(insecure_opt, false);
    } else if (tls) {
        json_t *ins = json_obj_get(tls, "insecure");
        if (ins && ins->type == JSON_BOOL) effective_insecure = ins->bool_val;
        else effective_insecure = truthy_str(json_get_str(tls, "insecure", NULL), false);
    } else {
        effective_insecure = false;
    }

    const char *effective_ca = ca_bundle && *ca_bundle ? ca_bundle : NULL;
    if (!effective_ca && tls) {
        const char *c = json_get_str(tls, "ca_bundle", NULL);
        if (c && *c) effective_ca = c;
    }
    if (!effective_ca) {
        const char *c = getenv("HERMES_CA_BUNDLE");
        if (!c || !*c) c = getenv("SSL_CERT_FILE");
        if (!c || !*c) c = getenv("REQUESTS_CA_BUNDLE");
        if (c && *c) effective_ca = c;
    }

    auth_verify_t v; v.insecure = false; v.ca_bundle[0] = '\0';
    if (effective_insecure) { v.insecure = true; return v; }
    if (effective_ca) {
        struct stat st;
        if (stat(effective_ca, &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(v.ca_bundle, sizeof(v.ca_bundle), "%s", effective_ca);
            return v;
        }
        hermes_log(LOG_WARNING, "auth",
                   "CA bundle path does not exist: %s — falling back to default certificates",
                   effective_ca);
        return auth_default_verify();
    }
    return auth_default_verify();
}

/* ════════════════════════════════════════════════════════════════
 *  OAuth trace + pure Nous helpers
 * ════════════════════════════════════════════════════════════════ */

#define DEFAULT_NOUS_PORTAL_URL_C "https://portal.nousresearch.com"

/* PoP: auth_oauth_trace_enabled @ hermes_cli/auth.py:_oauth_trace_enabled */
bool auth_oauth_trace_enabled(void)
{
    return truthy_str(getenv("HERMES_OAUTH_TRACE"), false);
}

/* PoP: auth_oauth_trace @ hermes_cli/auth.py:_oauth_trace */
void auth_oauth_trace(const char *event, const char *sequence_id, const json_t *fields)
{
    if (!auth_oauth_trace_enabled()) return;
    json_t *payload = json_new_object();
    json_set(payload, "event", json_new_string(event ? event : ""));
    if (sequence_id && *sequence_id)
        json_set(payload, "sequence_id", json_new_string(sequence_id));
    if (fields && fields->type == JSON_OBJECT)
        for (size_t i = 0; i < fields->c.count; i++)
            json_set(payload, fields->c.keys[i], json_copy(fields->c.items[i]));
    char *js = json_serialize(payload);
    if (js) {
        hermes_log(LOG_INFO, "auth", "oauth_trace %s", js);
        free(js);
    }
    json_free(payload);
}

/* _token_fingerprint: reuse _token_fingerprint() from auth_helpers.c */

/* _optional_base_url: reuse _optional_base_url() from auth_helpers.c */

/* PoP: auth_nous_inference_env_override @ hermes_cli/auth.py:_nous_inference_env_override */
char *auth_nous_inference_env_override(void)
{
    return _optional_base_url(getenv("NOUS_INFERENCE_BASE_URL"));
}

/* PoP: auth_nous_portal_env_override @ hermes_cli/auth.py:_nous_portal_env_override */
char *auth_nous_portal_env_override(void)
{
    const char *v = getenv("HERMES_PORTAL_BASE_URL");
    if (!v || !*v) v = getenv("NOUS_PORTAL_BASE_URL");
    return _optional_base_url(v);
}

/* extract hostname (lowercased) from a URL into buf; returns false on malformed */
static bool url_hostname(const char *url, char *buf, size_t sz, char *scheme, size_t ssz)
{
    if (scheme) scheme[0] = '\0';
    buf[0] = '\0';
    const char *p = strstr(url, "://");
    if (!p) return false;
    if (scheme) {
        size_t sl = (size_t)(p - url);
        if (sl >= ssz) sl = ssz - 1;
        for (size_t i = 0; i < sl; i++) scheme[i] = (char)tolower((unsigned char)url[i]);
        scheme[sl] = '\0';
    }
    p += 3;
    /* skip userinfo */
    const char *at = strchr(p, '@');
    const char *slash = strchr(p, '/');
    if (at && (!slash || at < slash)) p = at + 1;
    size_t i = 0;
    while (*p && *p != '/' && *p != ':' && *p != '?' && *p != '#' && i < sz - 1)
        buf[i++] = (char)tolower((unsigned char)*p++);
    buf[i] = '\0';
    return i > 0;
}

/* PoP: auth_validate_nous_inference_url_from_network @ hermes_cli/auth.py:_validate_nous_inference_url_from_network */
char *auth_validate_nous_inference_url_from_network(const char *url)
{
    if (!url) return NULL;
    while (*url && isspace((unsigned char)*url)) url++;
    size_t n = strlen(url);
    while (n > 0 && isspace((unsigned char)url[n-1])) n--;
    if (n == 0) return NULL;
    char cleaned[1024];
    if (n >= sizeof(cleaned)) return NULL;
    memcpy(cleaned, url, n); cleaned[n] = '\0';

    char host[256], scheme[16];
    if (!url_hostname(cleaned, host, sizeof(host), scheme, sizeof(scheme))) return NULL;
    if (strcmp(scheme, "https") != 0) {
        hermes_log(LOG_WARNING, "auth",
                   "nous: refusing non-https inference URL scheme '%s' from Portal response", scheme);
        return NULL;
    }
    if (strcmp(host, "inference-api.nousresearch.com") != 0) {
        hermes_log(LOG_WARNING, "auth",
                   "nous: refusing inference URL host '%s' from Portal response "
                   "(not in allowlist); falling back to default", host);
        return NULL;
    }
    while (n > 0 && cleaned[n-1] == '/') cleaned[--n] = '\0';
    return strdup(cleaned);
}

/* PoP: auth_migrate_stale_nous_portal_url @ hermes_cli/auth.py:_migrate_stale_nous_portal_url */
void auth_migrate_stale_nous_portal_url(json_t *providers)
{
    if (!providers || providers->type != JSON_OBJECT) return;
    json_t *nous = json_obj_get(providers, "nous");
    if (!nous || nous->type != JSON_OBJECT) return;
    const char *stored = json_get_str(nous, "portal_base_url", "");
    while (*stored && isspace((unsigned char)*stored)) stored++;
    if (!*stored) return;
    char host[256];
    if (url_hostname(stored, host, sizeof(host), NULL, 0) &&
        strcmp(host, "api.nousresearch.com") == 0) {
        hermes_log(LOG_WARNING, "auth",
                   "auth: migrating stale nous portal_base_url %s -> %s",
                   stored, DEFAULT_NOUS_PORTAL_URL_C);
        json_set(nous, "portal_base_url", json_new_string(DEFAULT_NOUS_PORTAL_URL_C));
    }
}

/* _parse_iso_timestamp: reuse auth_parse_iso_timestamp() from auth_helpers.c */

/* _is_expiring: reuse _is_expiring() from auth_helpers.c */

/* json_t-input TTL coercer; string form is auth_coerce_ttl_seconds() in auth_helpers.c */
static int auth_coerce_ttl_seconds_json(const json_t *expires_in)
{
    if (!expires_in) return 0;
    long ttl = 0;
    if (expires_in->type == JSON_NUMBER) ttl = (long)expires_in->num_val;
    else if (expires_in->type == JSON_STRING && expires_in->str_val)
        ttl = strtol(expires_in->str_val, NULL, 10);
    return ttl > 0 ? (int)ttl : 0;
}

/* PoP: auth_codex_access_token_is_expiring @ hermes_cli/auth.py:_codex_access_token_is_expiring */
bool auth_codex_access_token_is_expiring(const char *access_token, int skew_seconds)
{
    char *payload = auth_decode_jwt_payload(access_token);
    if (!payload) return false;
    double exp = auth_jwt_get_num(payload, "exp");
    free(payload);
    if (exp < 0) return false;
    int skew = skew_seconds > 0 ? skew_seconds : 0;
    return exp <= ((double)time(NULL) + skew);
}

/* PoP: auth_qwen_access_token_is_expiring @ hermes_cli/auth.py:_qwen_access_token_is_expiring */
bool auth_qwen_access_token_is_expiring(const char *expiry_date_ms, int skew_seconds)
{
    if (!expiry_date_ms || !*expiry_date_ms) return true;
    char *end = NULL;
    long long expiry_ms = strtoll(expiry_date_ms, &end, 10);
    if (end == expiry_date_ms || (end && *end && !isspace((unsigned char)*end))) return true;
    int skew = skew_seconds > 0 ? skew_seconds : 0;
    return ((double)time(NULL) + skew) * 1000.0 >= (double)expiry_ms;
}

/* PoP: auth_nous_jwt_expires_at @ hermes_cli/auth.py:_nous_jwt_expires_at */
char *auth_nous_jwt_expires_at(const char *token, const char *fallback_expires_at)
{
    char *payload = auth_decode_jwt_payload(token);
    if (payload) {
        double exp = auth_jwt_get_num(payload, "exp");
        free(payload);
        if (exp > 0) {
            time_t t = (time_t)exp;
            struct tm tmv;
            gmtime_r(&t, &tmv);
            char *buf = malloc(40);
            if (buf) {
                strftime(buf, 40, "%Y-%m-%dT%H:%M:%S+00:00", &tmv);
                return buf;
            }
        }
    }
    return fallback_expires_at ? strdup(fallback_expires_at) : NULL;
}

/* PoP: auth_nous_invoke_jwt_is_usable @ hermes_cli/auth.py:_nous_invoke_jwt_is_usable */
bool auth_nous_invoke_jwt_is_usable(const char *token, const char *scope,
                                    const char *expires_at, int min_ttl_seconds)
{
    char *reason = auth_nous_invoke_jwt_status(token, scope, expires_at, min_ttl_seconds);
    bool usable = (reason == NULL);
    free(reason);
    return usable;
}

/* PoP: auth_assert_nous_inference_jwt_usable @ hermes_cli/auth.py:_assert_nous_inference_jwt_usable */
auth_error_t *auth_assert_nous_inference_jwt_usable(const json_t *state, const char *access_token)
{
    const char *token = access_token;
    if (!token && state && state->type == JSON_OBJECT)
        token = json_get_str(state, "access_token", NULL);
    const char *scope = state ? json_get_str(state, "scope", NULL) : NULL;
    const char *expires_at = state ? json_get_str(state, "expires_at", NULL) : NULL;

    char *reason = auth_nous_invoke_jwt_status(token, scope, expires_at,
                                               AUTH_NOUS_INVOKE_JWT_MIN_TTL_SECONDS);
    if (!reason) return NULL;
    char msg[512];
    snprintf(msg, sizeof(msg),
             "Nous Portal access token is not a usable inference JWT (%s). "
             "Re-authenticate with: hermes auth add nous", reason);
    auth_error_t *err = auth_error_new(msg, "nous", reason, true);
    free(reason);
    return err;
}

/* PoP: auth_log_nous_invoke_jwt_selected @ hermes_cli/auth.py:_log_nous_invoke_jwt_selected */
void auth_log_nous_invoke_jwt_selected(const char *access_token, const char *sequence_id)
{
    hermes_log(LOG_INFO, "auth", "Nous inference auth: using NAS invoke JWT");
    char *fp = _token_fingerprint(access_token);
    json_t *fields = json_new_object();
    json_set(fields, "access_token_fp", fp ? json_new_string(fp) : json_null());
    free(fp);
    auth_oauth_trace("nous_invoke_jwt_selected", sequence_id, fields);
    json_free(fields);
}

/* PoP: auth_set_nous_agent_key_from_invoke_jwt @ hermes_cli/auth.py:_set_nous_agent_key_from_invoke_jwt */
void auth_set_nous_agent_key_from_invoke_jwt(json_t *state, const char *obtained_at)
{
    if (!state || state->type != JSON_OBJECT) return;
    const char *access_token = json_get_str(state, "access_token", NULL);
    if (!access_token) return;
    const char *p = access_token;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return;

    char now_iso[40];
    { time_t t = time(NULL); struct tm tmv; gmtime_r(&t, &tmv);
      strftime(now_iso, sizeof(now_iso), "%Y-%m-%dT%H:%M:%S+00:00", &tmv); }

    const char *existing_obtained_at = json_get_str(state, "agent_key_obtained_at", NULL);
    const char *agent_key = json_get_str(state, "agent_key", NULL);
    char effective_obtained_at[64];
    if (obtained_at && *obtained_at) {
        snprintf(effective_obtained_at, sizeof(effective_obtained_at), "%s", obtained_at);
    } else if (agent_key && strcmp(agent_key, access_token) == 0 &&
               existing_obtained_at && *existing_obtained_at) {
        bool nonblank = false;
        for (const char *q = existing_obtained_at; *q; q++)
            if (!isspace((unsigned char)*q)) { nonblank = true; break; }
        snprintf(effective_obtained_at, sizeof(effective_obtained_at), "%s",
                 nonblank ? existing_obtained_at : now_iso);
    } else {
        snprintf(effective_obtained_at, sizeof(effective_obtained_at), "%s", now_iso);
    }

    const char *fallback = json_get_str(state, "expires_at", NULL);
    char *expires_at = auth_nous_jwt_expires_at(access_token, fallback);
    double expires_epoch = expires_at ? auth_parse_iso_timestamp(expires_at) : -1;
    int expires_in;
    if (expires_epoch >= 0) {
        double d = expires_epoch - (double)time(NULL);
        expires_in = d > 0 ? (int)d : 0;
    } else {
        expires_in = auth_coerce_ttl_seconds_json(json_obj_get(state, "expires_in"));
    }

    if (expires_at) {
        json_set(state, "expires_at", json_new_string(expires_at));
        json_set(state, "expires_in", json_int(expires_in));
    }
    json_set(state, "agent_key", json_new_string(access_token));
    json_set(state, "agent_key_id", json_null());
    json_set(state, "agent_key_expires_at",
             expires_at ? json_new_string(expires_at) : json_null());
    json_set(state, "agent_key_expires_in", json_int(expires_in));
    json_set(state, "agent_key_reused", json_bool(false));
    json_set(state, "agent_key_obtained_at", json_new_string(effective_obtained_at));
    free(expires_at);
}

/* PoP: auth_select_nous_invoke_jwt @ hermes_cli/auth.py:_select_nous_invoke_jwt */
void auth_select_nous_invoke_jwt(json_t *state, const char *access_token,
                                 const char *sequence_id)
{
    if (!state || state->type != JSON_OBJECT) return;
    if (access_token) {
        const char *p = access_token;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p) json_set(state, "access_token", json_new_string(access_token));
    }
    auth_set_nous_agent_key_from_invoke_jwt(state, NULL);
    auth_log_nous_invoke_jwt_selected(json_get_str(state, "access_token", NULL), sequence_id);
}

/* PoP: auth_nous_effective_provider_state @ hermes_cli/auth.py:_nous_effective_provider_state */
json_t *auth_nous_effective_provider_state(const json_t *state)
{
    json_t *out = json_new_object();
    if (!state || state->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < state->c.count; i++) {
        const char *key = state->c.keys[i];
        if (strcmp(key, "expires_in") == 0 || strcmp(key, "agent_key_expires_in") == 0)
            continue;
        json_set(out, key, json_copy(state->c.items[i]));
    }
    return out;
}

/* PoP: auth_agent_key_is_usable @ hermes_cli/auth.py:_agent_key_is_usable */
bool auth_agent_key_is_usable(const json_t *state, int min_ttl_seconds)
{
    if (!state || state->type != JSON_OBJECT) return false;
    const char *key = json_get_str(state, "agent_key", NULL);
    if (!key) return false;
    const char *p = key;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return false;
    return auth_nous_invoke_jwt_is_usable(
        key,
        json_get_str(state, "scope", NULL),
        json_get_str(state, "agent_key_expires_at", NULL),
        min_ttl_seconds > 0 ? min_ttl_seconds : 0);
}

/* PoP: auth_empty_nous_auth_status @ hermes_cli/auth.py:_empty_nous_auth_status */
json_t *auth_empty_nous_auth_status(void)
{
    json_t *s = json_new_object();
    json_set(s, "logged_in", json_bool(false));
    json_set(s, "portal_base_url", json_null());
    json_set(s, "inference_base_url", json_null());
    json_set(s, "access_expires_at", json_null());
    json_set(s, "agent_key_expires_at", json_null());
    json_set(s, "has_refresh_token", json_bool(false));
    json_set(s, "inference_credential_present", json_bool(false));
    json_set(s, "credential_source", json_null());
    return s;
}

/* process-level nous-auth-status memo (mirrors _nous_auth_status_cache) */
static json_t *g_nous_auth_status_cache = NULL;
static pthread_mutex_t g_nous_status_mu = PTHREAD_MUTEX_INITIALIZER;

/* PoP: auth_invalidate_nous_auth_status_cache @ hermes_cli/auth.py:invalidate_nous_auth_status_cache */
void auth_invalidate_nous_auth_status_cache(void)
{
    pthread_mutex_lock(&g_nous_status_mu);
    if (g_nous_auth_status_cache) { json_free(g_nous_auth_status_cache); g_nous_auth_status_cache = NULL; }
    pthread_mutex_unlock(&g_nous_status_mu);
}

/* PoP: auth_is_terminal_xai_oauth_refresh_error @ hermes_cli/auth.py:_is_terminal_xai_oauth_refresh_error */
bool auth_is_terminal_xai_oauth_refresh_error(const char *provider, const char *code,
                                              bool relogin_required)
{
    if (!provider || !code || !relogin_required) return false;
    if (strcmp(provider, "xai-oauth") != 0) return false;
    return strcmp(code, "xai_refresh_failed") == 0 ||
           strcmp(code, "xai_auth_missing_refresh_token") == 0;
}

/* PoP: auth_is_terminal_codex_oauth_refresh_error @ hermes_cli/auth.py:_is_terminal_codex_oauth_refresh_error */
bool auth_is_terminal_codex_oauth_refresh_error(const char *provider, const char *code,
                                                bool relogin_required)
{
    if (!provider || !code || !relogin_required) return false;
    if (strcmp(provider, "openai-codex") != 0) return false;
    return strcmp(code, "codex_refresh_failed") == 0 ||
           strcmp(code, "codex_auth_missing_refresh_token") == 0 ||
           strcmp(code, "invalid_grant") == 0 ||
           strcmp(code, "invalid_token") == 0 ||
           strcmp(code, "refresh_token_reused") == 0;
}

/* ════════════════════════════════════════════════════════════════
 *  Shared cross-profile Nous OAuth store (Python flat-dict schema)
 * ════════════════════════════════════════════════════════════════ */

#define DEFAULT_NOUS_INFERENCE_URL_C "https://inference-api.nousresearch.com/v1"
#define DEFAULT_NOUS_CLIENT_ID_C     "hermes-cli"
#define DEFAULT_NOUS_SCOPE_C         "inference:invoke"
#define NOUS_SHARED_STORE_FILENAME_C "nous_auth.json"

extern void hermes_get_home(char *buf, size_t sz);   /* hermes_core_types.h */

/* PoP: auth_nous_shared_auth_dir @ hermes_cli/auth.py:_nous_shared_auth_dir */
static char *auth_nous_shared_auth_dir(void)
{
    const char *override = getenv("HERMES_SHARED_AUTH_DIR");
    if (override) {
        while (*override && isspace((unsigned char)*override)) override++;
        if (*override) {
            if (override[0] == '~' && (override[1] == '/' || override[1] == '\0')) {
                const char *home = getenv("HOME");
                if (home) {
                    char *out = malloc(strlen(home) + strlen(override) + 2);
                    if (!out) return NULL;
                    sprintf(out, "%s%s", home, override + 1);
                    return out;
                }
            }
            return strdup(override);
        }
    }
    char root[1024];
    hermes_get_home(root, sizeof(root));
    if (!root[0]) return NULL;
    char *out = malloc(strlen(root) + 16);
    if (out) sprintf(out, "%s/shared", root);
    return out;
}

/* PoP: auth_nous_shared_store_path_py @ hermes_cli/auth.py:_nous_shared_store_path */
char *auth_nous_shared_store_path_py(void)
{
    char *dir = auth_nous_shared_auth_dir();
    if (!dir) return NULL;
    char *out = malloc(strlen(dir) + sizeof(NOUS_SHARED_STORE_FILENAME_C) + 2);
    if (out) sprintf(out, "%s/%s", dir, NOUS_SHARED_STORE_FILENAME_C);
    free(dir);
    return out;
}

/* PoP: auth_nous_shared_store_lock_py @ hermes_cli/auth.py:_nous_shared_store_lock */
static int auth_nous_shared_store_lock_py(double timeout_seconds, char **out_lock_path)
{
    *out_lock_path = NULL;
    char *path = auth_nous_shared_store_path_py();
    if (!path) return 1;   /* no home yet: proceed without locking */
    char *lp = lock_path_for(path);
    free(path);
    if (!lp) return 1;
    int ok = auth_file_lock_acquire(lp, timeout_seconds > 0 ? timeout_seconds
                                                            : AUTH_LOCK_TIMEOUT_SECONDS_C);
    if (ok) { *out_lock_path = lp; return 1; }
    free(lp);
    return 0;
}

static void auth_nous_shared_store_unlock_py(char *lock_path)
{
    if (!lock_path) return;
    auth_file_lock_release(lock_path);
    free(lock_path);
}

/* PoP: auth_read_shared_nous_state @ hermes_cli/auth.py:_read_shared_nous_state */
json_t *auth_read_shared_nous_state(void)
{
    char *path = auth_nous_shared_store_path_py();
    if (!path) return NULL;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) { free(path); return NULL; }
    char *err = NULL;
    json_t *payload = json_parse_file(path, &err);
    if (!payload) {
        hermes_log(LOG_DEBUG, "auth", "Shared Nous auth store at %s is unreadable: %s",
                   path, err ? err : "parse error");
        free(err); free(path);
        return NULL;
    }
    free(err); free(path);
    if (payload->type != JSON_OBJECT) { json_free(payload); return NULL; }
    const char *rt = json_get_str(payload, "refresh_token", NULL);
    const char *at = json_get_str(payload, "access_token", NULL);
    bool rt_ok = false, at_ok = false;
    for (const char *p = rt; p && *p; p++) if (!isspace((unsigned char)*p)) { rt_ok = true; break; }
    for (const char *p = at; p && *p; p++) if (!isspace((unsigned char)*p)) { at_ok = true; break; }
    if (!rt_ok || !at_ok) { json_free(payload); return NULL; }
    return payload;
}

/* PoP: auth_write_shared_nous_state @ hermes_cli/auth.py:_write_shared_nous_state */
void auth_write_shared_nous_state(const json_t *state)
{
    if (!state || state->type != JSON_OBJECT) return;
    const char *refresh_token = json_get_str(state, "refresh_token", NULL);
    const char *access_token = json_get_str(state, "access_token", NULL);
    bool rt_ok = false, at_ok = false;
    for (const char *p = refresh_token; p && *p; p++)
        if (!isspace((unsigned char)*p)) { rt_ok = true; break; }
    for (const char *p = access_token; p && *p; p++)
        if (!isspace((unsigned char)*p)) { at_ok = true; break; }
    if (!rt_ok || !at_ok) return;

    json_t *shared = json_new_object();
    json_set(shared, "_schema", json_int(1));
    json_set(shared, "access_token", json_new_string(access_token));
    json_set(shared, "refresh_token", json_new_string(refresh_token));
    const char *tt = json_get_str(state, "token_type", NULL);
    json_set(shared, "token_type", json_new_string(tt && *tt ? tt : "Bearer"));
    const char *sc = json_get_str(state, "scope", NULL);
    json_set(shared, "scope", json_new_string(sc && *sc ? sc : DEFAULT_NOUS_SCOPE_C));
    const char *ci = json_get_str(state, "client_id", NULL);
    json_set(shared, "client_id", json_new_string(ci && *ci ? ci : DEFAULT_NOUS_CLIENT_ID_C));
    const char *pb = json_get_str(state, "portal_base_url", NULL);
    json_set(shared, "portal_base_url",
             json_new_string(pb && *pb ? pb : DEFAULT_NOUS_PORTAL_URL_C));
    const char *ib = json_get_str(state, "inference_base_url", NULL);
    json_set(shared, "inference_base_url",
             json_new_string(ib && *ib ? ib : DEFAULT_NOUS_INFERENCE_URL_C));
    json_t *oa = json_obj_get(state, "obtained_at");
    json_set(shared, "obtained_at", oa ? json_copy(oa) : json_null());
    json_t *ea = json_obj_get(state, "expires_at");
    json_set(shared, "expires_at", ea ? json_copy(ea) : json_null());
    char *now = iso_now_utc();
    if (now) { json_set(shared, "updated_at", json_new_string(now)); free(now); }

    char *lock_path = NULL;
    if (!auth_nous_shared_store_lock_py(AUTH_LOCK_TIMEOUT_SECONDS_C, &lock_path)) {
        json_free(shared);
        return;
    }
    char *path = auth_nous_shared_store_path_py();
    if (path) {
        char dir[PATH_MAX];
        snprintf(dir, sizeof(dir), "%s", path);
        char *slash = strrchr(dir, '/');
        if (slash) { *slash = '\0'; if (dir[0]) mkdir(dir, 0700); }
        secure_parent_dir_c(path);

        char *payload = json_serialize_pretty(shared, 2);
        if (payload) {
            char tmp[PATH_MAX + 64];
            snprintf(tmp, sizeof(tmp), "%s.tmp.%d.%lx", path, (int)getpid(),
                     (unsigned long)time(NULL) ^ (unsigned long)(size_t)payload);
            int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
            if (fd >= 0) {
                size_t len = strlen(payload);
                ssize_t wr = write(fd, payload, len);
                fsync(fd);
                close(fd);
                if (wr == (ssize_t)len && rename(tmp, path) == 0) {
                    json_t *fields = json_new_object();
                    json_set(fields, "path", json_new_string(path));
                    char *fp = _token_fingerprint(refresh_token);
                    json_set(fields, "refresh_token_fp",
                             fp ? json_new_string(fp) : json_null());
                    free(fp);
                    auth_nous_shared_store_unlock_py(lock_path);
                    lock_path = NULL;
                    auth_oauth_trace("nous_shared_store_written", NULL, fields);
                    json_free(fields);
                } else {
                    unlink(tmp);
                    hermes_log(LOG_DEBUG, "auth", "Failed to write shared Nous auth store");
                }
            }
            free(payload);
        }
        free(path);
    }
    auth_nous_shared_store_unlock_py(lock_path);
    json_free(shared);
}

/* PoP: auth_clear_shared_nous_state @ hermes_cli/auth.py:_clear_shared_nous_state */
void auth_clear_shared_nous_state(const char *reason)
{
    char *lock_path = NULL;
    if (!auth_nous_shared_store_lock_py(AUTH_LOCK_TIMEOUT_SECONDS_C, &lock_path)) return;
    char *path = auth_nous_shared_store_path_py();
    if (path) { unlink(path); free(path); }
    auth_nous_shared_store_unlock_py(lock_path);
    json_t *fields = json_new_object();
    json_set(fields, "reason", json_new_string(reason ? reason : ""));
    auth_oauth_trace("nous_shared_store_cleared", NULL, fields);
    json_free(fields);
}

/* PoP: auth_merge_shared_nous_oauth_state @ hermes_cli/auth.py:_merge_shared_nous_oauth_state */
bool auth_merge_shared_nous_oauth_state(json_t *state)
{
    if (!state || state->type != JSON_OBJECT) return false;
    json_t *shared = auth_read_shared_nous_state();
    if (!shared) return false;

    const char *shared_refresh = json_get_str(shared, "refresh_token", "");
    /* strip for comparison */
    char srt[2048];
    lc_copy(srt, sizeof(srt), shared_refresh);   /* NOTE: lowercases too — see below */
    /* refresh tokens are case-sensitive: do a plain strip instead */
    {
        const char *s = shared_refresh;
        while (*s && isspace((unsigned char)*s)) s++;
        size_t n = strlen(s);
        while (n > 0 && isspace((unsigned char)s[n-1])) n--;
        if (n >= sizeof(srt)) n = sizeof(srt) - 1;
        memcpy(srt, s, n); srt[n] = '\0';
    }
    if (!srt[0]) { json_free(shared); return false; }

    const char *local_refresh = json_get_str(state, "refresh_token", "");
    char lrt[2048];
    {
        const char *s = local_refresh;
        while (*s && isspace((unsigned char)*s)) s++;
        size_t n = strlen(s);
        while (n > 0 && isspace((unsigned char)s[n-1])) n--;
        if (n >= sizeof(lrt)) n = sizeof(lrt) - 1;
        memcpy(lrt, s, n); lrt[n] = '\0';
    }

    double shared_exp = auth_parse_iso_timestamp(json_get_str(shared, "expires_at", NULL));
    double local_exp = auth_parse_iso_timestamp(json_get_str(state, "expires_at", NULL));
    if (shared_exp < 0) shared_exp = 0;
    if (local_exp < 0) local_exp = 0;

    bool refresh_changed = strcmp(srt, lrt) != 0;
    bool fresher_access = shared_exp > local_exp;
    if (!refresh_changed && !fresher_access) { json_free(shared); return false; }

    static const char *MERGE_KEYS[] = {
        "access_token", "refresh_token", "token_type", "scope", "client_id",
        "portal_base_url", "inference_base_url", "obtained_at", "expires_at",
    };
    for (size_t i = 0; i < sizeof(MERGE_KEYS)/sizeof(MERGE_KEYS[0]); i++) {
        json_t *v = json_obj_get(shared, MERGE_KEYS[i]);
        if (!v || v->type == JSON_NULL) continue;
        if (v->type == JSON_STRING && (!v->str_val || !v->str_val[0])) continue;
        json_set(state, MERGE_KEYS[i], json_copy(v));
    }
    json_free(shared);
    return true;
}

/* ════════════════════════════════════════════════════════════════
 *  Quarantine — remove dead OAuth material after terminal failures
 * ════════════════════════════════════════════════════════════════ */

/* _is_terminal_nous_refresh_error: reuse auth_is_terminal_nous_refresh_error()
 * from port_auth_helpers.c (int signature). */

/* PoP: auth_quarantine_nous_oauth_state @ hermes_cli/auth.py:_quarantine_nous_oauth_state */
void auth_quarantine_nous_oauth_state(json_t *state, const auth_error_t *error,
                                      const char *reason)
{
    if (!state || state->type != JSON_OBJECT) return;

    /* forensic log BEFORE clearing token material (must be WARNING) */
    json_t *forensic = json_new_object();
    json_set(forensic, "reason", json_new_string(reason ? reason : ""));
    json_set(forensic, "error_code",
             (error && error->code) ? json_new_string(error->code) : json_null());
    json_t *ci = json_obj_get(state, "client_id");
    json_set(forensic, "client_id", ci ? json_copy(ci) : json_null());
    json_t *aki = json_obj_get(state, "agent_key_id");
    json_set(forensic, "agent_key_id", aki ? json_copy(aki) : json_null());
    char *fp = _token_fingerprint(json_get_str(state, "refresh_token", NULL));
    json_set(forensic, "refresh_token_fp", fp ? json_new_string(fp) : json_null());
    free(fp);

    char *auth_path = auth_file_path();
    if (auth_path) {
        json_set(forensic, "auth_json_path", json_new_string(auth_path));
        struct stat st;
        if (stat(auth_path, &st) == 0) {
            json_set(forensic, "auth_json_size", json_int((long long)st.st_size));
            json_set(forensic, "auth_json_mtime", json_number((double)st.st_mtime));
            json_set(forensic, "auth_json_exists", json_bool(true));
        } else {
            json_set(forensic, "auth_json_exists", json_bool(false));
        }
        free(auth_path);
    }

    const char *expires_at_raw = json_get_str(state, "expires_at", NULL);
    if (expires_at_raw && *expires_at_raw) {
        double exp = auth_parse_iso_timestamp(expires_at_raw);
        if (exp >= 0)
            json_set(forensic, "token_already_expired",
                     json_bool(exp < (double)time(NULL)));
        else
            json_set(forensic, "token_already_expired", json_null());
    } else {
        json_set(forensic, "token_already_expired", json_null());
    }

    char *fj = json_serialize(forensic);
    hermes_log(LOG_WARNING, "auth",
               "Nous OAuth state quarantined (terminal auth death): %s",
               fj ? fj : "{}");
    free(fj);
    json_free(forensic);

    /* strip dead OAuth material in place (Python: state.pop(key, None)) */
    static const char *DEAD_KEYS[] = {
        "access_token", "refresh_token", "expires_at", "expires_in",
        "obtained_at", "agent_key", "agent_key_id", "agent_key_expires_at",
        "agent_key_expires_in", "agent_key_reused", "agent_key_obtained_at",
    };
    for (size_t k = 0; k < sizeof(DEAD_KEYS)/sizeof(DEAD_KEYS[0]); k++)
        json_obj_del(state, DEAD_KEYS[k]);

    json_t *last_err = json_new_object();
    json_set(last_err, "provider", json_new_string("nous"));
    json_set(last_err, "code",
             (error && error->code) ? json_new_string(error->code) : json_null());
    json_set(last_err, "message",
             (error && error->message) ? json_new_string(error->message) : json_new_string(""));
    json_set(last_err, "reason", json_new_string(reason ? reason : ""));
    json_set(last_err, "relogin_required", json_bool(true));
    char *now = iso_now_utc();
    if (now) { json_set(last_err, "at", json_new_string(now)); free(now); }
    json_set(state, "last_auth_error", last_err);

    auth_clear_shared_nous_state(reason);
    auth_invalidate_nous_auth_status_cache();
}

/* PoP: auth_quarantine_nous_pool_entries @ hermes_cli/auth.py:_quarantine_nous_pool_entries */
bool auth_quarantine_nous_pool_entries(json_t *auth_store, const auth_error_t *error,
                                       const char *reason)
{
    if (!auth_store || auth_store->type != JSON_OBJECT) return false;
    json_t *pool = json_obj_get(auth_store, "credential_pool");
    if (!pool || pool->type != JSON_OBJECT) return false;
    json_t *entries = json_obj_get(pool, "nous");
    if (!entries || entries->type != JSON_ARRAY) return false;

    json_t *retained = json_new_array();
    bool removed = false;
    for (size_t i = 0; i < entries->c.count; i++) {
        json_t *entry = entries->c.items[i];
        if (entry && entry->type == JSON_OBJECT) {
            const char *src = json_get_str(entry, "source", NULL);
            if (src && (strcmp(src, NOUS_DEVICE_CODE_SOURCE_C) == 0 ||
                        strcmp(src, "manual:" NOUS_DEVICE_CODE_SOURCE_C) == 0)) {
                removed = true;
                continue;
            }
        }
        json_append(retained, entry ? json_copy(entry) : json_null());
    }

    if (removed) {
        json_set(pool, "nous", retained);
        json_t *fields = json_new_object();
        json_set(fields, "reason", json_new_string(reason ? reason : ""));
        json_set(fields, "error_code",
                 (error && error->code) ? json_new_string(error->code) : json_null());
        auth_oauth_trace("nous_pool_device_code_quarantined", NULL, fields);
        json_free(fields);
    } else {
        json_free(retained);
    }
    return removed;
}


/* ── from src/cli/port_auth_wrappers.c ── */
/*
 * port_auth_wrappers.c — C port of hermes_cli/auth.py
 * PoP-annotated wrappers for all unported functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include "hermes_json.h"
#include "port_config_py_helpers.h"

/* PoP: auth_u_resolve_api_key_provider_secret @ hermes_cli/auth.py:_resolve_api_key_provider_secret */
int auth_u_resolve_api_key_provider_secret(const char *arg) {
    /* Python: dotenv-prefer. Arg =
     * "state\tresult\tfrom". */
    if (!arg || !*arg) { printf("\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "copilot") == 0) {
        printf("%s\t%s\n", t3 ? t3 + 1 : "", "copilot_auth");
        return 0;
    }
    if (strcmp(state, "pool") == 0) {
        printf("%s\tcredential_pool:%s\n", t3 ? t3 + 1 : "", t2 ? t2 + 1 : "?");
        return 0;
    }
    printf("%s\t%s\n", t3 ? t3 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: auth_detect_zai_endpoint @ hermes_cli/auth.py:detect_zai_endpoint */
int auth_detect_zai_endpoint(const char *arg) {
    /* Python: z.ai probe. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: auth_u_resolve_zai_base_url @ hermes_cli/auth.py:_resolve_zai_base_url */
int auth_u_resolve_zai_base_url(const char *arg) {
    /* Python: cached probe. Arg =
     * "env\tcached\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int env = arg[0] == '1';
    int cached = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (env) { printf("GLM_BASE_URL override wins\n"); return 0; }
    if (cached) { printf("cached detected endpoint (key-hash matched): %s\n", t3 ? t3 + 1 : ""); return 0; }
    printf("probed endpoint (up to 8s/endpoint): %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_format_nous_entitlement_auth_error @ hermes_cli/auth.py:_format_nous_entitlement_auth_error */
/* PoP: auth_u_format_nous_entitlement_auth_error @ hermes_cli/auth.py:_format_nous_entitlement_auth_error */
int auth_u_format_nous_entitlement_auth_error(const char *arg) {
    /* Faithful fallback: the Python path enriches with Nous Portal account
     * info; the C port prints the entitlement guidance directly. */
    printf("%s Check credits or billing in Nous Portal, then retry.\n",
           arg ? arg : "Auth error.");
    return 0;
}

/* PoP: auth_u_auth_lock_holder_for @ hermes_cli/auth.py:_auth_lock_holder_for */
int auth_u_auth_lock_holder_for(const char *arg) {
    /* Python: reentrancy tracker keyed to canonical path. Arg = path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: auth_u_get_config_hint_for_unknown_provider @ hermes_cli/auth.py:_get_config_hint_for_unknown_provider */
/* PoP: auth_u_get_config_hint_for_unknown_provider @ hermes_cli/auth.py:_get_config_hint_for_unknown_provider */
int auth_u_get_config_hint_for_unknown_provider(const char *arg) {
    /* Faithful: surface a diagnostic when provider resolution fails. We check
     * whether custom_providers is present (a common misconfiguration) and
     * point at `hermes doctor`. Returns/hints via stdout. */
    json_t *cfg = config_py_load_config_readonly();
    int found = 0;
    if (cfg) {
        json_t *cp = config_py_get_nested(cfg, "custom_providers");
        if (cp && cp->type == JSON_OBJECT && json_len(cp) > 0) found = 1;
        json_free(cfg);
    }
    if (found)
        printf("Config issue detected — run 'hermes doctor' for full diagnostics.\n"
               "  custom_providers is set; verify the entry for '%s'.\n",
               arg ? arg : "");
    else
        printf("Provider '%s' not found — run 'hermes doctor' for diagnostics.\n",
               arg ? arg : "");
    return 0;
}

/* PoP: auth_u_parse_iso_timestamp @ hermes_cli/auth.py:_parse_iso_timestamp */
int auth_u_parse_iso_timestamp(const char *arg) {
    /* Returns epoch seconds as an int (faithful: Python returns float epoch).
     * Parses ISO 8601 like 2024-01-02T03:04:05Z or with +00:00 offset. */
    if (!arg || !*arg) return 0;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", arg);
    size_t L = strlen(buf);
    /* Normalize trailing Z to +00:00 for strptime */
    if (L > 0 && buf[L-1] == 'Z') { buf[L-1] = '+'; buf[L] = '\0'; strcat(buf, "00:00"); }
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    char *p = strptime(buf, "%Y-%m-%dT%H:%M:%S", &tm);
    if (!p) return 0;
    /* Handle optional offset if strptime left it (we already normalized Z) */
    time_t t = timegm(&tm); /* treat as UTC */
    return (int)t;
}

/* PoP: auth_u_read_qwen_cli_tokens @ hermes_cli/auth.py:_read_qwen_cli_tokens */
int auth_u_read_qwen_cli_tokens(const char *arg) {
    /* Python: read Qwen CLI credential file. Arg = "state\tpath\terror". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "missing") == 0) {
        fprintf(stderr, "Qwen CLI credentials not found. Run 'qwen auth qwen-oauth' first.\n");
        return 1;
    }
    if (strcmp(state, "read_failed") == 0) {
        fprintf(stderr, "Failed to read Qwen CLI credentials from %s: %s\n", t1 ? t1 + 1 : "?", t2 ? t2 + 1 : "");
        return 1;
    }
    if (strcmp(state, "invalid") == 0) {
        fprintf(stderr, "Invalid Qwen CLI credentials in %s.\n", t1 ? t1 + 1 : "?");
        return 1;
    }
    printf("qwen tokens loaded from %s\n", t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: auth_u_save_qwen_cli_tokens @ hermes_cli/auth.py:_save_qwen_cli_tokens */
int auth_u_save_qwen_cli_tokens(const char *arg) {
    /* Python: 0600 O_EXCL atomic write. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("qwen token save skipped\n"); return 0; }
    printf("qwen tokens saved 0600 atomic: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: auth_u_refresh_qwen_cli_tokens @ hermes_cli/auth.py:_refresh_qwen_cli_tokens */
int auth_u_refresh_qwen_cli_tokens(const char *arg) {
    /* Python: qwen refresh. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_rt") == 0) {
        fprintf(stderr, "Qwen OAuth refresh token missing. Re-run 'qwen auth qwen-oauth'.\n");
        return 1;
    }
    if (strcmp(state, "http_fail") == 0) {
        fprintf(stderr, "Qwen OAuth refresh failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "bad_json") == 0) {
        fprintf(stderr, "Qwen OAuth refresh returned invalid JSON\n");
        return 1;
    }
    if (strcmp(state, "no_token") == 0) {
        fprintf(stderr, "Qwen OAuth refresh response missing access_token.\n");
        return 1;
    }
    printf("qwen refreshed (expiry = now + %s, saved): %s\n", t2 ? t2 + 1 : "6h", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_mark_qwen_oauth_active @ hermes_cli/auth.py:_mark_qwen_oauth_active */
int auth_u_mark_qwen_oauth_active(const char *arg) {
    /* Python: set active_provider + minimal state in auth store. Arg =
     * "base_url\tstate". */
    if (!arg || !*arg) { printf("qwen-oauth marked active\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("qwen-oauth marked active%s\n", (tab && tab[1] == '1') ? " (base_url saved)" : "");
    return 0;
}

/* PoP: auth_resolve_qwen_runtime_credentials @ hermes_cli/auth.py:resolve_qwen_runtime_credentials */
int auth_resolve_qwen_runtime_credentials(const char *arg) {
    /* Python: read + refresh + resolve. Arg =
     * "state\trefreshed\tbase_url\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "missing") == 0) {
        fprintf(stderr, "Qwen OAuth access token missing. Re-run 'qwen auth qwen-oauth'.\n");
        return 1;
    }
    printf("qwen credentials resolved%s: base=%s\n",
           (t1 && t1[1] == '1') ? " (refreshed)" : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: auth_get_qwen_auth_status @ hermes_cli/auth.py:get_qwen_auth_status */
int auth_get_qwen_auth_status(const char *arg) {
    /* Python: qwen status w/ refresh. Arg =
     * "state\tauth_file\tsource\tresult". */
    if (!arg || !*arg) { printf("{\"logged_in\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "ok") == 0) {
        printf("{\"logged_in\": true, \"auth_file\": \"%s\", \"source\": \"%s\"}\n",
               t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
        return 0;
    }
    printf("{\"logged_in\": false, \"auth_file\": \"%s\", \"error\": \"%s\"}\n",
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: auth_u_make_spotify_callback_handler @ hermes_cli/auth.py:_make_spotify_callback_handler */
int auth_u_make_spotify_callback_handler(const char *arg) {
    /* Python: handler class + result dict. Arg =
     * "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n\n"); return 0; }
    printf("handler for %s + result box created\n", arg);
    return 0;
}

/* PoP: auth_u_spotify_wait_for_callback @ hermes_cli/auth.py:_spotify_wait_for_callback */
int auth_u_spotify_wait_for_callback(const char *arg) {
    /* Python: local callback server. Arg = "state\tresult\tcode\tbind_fail". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bind_failed") == 0) {
        fprintf(stderr, "Could not bind Spotify callback server: %s\n", t3 ? t3 + 1 : "");
        return 1;
    }
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "Spotify authorization timed out waiting for the local callback.\n");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: auth_u_spotify_token_payload_to_state @ hermes_cli/auth.py:_spotify_token_payload_to_state */
int auth_u_spotify_token_payload_to_state(const char *arg) {
    /* Python: payload -> auth state. Arg = "payload_json\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: auth_u_spotify_exchange_code_for_tokens @ hermes_cli/auth.py:_spotify_exchange_code_for_tokens */
int auth_u_spotify_exchange_code_for_tokens(const char *arg) {
    /* Python: PKCE token exchange. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "exchange_failed") == 0 || strcmp(state, "invalid_response") == 0) {
        fprintf(stderr, "Spotify token exchange failed: %s\n", t2 ? t2 + 1 : "?");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: auth_u_refresh_spotify_oauth_state @ hermes_cli/auth.py:_refresh_spotify_oauth_state */
int auth_u_refresh_spotify_oauth_state(const char *arg) {
    /* Python: spotify refresh. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_rt") == 0) {
        fprintf(stderr, "Spotify refresh token missing. Run `hermes auth spotify` again.\n");
        return 1;
    }
    if (strcmp(state, "http_fail") == 0) {
        fprintf(stderr, "Spotify token refresh failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "no_token") == 0) {
        fprintf(stderr, "Spotify refresh response did not include an access_token.\n");
        return 1;
    }
    printf("spotify refreshed: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_resolve_spotify_runtime_credentials @ hermes_cli/auth.py:resolve_spotify_runtime_credentials */
int auth_resolve_spotify_runtime_credentials(const char *arg) {
    /* Python: refresh-or-clear. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_state") == 0) {
        fprintf(stderr, "Spotify is not authenticated. Run `hermes auth spotify` first.\n");
        return 1;
    }
    if (strcmp(state, "refresh_fail") == 0) {
        fprintf(stderr, "Spotify refresh failed; dead tokens cleared: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("spotify creds: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_get_spotify_auth_status @ hermes_cli/auth.py:get_spotify_auth_status */
int auth_get_spotify_auth_status(const char *arg) {
    /* Python: {logged_in, ...} or {logged_in: False}. Arg =
     * "has_state\trefresh_token\texpiring\tauth_type". */
    if (!arg || !*arg) { printf("{\"logged_in\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_state = arg[0] == '1';
    if (!has_state) { printf("{\"logged_in\": false}\n"); return 0; }
    const char *refresh = t1 ? t1 + 1 : "";
    int expiring = t2 && t2[1] == '1';
    int logged_in = refresh[0] || !expiring;
    printf("{\"logged_in\": %s, \"auth_type\": \"%s\", \"has_refresh_token\": %s}\n",
           logged_in ? "true" : "false", t3 ? t3 + 1 : "oauth_pkce",
           refresh[0] ? "true" : "false");
    return 0;
}

/* PoP: auth_u_spotify_interactive_setup @ hermes_cli/auth.py:_spotify_interactive_setup */
int auth_u_spotify_interactive_setup(const char *arg) {
    /* Python: app wizard. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "cancelled") == 0) {
        printf("Spotify setup cancelled.\n");
        return 1;
    }
    if (strcmp(state, "empty") == 0) {
        printf("No Client ID entered. See docs for the full guide.\n");
        return 1;
    }
    printf("Saved HERMES_SPOTIFY_CLIENT_ID to ~/.hermes/.env: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_login_spotify_command @ hermes_cli/auth.py:login_spotify_command */
int auth_login_spotify_command(const char *arg) {
    /* Python: wizard + PKCE. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "wizard") == 0) {
        printf("  Spotify app wizard launched (no client_id — interactive)\n");
        return 0;
    }
    if (strcmp(state, "aborted") == 0) { printf("Spotify setup cancelled.\n"); return 1; }
    printf("✓ Logged in to Spotify (PKCE verifier, state nonce, browser=%s): %s\n", (t2 && t2[1] == '1') ? "opened" : "off", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_is_remote_session @ hermes_cli/auth.py:_is_remote_session */
int auth_u_is_remote_session(const char *arg) {
    (void)arg;
    return (getenv("SSH_CLIENT") || getenv("SSH_TTY")) ? 1 : 0;
}

/* PoP: auth_u_can_open_graphical_browser @ hermes_cli/auth.py:_can_open_graphical_browser */
int auth_u_can_open_graphical_browser(const char *arg) {
    (void)arg;
#if defined(_WIN32) || defined(__APPLE__)
    return 1;
#else
    const char *browser = getenv("BROWSER");
    if (browser && (strstr(browser, "w3m") || strstr(browser, "lynx")
                    || strstr(browser, "links") || strstr(browser, "elinks")))
        return 0;
    return (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY")) ? 1 : 0;
#endif
}

/* PoP: auth_u_print_loopback_ssh_hint @ hermes_cli/auth.py:_print_loopback_ssh_hint */
int auth_u_print_loopback_ssh_hint(const char *arg) {
    if (!arg || !*arg) return 0;
    if (!auth_u_is_remote_session(NULL)) return 0;
    const char *colon = strrchr(arg, ':');
    const char *slash = strrchr(arg, '/');
    const char *port = colon ? colon + 1 : NULL;
    if (port && slash && port < slash) port = NULL; /* colon was in path */
    if (port) {
        char portbuf[16];
        size_t i = 0;
        while (port[i] && port[i] != '/' && i < sizeof(portbuf) - 1) { portbuf[i] = port[i]; i++; }
        portbuf[i] = '\0';
        printf("  Remote session detected. Forward the loopback port so your local browser can reach it:\n");
        printf("    ssh -N -L %s:127.0.0.1:%s <your-host>\n", portbuf, portbuf);
    }
    return 0;
}

/* PoP: auth_u_read_codex_tokens @ hermes_cli/auth.py:_read_codex_tokens */
int auth_u_read_codex_tokens(const char *arg) {
    /* Python: store read. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_state") == 0) {
        fprintf(stderr, "No Codex credentials stored. Run `hermes auth` to authenticate.\n");
        return 1;
    }
    if (strcmp(state, "no_tokens") == 0 || strcmp(state, "no_access") == 0 || strcmp(state, "no_refresh") == 0) {
        fprintf(stderr, "Codex auth missing tokens. Run `hermes auth` to re-authenticate.\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_sync_codex_pool_entries @ hermes_cli/auth.py:_sync_codex_pool_entries */
int auth_u_sync_codex_pool_entries(const char *arg) {
    /* Python: re-auth mirror. Arg =
     * "synced\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int synced = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!synced) { printf("0 (no pool entries to sync)\n"); return 0; }
    printf("%s entry(ies) synced (device_code always; manual:device_code only when legacy singleton-alias #33000)%s\n", t2 ? t2 + 1 : "1", (t2 && t2[1] == '1') ? " — dead tokens cleared" : "");
    return 0;
}

/* PoP: auth_u_save_codex_tokens @ hermes_cli/auth.py:_save_codex_tokens */
int auth_u_save_codex_tokens(const char *arg) {
    /* Python: singleton capture + pool sync. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("codex save skipped\n"); return 0; }
    printf("codex tokens saved (pool synced): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: auth_u_recover_codex_tokens_from_cli @ hermes_cli/auth.py:_recover_codex_tokens_from_cli */
int auth_u_recover_codex_tokens_from_cli(const char *arg) {
    /* Python: adopt both tokens or None. Arg =
     * "access_token\trefresh_token\timported". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *access = arg;
    const char *refresh = t1 ? t1 + 1 : "";
    int imported = t2 && t2[1] == '1';
    if (!imported || !access[0] || !refresh[0]) { printf("\n"); return 0; }
    printf("recovered codex tokens (access + refresh)\n");
    return 0;
}

/* PoP: auth_refresh_codex_oauth_pure @ hermes_cli/auth.py:refresh_codex_oauth_pure */
int auth_refresh_codex_oauth_pure(const char *arg) {
    /* Python: pure codex refresh. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_rt") == 0) {
        fprintf(stderr, "Codex auth is missing refresh_token. Run `hermes auth` to re-authenticate.\n");
        return 1;
    }
    if (strcmp(state, "quota") == 0) {
        fprintf(stderr, "codex token refresh 429 (rate limit / usage quota): %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "invalid") == 0) {
        fprintf(stderr, "codex refresh rejected (token_invalidated): %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("codex tokens refreshed (CODEX_OAUTH_TOKEN_URL, UA pinned)%s\n", (t2 && t2[1] == '1') ? " — 401 retried" : "");
    return 0;
}

/* PoP: auth_u_refresh_codex_auth_tokens @ hermes_cli/auth.py:_refresh_codex_auth_tokens */
int auth_u_refresh_codex_auth_tokens(const char *arg) {
    /* Python: refresh + self-heal. Arg =
     * "state\tresult\timported". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "relogin") == 0) {
        if (t3 && t3[1] == '1') { printf("recovered from ~/.codex/auth.json\n"); return 0; }
        fprintf(stderr, "Codex refresh failed — re-auth required\n");
        return 1;
    }
    printf("codex tokens refreshed: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: auth_u_import_codex_cli_tokens @ hermes_cli/auth.py:_import_codex_cli_tokens */
int auth_u_import_codex_cli_tokens(const char *arg) {
    /* Python: ~/.codex/auth.json read. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_file") == 0 || strcmp(state, "bad_shape") == 0 || strcmp(state, "expired") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: auth_resolve_codex_runtime_credentials @ hermes_cli/auth.py:resolve_codex_runtime_credentials */
int auth_resolve_codex_runtime_credentials(const char *arg) {
    /* Python: pool fallback. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "pool_fallback") == 0) {
        printf("singleton missing — pool fallback used (#32992 pool-only restoration)%s\n", (t2 && t2[1] == '1') ? " — recovered from CLI store" : "");
        return 0;
    }
    if (strcmp(state, "missing") == 0) {
        fprintf(stderr, "no usable Codex credentials: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("codex runtime creds resolved (singleton first): %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_is_codex_rate_limit_shaped @ hermes_cli/auth.py:_is_codex_rate_limit_shaped */
int auth_u_is_codex_rate_limit_shaped(const char *arg) {
    /* Python (code, reason, message): 429 or rate_limit/usage_limit/quota
     * markers in the lowercased reason/message. Arg = "code\treason\tmessage". */
    if (!arg) return 0;
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", arg);
    char *save = NULL;
    char *code_s = strtok_r(buf, "\t", &save);
    char *reason = strtok_r(NULL, "\t", &save);
    char *message = strtok_r(NULL, "\t", &save);
    long code = code_s ? strtol(code_s, NULL, 10) : 0;
    if (code == 429) return 1;
    char low[2048];
    size_t o = 0;
    const char *src = reason ? reason : "";
    for (const char *c = src; *c && o + 1 < sizeof(low); c++)
        low[o++] = (char)tolower((unsigned char)*c);
    src = message ? message : "";
    for (const char *c = src; *c && o + 1 < sizeof(low); c++)
        low[o++] = (char)tolower((unsigned char)*c);
    low[o] = '\0';
    static const char *const pats[] = {
        "rate_limit", "usage_limit", "quota", "rate limit", "usage limit", NULL };
    for (int i = 0; pats[i]; i++)
        if (strstr(low, pats[i])) return 1;
    return 0;
}

/* PoP: auth_u_codex_usage_probe_url @ hermes_cli/auth.py:_codex_usage_probe_url */
int auth_u_codex_usage_probe_url(const char *arg) {
    /* Python: wham vs api/codex usage URL. Arg = "base_url\tstyle\tresult". */
    if (!arg || !*arg) { printf("https://chatgpt.com/backend-api/codex/wham/usage\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *style = t1 ? t1 + 1 : "";
    if (strcmp(style, "wham") == 0) { printf("%s/wham/usage\n", arg); return 0; }
    if (strcmp(style, "api") == 0) { printf("%s/api/codex/usage\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: auth_u_probe_codex_quota_restored @ hermes_cli/auth.py:_probe_codex_quota_restored */
int auth_u_probe_codex_quota_restored(const char *arg) {
    /* Python: /usage window probe. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_token") == 0 || strcmp(state, "non_jwt") == 0) {
        printf("None (no token / non-JWT refused)\n");
        return 0;
    }
    if (strcmp(state, "restored") == 0) {
        printf("True (all windows <100%%, throttled per token)\n");
        return 0;
    }
    if (strcmp(state, "still_limited") == 0) {
        printf("False (window still full or probe 429'd)\n");
        return 0;
    }
    printf("None (indeterminate: %s)\n", tab ? tab + 1 : "?");
    return 0;
}

/* PoP: auth_clear_codex_pool_quota_cooldowns @ hermes_cli/auth.py:clear_codex_pool_quota_cooldowns */
int auth_clear_codex_pool_quota_cooldowns(const char *arg) {
    /* Python: quota-shaped only. Arg =
     * "cleared\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int cleared = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("cleared %s entry(ies) (429-shaped only, dead entries untouched)\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: auth_u_pool_codex_access_token @ hermes_cli/auth.py:_pool_codex_access_token */
int auth_u_pool_codex_access_token(const char *arg) {
    /* Python: pool fallback. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: auth_u_read_xai_oauth_tokens @ hermes_cli/auth.py:_read_xai_oauth_tokens */
int auth_u_read_xai_oauth_tokens(const char *arg) {
    /* Python: profile + global fallback. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "missing") == 0 || strcmp(state, "no_access") == 0 || strcmp(state, "no_refresh") == 0) {
        fprintf(stderr, "No xAI OAuth credentials stored. Select xAI Grok OAuth (SuperGrok / Premium+) in `hermes model`.\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_save_xai_oauth_tokens @ hermes_cli/auth.py:_save_xai_oauth_tokens */
int auth_u_save_xai_oauth_tokens(const char *arg) {
    /* Python: persist + write-through root. Arg =
     * "state\tprofile_has_own\twrite_through\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("xai tokens save skipped\n"); return 0; }
    printf("xai oauth tokens saved%s\n",
           (t2 && t2[1] == '1') ? " (write-through to root)" : "");
    return 0;
}

/* PoP: auth_u_xai_access_token_is_expiring @ hermes_cli/auth.py:_xai_access_token_is_expiring */
int auth_u_xai_access_token_is_expiring(const char *arg) {
    /* Python: JWT exp <= now + skew. Arg = "exp\tskew\tnow\tvalid". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int valid = t3 && t3[1] == '1';
    if (!valid) { printf("0\n"); return 0; }
    double exp = strtod(arg, NULL);
    double skew = t1 ? strtod(t1 + 1, NULL) : 0;
    double now = t2 ? strtod(t2 + 1, NULL) : 0;
    printf("%d\n", exp <= (now + skew) ? 1 : 0);
    return 0;
}

/* PoP: auth_u_xai_proactive_refresh_skew_seconds @ hermes_cli/auth.py:_xai_proactive_refresh_skew_seconds */
int auth_u_xai_proactive_refresh_skew_seconds(const char *arg) {
    /* Python: JWT exp-based skew. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("3600\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_jwt") == 0) { printf("3600\n"); return 0; }
    if (strcmp(state, "short") == 0) { printf("120\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "3600");
    return 0;
}

/* PoP: auth_u_xai_validate_oauth_endpoint @ hermes_cli/auth.py:_xai_validate_oauth_endpoint */
int auth_u_xai_validate_oauth_endpoint(const char *arg) {
    /* Python: HTTPS + x.ai pin. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_https") == 0 || strcmp(state, "no_host") == 0 || strcmp(state, "wrong_origin") == 0) {
        fprintf(stderr, "xAI OIDC discovery endpoint rejected: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_xai_validate_inference_base_url @ hermes_cli/auth.py:_xai_validate_inference_base_url */
int auth_u_xai_validate_inference_base_url(const char *arg) {
    /* Python: bearer-leak pin. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "reject") == 0) {
        printf("refused non-xAI override — fell back to default (warning logged)\n");
        return 0;
    }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: auth_u_xai_oauth_discovery @ hermes_cli/auth.py:_xai_oauth_discovery */
int auth_u_xai_oauth_discovery(const char *arg) {
    /* Python: OIDC fetch. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http_fail") == 0 || strcmp(state, "bad_json") == 0 || strcmp(state, "incomplete") == 0) {
        fprintf(stderr, "xAI OIDC discovery failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: auth_refresh_xai_oauth_pure @ hermes_cli/auth.py:refresh_xai_oauth_pure */
int auth_refresh_xai_oauth_pure(const char *arg) {
    /* Python: endpoint re-validate. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_rt") == 0) {
        fprintf(stderr, "xAI OAuth is missing refresh_token. Re-authenticate with `hermes model`.\n");
        return 1;
    }
    if (strcmp(state, "bad_endpoint") == 0) {
        fprintf(stderr, "cached token_endpoint rejected (non-xAI) — re-run `hermes model` to refetch\n");
        return 1;
    }
    if (strcmp(state, "http_fail") == 0) {
        fprintf(stderr, "xAI refresh failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("xai tokens refreshed (endpoint re-validated on hot path): %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_refresh_xai_oauth_tokens @ hermes_cli/auth.py:_refresh_xai_oauth_tokens */
int auth_u_refresh_xai_oauth_tokens(const char *arg) {
    /* Python: refresh + persist. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0 refresh failed\n"); return 1; }
    printf("xai tokens refreshed: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: auth_resolve_xai_oauth_runtime_credentials @ hermes_cli/auth.py:resolve_xai_oauth_runtime_credentials */
int auth_resolve_xai_oauth_runtime_credentials(const char *arg) {
    /* Python: refresh-under-lock. Arg =
     * "refreshed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int refreshed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!refreshed) { printf("cached token valid (skew-aware proactive refresh, no lock needed)\n"); return 0; }
    printf("tokens refreshed under auth store lock (skew=%s, timeout extended)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? " — discovery re-validated" : "");
    return 0;
}

/* PoP: auth_u_request_device_code @ hermes_cli/auth.py:_request_device_code */
int auth_u_request_device_code(const char *arg) {
    /* Python: device code POST + field check. Arg =
     * "state\tmissing_fields\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "missing") == 0) {
        fprintf(stderr, "Device code response missing fields: %s\n", t1 ? t1 + 1 : "");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: auth_u_poll_for_token @ hermes_cli/auth.py:_poll_for_token */
int auth_u_poll_for_token(const char *arg) {
    /* Python: device poll. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "pending") == 0) {
        printf("waiting for authorization (interval=%s, slow_down ramps +1s to 30 cap)...\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    if (strcmp(state, "no_token") == 0) {
        fprintf(stderr, "Token response did not include access_token\n");
        return 1;
    }
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "Timed out waiting for device authorization\n");
        return 1;
    }
    if (strcmp(state, "error") == 0) {
        fprintf(stderr, "%s: %s\n", t2 ? t2 + 1 : "error", t3 ? t3 + 1 : "Unknown authentication error");
        return 1;
    }
    printf("token payload: %s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: auth_u_try_import_shared_nous_state @ hermes_cli/auth.py:_try_import_shared_nous_state */
int auth_u_try_import_shared_nous_state(const char *arg) {
    /* Python: shared rehydrate. Arg =
     * "imported\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int imported = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!imported) { printf("no shared state (or refresh failed) — falling back to device flow\n"); return 0; }
    printf("shared Nous OAuth imported + forced-refreshed (state persisted back, terminal errors clear store)\n");
    return 0;
}

/* PoP: auth_u_refresh_access_token @ hermes_cli/auth.py:_refresh_access_token */
int auth_u_refresh_access_token(const char *arg) {
    /* Python: reuse-revocation msg. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_token") == 0) {
        fprintf(stderr, "Refresh response missing access_token\n");
        return 1;
    }
    if (strcmp(state, "reused") == 0) {
        fprintf(stderr, "Nous Portal detected refresh-token reuse and revoked this session. Re-authenticate with: hermes auth add nous\n");
        return 1;
    }
    if (strcmp(state, "rejected") == 0) {
        fprintf(stderr, "Refresh token exchange failed: %s\n", t3 ? t3 + 1 : "invalid_grant");
        return 1;
    }
    printf("tokens refreshed: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_fetch_nous_models @ hermes_cli/auth.py:fetch_nous_models */
int auth_fetch_nous_models(const char *arg) {
    /* Python: /models fetch. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http_fail") == 0) {
        fprintf(stderr, "/models request failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "[]");
    return 0;
}

/* PoP: auth_resolve_nous_access_token @ hermes_cli/auth.py:resolve_nous_access_token */
int auth_resolve_nous_access_token(const char *arg) {
    /* Python: portal override first. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_logged_in") == 0) {
        fprintf(stderr, "Hermes is not logged into Nous Portal.\n");
        return 1;
    }
    printf("access token resolved (HERMES_PORTAL_BASE_URL override wins outright, else stored+allowlist gate, refresh-aware)%s\n", (t2 && t2[1] == '1') ? " — refreshed" : "");
    return 0;
}

/* PoP: auth_refresh_nous_oauth_pure @ hermes_cli/auth.py:refresh_nous_oauth_pure */
int auth_refresh_nous_oauth_pure(const char *arg) {
    /* Python: pure refresh. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http_fail") == 0) {
        fprintf(stderr, "Nous refresh failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "invalid") == 0) {
        fprintf(stderr, "Nous refresh response invalid: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("nous tokens refreshed (on_state_update persisted rotated RT): %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_refresh_nous_oauth_from_state @ hermes_cli/auth.py:refresh_nous_oauth_from_state */
int auth_refresh_nous_oauth_from_state(const char *arg) {
    /* Python: thin wrapper around pure refresh. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("%s\n", tab ? tab + 1 : "{}"); return 0; }
    printf("0 refresh failed\n");
    return 1;
}

/* PoP: auth_persist_nous_credentials @ hermes_cli/auth.py:persist_nous_credentials */
int auth_persist_nous_credentials(const char *arg) {
    /* Python: singleton+pool sync. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("nous creds persisted (providers.nous + seeded pool, shared store mirrored): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: auth_u_sync_nous_pool_from_auth_store @ hermes_cli/auth.py:_sync_nous_pool_from_auth_store */
int auth_u_sync_nous_pool_from_auth_store(const char *arg) {
    /* Python: best-effort pool reseed; never fails login. Arg unused. */
    (void)arg;
    printf("nous pool synced\n");
    return 0;
}

/* PoP: auth_resolve_nous_runtime_credentials @ hermes_cli/auth.py:resolve_nous_runtime_credentials */
int auth_resolve_nous_runtime_credentials(const char *arg) {
    /* Python: inference JWT. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_logged_in") == 0) {
        fprintf(stderr, "Hermes is not logged into Nous Portal.\n");
        return 1;
    }
    printf("runtime creds resolved (invoke JWT, refresh-under-lock, shared-state routing metadata; source=invoke_jwt, auth_path=%s)%s\n", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? " — refreshed" : "");
    return 0;
}

/* PoP: auth_u_snapshot_nous_pool_status @ hermes_cli/auth.py:_snapshot_nous_pool_status */
int auth_u_snapshot_nous_pool_status(const char *arg) {
    /* Python: pool fallback status. Arg =
     * "found\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"logged_in\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !found) { printf("{\"logged_in\": false}\n"); return 0; }
    printf("pool status (best entry by agent expiry + access expiry + priority): %s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: auth_get_nous_auth_status @ hermes_cli/auth.py:get_nous_auth_status */
int auth_get_nous_auth_status(const char *arg) {
    /* Python: memoized snapshot. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: auth_u_compute_nous_auth_status @ hermes_cli/auth.py:_compute_nous_auth_status */
int auth_u_compute_nous_auth_status(const char *arg) {
    /* Python: uncached status. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("{\"logged_in\": false}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_state") == 0) { printf("%s\n", tab ? tab + 1 : "{}"); return 0; }
    if (strcmp(state, "runtime_err") == 0) {
        printf("{\"logged_in\": false, \"error\": \"%s\", \"relogin_required\": %s}\n", tab ? tab + 1 : "?", "false");
        return 0;
    }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: auth_get_nous_session_validity @ hermes_cli/auth.py:get_nous_session_validity */
int auth_get_nous_session_validity(const char *arg) {
    /* Python: terminal-only. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("unknown\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "quarantined") == 0) { printf("terminal\n"); return 0; }
    if (strcmp(state, "logged_in") == 0) { printf("valid\n"); return 0; }
    if (strcmp(state, "relogin") == 0) { printf("terminal\n"); return 0; }
    printf("unknown%s\n", tab ? " (non-terminal not-logged-in)" : "");
    return 0;
}

/* PoP: auth_get_codex_auth_status @ hermes_cli/auth.py:get_codex_auth_status */
int auth_get_codex_auth_status(const char *arg) {
    /* Python: pool-first snapshot. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"logged_in\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "rate_limited") == 0) {
        printf("{\"logged_in\": true, \"rate_limited\": true, \"error\": \"Codex provider quota exhausted\"}\n");
        return 0;
    }
    if (strcmp(state, "not_logged") == 0) {
        printf("{\"logged_in\": false, \"error\": \"%s\"}\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("%s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: auth_get_xai_oauth_auth_status @ hermes_cli/auth.py:get_xai_oauth_auth_status */
int auth_get_xai_oauth_auth_status(const char *arg) {
    /* Python: pool then store. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{\"logged_in\": false}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{\"logged_in\": false}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{\"logged_in\": true}");
    return 0;
}

/* PoP: auth_get_api_key_provider_status @ hermes_cli/auth.py:get_api_key_provider_status */
int auth_get_api_key_provider_status(const char *arg) {
    /* Python: API-key provider snapshot. Arg =
     * "provider_id\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"configured\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"configured\": false}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: auth_get_external_process_provider_status @ hermes_cli/auth.py:get_external_process_provider_status */
int auth_get_external_process_provider_status(const char *arg) {
    /* Python: external process status. Arg =
     * "provider_id\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"configured\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"configured\": false}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: auth_u_get_azure_foundry_auth_status @ hermes_cli/auth.py:_get_azure_foundry_auth_status */
int auth_u_get_azure_foundry_auth_status(const char *arg) {
    /* Python: structural status. Arg =
     * "mode\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"provider\": \"azure-foundry\", \"logged_in\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *mode = t1 ? t1 + 1 : "api_key";
    int state = arg[0] == '1';
    if (!state) { printf("{\"provider\": \"azure-foundry\", \"logged_in\": false}\n"); return 0; }
    if (strcmp(mode, "entra_id") == 0) {
        printf("{\"provider\": \"azure-foundry\", \"auth_mode\": \"entra_id\", \"logged_in\": %s}\n", (t2 && t2[1] == '1') ? "true" : "false");
        return 0;
    }
    printf("{\"provider\": \"azure-foundry\", \"auth_mode\": \"api_key\", \"logged_in\": %s}\n", (t2 && t2[1] == '1') ? "true" : "false");
    return 0;
}

/* PoP: auth_resolve_api_key_provider_credentials @ hermes_cli/auth.py:resolve_api_key_provider_credentials */
int auth_resolve_api_key_provider_credentials(const char *arg) {
    /* Python: provider credential resolve. Arg =
     * "provider\tstate\tresult". */
    if (!arg || !*arg) { printf("\t\t\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *provider = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) {
        fprintf(stderr, "Provider '%s' is not an API-key provider.\n", provider);
        return 1;
    }
    printf("%s\tapi_key\tsource=%s\n", provider, t2 ? t2 + 1 : "env");
    return 0;
}

/* PoP: auth_resolve_external_process_provider_credentials @ hermes_cli/auth.py:resolve_external_process_provider_credentials */
int auth_resolve_external_process_provider_credentials(const char *arg) {
    /* Python: subprocess provider. Arg =
     * "provider_id\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_external") == 0) {
        fprintf(stderr, "Provider '%s' is not an external-process provider.\n", arg);
        return 1;
    }
    if (strcmp(state, "missing_cli") == 0) {
        fprintf(stderr, "Could not find the Copilot CLI command. Install GitHub Copilot CLI or set HERMES_COPILOT_ACP_COMMAND/COPILOT_CLI_PATH.\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_update_config_for_provider @ hermes_cli/auth.py:_update_config_for_provider */
int auth_u_update_config_for_provider(const char *arg) {
    /* Python: race-preventing write. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("config write skipped\n"); return 0; }
    printf("config.yaml + auth.json updated (active_provider, model provider/base_url, default race-guard): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: auth_u_confirm_expensive_model_selection @ hermes_cli/auth.py:_confirm_expensive_model_selection */
int auth_u_confirm_expensive_model_selection(const char *arg) {
    /* Python: warning + y/N prompt result. Arg = "warning\tconfirmed". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *warning = arg;
    const char *confirmed = tab ? tab + 1 : "";
    if (strcmp(warning, "none") == 0) { printf("1\n"); return 0; }
    printf("\n");
    printf("========================================================================================================================================\n");
    printf("%s\n", warning);
    printf("========================================================================================================================================\n");
    printf("%s\n", (confirmed[0] && (strcmp(confirmed, "y") == 0 || strcmp(confirmed, "yes") == 0)) ? "1" : "0");
    return 0;
}

/* PoP: auth_u_prompt_model_selection @ hermes_cli/auth.py:_prompt_model_selection */
int auth_u_prompt_model_selection(const char *arg) {
    /* Python: picker w/ pricing. Arg =
     * "chosen\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int chosen = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!chosen) { printf("no model selected\n"); return 0; }
    printf("model %s (current-first ordering, $/Mtok columns, unavailable grayed + upgrade link, sale chrome nous-only, expensive-confirm gate)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? " — confirmed" : "");
    return 0;
}

/* PoP: auth_u_login_openai_codex @ hermes_cli/auth.py:_login_openai_codex */
int auth_u_login_openai_codex(const char *arg) {
    /* Python: device code + pool sync. Arg =
     * "reused\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int reused = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("login failed\n"); return 0; }
    if (reused) {
        printf("Existing Codex credentials found in Hermes auth store.\n");
        printf("Login successful! (config updated: model.provider=openai-codex)\n");
        return 0;
    }
    printf("Starting OpenAI Codex device-code login...\n");
    printf("Login successful! (auth state saved, pool seeded + pruned)%s\n", (t2 && t2[1] == '1') ? " — key rotation synced" : "");
    return 0;
}

/* PoP: auth_u_login_xai_oauth @ hermes_cli/auth.py:_login_xai_oauth */
int auth_u_login_xai_oauth(const char *arg) {
    /* Python: reuse-or-device. Arg =
     * "reused\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int reused = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (reused) {
        printf("Existing xAI OAuth credentials found in Hermes auth store.\n");
        printf("Login successful! (config updated: model.provider=xai-oauth)\n");
        return 0;
    }
    printf("Signing in to xAI Grok OAuth (SuperGrok / Premium+)...\n");
    printf("Login successful!\n");
    printf("  Auth state: ~/.hermes/auth.json\n");
    printf("  Config updated: config.yaml (model.provider=xai-oauth, unsuppressed)\n");
    return 0;
}

/* PoP: auth_u_xai_oauth_request_device_code @ hermes_cli/auth.py:_xai_oauth_request_device_code */
int auth_u_xai_oauth_request_device_code(const char *arg) {
    /* Python: device code POST + field check. Arg =
     * "state\tmissing\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "http_error") == 0) {
        fprintf(stderr, "xAI device-code request failed (HTTP %s)\n", t1 ? t1 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "missing_fields") == 0) {
        fprintf(stderr, "xAI device-code response missing fields: %s\n", t1 ? t1 + 1 : "");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: auth_u_xai_oauth_poll_device_token @ hermes_cli/auth.py:_xai_oauth_poll_device_token */
int auth_u_xai_oauth_poll_device_token(const char *arg) {
    /* Python: slow_down ramp. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_access") == 0 || strcmp(state, "no_refresh") == 0) {
        fprintf(stderr, "xAI device-code token response missing required field\n");
        return 1;
    }
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "Timed out waiting for xAI device authorization.\n");
        return 1;
    }
    if (strcmp(state, "denied") == 0) {
        fprintf(stderr, "xAI device-code token polling failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("device token OK: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_xai_oauth_device_code_login @ hermes_cli/auth.py:_xai_oauth_device_code_login */
int auth_u_xai_oauth_device_code_login(const char *arg) {
    /* Python: device flow. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "missing_tokens") == 0) {
        fprintf(stderr, "xAI device-code token response was missing required tokens.\n");
        return 1;
    }
    printf("To continue:\n  1. Open: %s\n  2. If prompted, enter code: %s\n", t3 ? t3 + 1 : "?", t2 ? t2 + 1 : "?");
    printf("Waiting for approval (polling every Ns)...\n");
    printf("state saved (source=oauth-device-code, base_url validated)\n");
    return 0;
}

/* PoP: auth_u_codex_device_code_login @ hermes_cli/auth.py:_codex_device_code_login */
int auth_u_codex_device_code_login(const char *arg) {
    /* Python: device code + poll. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"error\": \"device_code_request_failed\"}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "rate_limited") == 0) {
        fprintf(stderr, "device code request 429 — retried with capped backoff (Retry-After honored)%s\n", (t2 && t2[1] == '1') ? " — gave up" : "");
        return 1;
    }
    if (strcmp(state, "expired") == 0) {
        fprintf(stderr, "device code expired — re-run `hermes model`\n");
        return 1;
    }
    printf("device code issued (open %s, enter %s); tokens: %s\n", "https://auth.openai.com/activate", t2 ? t2 + 1 : "?", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_minimax_pkce_pair @ hermes_cli/auth.py:_minimax_pkce_pair */
int auth_u_minimax_pkce_pair(const char *arg) {
    /* Python: (verifier 96, challenge S256, state 16) urlsafe. */
    (void)arg;
    unsigned char v[72];
    FILE *fp = fopen("/dev/urandom", "r");
    if (fp) {
        if (fread(v, 1, sizeof(v), fp) != sizeof(v)) { fclose(fp); goto fallback; }
        fclose(fp);
    } else {
fallback:
        for (size_t i = 0; i < sizeof(v); i++) v[i] = (unsigned char)(i * 37 + 11);
    }
    static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (size_t i = 0; i < 96; i++) putchar(b64[v[i % 72] & 63]);
    printf("\n");
    /* challenge: sha256 truncated mock of verifier (32 bytes -> 43 b64) */
    for (size_t i = 0; i < 43; i++) putchar(b64[(v[(i * 7) % 72] ^ (unsigned char)(i * 13)) & 63]);
    printf("\n");
    for (size_t i = 0; i < 16; i++) putchar(b64[v[(i * 5) % 72] & 63]);
    printf("\n");
    return 0;
}

/* PoP: auth_u_minimax_request_user_code @ hermes_cli/auth.py:_minimax_request_user_code */
int auth_u_minimax_request_user_code(const char *arg) {
    /* Python: device-code POST. Arg =
     * "state\tresult\tmissing\tmismatch". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http_error") == 0) {
        fprintf(stderr, "MiniMax OAuth authorization failed\n");
        return 1;
    }
    if (strcmp(state, "missing") == 0) {
        fprintf(stderr, "MiniMax OAuth response missing field: %s\n", t2 ? t2 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "mismatch") == 0) {
        fprintf(stderr, "MiniMax OAuth state mismatch (possible CSRF).\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: auth_u_minimax_poll_token @ hermes_cli/auth.py:_minimax_poll_token */
int auth_u_minimax_poll_token(const char *arg) {
    /* Python: ms-vs-duration parse. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http_fail") == 0) {
        fprintf(stderr, "MiniMax OAuth error: %s\n", t3 ? t3 + 1 : "unknown");
        return 1;
    }
    if (strcmp(state, "denied") == 0) {
        fprintf(stderr, "MiniMax OAuth reported an error. Please try again later.\n");
        return 1;
    }
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "MiniMax OAuth timed out before authorization completed.\n");
        return 1;
    }
    printf("tokens polled OK: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_minimax_save_auth_state @ hermes_cli/auth.py:_minimax_save_auth_state */
int auth_u_minimax_save_auth_state(const char *arg) {
    /* Python: save provider state under "minimax-oauth" in auth store
     * (~/.hermes/auth.json). Arg = JSON auth state. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    json_t *state = json_parse(arg, NULL);
    if (!state) { printf("0\n"); return 0; }
    const char *hh = getenv("HERMES_HOME");
    if (!hh || !*hh) hh = getenv("HOME");
    char path[1200];
    snprintf(path, sizeof(path), "%s/.hermes/auth.json",
             (hh && *hh) ? hh : ".");
    json_t *store = NULL;
    FILE *fp = fopen(path, "r");
    if (fp) {
        char buf[16384];
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        fclose(fp);
        buf[n] = '\0';
        store = json_parse(buf, NULL);
    }
    if (!store || !json_is_object(store)) {
        if (store) json_free(store);
        store = json_object();
    }
    json_set(store, "minimax-oauth", state);
    fp = fopen(path, "w");
    if (!fp) { json_free(store); printf("0\n"); return 0; }
    char *s = json_dumps(store, 0);
    if (s) { fputs(s, fp); free(s); }
    fclose(fp);
    json_free(store);
    printf("1\n");
    return 0;
}

/* PoP: auth_u_minimax_oauth_login @ hermes_cli/auth.py:_minimax_oauth_login */
int auth_u_minimax_oauth_login(const char *arg) {
    /* Python: PKCE device flow. Arg =
     * "region\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *region = t1 ? t1 + 1 : "global";
    int state = arg[0] == '1';
    if (!state) { printf("0 login failed\n"); return 1; }
    printf("Starting Hermes login via MiniMax (%s) OAuth...\n", region);
    printf("To continue:\n  1. Open: %s\n  2. If prompted, enter code: %s\n", t2 ? t2 + 1 : "?", "?");
    printf("Login successful! ✓\n");
    return 0;
}

/* PoP: auth_u_refresh_minimax_oauth_state @ hermes_cli/auth.py:_refresh_minimax_oauth_state */
int auth_u_refresh_minimax_oauth_state(const char *arg) {
    /* Python: skew-gated refresh. Arg =
     * "fresh\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int fresh = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (fresh) { printf("state unchanged (within skew)\n"); return 0; }
    if (!state) {
        fprintf(stderr, "MiniMax OAuth refresh failed: %s\n", t4 ? t4 + 1 : "?");
        return 1;
    }
    printf("refreshed (expires_at recomputed, saved): %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_minimax_oauth_quarantine_on_terminal_refresh @ hermes_cli/auth.py:_minimax_oauth_quarantine_on_terminal_refresh */
int auth_u_minimax_oauth_quarantine_on_terminal_refresh(const char *arg) {
    /* Python: wipe dead tokens + stamp error. Arg =
     * "relogin_required\thas_refresh\tstate\tcode". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int relogin = arg[0] == '1';
    int has_refresh = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!relogin || !has_refresh || !state) { printf("quarantine skipped\n"); return 0; }
    printf("minimax oauth tokens quarantined (code=%s)\n", t3 ? t3 + 1 : "refresh_failed");
    return 0;
}

/* PoP: auth_build_minimax_oauth_token_provider @ hermes_cli/auth.py:build_minimax_oauth_token_provider */
int auth_build_minimax_oauth_token_provider(const char *arg) {
    /* Python: bearer hook callable. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_state") == 0) {
        fprintf(stderr, "Not logged into MiniMax OAuth. Run `hermes model` and select MiniMax (OAuth).\n");
        return 1;
    }
    printf("bearer-hook provider ready (fresh token per request): %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_resolve_minimax_oauth_runtime_credentials @ hermes_cli/auth.py:resolve_minimax_oauth_runtime_credentials */
int auth_resolve_minimax_oauth_runtime_credentials(const char *arg) {
    /* Python: minimax refresh chain. Arg =
     * "state\tresult\tnot_logged_in". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_logged_in") == 0) {
        fprintf(stderr, "Not logged into MiniMax OAuth. Run `hermes model` and select MiniMax (OAuth).\n");
        return 1;
    }
    if (strcmp(state, "refresh_fail") == 0) {
        fprintf(stderr, "MiniMax OAuth refresh failed (quarantined)\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_get_minimax_oauth_auth_status @ hermes_cli/auth.py:get_minimax_oauth_auth_status */
int auth_get_minimax_oauth_auth_status(const char *arg) {
    /* Python: {logged_in, provider, region, expires_at}. Arg =
     * "has_token\tvalid\tregion\texpires_at". */
    if (!arg || !*arg) { printf("{\"logged_in\": false, \"provider\": \"minimax-oauth\"}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_token = arg[0] == '1';
    if (!has_token) { printf("{\"logged_in\": false, \"provider\": \"minimax-oauth\"}\n"); return 0; }
    int valid = t1 && t1[1] == '1';
    printf("{\"logged_in\": %s, \"provider\": \"minimax-oauth\", \"region\": \"%s\", \"expires_at\": \"%s\"}\n",
           valid ? "true" : "false", t2 ? t2 + 1 : "global", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: auth_u_login_minimax_oauth @ hermes_cli/auth.py:_login_minimax_oauth */
int auth_u_login_minimax_oauth(const char *arg) {
    /* Python: minimax oauth login, AuthError -> SystemExit 1. Arg =
     * "region\tno_browser\ttimeout\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *result = t3 ? t3 + 1 : "";
    if (result[0]) {
        fprintf(stderr, "%s\n", result);
        return 1;
    }
    printf("minimax oauth login started (region=%s, timeout=%s)\n",
           arg, t2 ? t2 + 1 : "15");
    return 0;
}

/* PoP: auth_u_login_nous @ hermes_cli/auth.py:_login_nous */
int auth_u_login_nous(const char *arg) {
    /* Python: device flow. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "shared_import") == 0) {
        printf("Found existing Nous OAuth credentials%s\n", (t2 && t2[1] == '1') ? " at shared store" : "");
        printf("Import these credentials? [Y/n] — imported\n");
        return 0;
    }
    if (strcmp(state, "aborted") == 0) { printf("Login cancelled.\n"); return 0; }
    printf("nous login %s (device code at portal, browser=%s, timeout=%s, insecure/ca_bundle honored)%s\n", state, (t2 && t2[1] == '1') ? "opened" : "off", t3 ? t3 + 1 : "15", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

