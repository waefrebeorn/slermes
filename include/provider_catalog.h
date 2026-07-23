#ifndef HERMES_PROVIDER_CATALOG_H
#define HERMES_PROVIDER_CATALOG_H

/*
 * provider_catalog.h — C11 port of hermes_cli/provider_catalog.py helpers.
 *
 * Pure, IO-free helpers from the unified provider catalog.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Return the desktop tab ("accounts" | "keys") for an auth_type.
 * NULL auth_type -> "keys". */
const char *provider_catalog_tab_for_auth_type(const char *auth_type);

/* Split an env_vars list into (api_key_vars, base_url_var).
 * env_vars: array of n C strings.
 * out_keys: receives pointers to the non-URL entries (first-match-wins for base).
 * out_keys_len: number of entries written to out_keys.
 * out_base: receives the first *_BASE_URL / *_URL entry ("" if none).
 * out_base_cap: capacity of out_base. */
void provider_catalog_split_env_vars(const char *const *env_vars, int n,
                                      const char *out_keys[], int *out_keys_len,
                                      char *out_base, size_t out_base_cap);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_PROVIDER_CATALOG_H */
