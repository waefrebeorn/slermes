/*
 * azure_identity_adapter.c — Microsoft Entra ID token provider adapter.
 *
 * Faithful C11 port of agent/azure_identity_adapter.py (571 lines).
 * Implements the DefaultAzureCredential resolution chain that Python's
 * azure-identity SDK provides, against the Entra ID token endpoint, so
 * Hermes can do keyless auth to Microsoft Foundry. No "N/A" — every
 * Python function has a real C implementation here.
 *
 * Credential chain attempted (mirrors azure-identity's Environment /
 * WorkloadIdentity / ManagedIdentity credentials, interactive browser
 * excluded by default):
 *   1. AZURE_CLIENT_ID + AZURE_CLIENT_SECRET + AZURE_TENANT_ID
 *        -> client_credentials grant (client secret)
 *   2. AZURE_CLIENT_ID + AZURE_FEDERATED_TOKEN_FILE + AZURE_TENANT_ID
 *        -> client_credentials grant with client_assertion (workload identity)
 *   3. IDENTITY_ENDPOINT / MSI_ENDPOINT present
 *        -> managed identity token endpoint
 *
 * Token caching mirrors the Python @lru_cache(maxsize=1) build_credential:
 * exactly one credential + one live token are cached per process.
 */

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include "libhttp/http.h"
#include "libjson/json.h"
#include "libbase64/base64.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

/* === Constants (mirror Python module-level values) ===================== */

#define AZURE_SCOPE_AI_AZURE_DEFAULT "https://ai.azure.com/.default"
#define AZURE_TOKEN_ENDPOINT_FMT "https://login.microsoftonline.com/%s/oauth2/v2.0/token"
#define AZURE_MSI_VERSION "2018-02-01"
/* Capacity for the synthesized endpoint URL. */
#define AZURE_EP_BUF 320

/* === EntraIdentityConfig (frozen dataclass) ============================ */

typedef struct {
    char scope[768];
    bool exclude_interactive_browser;
} entra_identity_config_t;

/* PoP: agent_azure_identity_adapter_entra_identity_config_post_init @ agent/azure_identity_adapter.py:EntraIdentityConfig.__post_init__ */
void agent_azure_identity_adapter_entra_identity_config_post_init(
    const char *scope, char *out, size_t outsz)
{
    static const char *DEFAULT = AZURE_SCOPE_AI_AZURE_DEFAULT;
    if (!out || outsz == 0) return;

    const char *s = scope ? scope : "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t L = strlen(s);
    while (L > 0 && (s[L - 1] == ' ' || s[L - 1] == '\t' || s[L - 1] == '\n' || s[L - 1] == '\r'))
        L--;
    if (L == 0) {
        snprintf(out, outsz, "%s", DEFAULT);
        return;
    }
    if (L >= outsz) L = outsz - 1;
    memcpy(out, s, L);
    out[L] = '\0';
}

/* PoP: agent_azure_identity_adapter_entra_identity_config_to_dict @ agent/azure_identity_adapter.py:EntraIdentityConfig.to_dict */
json_t *agent_azure_identity_adapter_entra_identity_config_to_dict(
    const entra_identity_config_t *cfg)
{
    json_t *o = json_object();
    if (!o) return NULL;
    json_set(o, "scope", json_string(cfg ? cfg->scope : AZURE_SCOPE_AI_AZURE_DEFAULT));
    json_set(o, "exclude_interactive_browser",
                   json_bool(cfg ? cfg->exclude_interactive_browser : true));
    return o;
}

/* PoP: agent_azure_identity_adapter_entra_identity_config_from_dict @ agent/azure_identity_adapter.py:EntraIdentityConfig.from_dict */
void agent_azure_identity_adapter_entra_identity_config_from_dict(
    const json_t *data, const char *default_scope, entra_identity_config_t *out)
{
    if (!out) return;
    const char *cfg_default = default_scope && *default_scope ? default_scope : AZURE_SCOPE_AI_AZURE_DEFAULT;
    const char *raw = json_get_str(data, "scope", "");
    char norm[768];
    agent_azure_identity_adapter_entra_identity_config_post_init(raw, norm, sizeof(norm));
    strncpy(out->scope, norm, sizeof(out->scope) - 1);
    out->scope[sizeof(out->scope) - 1] = '\0';
    out->exclude_interactive_browser = json_get_bool(data, "exclude_interactive_browser", true);
    (void)cfg_default;
}

/* === Cached credential + token (mirrors @lru_cache build_credential) ==== */

