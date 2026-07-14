/*
 * credential_pool.h — Credential pool for Hermes C (P82).
 *
 * Manages multiple API keys per provider with round-robin rotation,
 * rate-limit tracking, consecutive-failure escalation, and quota
 * management. Each provider can have N keys; the pool auto-rotates
 * when a key is rate-limited or hits consecutive failures.
 */

#ifndef CREDENTIAL_POOL_H
#define CREDENTIAL_POOL_H

#include "hermes.h"
#include "hermes_json.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Credential Entry — one API key with usage tracking
 * ================================================================ */

#define CREDENTIAL_POOL_NAME_MAX 64
#define CREDENTIAL_POOL_MAX_KEYS 16

typedef enum {
    CRED_OK,             /* Key healthy, usable */
    CRED_RATE_LIMITED,   /* 429 received, waiting for reset */
    CRED_FAILED,         /* Consecutive failures exceeded, skip */
    CRED_EXHAUSTED,      /* Quota exhausted (tokens or requests) */
} credential_status_t;

typedef struct {
    char  api_key[2048];           /* The actual key (api_key auth_type) */
    char  label[64];              /* Human label (e.g. "prod-1", "backup") */
    credential_status_t status;
    int   weight;                 /* B11: selection weight (1=normal, higher=more likely) */
    int   consecutive_failures;   /* Incremented on 4xx/5xx (not 429) */
    int   max_consecutive_failures; /* Defaults to 3, then mark FAILED */

    /* Rate-limit tracking */
    int   rate_limit_remaining;   /* -1 = unknown */
    time_t rate_limit_reset;      /* epoch seconds when RPM resets */
    int   rpm_limit;              /* max requests per minute (from header) */

    /* Quota tracking */
    long long total_tokens_used;  /* lifetime tokens consumed via this key */
    long long total_requests;     /* lifetime request count */
    long long quota_limit;        /* -1 = unlimited */
    time_t   last_used;           /* epoch seconds of last use */
    double   lease_expiry;        /* epoch seconds; >now means leased exclusively */
    char     source[CREDENTIAL_POOL_NAME_MAX]; /* origin of the key (e.g. env var, file, manual) */

    /* === PooledCredential parity fields (agent/credential_pool.py) ===
     * The C pool originally tracked only api_key; the Python PooledCredential
     * carries OAuth token state. These fields let the runtime_* helpers and the
     * anthropic/claude_code sync path behave faithfully. */
    char     access_token[2048];  /* OAuth access token (also used as the pool key) */
    char     refresh_token[2048]; /* OAuth refresh token (single-use per grant) */
    long long expires_at_ms;      /* access_token expiry (epoch ms), 0 = unknown */
    char     agent_key[2048];     /* Nous NAS invoke JWT */
    char     agent_key_expires_at[64]; /* Nous agent_key expiry (ISO or epoch) */
    char     base_url[512];       /* provider base URL */
    char     inference_base_url[512]; /* Nous inference base URL */
    char     scope[256];          /* OAuth scope */
    json_node_t *extra;           /* _EXTRA_KEYS round-trip dict (NULL if empty) */
} credential_entry_t;

/* ================================================================
 *  Credential Pool — one pool per provider
 * ================================================================ */

typedef struct {
    char              provider_name[CREDENTIAL_POOL_NAME_MAX];
    credential_entry_t entries[CREDENTIAL_POOL_MAX_KEYS];
    int               entry_count;
    int               current_index;   /* round-robin cursor */

    int               max_retries_per_key; /* calls before rotating (default 1) */
    int               cooloff_seconds;     /* how long to wait after FAILED before retry (default 300) */
} credential_pool_t;

/* ================================================================
 *  API
 * ================================================================ */

/* Create an empty pool for the given provider. Returns NULL on alloc fail. */
credential_pool_t *credential_pool_create(const char *provider_name);

/* Add a key to the pool. label is optional (can be ""). Returns entry index or -1. */
int credential_pool_add_key(credential_pool_t *pool,
                            const char *api_key,
                            const char *label);

/* Get the next available key (round-robin, skipping non-CRED_OK entries).
 * Returns NULL if no usable key. Sets *out_index if non-NULL. */
const char *credential_pool_next_key(const credential_pool_t *pool, int *out_index);

/* Report result after a request using key at entry_index.
 * - http_status: HTTP status code (200, 429, 401, 500, etc.)
 * - tokens_used: tokens consumed (-1 if unknown)
 * - rate_limit_remaining: from x-ratelimit-remaining header (-1 if unknown)
 * - rate_limit_reset: epoch seconds of rate limit reset (0 if unknown)
 * Updates consecutive_failures, status, quota fields.
 * Returns the updated entry status. */
credential_status_t credential_pool_report(credential_pool_t *pool,
                                           int entry_index,
                                           int http_status,
                                           long long tokens_used,
                                           int rate_limit_remaining,
                                           time_t rate_limit_reset);

/* Reset a failed/rate-limited entry back to CRED_OK (e.g. after cooloff).
 * Returns true if the entry exists. */
bool credential_pool_reset(credential_pool_t *pool, int entry_index);

