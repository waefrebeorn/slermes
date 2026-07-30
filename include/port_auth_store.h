/*
 * port_auth_store.h — auth.json persistence layer ported from hermes_cli/auth.py.
 *
 * Covers: auth-store load/save (atomic 0600 O_EXCL writes), cross-process
 * advisory flock with per-path reentrancy, provider-state read/write with
 * profile->global fallback, credential-pool read/write with disk-cooldown
 * merge, provider management (active/known/routable/clear/deactivate),
 * config.yaml provider helpers, and the pure Nous OAuth state helpers
 * (invoke-JWT selection, shared cross-profile store, quarantine).
 *
 * Opaque txn struct + minimal includes (C11). JSON values use the libjson
 * json_t tree (hermes_json.h compat layer).
 */
#ifndef PORT_AUTH_STORE_H
#define PORT_AUTH_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include "../lib/libjson/json.h"
#include "auth_helpers.h"   /* auth_error_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ── constants (mirror hermes_cli/auth.py module constants) ── */
#define AUTH_STORE_VERSION_C            1
#define AUTH_LOCK_TIMEOUT_SECONDS_C     15.0
#define ACCESS_TOKEN_REFRESH_SKEW_SECONDS_C        120
#define CODEX_ACCESS_TOKEN_REFRESH_SKEW_SECONDS_C  120
#define XAI_ACCESS_TOKEN_REFRESH_SKEW_SECONDS_C    3600
#define QWEN_ACCESS_TOKEN_REFRESH_SKEW_SECONDS_C   120
#define NOUS_DEVICE_CODE_SOURCE_C       "device_code"
#define OPENROUTER_BASE_URL_C           "https://openrouter.ai/api/v1"

/* ── file lock (generic; used for auth.json + shared Nous store) ── */
int  auth_file_lock_acquire(const char *lock_path, double timeout_seconds);
void auth_file_lock_release(const char *lock_path);

/* ── auth store lock / path helpers ── */
char *authstore_lock_path(void);                       /* malloc'd */
bool  authstore_same_path(const char *a, const char *b);
int   authstore_lock(const char *target_path, double timeout_seconds); /* NULL = active store */
void  authstore_unlock(const char *target_path);

/* ── auth store load / save ── */
json_t *authstore_load(const char *auth_file);         /* NULL = active store; never NULL result */
json_t *authstore_load_global(void);                   /* {} when no global fallback */
char   *authstore_save(json_t *auth_store, const char *target_path); /* malloc'd path or NULL */

/* ── provider state ── */
json_t *authstore_load_provider_state_with_source(json_t *auth_store, const char *provider_id,
                                                  char **out_source_path);
json_t *authstore_load_provider_state(json_t *auth_store, const char *provider_id);
void    authstore_save_provider_state(json_t *auth_store, const char *provider_id, const json_t *state);
void    authstore_store_provider_state(json_t *auth_store, const char *provider_id,
                                       const json_t *state, bool set_active);
char   *authstore_persist_provider_state_to_store(const char *provider_id, const json_t *state,
                                                  const char *target_path, bool set_active);
void    authstore_save_provider_state_to_source(json_t *auth_store, const char *provider_id,
                                                const json_t *state, const char *source_path);

/* Opaque provider-state transaction (mirrors _provider_state_transaction). */
typedef struct auth_provider_txn auth_provider_txn_t;
auth_provider_txn_t *authstore_provider_state_transaction(const char *provider_id);
json_t *auth_provider_txn_store(auth_provider_txn_t *txn);
json_t *auth_provider_txn_state(auth_provider_txn_t *txn);        /* may be NULL */
const char *auth_provider_txn_source_path(auth_provider_txn_t *txn); /* may be NULL */
void    auth_provider_txn_end(auth_provider_txn_t *txn);

/* ── provider management ── */
void  mark_provider_active_if_unset(const char *provider_id);
bool  is_known_auth_provider(const char *provider_id);
char *get_auth_provider_display_name(const char *provider_id);    /* malloc'd */
bool  is_runtime_provider_routable(const char *provider_id);
json_t *read_credential_pool(const char *provider_id);            /* NULL = whole pool */
json_t *auth_merge_disk_cooldown_state(const json_t *entry, const json_t *disk_entry,
                                       const char *provider_id);  /* new node */
