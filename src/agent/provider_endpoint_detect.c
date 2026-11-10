/*
 * provider_endpoint_detect.c — provider endpoint / auth-classification detectors.
 *
 * Self-contained module carved out of provider_anthropic.c (monolith split).
 * Pure URL / token / model-id classifiers; no file-scope state. All public
 * functions declared in include/provider.h; deps only on url_host_matches
 * (hermes_url_safety.h) and hermes_log (hermes_logger.h).
 */
#include "provider.h"
#include "hermes_url_safety.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>


/* ================================================================
 *  Free response
 * ================================================================ */

/* ================================================================
 *  Endpoint detection utilities — ported from Python anthropic_adapter.py
 * ================================================================ */

/* Port of Python anthropic_adapter.py:_is_oauth_token(). */
bool is_oauth_token(const char *key) {
    if (!key || !*key) return false;
    /* Anthropic Console API-key families — x-api-key auth, never OAuth.
     * Python excludes all four (agent/anthropic_credentials.py:67); this used
     * to exclude only sk-ant-api, so sk-ant-usr/svc/admin were misread as
     * OAuth and picked up the Claude Code identity header. */
    static const char *const CONSOLE_PREFIXES[] = {
        "sk-ant-api", "sk-ant-usr", "sk-ant-svc", "sk-ant-admin",
    };
    for (size_t i = 0; i < sizeof(CONSOLE_PREFIXES) / sizeof(CONSOLE_PREFIXES[0]); i++) {
        if (strncmp(key, CONSOLE_PREFIXES[i], strlen(CONSOLE_PREFIXES[i])) == 0) return false;
    }
    /* Anthropic-issued tokens (setup-tokens sk-ant-oat-*, managed keys) */
    if (strncmp(key, "sk-ant-", 7) == 0) return true;
    /* JWTs from Anthropic OAuth flow */
    if (strncmp(key, "eyJ", 3) == 0) return true;
    /* Claude Code OAuth access tokens */
    if (strncmp(key, "cc-", 3) == 0) return true;
    return false;
}

/* Port of Python anthropic_adapter.py:_normalize_base_url_text(). */
char *anthropic_normalize_base_url_text(const char *base_url) {
    if (!base_url || !*base_url) return strdup("");
    /* Strip leading/trailing whitespace */
    while (*base_url == ' ' || *base_url == '\t') base_url++;
    if (!*base_url) return strdup("");
    size_t len = strlen(base_url);
    while (len > 0 && (base_url[len-1] == ' ' || base_url[len-1] == '\t')) len--;
    return strndup(base_url, len);
}

/* Port of Python anthropic_adapter.py:_is_third_party_anthropic_endpoint(). */
bool anthropic_is_third_party_endpoint(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    /* Lowercase for comparison */
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    /* Strip trailing slash */
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (strstr(lower, "anthropic.com") == NULL);
    free(lower);
    free(norm);
    return result;
}

/* Port of Python anthropic_adapter.py:_is_kimi_coding_endpoint(). */
bool is_kimi_coding_endpoint(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (strncmp(lower, "https://api.kimi.com/coding", 27) == 0);
    free(lower);
    free(norm);
    return result;
}
static bool str_is_kimi_prefix(const char *m) {
    if (!m || !*m) return false;
    static const char *prefixes[] = {
        "kimi-", "kimi_", "moonshot-", "moonshot_",
        "k1.", "k1-", "k2.", "k2-", "k25", "k2.5", NULL
    };
    for (int i = 0; prefixes[i]; i++) {
        size_t plen = strlen(prefixes[i]);
        if (strncmp(m, prefixes[i], plen) == 0) return true;
    }
    return false;
}

/* Port of Python anthropic_adapter.py:_model_name_is_kimi_family(). */
bool model_name_is_kimi_family(const char *model) {
    if (!model || !*model) return false;
    const char *m = model;
    while (*m == ' ' || *m == '\t') m++;
    if (!*m) return false;
    size_t len = strlen(m);
    char *lower = malloc(len + 1);
    if (!lower) return false;
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)m[i]);
    lower[len] = '\0';
    /* Strip vendor prefix */
    char *slash = strrchr(lower, '/');
    char *prefix = slash ? slash + 1 : lower;
    /* Strip trailing whitespace */
    size_t plen = strlen(prefix);
    while (plen > 0 && (prefix[plen-1] == ' ' || prefix[plen-1] == '\t')) prefix[--plen] = '\0';
    bool result = str_is_kimi_prefix(prefix);
    free(lower);
    return result;
}

