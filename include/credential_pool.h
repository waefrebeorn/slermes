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

#include "hermes_core_types.h"
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
    char  api_key[2048];           /* The actual key */
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

    /* OAuth / provider-specific credential material */
    char     access_token[2048];
    char     refresh_token[2048];
    char     base_url[1024];
    char     inference_base_url[1024];
    char     scope[512];
    char     agent_key[2048];
    char     agent_key_expires_at[64];
    long long expires_at_ms;      /* OAuth token expiry, epoch ms */

    json_t  *extra;               /* optional extra key/value bag (JSON object) */
    char     source[CREDENTIAL_POOL_NAME_MAX]; /* origin of the key (e.g. env var, file, manual) */
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

/* Serialize one credential_entry_t to its disk-safe JSON object (port of
 * Python CredentialEntry.to_dict() — runs sanitize_borrowed_credential_payload).
 * Caller frees. */
json_node_t *credential_entry_to_json(const credential_entry_t *e, const char *provider);

/* JSON-in variant of credential_entry_to_json(): build the entry from a JSON
 * object (load-path shape) and serialize it disk-safe. Reuses the above. */
json_node_t *credential_entry_to_json_from_obj(const json_t *entry, const char *provider);

/* Serialize all entries of a pool to the providers.<provider> entry-array JSON
 * string Python writes to auth.json. Caller frees. */
char *credential_pool_entries_json(const credential_pool_t *pool);

/* Persist a pool's entries to auth.json's providers.<provider> array (the
 * sanitized entry-array write path, mirroring Python's to_dict persistence).
 * Best-effort; skips under pytest. */
void credential_pool_persist_entries(const credential_pool_t *pool);

/* === Prune control (port of Python credential_pool._is_prunable + module global) ===
 * `env:*` entries are re-hydrated from the environment on every load; a process
 * that merely lacks the env var must NOT delete the on-disk entry for every other
 * process (that destructive read is bug #9331). Pruning an env source is only
 * allowed when explicitly enabled (e.g. an `hermes auth` action confirmed gone). */
void credential_pool_set_prune_env_sources(bool enable);
bool credential_pool_get_prune_env_sources(void);

/* Returns true if the persisted entry may be removed during a prune pass. */
bool credential_pool_is_prunable(const credential_entry_t *entry);

#ifdef __cplusplus
}
#endif

#endif /* CREDENTIAL_POOL_H */
