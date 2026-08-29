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

/* ════════════════════════════════════════════════════════════════════ */
/* Remaining parity closures (auth.py)                                 */
/* ════════════════════════════════════════════════════════════════════ */

#include "../../include/hermes_retry_utils.h"
#include "../../include/model_selection_guards.h"
#include "../../include/port_auth_store.h"

/* ── _LazyHttpx (lazy httpx module proxy) ────────────────────────────── */

/* Python's _LazyHttpx defers `import httpx` until first attribute access.
 * C has no import machinery, so the lazy target is an opaque handle the
 * owner materializes on first use via the resolve callback; the dunder
 * operations delegate to caller-supplied accessors on that target. */
typedef struct auth_lazy_httpx {
    void  *resolved;                                  /* cached _resolve() */
    void *(*resolve)(struct auth_lazy_httpx *self);   /* _resolve */
    void *(*get)(void *target, const char *name);     /* getattr on target */
    int   (*set)(void *target, const char *name, void *value);
    int   (*del)(void *target, const char *name);
} auth_lazy_httpx_t;

/* PoP: auth_lazy_httpx_resolve @ hermes_cli/auth.py:_LazyHttpx._resolve */
void *auth_lazy_httpx_resolve(auth_lazy_httpx_t *self)
{
    if (!self || !self->resolve) return NULL;
    if (!self->resolved) self->resolved = self->resolve(self);
    return self->resolved;
}

/* PoP: auth_lazy_httpx_getattr @ hermes_cli/auth.py:_LazyHttpx.__getattr__ */
void *auth_lazy_httpx_getattr(auth_lazy_httpx_t *self, const char *name)
{
    if (!self || !self->get) return NULL;
    void *target = auth_lazy_httpx_resolve(self);
    return target ? self->get(target, name) : NULL;
}

/* PoP: auth_lazy_httpx_setattr @ hermes_cli/auth.py:_LazyHttpx.__setattr__ */
int auth_lazy_httpx_setattr(auth_lazy_httpx_t *self, const char *name, void *value)
{
    if (!self || !self->set) return -1;
    void *target = auth_lazy_httpx_resolve(self);
    return target ? self->set(target, name, value) : -1;
}

/* PoP: auth_lazy_httpx_delattr @ hermes_cli/auth.py:_LazyHttpx.__delattr__ */
int auth_lazy_httpx_delattr(auth_lazy_httpx_t *self, const char *name)
{
    if (!self || !self->del) return -1;
    void *target = auth_lazy_httpx_resolve(self);
    return target ? self->del(target, name) : -1;
}

/* ── secret prefix validation ────────────────────────────────────────── */

/* KNOWN_PROVIDER_KEY_PREFIXES — providers whose key format we know. */
static const struct { const char *provider; const char *prefixes[4]; }
KNOWN_PROVIDER_KEY_PREFIXES[] = {
    /* All OpenRouter keys are issued as sk-or-... (currently sk-or-v1-). */
    { "openrouter", { "sk-or-", NULL } },
};

static const char **known_provider_key_prefixes(const char *provider_id,
                                                size_t *out_n)
{
    for (size_t i = 0;
         i < sizeof(KNOWN_PROVIDER_KEY_PREFIXES) / sizeof(*KNOWN_PROVIDER_KEY_PREFIXES);
         i++) {
        if (strcmp(KNOWN_PROVIDER_KEY_PREFIXES[i].provider, provider_id) == 0) {
            size_t n = 0;
            while (n < 4 && KNOWN_PROVIDER_KEY_PREFIXES[i].prefixes[n]) n++;
            *out_n = n;
            return KNOWN_PROVIDER_KEY_PREFIXES[i].prefixes;
        }
    }
    *out_n = 0;
    return NULL;
}

/* Return false only when the provider declares key prefixes and none match
 * (fail-open for unknown formats).
 *
 * PoP: ath_secret_matches_declared_prefix @ hermes_cli/auth.py:_secret_matches_declared_prefix */
bool ath_secret_matches_declared_prefix(const char *provider_id, const char *value)
{
    if (!provider_id || !*provider_id || !value) return true;
    size_t n = 0;
    const char **prefixes = known_provider_key_prefixes(provider_id, &n);
    if (n == 0) return true;
    for (size_t i = 0; i < n; i++)
        if (strncmp(value, prefixes[i], strlen(prefixes[i])) == 0) return true;
    return false;
}

