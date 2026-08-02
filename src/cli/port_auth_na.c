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
} auth_error_t;

/*
 * PoP: is_rate_limited_auth_error @ hermes_cli/auth.py:is_rate_limited_auth_error */
int is_rate_limited_auth_error(const auth_error_t *error)
{
    if (!error) return 0;
    return (error->code != NULL) && (error->relogin_required == 0) &&
           (strcmp(error->code, "429") == 0);
}

/*
 * PoP: _parse_retry_after_seconds @ hermes_cli/auth.py:_parse_retry_after_seconds
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
 * PoP: _token_fingerprint @ hermes_cli/auth.py:_token_fingerprint
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
 * PoP: format_auth_error @ hermes_cli/auth.py:format_auth_error
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
 * PoP: _auth_file_path @ hermes_cli/auth.py:_auth_file_path
 * Returns malloc'd "<hermes_home>/auth.json". Caller frees. */
char *auth_file_path(void)
{
    const char *home = auth_hermes_home();
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/auth.json", home);
    return strdup(out);
}

/*
 * PoP: _global_auth_file_path @ hermes_cli/auth.py:_global_auth_file_path
 * Returns malloc'd global-root auth.json when in profile mode (root != home),
 * else NULL (classic mode). Caller frees result (or ignores NULL). */
char *global_auth_file_path(void)
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
 * PoP: has_usable_secret @ hermes_cli/auth.py:has_usable_secret
 * Returns 1 when value is a non-empty, non-placeholder secret string. */
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
 * PoP: _is_expiring @ hermes_cli/auth.py:_is_expiring
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
 * PoP: _optional_base_url @ hermes_cli/auth.py:_optional_base_url
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
 * PoP: _decode_jwt_claims @ hermes_cli/auth.py:_decode_jwt_claims
 * Decode the payload segment of a JWT into a JSON object. Returns NULL on
 * failure; caller frees via json_free. */
json_t *decode_jwt_claims(const char *token)
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
 * PoP: _scope_values @ hermes_cli/auth.py:_scope_values
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
 * PoP: _spotify_scope_list @ hermes_cli/auth.py:_spotify_scope_list
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
 * PoP: _spotify_client_id @ hermes_cli/auth.py:_spotify_client_id
 * Returns malloc'd client_id from explicit/state/env, or NULL if missing. */
char *spotify_client_id(const char *explicit, const char *state_client_id)
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
 * PoP: _spotify_code_verifier @ hermes_cli/auth.py:_spotify_code_verifier
 * Returns malloc'd PKCE code_verifier (urlsafe base64 of `length` random bytes,
 * stripped of '=' and truncated to 128). Caller frees. */
char *spotify_code_verifier(int length)
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
    return spotify_code_verifier(length);
}

/*
 * PoP: _oauth_pkce_code_challenge @ hermes_cli/auth.py:_oauth_pkce_code_challenge */
char *oauth_pkce_code_challenge(const char *code_verifier)
{
    return spotify_code_challenge(code_verifier);
}

/*
 * PoP: _spotify_build_authorize_url @ hermes_cli/auth.py:_spotify_build_authorize_url
 * Build the Spotify PKCE authorize URL. Returns malloc'd string. Caller frees. */
char *spotify_build_authorize_url(const char *client_id, const char *redirect_uri,
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
 * PoP: _spotify_validate_redirect_uri @ hermes_cli/auth.py:_spotify_validate_redirect_uri
 * Validate a Spotify loopback redirect URI. Returns 0 on success (filling
 * *host_out, *port_out, *path_out via malloc), -1 on invalid (err filled). */
int spotify_validate_redirect_uri(const char *redirect_uri, char **host_out,
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
 * PoP: _xai_validate_loopback_redirect_uri @ hermes_cli/auth.py:_xai_validate_loopback_redirect_uri
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
 * PoP: _minimax_resolve_token_expiry_unix @ hermes_cli/auth.py:_minimax_resolve_token_expiry_unix
 * Returns access-token expiry as unix seconds. */
double minimax_resolve_token_expiry_unix(long expired_in, double now_unix)
{
    long now_ms = (long)(now_unix * 1000.0);
    if (minimax_expired_in_looks_like_unix_ms(expired_in, now_ms))
        return (double)expired_in / 1000.0;
    return now_unix + (expired_in > 1 ? expired_in : 1);
}

/*
 * PoP: _qwen_cli_auth_path @ hermes_cli/auth.py:_qwen_cli_auth_path
 * Returns malloc'd path to the Qwen CLI auth tokens file. Caller frees. */
char *qwen_cli_auth_path(void)
{
    const char *home = getenv("HOME");
    if (!home) home = "~";
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/.config/qwen-cli/auth.json", home);
    return strdup(out);
}