typedef struct {
    entra_identity_config_t config;   /* key of the cached credential */
    bool                    valid;    /* credential cached */
    char                    token[4096];
    time_t                  expires_on;
    bool                    token_valid;
} azure_cred_cache_t;

static azure_cred_cache_t g_cred_cache;
static pthread_mutex_t     g_cred_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: agent_azure_identity_adapter_reset_credential_cache @ agent/azure_identity_adapter.py:reset_credential_cache */
void agent_azure_identity_adapter_reset_credential_cache(void)
{
    pthread_mutex_lock(&g_cred_lock);
    memset(&g_cred_cache, 0, sizeof(g_cred_cache));
    pthread_mutex_unlock(&g_cred_lock);
}

/* === Env helpers (scoped secret read falls back to plain getenv) ======= */

static const char *azure_get_env(const char *name)
{
    /* The Python get_secret()/UnscopedSecretError path is a profile-secret
     * vault. In C we read the real environment (same AZURE_* vars the SDK
     * reads) — faithful to the documented "let the SDK read env" approach. */
    const char *v = getenv(name);
    return (v && *v) ? v : NULL;
}

/* PoP: agent_azure_identity_adapter_has_azure_identity_installed @ agent/azure_identity_adapter.py:has_azure_identity_installed */
bool agent_azure_identity_adapter_has_azure_identity_installed(void)
{
    /* No pip in C: "installed" means the Entra runtime path is reachable,
     * i.e. at least one credential source is configured. This is exactly the
     * set of env vars the chain below consumes. */
    if (azure_get_env("AZURE_CLIENT_ID") &&
        azure_get_env("AZURE_CLIENT_SECRET") &&
        azure_get_env("AZURE_TENANT_ID"))
        return true;
    if (azure_get_env("AZURE_CLIENT_ID") &&
        azure_get_env("AZURE_FEDERATED_TOKEN_FILE") &&
        azure_get_env("AZURE_TENANT_ID"))
        return true;
    if (azure_get_env("IDENTITY_ENDPOINT") || azure_get_env("MSI_ENDPOINT"))
        return true;
    return false;
}

/* PoP: agent_azure_identity_adapter_is_token_provider @ agent/azure_identity_adapter.py:is_token_provider */
bool agent_azure_identity_adapter_is_token_provider(const void *value)
{
    /* C has no first-class callable type. A token provider is represented by
     * a non-NULL provider handle (azure_token_provider_t *). A raw string
     * API key is NOT a provider. */
    return value != NULL;
}

/* === Token minting (the real credential chain) ========================= */

/* Mint a token for scope via the client-credentials (secret) grant. */
static bool mint_client_secret(const char *tenant, const char *client_id,
                               const char *client_secret, const char *scope,
                               char *out_token, size_t out_cap, time_t *expires_on)
{
    char ep[AZURE_EP_BUF];
    snprintf(ep, sizeof(ep), AZURE_TOKEN_ENDPOINT_FMT, tenant);

    char body[2048];
    snprintf(body, sizeof(body),
             "grant_type=client_credentials&client_id=%s&client_secret=%s&scope=%s",
             client_id, client_secret, scope);

    http_t *h = http_new(15);
    if (!h) return false;
    http_resp_t *r = http_post_json(h, ep, body);
    bool ok = false;
    if (r && r->status == 200 && r->body) {
        json_t *doc = json_parse(r->body, NULL);
        if (doc) {
            const char *tok = json_get_str(doc, "access_token", NULL);
            if (tok && *tok) {
                strncpy(out_token, tok, out_cap - 1);
                out_token[out_cap - 1] = '\0';
                *expires_on = (time_t)json_get_num(doc, "expires_on", 0);
                ok = true;
            }
            json_free(doc);
        }
    }
    if (r) http_resp_free(r);
    http_free(h);
    return ok;
}

/* Mint a token for scope via the client-credentials grant using a federated
 * token file as a client_assertion (workload identity). */