char *write_credential_pool(const char *provider_id, const json_t *entries,
                            const json_t *removed_ids);           /* malloc'd path */
bool  unsuppress_credential_source(const char *provider_id, const char *source);
json_t *get_provider_auth_state(const char *provider_id);         /* copy or NULL */
bool  is_provider_explicitly_configured(const char *provider_id);
bool  clear_provider_auth(const char *provider_id);               /* NULL = active */
void  deactivate_provider(void);
char *get_anthropic_key(void);                                    /* malloc'd, "" when none */

/* ── config.yaml provider helpers ── */
char *auth_get_config_provider(void);                             /* malloc'd or NULL */
bool  auth_config_provider_matches(const char *provider_id);
bool  auth_should_reset_config_provider_on_logout(const char *provider_id);
char *auth_logout_default_provider_from_config(void);             /* malloc'd or NULL */
char *auth_reset_config_provider(void);                           /* malloc'd config path */
void  auth_save_model_choice(const char *model_id);

/* ── TLS verify resolution (behavioral port of ssl.SSLContext selection) ── */
typedef struct {
    bool insecure;              /* True => verification disabled */
    char ca_bundle[1024];       /* explicit CA bundle path, or "" for default */
} auth_verify_t;
auth_verify_t auth_default_verify(void);
auth_verify_t auth_resolve_verify(const char *insecure_opt, const char *ca_bundle,
                                  const json_t *auth_state);

/* ── OAuth trace + pure Nous helpers ── */
bool  auth_oauth_trace_enabled(void);
void  auth_oauth_trace(const char *event, const char *sequence_id, const json_t *fields);
char *auth_nous_inference_env_override(void);                     /* malloc'd or NULL */
char *auth_nous_portal_env_override(void);                        /* malloc'd or NULL */
char *auth_validate_nous_inference_url_from_network(const char *url); /* malloc'd or NULL */
void  auth_migrate_stale_nous_portal_url(json_t *providers);
bool  auth_codex_access_token_is_expiring(const char *access_token, int skew_seconds);
bool  auth_qwen_access_token_is_expiring(const char *expiry_date_ms, int skew_seconds);
char *auth_nous_jwt_expires_at(const char *token, const char *fallback_expires_at); /* malloc'd or NULL */
bool  auth_nous_invoke_jwt_is_usable(const char *token, const char *scope,
                                     const char *expires_at, int min_ttl_seconds);
auth_error_t *auth_assert_nous_inference_jwt_usable(const json_t *state, const char *access_token);
void  auth_log_nous_invoke_jwt_selected(const char *access_token, const char *sequence_id);
void  auth_set_nous_agent_key_from_invoke_jwt(json_t *state, const char *obtained_at);
void  auth_select_nous_invoke_jwt(json_t *state, const char *access_token, const char *sequence_id);
json_t *auth_nous_effective_provider_state(const json_t *state);  /* new node */
bool  auth_agent_key_is_usable(const json_t *state, int min_ttl_seconds);
json_t *auth_empty_nous_auth_status(void);                        /* new node */
void  auth_invalidate_nous_auth_status_cache(void);
bool  auth_is_terminal_xai_oauth_refresh_error(const char *provider, const char *code,
                                               bool relogin_required);
bool  auth_is_terminal_codex_oauth_refresh_error(const char *provider, const char *code,
                                                 bool relogin_required);
/* _is_terminal_nous_refresh_error lives in port_auth_helpers.c:
 * int auth_is_terminal_nous_refresh_error(const char*, const char*, int) */

/* ── shared cross-profile Nous OAuth store (flat JSON dict, Python schema) ── */
char   *auth_nous_shared_store_path_py(void);                     /* malloc'd */
json_t *auth_read_shared_nous_state(void);                        /* NULL when absent/invalid */
void    auth_write_shared_nous_state(const json_t *state);        /* best-effort */
void    auth_clear_shared_nous_state(const char *reason);
bool    auth_merge_shared_nous_oauth_state(json_t *state);
void    auth_quarantine_nous_oauth_state(json_t *state, const auth_error_t *error, const char *reason);
bool    auth_quarantine_nous_pool_entries(json_t *auth_store, const auth_error_t *error,
                                          const char *reason);

#ifdef __cplusplus
}
#endif

#endif /* PORT_AUTH_STORE_H */
