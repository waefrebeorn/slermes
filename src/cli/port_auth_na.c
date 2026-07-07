/*
 * port_auth_na.c — Port of Python hermes_cli/auth.py (NA_CLI functions)
 * Functions that don't exist in port_auth.c.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "libcrypto/crypto.h"

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
} auth_error_t;

/* PoP: is_rate_limited_auth_error @ hermes_cli/auth.py:is_rate_limited_auth_error */
int is_rate_limited_auth_error(const auth_error_t *error)
{
    if (!error) return 0;
    return (error->code != NULL) && (error->relogin_required == 0) &&
           (strcmp(error->code, "429") == 0);
}

/* PoP: _parse_retry_after_seconds @ hermes_cli/auth.py:_parse_retry_after_seconds
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

/* PoP: _token_fingerprint @ hermes_cli/auth.py:_token_fingerprint
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

/* PoP: format_auth_error @ hermes_cli/auth.py:format_auth_error
 * Returns malloc'd user-facing message. Caller frees. For non-AuthError
 * inputs returns a copy of msg. */
char *format_auth_error(const auth_error_t *error)
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