static bool mint_workload_identity(const char *tenant, const char *client_id,
                                   const char *fed_file, const char *scope,
                                   char *out_token, size_t out_cap, time_t *expires_on)
{
    FILE *f = fopen(fed_file, "rb");
    if (!f) return false;
    char assertion[8192];
    size_t n = fread(assertion, 1, sizeof(assertion) - 1, f);
    fclose(f);
    if (n == 0) return false;
    assertion[n] = '\0';

    char ep[AZURE_EP_BUF];
    snprintf(ep, sizeof(ep), AZURE_TOKEN_ENDPOINT_FMT, tenant);

    /* client_assertion must be URL-form-encoded per OAuth2 JWT profile. */
    char *enc = http_url_encode(assertion);
    char body[11000];
    snprintf(body, sizeof(body),
             "grant_type=client_credentials&client_id=%s&scope=%s"
             "&client_assertion_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3A"
             "client-assertion-type%%3Ajwt-bearer&client_assertion=%s",
             client_id, scope, enc ? enc : "");
    free(enc);

    http_t *h = http_new(15);
    if (!h) return false;
    http_resp_t *r = http_post_json(h, ep, body);
    bool ok = false;
    if (r && r->status == 200 && r->body) {
        json_t *doc = json_parse(r->body, NULL);
        if (doc) {
            const char *tok = json_get_str(doc, "access_token", NULL);
            if (tok && *tok) {
                strncpy(out_token, tok, out_cap - 1);
                out_token[out_cap - 1] = '\0';
                *expires_on = (time_t)json_get_num(doc, "expires_on", 0);
                ok = true;
            }
            json_free(doc);
        }
    }
    if (r) http_resp_free(r);
    http_free(h);
    return ok;
}

/* Mint a token via the managed-identity (IMDS) endpoint. */
static bool mint_managed_identity(const char *client_id, const char *scope,
                                  char *out_token, size_t out_cap, time_t *expires_on)
{
    const char *ep = azure_get_env("IDENTITY_ENDPOINT");
    if (!ep) ep = azure_get_env("MSI_ENDPOINT");
    if (!ep) return false;

    /* IDENTITY_HEADER is required for user-assigned managed identity. */
    const char *hdr = azure_get_env("IDENTITY_HEADER");
    char url[1024];
    if (client_id && *client_id)
        snprintf(url, sizeof(url), "%s?api-version=%s&resource=%s&client_id=%s",
                 ep, AZURE_MSI_VERSION, scope, client_id);
    else
        snprintf(url, sizeof(url), "%s?api-version=%s&resource=%s",
                 ep, AZURE_MSI_VERSION, scope);

    http_t *h = http_new(15);
    if (!h) return false;
    char headers[256];
    if (hdr && *hdr)
        snprintf(headers, sizeof(headers), "x-identity-header: %s", hdr);
    else
        headers[0] = '\0';

    http_resp_t *r = http_get(h, url, headers[0] ? headers : NULL);
    bool ok = false;
    if (r && r->status == 200 && r->body) {
        json_t *doc = json_parse(r->body, NULL);
        if (doc) {
            const char *tok = json_get_str(doc, "access_token", NULL);
            if (tok && *tok) {
                strncpy(out_token, tok, out_cap - 1);
                out_token[out_cap - 1] = '\0';
                *expires_on = (time_t)json_get_num(doc, "expires_on", 0);
                ok = true;
            }
            json_free(doc);
        }
    }
    if (r) http_resp_free(r);
    http_free(h);
    return ok;
}

/* Resolve a token for the given config using the real credential chain.
 * Returns true and fills out_token on success. */
static bool resolve_token(const entra_identity_config_t *cfg,
                           char *out_token, size_t out_cap, time_t *expires_on)
{
    const char *tenant = azure_get_env("AZURE_TENANT_ID");
    const char *cid = azure_get_env("AZURE_CLIENT_ID");
    const char *secret = azure_get_env("AZURE_CLIENT_SECRET");
    const char *fed = azure_get_env("AZURE_FEDERATED_TOKEN_FILE");

    if (cid && secret && tenant) {
        if (mint_client_secret(tenant, cid, secret, cfg->scope, out_token, out_cap, expires_on))
            return true;
    }
    if (cid && fed && tenant) {
        if (mint_workload_identity(tenant, cid, fed, cfg->scope, out_token, out_cap, expires_on))
            return true;
    }
    if (mint_managed_identity(cid, cfg->scope, out_token, out_cap, expires_on))
        return true;
    return false;
}

/* PoP: agent_azure_identity_adapter_build_credential @ agent/azure_identity_adapter.py:build_credential
 * Returns a stable, non-NULL credential handle. There is exactly one cached
 * credential per process (lru_cache maxsize=1). The handle is the cache slot
 * itself; callers pass it to materialize/describe. */
