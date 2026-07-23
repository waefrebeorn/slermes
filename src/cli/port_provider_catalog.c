/*
 * port_provider_catalog.c — Faithful C11 port of hermes_cli/provider_catalog.py
 *
 * Pure helper functions from the unified provider catalog:
 *   - tab_for_auth_type(auth_type)        -> "accounts" | "keys"
 *   - _split_env_vars(env_vars)           -> (api_key_vars, base_url_var)
 *
 * The full provider_catalog() / provider_catalog_by_slug() derive from the
 * CANONICAL_PROVIDERS / PROVIDER_REGISTRY data tables (ported in
 * model_catalog.c) and are a separate, data-driven port.
 *
 * Angel-coder note: only the two pure, IO-free helpers are ported here;
 * the data-derived functions are not faked.
 */

#include <string.h>
#include <stdlib.h>
#include "provider_catalog.h"

/* Auth types that route to the desktop "Accounts" tab rather than "API keys".
 * Mirrors _ACCOUNTS_AUTH_TYPES in hermes_cli/provider_catalog.py. */
static const char *G_ACCOUNTS_AUTH_TYPES[] = {
    "oauth_device_code",
    "oauth_external",
    "oauth_minimax",
    "external_process",
    "copilot",
    NULL,
};

/* PoP: provider_catalog_tab_for_auth_type @ hermes_cli/provider_catalog.py:tab_for_auth_type */
const char *provider_catalog_tab_for_auth_type(const char *auth_type)
{
    if (!auth_type) return "keys";
    for (int i = 0; G_ACCOUNTS_AUTH_TYPES[i]; i++) {
        if (strcmp(auth_type, G_ACCOUNTS_AUTH_TYPES[i]) == 0) {
            return "accounts";
        }
    }
    return "keys";
}

/* PoP: provider_catalog_split_env_vars @ hermes_cli/provider_catalog.py:_split_env_vars */
void provider_catalog_split_env_vars(const char *const *env_vars, int n,
                                      const char *out_keys[], int *out_keys_len,
                                      char *out_base, size_t out_base_cap)
{
    *out_keys_len = 0;
    out_base[0] = '\0';
    if (!env_vars) return;
    for (int i = 0; i < n; i++) {
        const char *v = env_vars[i];
        size_t len = strlen(v);
        int is_base = (len >= 9 && strcmp(v + len - 9, "_BASE_URL") == 0) ||
                      (len >= 4 && strcmp(v + len - 4, "_URL") == 0);
        if (is_base) {
            if (out_base[0] == '\0') {  /* first match wins, like next(...) */
                strncpy(out_base, v, out_base_cap - 1);
                out_base[out_base_cap - 1] = '\0';
            }
        } else {
            out_keys[(*out_keys_len)++] = v;
        }
    }
}