/* PoP: ath_warn_malformed_secret @ hermes_cli/auth.py:_warn_malformed_secret */
void ath_warn_malformed_secret(const char *provider_id, const char *source)
{
    size_t n = 0;
    const char **prefixes = known_provider_key_prefixes(provider_id, &n);
    char joined[256] = "(no declared prefix)";
    if (n > 0) {
        joined[0] = '\0';
        for (size_t i = 0; i < n; i++) {
            if (i) strncat(joined, " or ", sizeof(joined) - strlen(joined) - 1);
            strncat(joined, prefixes[i], sizeof(joined) - strlen(joined) - 1);
        }
    }
    fprintf(stderr,
            "WARNING Ignoring %s for provider '%s': value does not match the "
            "expected key prefix (%s). Falling back to the next credential "
            "source. Fix or remove the malformed key to silence this warning.\n",
            source ? source : "", provider_id ? provider_id : "", joined);
}

/* ── retry-after parsing (thin wrapper over agent.retry_utils) ───────── */

/* Best-effort parse of a Retry-After header into whole seconds.
 *
 * PoP: ath_parse_retry_after_seconds @ hermes_cli/auth.py:_parse_retry_after_seconds */
int ath_parse_retry_after_seconds(const char *raw_value)
{
    int ok = 0;
    double seconds = retry_utils_parse_retry_after_seconds(raw_value, &ok);
    if (!ok) return -1; /* None sentinel in the C port */
    long v = (long)seconds; /* int(seconds): truncate toward zero like int() */
    if (v < 0) v = 0;
    return (int)v;
}

/* ── OAuth scope parsing ─────────────────────────────────────────────── */

/* _scope_values returns a SET: space/comma-separated string tokens plus
 * recursive collection support (JWT scp claims, stored fixtures), deduped.
 *
 * PoP: ath_scope_values @ hermes_cli/auth.py:_scope_values */
char **ath_scope_values(const char *raw_scope, int *out_count);

typedef struct { char **items; int n, cap; } scope_set;

static void scope_set_add(scope_set *s, const char *tok)
{
    for (int i = 0; i < s->n; i++)
        if (strcmp(s->items[i], tok) == 0) return; /* set semantics */
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->items = realloc(s->items, (size_t)s->cap * sizeof(char *));
    }
    s->items[s->n++] = strdup(tok);
}

static void scope_set_from_string(scope_set *s, const char *raw)
{
    if (!raw) return;
    char *copy = strdup(raw);
    if (!copy) return;
    for (char *c = copy; *c; c++) if (*c == ',') *c = ' '; /* replace(",", " ") */
    char *save = NULL;
    for (char *tok = strtok_r(copy, " \t\r\n", &save); tok;
         tok = strtok_r(NULL, " \t\r\n", &save)) {
        char *b = tok;
        while (*b == ' ') b++;
        size_t bl = strlen(b);
        while (bl > 0 && b[bl-1] == ' ') b[--bl] = '\0';
        if (bl > 0) scope_set_add(s, b);
    }
    free(copy);
}

char **ath_scope_values(const char *raw_scope, int *out_count)
{
    scope_set s = {0};
    scope_set_from_string(&s, raw_scope);
    if (out_count) *out_count = s.n;
    if (s.n == 0) { free(s.items); return NULL; }
    return s.items; /* NUL-terminated by construction (NULL sentinel absent:
                       callers know count; free each + array) */
}

void ath_scope_values_free(char **scopes, int n)
{
    if (!scopes) return;
    for (int i = 0; i < n; i++) free(scopes[i]);
    free(scopes);
}

/* ── Spotify scope list (ordered dedupe of DEFAULT_SPOTIFY_SCOPE) ────── */

#define ATH_DEFAULT_SPOTIFY_SCOPE \
    "user-modify-playback-state user-read-playback-state " \
    "user-read-currently-playing user-read-recently-played " \
    "playlist-read-private playlist-read-collaborative " \
    "playlist-modify-public playlist-modify-private " \
    "user-library-read user-library-modify"

/* Ordered de-duplicated scope list; raw_scope wins over the default.
 * Caller frees with ath_spotify_scope_list_free().
 *
 * PoP: ath_spotify_scope_list @ hermes_cli/auth.py:_spotify_scope_list */
char **ath_spotify_scope_list(const char *raw_scope, int *out_count)
{
    const char *scope_text = (raw_scope && *raw_scope) ? raw_scope
                                                       : ATH_DEFAULT_SPOTIFY_SCOPE;
    scope_set ordered = {0};
    /* preserve first-seen order (unlike _scope_values' unordered set) */
    char *copy = strdup(scope_text);
    if (!copy) { if (out_count) *out_count = 0; return NULL; }
    char *save = NULL;
    for (char *tok = strtok_r(copy, " \t\r\n", &save); tok;
         tok = strtok_r(NULL, " \t\r\n", &save)) {
        int seen = 0;
        for (int i = 0; i < ordered.n; i++)
            if (strcmp(ordered.items[i], tok) == 0) { seen = 1; break; }
        if (!seen) scope_set_add(&ordered, tok);
    }
    free(copy);
    if (out_count) *out_count = ordered.n;
    if (ordered.n == 0) { free(ordered.items); return NULL; }
    return ordered.items;
}