/* B11: Set weight for a specific entry (higher = more likely selected).
 * weight=0 excludes the entry. Returns false if entry_index invalid. */
bool credential_pool_set_weight(credential_pool_t *pool, int entry_index, int weight);

/* Get stats for all entries in the pool. Returns a malloc'd JSON string or NULL.
 * Caller must free(). */
char *credential_pool_stats_json(const credential_pool_t *pool);

/* Free the pool and all entries. */
void credential_pool_free(credential_pool_t *pool);

/* Check if any entry in the pool is usable. */
bool credential_pool_has_available(const credential_pool_t *pool);

/* Port of Python agent/agent_runtime_helpers.py:recover_with_credential_pool().
 * Report an HTTP result to the credential pool and return a recovery action.
 * current_provider: provider name for cross-contamination guard (NULL/"" = skip).
 * error_body: JSON error body for usage-limit detection (NULL = skip).
 * has_retried: in/out — set true on first 429 retry, pass back for subsequent calls.
 * Returns cred_recover_t: NONE, RETRY, ROTATE, or FALLBACK. */
typedef enum {
    CRED_RECOVER_NONE,     /* no pool recovery needed (success) */
    CRED_RECOVER_RETRY,    /* retry same entry (first 429 hit) */
    CRED_RECOVER_ROTATE,   /* mark entry exhausted, use next */
    CRED_RECOVER_FALLBACK, /* pool exhausted — trigger fallback */
} cred_recover_t;
cred_recover_t recover_with_credential_pool(credential_pool_t *pool,
                                        const char *current_provider,
                                        int  http_status,
                                        const char *error_body,
                                        bool *has_retried);

/* Global cooloff checker — call periodically (e.g. every turn) to resurrect
 * cooled-off entries. Returns number of entries resurrected. */
int credential_pool_tick(credential_pool_t *pool);

/* === Credential persistence sanitization (AG29) === */
bool is_borrowed_credential_source(const char *source, const char *provider_id);
json_node_t *sanitize_borrowed_credential_payload(const json_node_t *payload, const char *provider_id);

/* === Prune control (port of Python credential_pool._is_prunable + module global) ===
 * `env:*` entries are re-hydrated from the environment on every load; a process
 * that merely lacks the env var must NOT delete the on-disk entry for every other
 * process (that destructive read is bug #9331). Pruning an env source is only
 * allowed when explicitly enabled (e.g. an `hermes auth` action confirmed gone). */
void credential_pool_set_prune_env_sources(bool enable);
bool credential_pool_get_prune_env_sources(void);

/* Returns true if the persisted entry may be removed during a prune pass. */
bool credential_pool_is_prunable(const credential_entry_t *entry);

/* ================================================================
 *  PooledCredential runtime helpers (agent/credential_pool.py gaps)
 *  Close the remaining REAL_GAPs on credential_pool.py by extending the
 *  simpler C entry with the PooledCredential fields above.
 * ================================================================ */

/* Nous NAS invoke-JWT usability check: decode the JWT `exp` claim and verify
 * it is still valid for `scope` and not past `expires_at`. Returns true if the
 * token can be used as a runtime inference credential. Mirrors
 * hermes_cli.auth._nous_invoke_jwt_is_usable(). */
bool nous_invoke_jwt_is_usable(const char *token, const char *scope, const char *expires_at);

/* PooledCredential.runtime_api_key — Nous uses the agent_key (NAS invoke JWT)
 * when usable, else the access_token; other providers return access_token. */
char *credential_entry_runtime_api_key(const credential_entry_t *e, const char *provider);

/* PooledCredential.runtime_base_url — Nous uses inference_base_url or base_url;
 * other providers return base_url. Caller must free the result. */
char *credential_entry_runtime_base_url(const credential_entry_t *e, const char *provider);

/* PooledCredential.__getattr__ — resolve a key from the entry's `extra` dict.
 * Returns a malloc'd string (caller frees) or NULL if absent. */
char *credential_entry_get_extra(const credential_entry_t *e, const char *key);

/* _write_through_provider_state_to_global_root — best-effort write of a
 * rotated provider `state` JSON object into the global-root auth.json
 * providers.<provider_id> section. Swallows all errors. Mirrors the Python fn. */
void credential_pool_write_through_provider_state_to_global_root(const char *provider_id,
                                                                  const char *state_json);

/* _sync_anthropic_entry_from_credentials_file — if the entry is an anthropic
 * claude_code entry, sync its tokens from ~/.claude/.credentials.json when they
 * differ. Mutates *e in place. Returns true if a sync was applied. */
bool credential_pool_sync_anthropic_entry_from_credentials_file(credential_entry_t *e);

/* _refresh_entry_impl — refresh a pool entry's tokens per provider, adopting
 * fresher tokens from the auth store first where applicable. force=true skips
 * the expiry short-circuit. Returns true if the entry was refreshed in place. */
bool credential_pool_refresh_entry_impl(credential_pool_t *pool, int entry_index, bool force);

#ifdef __cplusplus
}
#endif

#endif /* CREDENTIAL_POOL_H */