/* Port of Python anthropic_adapter.py:_is_kimi_family_endpoint(). */
bool is_kimi_family_endpoint(const char *base_url, const char *model) {
    if (is_kimi_coding_endpoint(base_url)) return true;
    /* Check known domains */
    if (base_url && *base_url) {
        if (url_host_matches(base_url, "api.kimi.com") ||
            url_host_matches(base_url, "moonshot.ai") ||
            url_host_matches(base_url, "moonshot.cn"))
            return true;
    }
    if (model_name_is_kimi_family(model)) return true;
    return false;
}

/* Port of Python anthropic_adapter.py:_is_deepseek_anthropic_endpoint(). */
bool anthropic_is_deepseek_endpoint(const char *base_url) {
    if (!base_url || !*base_url) return false;
    if (!url_host_matches(base_url, "api.deepseek.com")) return false;
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (strstr(lower, "/anthropic") != NULL);
    free(lower);
    free(norm);
    return result;
}

/* Port of Python anthropic_adapter.py:_requires_bearer_auth(). */
bool requires_bearer_auth(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (
        (strncmp(lower, "https://api.minimax.io/anthropic", 33) == 0 ||
         strncmp(lower, "https://api.minimaxi.com/anthropic", 34) == 0 ||
         strstr(lower, "azure.com") != NULL)
    );
    free(lower);
    free(norm);
    return result;
}

/* Port of Python anthropic_adapter.py:_base_url_needs_context_1m_beta(). */
bool anthropic_base_url_needs_1m_beta(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    bool result = (strstr(lower, "azure.com") != NULL);
    free(lower);
    free(norm);
    return result;
}

/* Port of Python anthropic_adapter.py:_is_minimax_anthropic_endpoint(). */
bool anthropic_is_minimax_endpoint(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (
        strncmp(lower, "https://api.minimax.io/anthropic", 33) == 0 ||
        strncmp(lower, "https://api.minimaxi.com/anthropic", 34) == 0
    );
    free(lower);
    free(norm);
    return result;
}

/* Port of Python anthropic_adapter.py:_is_azure_anthropic_endpoint(). */
bool is_azure_anthropic_endpoint(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    /* URL parsing: extract hostname */
    const char *host_start = NULL;
    const char *proto = strstr(lower, "://");
    if (proto) host_start = proto + 3; else host_start = lower;
    const char *path_start = strchr(host_start, '/');
    size_t host_len = path_start ? (size_t)(path_start - host_start) : strlen(host_start);
    /* Extract host portion for dot-padded matching */
    char host_buf[512];
    size_t hl = host_len < 511 ? host_len : 511;
    memcpy(host_buf, host_start, hl);
    host_buf[hl] = '\0';
    /* Check with dot-padded boundaries */
    char padded[520];
    snprintf(padded, sizeof(padded), ".%s.", host_buf);
    const char *path = path_start ? path_start : "";
    bool is_foundry = (strstr(padded, ".services.ai.azure.") != NULL);
    bool is_legacy = (strstr(padded, ".openai.azure.") != NULL);
    bool has_anthropic_path = (strstr(path, "/anthropic") != NULL);
    bool result = (is_foundry || is_legacy) && has_anthropic_path;
    free(lower);
    free(norm);
    return result;
}

/* Port of Python anthropic_adapter.py:_is_bedrock_model_id(). */
bool is_bedrock_model_id(const char *model_id) {
    if (!model_id || !*model_id) return false;
    /* Bedrock model IDs: anthropic.claude-* */
    if (strncmp(model_id, "anthropic.claude-", 17) == 0) return true;
    /* Also check for common Bedrock ARN format */
    if (strstr(model_id, "bedrock") && strstr(model_id, "anthropic")) return true;
    return false;
}

/* PoP: anthropic_resolve_positive_max_tokens @ agent/anthropic_adapter.py:_resolve_positive_anthropic_max_tokens */
/* Port of Python anthropic_adapter.py:_resolve_positive_anthropic_max_tokens(). */
int anthropic_resolve_positive_max_tokens(int value) {
    if (value <= 0) return 0;
    hermes_log(LOG_DEBUG, "anthropic", "resolve_positive_max_tokens(%d) = %d", value, value);
    return value;
}

/* Port of Python anthropic_adapter.py:is_claude_code_token_valid().
 * Check if Claude Code credentials have a non-expired access token.
 * Takes a parsed JSON object of credentials and returns true if valid.
 */
bool is_claude_code_token_valid(json_t *creds) {
    if (!creds) return false;

    double expires_at = json_get_num(creds, "expiresAt", 0.0);
    if (expires_at <= 0.0) {
        /* No expiry set (managed keys) — valid if token is present */
        const char *token = json_get_str(creds, "accessToken", NULL);
        return token && token[0] != '\0';
    }

    /* expiresAt is in milliseconds since epoch */
    time_t now_s = time(NULL);
    double now_ms = (double)now_s * 1000.0;
    /* Allow 60 seconds of buffer */
    return now_ms < (expires_at - 60000.0);
}