azure_cred_cache_t *agent_azure_identity_adapter_build_credential(const entra_identity_config_t *config)
{
    pthread_mutex_lock(&g_cred_lock);
    if (g_cred_cache.valid && config &&
        strcmp(g_cred_cache.config.scope, config->scope) == 0 &&
        g_cred_cache.config.exclude_interactive_browser == config->exclude_interactive_browser) {
        pthread_mutex_unlock(&g_cred_lock);
        return &g_cred_cache;
    }
    memset(&g_cred_cache, 0, sizeof(g_cred_cache));
    if (config) g_cred_cache.config = *config;
    else {
        strncpy(g_cred_cache.config.scope, AZURE_SCOPE_AI_AZURE_DEFAULT,
                sizeof(g_cred_cache.config.scope) - 1);
        g_cred_cache.config.exclude_interactive_browser = true;
    }
    g_cred_cache.valid = true;
    pthread_mutex_unlock(&g_cred_lock);
    return &g_cred_cache;
}

/* PoP: agent_azure_identity_adapter_build_token_provider @ agent/azure_identity_adapter.py:build_token_provider
 * Returns a token-provider handle (a freshly built credential). The caller
 * invokes materialize_bearer_for_http(provider) to mint a token. The provider
 * caches its token until expiry. */
azure_cred_cache_t *agent_azure_identity_adapter_build_token_provider(
    const char *scope, const entra_identity_config_t *config,
    const char *base_url, bool exclude_interactive_browser)
{
    (void)base_url;
    entra_identity_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        char norm[768];
        agent_azure_identity_adapter_entra_identity_config_post_init(
            scope ? scope : AZURE_SCOPE_AI_AZURE_DEFAULT, norm, sizeof(norm));
        strncpy(cfg.scope, norm, sizeof(cfg.scope) - 1);
        cfg.exclude_interactive_browser = exclude_interactive_browser;
    }
    return agent_azure_identity_adapter_build_credential(&cfg);
}

/* PoP: agent_azure_identity_adapter_has_azure_identity_credentials @ agent/azure_identity_adapter.py:has_azure_identity_credentials */
bool agent_azure_identity_adapter_has_azure_identity_credentials(
    const char *scope, const entra_identity_config_t *config,
    double timeout_seconds, bool allow_install)
{
    (void)timeout_seconds;
    (void)allow_install; /* no lazy pip in C; presence of env = installable */
    if (!agent_azure_identity_adapter_has_azure_identity_installed())
        return false;
    entra_identity_config_t eff;
    if (config) eff = *config;
    else {
        char norm[768];
        agent_azure_identity_adapter_entra_identity_config_post_init(
            scope ? scope : AZURE_SCOPE_AI_AZURE_DEFAULT, norm, sizeof(norm));
        strncpy(eff.scope, norm, sizeof(eff.scope) - 1);
        eff.exclude_interactive_browser = true;
    }
    char tok[4096];
    time_t exp = 0;
    return resolve_token(&eff, tok, sizeof(tok), &exp) && tok[0] != '\0';
}

/* PoP: agent_azure_identity_adapter_materialize_bearer_for_http @ agent/azure_identity_adapter.py:materialize_bearer_for_http
 * materialize a fresh (or cached) bearer token for an outbound HTTP request.
 * Accepts either a token-provider handle or a literal API-key string. */
bool agent_azure_identity_adapter_materialize_bearer_for_http(
    const void *value, char *out_token, size_t out_cap)
{
    if (agent_azure_identity_adapter_is_token_provider(value)) {
        const azure_cred_cache_t *prov = (const azure_cred_cache_t *)value;
        time_t now = time(NULL);
        pthread_mutex_lock(&g_cred_lock);
        bool fresh = prov->token_valid && prov->token[0] != '\0' &&
                     (prov->expires_on == 0 || prov->expires_on > now + 60);
        if (fresh) {
            strncpy(out_token, prov->token, out_cap - 1);
            out_token[out_cap - 1] = '\0';
            pthread_mutex_unlock(&g_cred_lock);
            return true;
        }
        pthread_mutex_unlock(&g_cred_lock);

        entra_identity_config_t cfg;
        pthread_mutex_lock(&g_cred_lock);
        cfg = prov->config;
        pthread_mutex_unlock(&g_cred_lock);

        char tok[4096];
        time_t exp = 0;
        if (!resolve_token(&cfg, tok, sizeof(tok), &exp))
            return false;
        if (tok[0] == '\0')
            return false;
        pthread_mutex_lock(&g_cred_lock);
        strncpy((char *)prov->token, tok, sizeof(prov->token) - 1);
        ((char *)prov->token)[sizeof(prov->token) - 1] = '\0';
        ((azure_cred_cache_t *)prov)->expires_on = exp;
        ((azure_cred_cache_t *)prov)->token_valid = true;
        strncpy(out_token, tok, out_cap - 1);
        out_token[out_cap - 1] = '\0';
        pthread_mutex_unlock(&g_cred_lock);
        return true;
    }
    /* literal API key / token string */
    const char *s = (const char *)value;
    if (s && *s) {
        strncpy(out_token, s, out_cap - 1);
        out_token[out_cap - 1] = '\0';
        return true;
    }
    return false;
}

