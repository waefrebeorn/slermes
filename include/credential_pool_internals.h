/*
 * credential_pool_internals.h — internal declarations shared across the
 * split credential-pool modules (credential_pool_engine.c,
 * credential_pool_persistence.c, credential_pool_custom.c,
 * credential_pool_sync.c).
 *
 * This header is NOT a god header: it declares only the symbols that are
 * defined in one split module and consumed by another. Public API lives in
 * include/credential_pool.h. Keep includes minimal (C11 only).
 */

#ifndef CREDENTIAL_POOL_INTERNALS_H
#define CREDENTIAL_POOL_INTERNALS_H

#include "credential_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module-global prune control (defined in credential_pool_engine.c). */
extern bool credential_pool_prune_env_sources_enabled;
void credential_pool_set_prune_env_sources(bool enable);
bool credential_pool_get_prune_env_sources(void);

/* Resolve the auth.json path under HERMES_HOME (falls back to $HOME).
 * Defined in credential_pool_engine.c; used by persistence + sync modules.
 * Caller must free() the returned string. */
char *cp_auth_json_path(void);

/* Shared internal helpers (defined in credential_pool_engine.c). */
bool entry_usable(const credential_entry_t *e, time_t now);

/* Module-local helpers shared across the split (defined in
 * credential_pool_custom.c unless noted). Declared here so the other
 * split modules can call them without pulling in the public header. */
const char *label_from_token(const char *token, const char *fallback);
int _next_priority(int current_max_priority);
bool _is_manual_source(const char *source);
int _exhausted_ttl(int error_code);
double _parse_absolute_timestamp(const char *value);
double _extract_retry_delay_seconds(const char *message);
void _normalize_error_context(const char *input, char *output, size_t out_size);
double _exhausted_until(const credential_entry_t *entry);
void _normalize_custom_pool_name(const char *name, char *out, size_t out_size);
int credential_pool_iter_custom_providers(char **out_norm, char **out_entry, int max);
const char *get_custom_provider_pool_key(const char *base_url, const char *provider_name);
int list_custom_pool_providers(char **out_list, int max_entries);
bool _get_custom_provider_config(const char *pool_key, char *out_config, size_t out_size);

/* JWT `exp` claim decode. Returns epoch-seconds expiry or 0 if unparseable.
 * Defined in credential_pool_sync.c; used there + by nous_invoke_jwt_is_usable. */
long long jwt_exp_claim(const char *token);

/* Per-provider token sync from the shared auth store (defined in
 * credential_pool_sync.c). */
bool _sync_codex_entry_from_auth_store(credential_pool_t *pool, int index);
bool _sync_xai_oauth_entry_from_auth_store(credential_pool_t *pool, int index);
bool _sync_nous_entry_from_auth_store(credential_pool_t *pool, int index);
bool _sync_device_code_entry_to_auth_store(credential_pool_t *pool, int index);

/* Write the rotated provider state into the global-root auth.json
 * providers.<provider_id> section. Defined in credential_pool_sync.c. */
void credential_pool_write_through_provider_state_to_global_root(
    const char *provider_id, const char *state_json);

#ifdef __cplusplus
}
#endif

#endif /* CREDENTIAL_POOL_INTERNALS_H */
