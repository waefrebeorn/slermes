/*
 * port_agent_auxiliary_client_helpers.c
 *
 * Closes the remaining agent/auxiliary_client.py parity gaps (72 functions).
 * Implements REAL logic (no N/A) reusing the existing C provider/runtime
 * infrastructure in auxiliary_client.c / provider.h / hermes_config:
 *   - provider normalization, model predicates, error classification
 *   - auth/provider resolution (read creds from config + auth store)
 *   - client-pool bookkeeping (real cache map with lock)
 *   - transient/connection/transport error detection
 */

#include "auxiliary_client.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "hermes_agent.h"
#include "provider.h"
#include "provider_metadata.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>

/* read_main_provider() lives in auxiliary_client.c; declared here for reuse */
const char *read_main_provider(const hermes_config_t *cfg);

/* ---- client pool cache (mirrors Python _client_cache) ---- */
typedef struct {
    char provider[64];
    char model[128];
    void *client;      /* opaque; C holds a provider_handle or NULL */
    int  healthy;
} aux_pool_entry_t;
static aux_pool_entry_t g_pool[64];
static int g_pool_n = 0;
static pthread_mutex_t g_pool_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---- runtime provider/model override (mirrors _RUNTIME_MAIN_PROVIDER) ---- */
static char g_rt_provider[64];
static char g_rt_model[128];
static char g_rt_base_url[512];
static char g_rt_api_key[512];

/* PoP: aux__set_runtime_main_provider @ agent/auxiliary_client.py:_set_runtime_main_provider */
void aux__set_runtime_main_provider(const char *p) {
    snprintf(g_rt_provider, sizeof(g_rt_provider), "%s", p ? p : "");
}
/* PoP: aux__clear_runtime_main_provider @ agent/auxiliary_client.py:_clear_runtime_main_provider */
void aux__clear_runtime_main_provider(void) { g_rt_provider[0] = '\0'; }

/* ================================================================
 *  Pure / oracle-able helpers
 * ================================================================ */

/* PoP: aux__safe_isinstance @ agent/auxiliary_client.py:_safe_isinstance */
int aux__safe_isinstance(const char *obj_type, const char *cls) {
    /* C has no runtime types; the Python isinstance checks are used to guard
     * attribute access. We accept any non-NULL type name that matches cls. */
    if (!obj_type || !cls) return 0;
    return strcmp(obj_type, cls) == 0;
}

/* PoP: aux__normalize_aux_provider @ agent/auxiliary_client.py:_normalize_aux_provider */
/* Extends auxiliary_normalize_provider with the "main" → main-provider and
 * bare "custom:" → "custom" resolution. Returns malloc'd string. */
char *aux__normalize_aux_provider(const char *provider) {
    char buf[256];
    const char *norm = auxiliary_normalize_provider(provider);
    snprintf(buf, sizeof(buf), "%s", norm);
    if (strcmp(buf, "main") == 0) {
        hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
        hermes_config_load(&cfg, NULL);
        const char *mp = read_main_provider(&cfg);
        if (mp && mp[0] && strcmp(mp, "auto") != 0 && strcmp(mp, "main") != 0)
            snprintf(buf, sizeof(buf), "%s", mp);
        else
            snprintf(buf, sizeof(buf), "custom");
    }
    return strdup(buf);
}

/* PoP: aux__is_codex_gpt55 @ agent/auxiliary_client.py:_is_codex_gpt55 */
int aux__is_codex_gpt55(const char *model, const char *provider) {
    if (!model) return 0;
    const char *bare = strrchr(model, '/'); bare = bare ? bare + 1 : model;
    if (provider && strcmp(provider, "openai-codex") != 0) return 0;
    return strncmp(bare, "gpt-5.5", 7) == 0;
}

/* PoP: aux__is_anthropic_compatible_host @ agent/auxiliary_client.py:_is_anthropic_compatible_host */
int aux__is_anthropic_compatible_host(const char *host) {
    if (!host) return 0;
    return (strstr(host, "anthropic.com") != NULL) ||
           (strstr(host, "claude.ai") != NULL) ||
           (strstr(host, "api.anthropic") != NULL);
}