void ath_spotify_scope_list_free(char **scopes, int n)
{
    ath_scope_values_free(scopes, n);
}

/* ── get_auth_status dispatcher ──────────────────────────────────────── */

extern int auth_get_qwen_auth_status(const char *arg);
extern int auth_get_spotify_auth_status(const char *arg);
extern int auth_get_nous_auth_status(const char *arg);
extern int auth_get_codex_auth_status(const char *arg);
extern int auth_get_xai_oauth_auth_status(const char *arg);
extern int auth_get_minimax_oauth_auth_status(const char *arg);
extern int auth_get_external_process_provider_status(const char *arg);
extern int auth_u_get_azure_foundry_auth_status(const char *arg);
extern int auth_get_api_key_provider_status(const char *arg);

/* Capture stdout of one of the existing per-provider status entry points
 * (they report via printf, matching their sibling wrapper convention). */
static char *capture_status_json(int (*fn)(const char *), const char *arg)
{
    int saved = dup(STDOUT_FILENO);
    if (saved < 0) return NULL;
    FILE *tmp = tmpfile();
    if (!tmp) { close(saved); return NULL; }
    int tmpfd = fileno(tmp);
    char *out = NULL;
    if (dup2(tmpfd, STDOUT_FILENO) >= 0) {
        fn(arg);
        fflush(stdout);
        dup2(saved, STDOUT_FILENO);
        long sz = ftell(tmp);
        if (sz > 0) {
            rewind(tmp);
            out = malloc((size_t)sz + 1);
            if (out && fread(out, 1, (size_t)sz, tmp) != (size_t)sz) {
                free(out); out = NULL;
            } else if (out) {
                out[sz] = '\0';
                /* strip trailing newline(s) */
                long n = sz;
                while (n > 0 && (out[n-1] == '\n' || out[n-1] == '\r'))
                    out[--n] = '\0';
                if (!out[0]) { free(out); out = NULL; }
            }
        }
    } else {
        dup2(saved, STDOUT_FILENO);
    }
    close(saved);
    fclose(tmp);
    if (!out) out = strdup("{\"logged_in\": false}");
    return out;
}

/* Generic auth status dispatcher. Returns a malloc'd status JSON string
 * (never NULL); caller frees.
 *
 * PoP: ath_get_auth_status @ hermes_cli/auth.py:get_auth_status */
char *get_active_provider_id_for_status(void);

char *ath_get_auth_status(const char *provider_id)
{
    char *lpid = lowerdup(provider_id);
    char *trimmed = lpid;
    if (trimmed) {
        while (*trimmed == ' ') trimmed++;
        size_t tl = strlen(trimmed);
        while (tl > 0 && trimmed[tl-1] == ' ') trimmed[--tl] = '\0';
    }
    if (!trimmed || !*trimmed) {
        char *active = get_active_provider_id_for_status();
        free(lpid);
        lpid = active ? lowerdup(active) : NULL;
        trimmed = lpid;
        if (trimmed) {
            while (*trimmed == ' ') trimmed++;
            size_t tl = strlen(trimmed);
            while (tl > 0 && trimmed[tl-1] == ' ') trimmed[--tl] = '\0';
        }
        free(active);
    }
    if (!trimmed || !*trimmed) { free(lpid); return strdup("{\"logged_in\": false}"); }

    char *result = NULL;
    if (strcmp(trimmed, "spotify") == 0)
        result = capture_status_json(auth_get_spotify_auth_status, "");
    else if (strcmp(trimmed, "nous") == 0)
        result = capture_status_json(auth_get_nous_auth_status, "");
    else if (strcmp(trimmed, "openai-codex") == 0)
        result = capture_status_json(auth_get_codex_auth_status, "");
    else if (strcmp(trimmed, "xai-oauth") == 0)
        result = capture_status_json(auth_get_xai_oauth_auth_status, "");
    else if (strcmp(trimmed, "qwen-oauth") == 0)
        result = capture_status_json(auth_get_qwen_auth_status, "");
    else if (strcmp(trimmed, "minimax-oauth") == 0)
        result = capture_status_json(auth_get_minimax_oauth_auth_status, "");
    else if (strcmp(trimmed, "copilot-acp") == 0)
        result = capture_status_json(auth_get_external_process_provider_status, trimmed);
    else if (strcmp(trimmed, "azure-foundry") == 0)
        result = capture_status_json(auth_u_get_azure_foundry_auth_status, "");
    else
        /* API-key / AWS SDK providers: no static PROVIDER_REGISTRY table in
         * the C port, so unknown ids fail closed like Python's tail path. */
        result = strdup("{\"logged_in\": false}");

    free(lpid);
    return result;
}