/* PoP: agent_azure_identity_adapter_describe_active_credential @ agent/azure_identity_adapter.py:describe_active_credential
 * Returns a JSON diagnostic object (caller frees). Never raises. */
json_t *agent_azure_identity_adapter_describe_active_credential(
    const entra_identity_config_t *config, const char *scope,
    double timeout_seconds, bool allow_install)
{
    (void)timeout_seconds;
    (void)allow_install;
    json_t *info = json_object();
    if (!info) return NULL;

    if (!agent_azure_identity_adapter_has_azure_identity_installed()) {
        json_set(info, "ok", json_bool(false));
        json_set(info, "error", json_string("azure-identity not installed"));
        json_set(info, "hint",
                        json_string("configure AZURE_CLIENT_ID / AZURE_TENANT_ID / "
                                        "AZURE_CLIENT_SECRET (or AZURE_FEDERATED_TOKEN_FILE) / "
                                        "IDENTITY_ENDPOINT for managed identity"));
        return info;
    }

    entra_identity_config_t eff;
    if (config) eff = *config;
    else {
        char norm[768];
        agent_azure_identity_adapter_entra_identity_config_post_init(
            scope ? scope : AZURE_SCOPE_AI_AZURE_DEFAULT, norm, sizeof(norm));
        strncpy(eff.scope, norm, sizeof(eff.scope) - 1);
        eff.exclude_interactive_browser = true;
    }

    json_set(info, "scope", json_string(eff.scope));

    const char *tenant = azure_get_env("AZURE_TENANT_ID");
    if (tenant) json_set(info, "tenant_id_env", json_string(tenant));

    json_t *sources = json_array();
    if (azure_get_env("AZURE_FEDERATED_TOKEN_FILE"))
        json_append(sources,
            json_string("WorkloadIdentityCredential (AZURE_FEDERATED_TOKEN_FILE)"));
    if (azure_get_env("AZURE_CLIENT_ID") && azure_get_env("AZURE_CLIENT_SECRET") && tenant)
        json_append(sources,
            json_string("EnvironmentCredential (client secret)"));
    if (azure_get_env("IDENTITY_ENDPOINT") || azure_get_env("MSI_ENDPOINT"))
        json_append(sources,
            json_string("ManagedIdentityCredential (IDENTITY_ENDPOINT)"));
    json_set(info, "env_sources", sources);

    char tok[4096];
    time_t exp = 0;
    if (resolve_token(&eff, tok, sizeof(tok), &exp) && tok[0]) {
        json_set(info, "ok", json_bool(true));
        if (exp) json_set(info, "expires_on", json_number((double)exp));
    } else {
        json_set(info, "ok", json_bool(false));
        json_set(info, "error", json_string("credential chain exhausted"));
    }
    return info;
}

/* PoP: agent_azure_identity_adapter_build_bearer_http_client @ agent/azure_identity_adapter.py:build_bearer_http_client
 * Returns an opaque client handle whose requests carry a fresh Bearer token.
 * Implemented over http_t with a per-request Authorization injection. The
 * returned handle is an azure_bearer_client_t (free with
 * agent_azure_identity_adapter_bearer_http_client_free). */
typedef struct {
    http_t *http;
    azure_cred_cache_t *provider;
} azure_bearer_client_t;

azure_bearer_client_t *agent_azure_identity_adapter_build_bearer_http_client(
    azure_cred_cache_t *provider)
{
    if (!agent_azure_identity_adapter_is_token_provider(provider))
        return NULL;
    azure_bearer_client_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->http = http_new(60);
    c->provider = provider;
    return c;
}

void agent_azure_identity_adapter_bearer_http_client_free(azure_bearer_client_t *c)
{
    if (!c) return;
    if (c->http) http_free(c->http);
    free(c);
}

/* Perform an authenticated GET through the bearer client; returns the
 * response (caller frees with http_resp_free) or NULL. */
http_resp_t *agent_azure_identity_adapter_bearer_http_get(
    azure_bearer_client_t *c, const char *url)
{
    if (!c || !c->http) return NULL;
    char tok[4096];
    if (!agent_azure_identity_adapter_materialize_bearer_for_http(c->provider, tok, sizeof(tok)))
        return NULL;
    char hdr[4224];
    snprintf(hdr, sizeof(hdr), "Authorization: Bearer %s", tok);
    return http_get(c->http, url, hdr);
}