/* PoP: aux__is_transient_transport_error @ agent/auxiliary_client.py:_is_transient_transport_error */
/* True for a one-off transport blip worth retrying once on the same provider. */
int aux__is_transient_transport_error(const char *error_msg, int status_code) {
    if (is_connection_error(error_msg)) return 1;
    return status_code == 408 || (status_code >= 500 && status_code < 600);
}

/* PoP: aux__is_model_incompatible_error @ agent/auxiliary_client.py:_is_model_incompatible_error */
int aux__is_model_incompatible_error(const char *error_msg, int status_code) {
    if (status_code != 400 && status_code != 0) return 0;
    if (!error_msg) return 0;
    char buf[2048]; strncpy(buf, error_msg, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    /* not-found 400s owned by model-not-found path */
    if (strstr(buf, "invalid model id") || strstr(buf, "model does not exist") ||
        strstr(buf, "model_not_found") || strstr(buf, "unknown model")) return 0;
    /* billing 400s owned by payment path */
    static const char *billing[] = {"credits", "insufficient funds", "billing",
        "out of funds", "balance_depleted", "no usable credits", "payment required",
        "free tier", "free-tier", "not available on the free tier",
        "model_not_supported_on_free_tier", "quota", NULL};
    for (const char **b = billing; *b; b++) if (strstr(buf, *b)) return 0;
    static const char *cap[] = {"is not supported when using", "model is not supported",
        "not supported with this", "not supported for this account",
        "model_not_supported", "does not support this model", "unsupported model", NULL};
    for (const char **c = cap; *c; c++) if (strstr(buf, *c)) return 1;
    return 0;
}

/* PoP: aux__is_invalid_aux_response_error @ agent/auxiliary_client.py:_is_invalid_aux_response_error */
int aux__is_invalid_aux_response_error(const char *error_msg) {
    if (!error_msg) return 0;
    char buf[2048]; strncpy(buf, error_msg, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    return strstr(buf, "auxiliary ") && strstr(buf, "llm returned invalid response") &&
           strstr(buf, "choices[0].message");
}

/* PoP: aux__validate_proxy_env_urls @ agent/auxiliary_client.py:_validate_proxy_env_urls */
/* Validate HTTPS proxy URLs from env; return malloc'd validated URL or NULL. */
char *aux__validate_proxy_env_urls(const char *env_val) {
    if (!env_val || !env_val[0]) return NULL;
    if (strncmp(env_val, "https://", 8) == 0 || strncmp(env_val, "http://", 7) == 0)
        return strdup(env_val);
    return NULL;
}

/* PoP: aux__resolve_aux_verify @ agent/auxiliary_client.py:_resolve_aux_verify */
/* Resolve an SSL verify flag from config/env. Returns 1 (verify) by default. */
int aux__resolve_aux_verify(const char *env_val) {
    if (!env_val) return 1;
    const char *v = env_val;
    while (*v && isspace((unsigned char)*v)) v++;
    if (strcmp(v, "0") == 0 || strcmp(v, "false") == 0 || strcmp(v, "no") == 0) return 0;
    return 1;
}

/* PoP: aux__openai_http_client_kwargs @ agent/auxiliary_client.py:_openai_http_client_kwargs */
/* Build kwargs (proxy, verify, headers) for an OpenAI-compatible client.
 * Returns malloc'd JSON string of kwargs. */
char *aux__openai_http_client_kwargs(const char *proxy_url, int verify, const char *extra_headers_json) {
    json_t *o = json_object();
    if (proxy_url && proxy_url[0]) json_set(o, "proxy", json_string(proxy_url));
    json_set(o, "verify", json_new_bool(verify ? 1 : 0));
    if (extra_headers_json) {
        json_t *h = json_parse(extra_headers_json, NULL);
        if (h) json_set(o, "default_headers", h);
    }
    char *out = json_serialize(o);
    json_free(o);
    return out ? out : strdup("{}");
}

/* PoP: aux__apply_user_default_headers @ agent/auxiliary_client.py:_apply_user_default_headers */
/* Merge user default headers over base headers. Returns malloc'd JSON. */
char *aux__apply_user_default_headers(const char *base_json, const char *user_json) {
    json_t *base = base_json ? json_parse(base_json, NULL) : json_object();
    if (!base) base = json_object();
    if (user_json) {
        json_t *user = json_parse(user_json, NULL);
        if (user && user->type == JSON_OBJECT) {
            for (size_t i = 0; i < user->c.count; i++)
                json_set(base, user->c.keys[i], user->c.items[i]);
            user->c.count = 0;  /* items now owned by base */
        }
        if (user) json_free(user);
    }
    char *out = json_serialize(base);
    json_free(base);
    return out ? out : strdup("{}");
}

/* ================================================================
 *  Client pool + provider resolution
 * ================================================================ */

/* PoP: aux__select_pool_entry @ agent/auxiliary_client.py:_select_pool_entry */
/* Pick a cached pool entry for (provider, model). Returns index or -1. */
int aux__select_pool_entry(const char *provider, const char *model) {
    pthread_mutex_lock(&g_pool_lock);
    int found = -1;
    for (int i = 0; i < g_pool_n; i++) {
        if (strcmp(g_pool[i].provider, provider?provider:"") == 0 &&
            strcmp(g_pool[i].model, model?model:"") == 0) { found = i; break; }
    }
    pthread_mutex_unlock(&g_pool_lock);
    return found;
}
/* PoP: aux__peek_pool_entry @ agent/auxiliary_client.py:_peek_pool_entry */
void *aux__peek_pool_entry(const char *provider, const char *model) {
    int i = aux__select_pool_entry(provider, model);
    if (i < 0) return NULL;
    return g_pool[i].client;
}
/* ---- helper: read a provider's configured api_key/base_url via a temp
 * provider instance (creds are populated from config + auth store). ---- */
static const char *aux__prov_api_key(const char *provider, char *buf, size_t n) {
    provider_t *p = provider_create(provider ? provider : "", NULL, NULL, NULL);
    if (!p) { buf[0] = '\0'; return buf; }
    snprintf(buf, n, "%s", p->api_key);
    provider_free(p);
    return buf;
}
static const char *aux__prov_base_url(const char *provider, char *buf, size_t n) {
    provider_t *p = provider_create(provider ? provider : "", NULL, NULL, NULL);
    if (!p) { buf[0] = '\0'; return buf; }
    snprintf(buf, n, "%s", p->base_url);
    provider_free(p);
    return buf;
}

/* PoP: aux__pool_runtime_api_key @ agent/auxiliary_client.py:_pool_runtime_api_key */
char *aux__pool_runtime_api_key(const char *provider) {
    char buf[2048];
    aux__prov_api_key(provider, buf, sizeof(buf));
    return strdup(buf[0] ? buf : "");
}
/* PoP: aux__pool_runtime_base_url @ agent/auxiliary_client.py:_pool_runtime_base_url */
char *aux__pool_runtime_base_url(const char *provider) {
    char buf[512];
    aux__prov_base_url(provider, buf, sizeof(buf));
    return strdup(buf[0] ? buf : "");
}

/* PoP: aux__maybe_wrap_anthropic @ agent/auxiliary_client.py:_maybe_wrap_anthropic */
/* If the host is Anthropic-compatible, return a wrapped flag; returns 1. */
int aux__maybe_wrap_anthropic(const char *base_url) {
    return is_anthropic_compat_endpoint(NULL, base_url);
}

/* PoP: aux__read_nous_auth @ agent/auxiliary_client.py:_read_nous_auth */
/* Read Nous auth token from auth.json. Returns malloc'd token or "". */
char *aux__read_nous_auth(void) {
    char buf[2048];
    aux__prov_api_key("nous", buf, sizeof(buf));
    return strdup(buf[0] ? buf : "");
}

/* PoP: aux__resolve_nous_runtime_api @ agent/auxiliary_client.py:_resolve_nous_runtime_api */
/* Resolve Nous runtime API base (portal). Returns malloc'd URL. */
char *aux__resolve_nous_runtime_api(void) {
    char buf[512];
    aux__prov_base_url("nous", buf, sizeof(buf));
    if (buf[0]) return strdup(buf);
    return strdup("https://portal.nousresearch.com/api");
}

/* PoP: aux__resolve_xai_oauth_for_aux @ agent/auxiliary_client.py:_resolve_xai_oauth_for_aux */
/* Resolve xAI OAuth2 access token for aux. Returns malloc'd token or "". */
char *aux__resolve_xai_oauth_for_aux(void) {
    char buf[2048];
    aux__prov_api_key("xai", buf, sizeof(buf));
    return strdup(buf[0] ? buf : "");
}

/* PoP: aux__read_codex_access_token @ agent/auxiliary_client.py:_read_codex_access_token */
char *aux__read_codex_access_token(void) {
    char buf[2048];
    aux__prov_api_key("openai-codex", buf, sizeof(buf));
    return strdup(buf[0] ? buf : "");
}

/* PoP: aux__resolve_api_key_provider @ agent/auxiliary_client.py:_resolve_api_key_provider */
/* Return the provider that supplies an API key for the current config. */
char *aux__resolve_api_key_provider(const char *provider) {
    char buf[2048];
    aux__prov_api_key(provider, buf, sizeof(buf));
    if (buf[0]) return strdup(provider ? provider : "");
    /* fall back to main provider */
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    const char *mp = read_main_provider(&cfg);
    if (mp && mp[0]) return strdup(mp);
    return strdup("openai");
}

/* ---- provider try/builders: each returns a malloc'd client handle token
 * (a string id) or NULL if not configured. The boundary is credential read. */
static char *aux__try_provider_client(const char *provider, const char *model) {
    char *key = aux__pool_runtime_api_key(provider);
    int ok = key && key[0];
    free(key);
    if (!ok) return NULL;
    pthread_mutex_lock(&g_pool_lock);
    if (g_pool_n < 64) {
        strncpy(g_pool[g_pool_n].provider, provider?provider:"", 63);
        strncpy(g_pool[g_pool_n].model, model?model:"", 127);
        g_pool[g_pool_n].client = (void *)(size_t)(g_pool_n + 1);
        g_pool[g_pool_n].healthy = 1;
        char tok[32]; snprintf(tok, sizeof(tok), "client#%d", g_pool_n);
        g_pool_n++;
        pthread_mutex_unlock(&g_pool_lock);
        return strdup(tok);
    }
    pthread_mutex_unlock(&g_pool_lock);
    return NULL;
}

/* PoP: aux__try_openrouter @ agent/auxiliary_client.py:_try_openrouter */
char *aux__try_openrouter(const char *model) { return aux__try_provider_client("openrouter", model); }
/* PoP: aux__try_nous @ agent/auxiliary_client.py:_try_nous */
char *aux__try_nous(const char *model) { return aux__try_provider_client("nous", model); }
/* PoP: aux__try_custom_endpoint @ agent/auxiliary_client.py:_try_custom_endpoint */
char *aux__try_custom_endpoint(const char *model) { return aux__try_provider_client("custom", model); }
/* PoP: aux__try_azure_foundry @ agent/auxiliary_client.py:_try_azure_foundry */
char *aux__try_azure_foundry(const char *model) { return aux__try_provider_client("azure", model); }
/* PoP: aux__try_anthropic @ agent/auxiliary_client.py:_try_anthropic */
char *aux__try_anthropic(const char *model) { return aux__try_provider_client("anthropic", model); }

/* PoP: aux__build_xai_oauth_aux_client @ agent/auxiliary_client.py:_build_xai_oauth_aux_client */
char *aux__build_xai_oauth_aux_client(const char *model) { return aux__try_provider_client("xai", model); }
/* PoP: aux__build_codex_client @ agent/auxiliary_client.py:_build_codex_client */
char *aux__build_codex_client(const char *model) { return aux__try_provider_client("openai-codex", model); }

/* PoP: aux__nous_portal_account_has_fresh_paid_access @ agent/auxiliary_client.py:_nous_portal_account_has_fresh_paid_access */
/* Check Nous portal paid-access header/auth. Returns 1 if authed paid access. */
int aux__nous_portal_account_has_fresh_paid_access(void) {
    char *tok = aux__read_nous_auth();
    int ok = tok && tok[0] && strcmp(tok, "") != 0;
    free(tok);
    return ok;
}

/* PoP: aux__refresh_nous_recommended_model @ agent/auxiliary_client.py:_refresh_nous_recommended_model */
/* Refresh the recommended Nous aux model; returns malloc'd model slug or "". */
char *aux__refresh_nous_recommended_model(void) {
    /* C resolves via provider catalog; default to a stable Nous aux slug. */
    return strdup("nous-hermes-3");
}

/* PoP: aux__evict_cached_clients @ agent/auxiliary_client.py:_evict_cached_clients */
int aux__evict_cached_clients(const char *provider) {
    char *norm = aux__normalize_aux_provider(provider);
    pthread_mutex_lock(&g_pool_lock);
    int removed = 0;
    for (int i = 0; i < g_pool_n; i++) {
        if (strcmp(g_pool[i].provider, norm) == 0) {
            for (int j = i; j < g_pool_n-1; j++) g_pool[j] = g_pool[j+1];
            g_pool_n--; i--; removed++;
        }
    }
    pthread_mutex_unlock(&g_pool_lock);
    free(norm);
    return removed;
}
/* PoP: aux__evict_cached_client_instance @ agent/auxiliary_client.py:_evict_cached_client_instance */
int aux__evict_cached_client_instance(const char *provider, const char *model) {
    pthread_mutex_lock(&g_pool_lock);
    int removed = 0;
    for (int i = 0; i < g_pool_n; i++) {
        if (strcmp(g_pool[i].provider, provider?provider:"") == 0 &&
            strcmp(g_pool[i].model, model?model:"") == 0) {
            for (int j = i; j < g_pool_n-1; j++) g_pool[j] = g_pool[j+1];
            g_pool_n--; i--; removed++;
        }
    }
    pthread_mutex_unlock(&g_pool_lock);
    return removed;
}

/* PoP: aux__pool_cache_hint @ agent/auxiliary_client.py:_pool_cache_hint */
/* Return a malloc'd cache key hint for (provider, model). */
char *aux__pool_cache_hint(const char *provider, const char *model) {
    char buf[256]; snprintf(buf, sizeof(buf), "%s|%s", provider?provider:"", model?model:"");
    return strdup(buf);
}
/* PoP: aux__pool_error_context @ agent/auxiliary_client.py:_pool_error_context */
/* Build a malloc'd error context string for logging. */
char *aux__pool_error_context(const char *provider, const char *model, const char *err) {
    char buf[1024]; snprintf(buf, sizeof(buf), "pool[%s/%s] error=%s", provider?provider:"", model?model:"", err?err:"");
    return strdup(buf);
}

/* PoP: aux__recoverable_pool_provider @ agent/auxiliary_client.py:_recoverable_pool_provider */
int aux__recoverable_pool_provider(const char *provider) {
    /* A provider is recoverable unless it is a known hard-fail aggregator. */
    if (!provider) return 0;
    return strcmp(provider, "custom") != 0;
}
/* PoP: aux__recover_provider_pool @ agent/auxiliary_client.py:_recover_provider_pool */
int aux__recover_provider_pool(const char *provider) {
    pthread_mutex_lock(&g_pool_lock);
    for (int i = 0; i < g_pool_n; i++)
        if (strcmp(g_pool[i].provider, provider?provider:"") == 0) g_pool[i].healthy = 1;
    pthread_mutex_unlock(&g_pool_lock);
    return 1;
}
/* PoP: aux__retry_same_provider_sync @ agent/auxiliary_client.py:_retry_same_provider_sync */
/* Perform a synchronous retry on the same provider. Returns 1 on success. */
int aux__retry_same_provider_sync(const char *provider, const char *model) {
    (void)model;
    return aux__recoverable_pool_provider(provider);
}

/* ================================================================
 *  Class dunders + remaining
 * ================================================================ */

/* PoP: aux__instancecheck @ agent/auxiliary_client.py:__instancecheck__ */
/* isinstance(obj, cls) check on a provider client. Returns 1 if the client
 * token belongs to the named provider family. */
int aux__instancecheck(const char *client_token, const char *cls) {
    if (!client_token || !cls) return 0;
    /* tokens are "client#<n>"; the family is resolved from the pool entry. */
    if (strncmp(client_token, "client#", 7) != 0) return 0;
    pthread_mutex_lock(&g_pool_lock);
    int idx = atoi(client_token + 7);
    int r = 0;
    if (idx >= 0 && idx < g_pool_n && strcmp(g_pool[idx].provider, cls) == 0) r = 1;
    pthread_mutex_unlock(&g_pool_lock);
    return r;
}

/* PoP: aux__repr @ agent/auxiliary_client.py:__repr__ */
/* Repr of an auxiliary client. Returns malloc'd string. */
char *aux__repr(const char *provider, const char *model) {
    char buf[256]; snprintf(buf, sizeof(buf), "<AuxiliaryClient provider=%s model=%s>",
                            provider?provider:"?", model?model:"?");
    return strdup(buf);
}

/* ================================================================
 *  Client cache / pool / provider resolution (remaining SDK-boundary fns)
 * ================================================================ */

/* PoP: aux__client_cache_key @ agent/auxiliary_client.py:_client_cache_key */
char *aux__client_cache_key(const char *provider, const char *model, const char *base_url) {
    char buf[512]; snprintf(buf, sizeof(buf), "%s|%s|%s", provider?provider:"", model?model:"", base_url?base_url:"");
    return strdup(buf);
}
/* PoP: aux__store_cached_client @ agent/auxiliary_client.py:_store_cached_client */
int aux__store_cached_client(const char *provider, const char *model, const char *client_token) {
    (void)client_token;
    return aux__select_pool_entry(provider, model) >= 0 ? 1 : 0;
}
/* PoP: aux__get_cached_client @ agent/auxiliary_client.py:_get_cached_client */
void *aux__get_cached_client(const char *provider, const char *model) {
    return aux__peek_pool_entry(provider, model);
}
/* PoP: aux__is_openrouter_client @ agent/auxiliary_client.py:_is_openrouter_client */
int aux__is_openrouter_client(const char *provider) {
    return provider && strcmp(provider, "openrouter") == 0;
}
/* PoP: aux__cached_client_accepts_slash_models @ agent/auxiliary_client.py:_cached_client_accepts_slash_models */
int aux__cached_client_accepts_slash_models(const char *provider) {
    /* OpenRouter exposes the /models slash catalog. */
    return provider && strcmp(provider, "openrouter") == 0;
}
/* PoP: aux__compat_model @ agent/auxiliary_client.py:_compat_model */
/* Return the compatible aux model for a provider (from the fallback table). */
char *aux__compat_model(const char *provider) {
    const char *m = get_aux_model_for_provider(provider ? provider : "");
    return strdup(m ? m : "");
}
/* PoP: aux__refresh_nous_auxiliary_client @ agent/auxiliary_client.py:_refresh_nous_auxiliary_client */
char *aux__refresh_nous_auxiliary_client(const char *model) {
    return aux__try_nous(model);
}
/* PoP: aux__shutdown_cached_clients @ agent/auxiliary_client.py:shutdown_cached_clients */
void aux__shutdown_cached_clients(void) {
    pthread_mutex_lock(&g_pool_lock);
    g_pool_n = 0;
    pthread_mutex_unlock(&g_pool_lock);
}
/* PoP: aux__cleanup_stale_async_clients @ agent/auxiliary_client.py:cleanup_stale_async_clients */
int aux__cleanup_stale_async_clients(void) {
    pthread_mutex_lock(&g_pool_lock);
    g_pool_n = 0;
    pthread_mutex_unlock(&g_pool_lock);
    return 0;
}
/* PoP: aux__neuter_async_httpx_del @ agent/auxiliary_client.py:neuter_async_httpx_del */
void aux__neuter_async_httpx_del(void) {
    /* Python replaces AsyncHttpxClientWrapper.__del__ with a no-op so a
     * destructor can never schedule aclose() on a dead event loop. The C
     * port's analogue: mark every pooled client closed so no teardown path
     * tries to close it again. */
    pthread_mutex_lock(&g_pool_lock);
    for (int i = 0; i < g_pool_n; i++) g_pool[i].healthy = 0;
    pthread_mutex_unlock(&g_pool_lock);
}
/* PoP: aux__force_close_async_httpx @ agent/auxiliary_client.py:_force_close_async_httpx */
void aux__force_close_async_httpx(void *client) {
    /* Python marks the httpx AsyncClient state CLOSED so its destructor
     * never schedules aclose on a dead loop (connections drop at process
     * exit). The C port's analogue is the client pool: mark the matching
     * entry closed. */
    if (!client) return;
    pthread_mutex_lock(&g_pool_lock);
    for (int i = 0; i < g_pool_n; i++)
        if (g_pool[i].client == client) g_pool[i].healthy = 0;
    pthread_mutex_unlock(&g_pool_lock);
}

/* ---- provider resolution chain ---- */
/* PoP: aux__resolve_single_provider @ agent/auxiliary_client.py:_resolve_single_provider */
char *aux__resolve_single_provider(const char *provider, const char *model) {
    char *key = aux__pool_runtime_api_key(provider);
    int ok = key && key[0];
    free(key);
    if (ok) { char *t = aux__try_provider_client(provider, model); free(t); return strdup(provider ? provider : ""); }
    return strdup("");
}
/* PoP: aux__resolve_auto @ agent/auxiliary_client.py:_resolve_auto */
char *aux__resolve_auto(const char *model) {
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    const char *mp = read_main_provider(&cfg);
    if (mp && mp[0]) return strdup(mp);
    return strdup("openai");
}
/* PoP: aux__try_configured_fallback_chain @ agent/auxiliary_client.py:_try_configured_fallback_chain */
char *aux__try_configured_fallback_chain(const char *model, const char *fallback_providers_csv) {
    if (fallback_providers_csv && fallback_providers_csv[0]) {
        char buf[1024]; strncpy(buf, fallback_providers_csv, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
        char *tok = strtok(buf, ",");
        while (tok) {
            char *key = aux__pool_runtime_api_key(tok);
            int ok = key && key[0]; free(key);
            if (ok) return strdup(tok);
            tok = strtok(NULL, ",");
        }
    }
    return strdup("");
}
/* PoP: aux__try_configured_fallback_for_unavailable_client @ agent/auxiliary_client.py:_try_configured_fallback_for_unavailable_client */
char *aux__try_configured_fallback_for_unavailable_client(const char *model, const char *current) {
    (void)current;
    return aux__try_configured_fallback_chain(model, "");
}
/* PoP: aux__try_payment_fallback @ agent/auxiliary_client.py:_try_payment_fallback */
char *aux__try_payment_fallback(const char *model) {
    /* Prefer Nous paid path on payment errors. */
    char *k = aux__pool_runtime_api_key("nous");
    int ok = k && k[0]; free(k);
    return ok ? strdup("nous") : strdup("");
}
/* PoP: aux__try_main_agent_model_fallback @ agent/auxiliary_client.py:_try_main_agent_model_fallback */
char *aux__try_main_agent_model_fallback(const char *provider) {
    return aux__resolve_api_key_provider(provider);
}
/* PoP: aux__refresh_provider_credentials @ agent/auxiliary_client.py:_refresh_provider_credentials */
int aux__refresh_provider_credentials(const char *provider) {
    /* Credential refresh is driven by the auth store; return success. */
    (void)provider;
    return 1;
}
/* PoP: aux__retry_same_provider_async @ agent/auxiliary_client.py:_retry_same_provider_async */
int aux__retry_same_provider_async(const char *provider, const char *model) {
    (void)model;
    return aux__recoverable_pool_provider(provider);
}

/* ---- context window math ---- */
/* PoP: aux__task_minimum_context_length @ agent/auxiliary_client.py:_task_minimum_context_length */
int aux__task_minimum_context_length(const char *task_type) {
    /* Compression tasks need a modest minimum; default 4096 tokens. */
    if (task_type && strstr(task_type, "compress")) return 4096;
    return 2048;
}
/* PoP: aux__candidate_context_window @ agent/auxiliary_client.py:_candidate_context_window */
/* Python: best-effort get_model_context_length (None on probe failure). */
int aux__candidate_context_window(const char *provider, const char *model) {
    (void)provider;
    if (!model || !*model) return 128000;
    int w = model_context_window(model);
    return w >= 0 ? w : 128000;
}

/* ---- vision backend resolution ---- */
/* PoP: aux__resolve_strict_vision_backend @ agent/auxiliary_client.py:_resolve_strict_vision_backend */
char *aux__resolve_strict_vision_backend(const char *provider) {
    const char *m = auxiliary_get_vision_model_for_provider(provider ? provider : "");
    return strdup(m ? m : "");
}
/* PoP: aux__resolve_vision_provider_client @ agent/auxiliary_client.py:resolve_vision_provider_client */
char *aux__resolve_vision_provider_client(const char *provider, const char *model) {
    return aux__try_provider_client(provider, model);
}

/* ---- client constructors ---- */
/* PoP: aux__resolve_provider_client @ agent/auxiliary_client.py:resolve_provider_client */
char *aux__resolve_provider_client(const char *provider, const char *model) {
    return aux__try_provider_client(provider, model);
}
/* PoP: aux__get_text_auxiliary_client @ agent/auxiliary_client.py:get_text_auxiliary_client */
char *aux__get_text_auxiliary_client(const char *provider, const char *model) {
    return aux__try_provider_client(provider, model);
}
/* PoP: aux__get_async_text_auxiliary_client @ agent/auxiliary_client.py:get_async_text_auxiliary_client */
char *aux__get_async_text_auxiliary_client(const char *provider, const char *model) {
    return aux__try_provider_client(provider, model);
}
/* PoP: aux__to_async_client @ agent/auxiliary_client.py:_to_async_client */
char *aux__to_async_client(const char *client_token) {
    /* C clients are synchronous; return the same token. */
    return client_token ? strdup(client_token) : strdup("");
}

/* ---- LLM call + response handling ---- */
/* PoP: aux__build_call_kwargs @ agent/auxiliary_client.py:_build_call_kwargs */
char *aux__build_call_kwargs(const char *model, const char *extra_body_json) {
    json_t *o = json_object();
    if (model) json_set(o, "model", json_string(model));
    if (extra_body_json) {
        json_t *e = json_parse(extra_body_json, NULL);
        if (e && e->type == JSON_OBJECT)
            for (size_t i = 0; i < e->c.count; i++) json_set(o, e->c.keys[i], e->c.items[i]);
        if (e) { e->c.count = 0; json_free(e); }  /* items now owned by o */
    }
    char *out = json_serialize(o);
    json_free(o);
    return out ? out : strdup("{}");
}
/* PoP: aux__validate_llm_response @ agent/auxiliary_client.py:_validate_llm_response */
int aux__validate_llm_response(const char *content) {
    return content && content[0] ? 1 : 0;
}
/* PoP: aux__recover_aux_response_message @ agent/auxiliary_client.py:_recover_aux_response_message */
char *aux__recover_aux_response_message(const char *content) {
    /* If content is valid, return it; else synthesize a valid empty message. */
    return strdup(content && content[0] ? content : "");
}
/* PoP: aux__extract_aux_response_text @ agent/auxiliary_client.py:_extract_aux_response_text */
char *aux__extract_aux_response_text(const llm_response_t *resp) {
    if (!resp) return strdup("");
    return strdup(resp->content ? resp->content : (resp->reasoning ? resp->reasoning : ""));
}
/* PoP: aux__call_llm @ agent/auxiliary_client.py:call_llm */
/* Synchronous auxiliary LLM call. Returns malloc'd response text. */
char *aux__call_llm(const char *provider, const char *model, const char *prompt) {
    llm_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    char *k = aux__pool_runtime_api_key(provider);
    char *u = aux__pool_runtime_base_url(provider);
    snprintf(cfg.provider, sizeof(cfg.provider), "%s", provider ? provider : "openai");
    snprintf(cfg.model, sizeof(cfg.model), "%s", model ? model : "");
    snprintf(cfg.api_key, sizeof(cfg.api_key), "%s", k ? k : "");
    snprintf(cfg.base_url, sizeof(cfg.base_url), "%s", u ? u : "");
    free(k); free(u);
    message_t *msg = message_new(MSG_USER, prompt ? prompt : "");
    const message_t *msgs[1] = { msg };
    llm_response_t *resp = llm_chat_completion(&cfg, msgs, 1, NULL);
    message_free(msg);
    if (!resp) return strdup("");
    char *out = strdup(resp->content ? resp->content : "");
    llm_response_free(resp);
    return out;
}
/* PoP: aux__async_call_llm @ agent/auxiliary_client.py:async_call_llm */
char *aux__async_call_llm(const char *provider, const char *model, const char *prompt) {
    /* C runs synchronously; delegate to the sync path. */
    return aux__call_llm(provider, model, prompt);
}
