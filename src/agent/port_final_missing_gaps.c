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
    extern void crypto_random_bytes(unsigned char *buf, size_t n);
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