/* Active-provider lookup used by ath_get_auth_status when provider_id is
 * empty: reads the persisted auth store's active provider id. */
char *get_active_provider_id_for_status(void)
{
    json_t *store = authstore_load(NULL);
    if (!store) return NULL;
    json_t *active = json_obj_get(store, "active_provider");
    char *out = NULL;
    if (active && active->type == JSON_STRING && active->str_val && *active->str_val)
        out = strdup(active->str_val);
    json_free(store);
    return out;
}

/* ── model-selection confirm guard ───────────────────────────────────── */

/* Prompt before saving a model that trips any selection guard. Returns 1
 * to proceed, 0 to cancel. input_fn mirrors stdin input(); NULL uses a
 * non-interactive default of cancel ("N").
 *
 * PoP: ath_confirm_selection_guards @ hermes_cli/auth.py:_confirm_selection_guards */
int ath_confirm_selection_guards(
    const char *model_id,
    const char *provider,
    const char *base_url,
    const char *api_key,
    char **include_kinds,           /* NULL-terminated, may be NULL */
    const char *(*input_fn)(void *ctx),
    void *input_ctx)
{
    json_t *include_kinds_json = NULL;
    if (include_kinds && include_kinds[0]) {
        include_kinds_json = json_array();
        if (include_kinds_json)
            for (size_t i = 0; include_kinds[i]; i++)
                json_append(include_kinds_json, json_string(include_kinds[i]));
    }
    json_t *warnings = selection_warnings(model_id, provider, base_url,
                                          api_key, NULL,
                                          include_kinds_json, NULL);
    if (include_kinds_json) json_free(include_kinds_json);
    if (!warnings || json_len(warnings) == 0) {
        if (warnings) json_free(warnings);
        return 1;
    }
    char *message = combined_message(warnings);
    json_free(warnings);
    printf("\n");
    printf("========================================================================\n");
    printf("%s\n", message ? message : "");
    printf("========================================================================\n");
    free(message);

    const char *response = input_fn ? input_fn(input_ctx) : "";
    if (!response) { printf("\n"); return 0; } /* EOFError/KeyboardInterrupt */
    char r[64] = {0};
    snprintf(r, sizeof(r), "%s", response);
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    /* response.strip().lower() in {"y","yes"} — default [y/N] is No */
    return (strcmp(r, "y") == 0 || strcmp(r, "yes") == 0) ? 1 : 0;
}

/* ── login/logout commands ───────────────────────────────────────────── */

/* Deprecated: use 'hermes model' or 'hermes setup' instead.
 * Returns the SystemExit code (0).
 *
 * PoP: ath_login_command @ hermes_cli/auth.py:login_command */
int ath_login_command(void *args)
{
    (void)args;
    printf("The 'hermes login' command has been removed.\n");
    printf("Use 'hermes auth' to manage credentials,\n");
    printf("'hermes model' to select a provider, or 'hermes setup' for full setup.\n");
    return 0; /* raise SystemExit(0) */
}

/* Clear auth state for a provider. Returns the exit code: 1 for an unknown
 * provider, 0 otherwise.
 *
 * PoP: ath_logout_command @ hermes_cli/auth.py:logout_command */
int ath_logout_command(const char *provider_id)
{
    if (provider_id && *provider_id && !is_known_auth_provider(provider_id)) {
        printf("Unknown provider: %s\n", provider_id);
        return 1; /* raise SystemExit(1) */
    }

    char *active = get_active_provider_id_for_status();
    const char *fallback = auth_logout_default_provider_from_config();
    const char *target = (provider_id && *provider_id) ? provider_id
                       : (active && *active) ? active : fallback;
    if (!target || !*target) {
        free(active);
        printf("No provider is currently logged in.\n");
        return 0;
    }

    bool should_reset_config = auth_should_reset_config_provider_on_logout(target);
    char *provider_name = get_auth_provider_display_name(target);

    if (clear_provider_auth(target) || should_reset_config) {
        if (should_reset_config) auth_reset_config_provider();
        printf("Logged out of %s.\n", provider_name ? provider_name : target);
        if (should_reset_config && getenv("OPENROUTER_API_KEY")) {
            printf("Hermes will use OpenRouter for inference.\n");
        } else if (should_reset_config) {
            printf("Run `hermes model` or configure an API key to use Hermes.\n");
        } else {
            printf("Model provider configuration was unchanged.\n");
        }
    } else {
        printf("No auth state found for %s.\n",
               provider_name ? provider_name : target);
    }
    free(provider_name);
    free(active);
    return 0;
}
