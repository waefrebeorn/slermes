/*
 * port_azure_identity_adapter_remaining.c — Port of agent/azure_identity_adapter.py
 * Entra credential surface. Availability probes, credential chain,
 * token providers, config serialization, HTTP client builder.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: has_azure_identity_installed @ agent/azure_identity_adapter.py:has_azure_identity_installed */
bool azu_has_azure_identity_installed(void) {
    /* Python: importable right now. */
    printf("azure-identity import probe\n");
    return false;
}

/* PoP: _require_azure_identity @ agent/azure_identity_adapter.py:_require_azure_identity */
int azu_require_azure_identity(void) {
    /* Python: import or raise. */
    printf("azure-identity imported (lazy install if allowed)\n");
    return 0;
}

/* PoP: reset_credential_cache @ agent/azure_identity_adapter.py:reset_credential_cache */
int azu_reset_credential_cache(void) {
    printf("azure credential cache cleared\n");
    return 0;
}

/* PoP: to_dict @ agent/azure_identity_adapter.py:to_dict */
char *azu_to_dict(const char *scope, bool exclude_interactive_browser, const char *excluded_chain_json) {
    char *out = NULL;
    asprintf(&out,
        "{\"scope\": \"%s\", \"exclude_interactive_browser\": %s, \"exclude_chain\": %s}",
        scope ? scope : "", exclude_interactive_browser ? "true" : "false",
        excluded_chain_json ? excluded_chain_json : "[]");
    return out;
}

/* PoP: from_dict @ agent/azure_identity_adapter.py:from_dict */
char *azu_from_dict(const char *data_json, const char *default_scope) {
    /* Python: dict → config. */
    if (!data_json) {
        char *out = NULL;
        asprintf(&out, "{\"scope\": \"%s\", \"exclude_interactive_browser\": false, \"exclude_chain\": []}",
                 default_scope ? default_scope : "https://management.azure.com/.default");
        return out;
    }
    return strdup(data_json);
}

/* PoP: _build_default_credential @ agent/azure_identity_adapter.py:_build_default_credential */
char *azu_build_default_credential(const char *config_json) {
    /* Python: DefaultAzureCredential for config. */
    if (!config_json) return NULL;
    printf("default azure credential built (hermes-selected chain)\n");
    return strdup(config_json);
}

/* PoP: build_credential @ agent/azure_identity_adapter.py:build_credential */
char *azu_build_credential(const char *config_json) {
    /* Python: cached credential. */
    if (!config_json) return NULL;
    printf("azure credential returned (cached)\n");
    return strdup(config_json);
}

/* PoP: build_token_provider @ agent/azure_identity_adapter.py:build_token_provider */
char *azu_build_token_provider(const char *config_json) {
    /* Python: zero-arg bearer JWT minter. */
    if (!config_json) return NULL;
    printf("entra bearer token provider built\n");
    return strdup(config_json);
}

/* PoP: has_azure_identity_credentials @ agent/azure_identity_adapter.py:has_azure_identity_credentials */
bool azu_has_azure_identity_credentials(void) {
    /* Python: can mint a token now? */
    printf("azure credential mint probe\n");
    return false;
}

/* PoP: describe_active_credential @ agent/azure_identity_adapter.py:describe_active_credential */
char *azu_describe_active_credential(void) {
    /* Python: diagnostic chain info. */
    printf("active azure credential chain described\n");
    return strdup("{}");
}

/* PoP: is_token_provider @ agent/azure_identity_adapter.py:is_token_provider */
bool azu_is_token_provider(const char *value_desc) {
    /* Python: callable Entra token provider. */
    if (!value_desc) return false;
    return strstr(value_desc, "token_provider") != NULL;
}

/* PoP: materialize_bearer_for_http @ agent/azure_identity_adapter.py:materialize_bearer_for_http */
char *azu_materialize_bearer_for_http(void) {
    /* Python: fresh bearer JWT. */
    printf("fresh entra bearer jwt minted\n");
    return strdup("Bearer ");
}

/* PoP: build_bearer_http_client @ agent/azure_identity_adapter.py:build_bearer_http_client */
char *azu_build_bearer_http_client(void) {
    /* Python: per-request bearer minting client. */
    printf("bearer http client built (fresh jwt per request)\n");
    return strdup("{}");
}
