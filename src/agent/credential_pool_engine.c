/* Port of Python agent/credential_pool.py (split module).

Self-contained credential-pool subsystem component. credential_pool_t /
credential_entry_t are defined in include/credential_pool.h; internal helpers
shared across the split modules are declared in include/credential_pool_internals.h.
No god headers — only the minimal includes each module requires. C11 only.
*/

#include "credential_pool.h"
#include "credential_pool_internals.h"
#include "slermes_home.h"
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "hermes_auth.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/*
 * Component 1/4 — pool engine: create / add / select / report /
 * rotate / stats / free / tick / recover, plus load_pool / select /
 * peek / current / mark_exhausted_and_rotate / reset_statuses /
 * has_available / add_entry / _upsert_entry / get_pool_strategy /
 * cp_auth_json_path. Faithful port of the multi-key rotation core +
 * agent_runtime_helpers.py:recover_with_credential_pool.
 */

/* ================================================================
 *  Multi-key rotation per provider with rate-limit tracking,
 *  consecutive-failure backoff, and quota management.
 * ================================================================ */

#include "credential_pool.h"
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "hermes_auth.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Default max retries before rotating */
#define DEFAULT_MAX_CONSECUTIVE_FAILURES 3
#define DEFAULT_COOLOFF_SECONDS 300
#define DEFAULT_RETRIES_PER_KEY 1

/* When true, env:* entries may be pruned from the on-disk pool. Mirrors the
 * Python module global `prune_env_sources`. Normally false: an env-backed
 * entry is re-hydrated from the environment on every load, so a process that
 * merely lacks the env var must NOT delete the on-disk entry for every other
 * process (that destructive read is the bug behind #9331). Only set it when an
 * explicit `hermes auth` action has confirmed the source is gone. */
bool credential_pool_prune_env_sources_enabled = false;

void credential_pool_set_prune_env_sources(bool enable) {
    credential_pool_prune_env_sources_enabled = enable;
}

bool credential_pool_get_prune_env_sources(void) {
    return credential_pool_prune_env_sources_enabled;
}

bool entry_usable(const credential_entry_t *e, time_t now) {
    switch (e->status) {
    case CRED_OK:
        return true;
    case CRED_RATE_LIMITED:
        /* Check if rate limit has expired */
        if (e->rate_limit_reset > 0 && now >= e->rate_limit_reset)
            return true;
        return false;
    case CRED_FAILED:
        /* Check cooloff: if enough time passed since last_used, try again */
        if (e->last_used > 0 && (now - e->last_used) >= DEFAULT_COOLOFF_SECONDS)
            return true;
        return false;
    case CRED_EXHAUSTED:
        return false;
    }
    return false;
}

/* ================================================================
 *  Public API
 * ================================================================ */

credential_pool_t *credential_pool_create(const char *provider_name) {
    credential_pool_t *pool = (credential_pool_t *)calloc(1, sizeof(credential_pool_t));
    if (!pool) return NULL;

    /* Seed random for weighted selection (one-shot, ok if redundant) */
    static bool seeded = false;
    if (!seeded) { srand((unsigned int)(time(NULL) ^ (uintptr_t)pool)); seeded = true; }

    if (provider_name)
        snprintf(pool->provider_name, sizeof(pool->provider_name), "%s", provider_name);
    else
        snprintf(pool->provider_name, sizeof(pool->provider_name), "default");

    pool->current_index = 0;
    pool->max_retries_per_key = DEFAULT_RETRIES_PER_KEY;
    pool->cooloff_seconds = DEFAULT_COOLOFF_SECONDS;
    return pool;
}

int credential_pool_add_key(credential_pool_t *pool,
                             const char *api_key,
                             const char *label) {
    if (!pool || !api_key) return -1;
    if (pool->entry_count >= CREDENTIAL_POOL_MAX_KEYS) return -1;

    int idx = pool->entry_count;
    credential_entry_t *e = &pool->entries[idx];

    snprintf(e->api_key, sizeof(e->api_key), "%s", api_key);
    if (label && label[0])
        snprintf(e->label, sizeof(e->label), "%s", label);
    else
        snprintf(e->label, sizeof(e->label), "key-%d", idx);

    e->status = CRED_OK;
    e->consecutive_failures = 0;
    e->max_consecutive_failures = DEFAULT_MAX_CONSECUTIVE_FAILURES;
    e->rate_limit_remaining = -1;
    e->rate_limit_reset = 0;
    e->rpm_limit = 0;
    e->total_tokens_used = 0;
    e->total_requests = 0;
    e->quota_limit = -1;
    e->last_used = 0;
    e->weight = 1;  /* B11: default weight = normal */
    e->source[0] = '\0';  /* no provenance unless caller sets it */

    pool->entry_count++;
    return idx;
}

/* B11: Set weight for a specific entry */
bool credential_pool_set_weight(credential_pool_t *pool, int entry_index, int weight) {
    if (!pool || entry_index < 0 || entry_index >= pool->entry_count)
        return false;
    pool->entries[entry_index].weight = weight < 0 ? 0 : weight;
    return true;
}

const char *credential_pool_next_key(const credential_pool_t *pool, int *out_index) {
    if (!pool || pool->entry_count == 0) return NULL;

    time_t now = time(NULL);
    int n = pool->entry_count;

    /* B11: Check if any entry has non-default weight (weight != 1) */
    bool has_weights = false;
    int total_weight = 0;
    for (int i = 0; i < n; i++) {
        if (pool->entries[i].weight != 1) { has_weights = true; }
        if (entry_usable(&pool->entries[i], now) && pool->entries[i].weight > 0)
            total_weight += pool->entries[i].weight;
    }

    if (has_weights && total_weight > 0) {
        /* Weighted random selection */
        int roll = rand() % total_weight;
        int accum = 0;
        for (int i = 0; i < n; i++) {
            if (!entry_usable(&pool->entries[i], now) || pool->entries[i].weight <= 0)
                continue;
            accum += pool->entries[i].weight;
            if (roll < accum) {
                if (out_index) *out_index = i;
                ((credential_pool_t *)pool)->current_index = (i + 1) % n;
                return pool->entries[i].api_key;
            }
        }
    }

    /* Round-robin search (fallback for uniform weights or empty usable after weighting) */
    for (int i = 0; i < n; i++) {
        int idx = (pool->current_index + i) % n;
        if (entry_usable(&pool->entries[idx], now)) {
            if (out_index) *out_index = idx;
            ((credential_pool_t *)pool)->current_index = (idx + 1) % n;
            return pool->entries[idx].api_key;
        }
    }

    /* No usable key — try resurrecting any cooled-off failed entry */
    for (int i = 0; i < n; i++) {
        if (pool->entries[i].status == CRED_FAILED) {
            if (out_index) *out_index = i;
            ((credential_pool_t *)pool)->current_index = (i + 1) % n;
            return pool->entries[i].api_key;
        }
    }

    return NULL; /* All keys exhausted */
}

credential_status_t credential_pool_report(credential_pool_t *pool,
                                            int entry_index,
                                            int http_status,
                                            long long tokens_used,
                                            int rate_limit_remaining,
                                            time_t rate_limit_reset) {
    if (!pool || entry_index < 0 || entry_index >= pool->entry_count)
        return CRED_FAILED;

    credential_entry_t *e = &pool->entries[entry_index];
    e->last_used = time(NULL);
    e->total_requests++;

    if (tokens_used > 0)
        e->total_tokens_used += tokens_used;

    /* Track rate-limit headers */
    if (rate_limit_remaining >= 0)
        e->rate_limit_remaining = rate_limit_remaining;
    if (rate_limit_reset > 0)
        e->rate_limit_reset = rate_limit_reset;

    /* Categorize HTTP status */
    if (http_status >= 200 && http_status < 300) {
        /* Success */
        e->consecutive_failures = 0;
        e->status = CRED_OK;
        /* Advance round-robin cursor for next call */
        pool->current_index = (entry_index + 1) % pool->entry_count;
    } else if (http_status == 429) {
        /* Rate limited */
        e->status = CRED_RATE_LIMITED;
        e->consecutive_failures++;
        if (rate_limit_reset == 0) {
            /* No reset header — assume 60s cooloff */
            e->rate_limit_reset = time(NULL) + 60;
        }
    } else if (http_status == 401 || http_status == 403) {
        /* Auth failure — key is dead */
        e->status = CRED_EXHAUSTED;
        e->consecutive_failures++;
    } else if (http_status >= 500) {
        /* Server error — increment failures */
        e->consecutive_failures++;
        if (e->consecutive_failures >= e->max_consecutive_failures) {
            e->status = CRED_FAILED;
        }
    } else if (http_status >= 400) {
        /* Client error (not auth/rate-limit) */
        e->consecutive_failures++;
        if (e->consecutive_failures >= e->max_consecutive_failures) {
            e->status = CRED_FAILED;
        }
    }

    /* Check if quota exhausted */
    if (e->quota_limit > 0 && e->total_tokens_used >= e->quota_limit) {
        e->status = CRED_EXHAUSTED;
    }

    return e->status;
}

bool credential_pool_reset(credential_pool_t *pool, int entry_index) {
    if (!pool || entry_index < 0 || entry_index >= pool->entry_count)
        return false;

    credential_entry_t *e = &pool->entries[entry_index];
    e->status = CRED_OK;
    e->consecutive_failures = 0;
    e->rate_limit_remaining = -1;
    e->rate_limit_reset = 0;
    return true;
}

char *credential_pool_stats_json(const credential_pool_t *pool) {
    if (!pool) return NULL;

    json_node_t *root = json_object();
    json_node_t *entries_arr = json_array();

    for (int i = 0; i < pool->entry_count; i++) {
        const credential_entry_t *e = &pool->entries[i];
        json_node_t *entry = json_object();

        json_set(entry, "label", json_string(e->label));
        json_set(entry, "weight", json_number(e->weight));
        json_set(entry, "status", json_string(
            e->status == CRED_OK ? "ok" :
            e->status == CRED_RATE_LIMITED ? "rate_limited" :
            e->status == CRED_FAILED ? "failed" : "exhausted"));
        json_set(entry, "consecutive_failures", json_number(e->consecutive_failures));
        json_set(entry, "total_tokens_used", json_number((double)e->total_tokens_used));
        json_set(entry, "total_requests", json_number((double)e->total_requests));

        if (e->rate_limit_remaining >= 0)
            json_set(entry, "rate_limit_remaining", json_number(e->rate_limit_remaining));
        if (e->quota_limit > 0)
            json_set(entry, "quota_limit", json_number((double)e->quota_limit));

        json_append(entries_arr, entry);
    }

    json_set(root, "provider", json_string(pool->provider_name));
    json_set(root, "entries", entries_arr);
    json_set(root, "current_index", json_number(pool->current_index));

    char *result = json_serialize_pretty(root, 2);
    json_free(root);
    return result;
}

void credential_pool_free(credential_pool_t *pool) {
    if (!pool) return;
    for (int i = 0; i < pool->entry_count; i++) {
        if (pool->entries[i].extra) {
            json_free(pool->entries[i].extra);
            pool->entries[i].extra = NULL;
        }
    }
    free(pool);
}

bool credential_pool_has_available(const credential_pool_t *pool) {
    if (!pool || pool->entry_count == 0) return false;
    time_t now = time(NULL);
    for (int i = 0; i < pool->entry_count; i++) {
        if (entry_usable(&pool->entries[i], now))
            return true;
    }
    return false;
}

int credential_pool_tick(credential_pool_t *pool) {
    if (!pool) return 0;
    time_t now = time(NULL);
    int resurrected = 0;

    for (int i = 0; i < pool->entry_count; i++) {
        credential_entry_t *e = &pool->entries[i];

        /* Resurrect rate-limited entries past their reset time */
        if (e->status == CRED_RATE_LIMITED &&
            e->rate_limit_reset > 0 &&
            now >= e->rate_limit_reset) {
            e->status = CRED_OK;
            e->consecutive_failures = 0;
            resurrected++;
        }

        /* Resurrect failed entries past cooloff */
        if (e->status == CRED_FAILED &&
            e->last_used > 0 &&
            (now - e->last_used) >= (time_t)pool->cooloff_seconds) {
            e->status = CRED_OK;
            e->consecutive_failures = 0;
            resurrected++;
        }
    }

    return resurrected;
}


/* Port of Python agent/agent_runtime_helpers.py:recover_with_credential_pool().
 * Report an HTTP result to the credential pool and return a recovery action.
 *
 * current_provider: current provider name (for cross-contamination guard,
 *   NULL or "" to skip the check).
 * Returns a cred_recover_t action for the caller. */
cred_recover_t recover_with_credential_pool(
    credential_pool_t *pool,
    const char *current_provider,
    int  http_status,
    const char *error_body,
    bool *has_retried)
{
    if (!pool)
        return CRED_RECOVER_NONE;

    if (current_provider && current_provider[0]
        && strcasecmp(current_provider, pool->provider_name) != 0)
        return CRED_RECOVER_NONE;

    int idx = pool->current_index;
    if (idx < 0 || idx >= pool->entry_count)
        return CRED_RECOVER_NONE;

    credential_entry_t *e = &pool->entries[idx];

    /* Update pool state via report */
    credential_pool_report(pool, idx, http_status, -1, -1, 0);

    /* Billing detection (402 or usage_limit_reached in body) */
    bool billing = (http_status == 402);
    if (!billing && error_body && error_body[0]) {
        const char *b = error_body;
        billing = (strstr(b, "usage_limit_reached")
                || strstr(b, "usage limit reached")
                || strstr(b, "usage limit has been reached")
                || strstr(b, "gousagelimit"));
    }
    if (billing) {
        e->status = CRED_EXHAUSTED;
        int ni;
        if (credential_pool_next_key(pool, &ni)) {
            pool->current_index = ni;
            return CRED_RECOVER_ROTATE;
        }
        return CRED_RECOVER_FALLBACK;
    }

    /* Rate-limit handling with retry counting */
    if (http_status == 429 || e->status == CRED_RATE_LIMITED) {
        if (e->status == CRED_EXHAUSTED || (has_retried && *has_retried)) {
            int ni;
            if (credential_pool_next_key(pool, &ni)) {
                pool->current_index = ni;
                return CRED_RECOVER_ROTATE;
            }
            return CRED_RECOVER_FALLBACK;
        }
        if (has_retried) *has_retried = true;
        return CRED_RECOVER_RETRY;
    }

    /* Auth failure — key is dead */
    if (http_status == 401 || http_status == 403) {
        e->status = CRED_EXHAUSTED;
        int ni;
        if (credential_pool_next_key(pool, &ni)) {
            pool->current_index = ni;
            return CRED_RECOVER_ROTATE;
        }
        return CRED_RECOVER_FALLBACK;
    }

    /* Other failures — check if pool is empty */
    if (http_status >= 400 && !credential_pool_has_available(pool))
        return CRED_RECOVER_FALLBACK;

    return CRED_RECOVER_NONE;
}


/* PoP: load_pool @ hermes_cli/proxy/adapters/xai.py:_load_pool */
/* PoP: load_pool @ agent/credential_pool.py:load_pool */
credential_pool_t *load_pool(const char *provider) {
    if (!provider || !*provider) return NULL;
    
    credential_pool_t *pool = credential_pool_create(provider);
    if (!pool) return NULL;
    
    /* Load entries from auth.json — slermes identity: credential
     * files live in the slermes root, never in the Python project's
     * ~/.hermes (which is a different project's home). */
    const char *cred_root = getenv("SLERMES_HOME");
    if (!cred_root || !cred_root[0]) cred_root = slermes_home();
    if (!cred_root || !cred_root[0]) cred_root = getenv("HOME");
    if (!cred_root || !cred_root[0]) return pool;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.slermes/auth.json", cred_root);
    
    FILE *f = fopen(path, "r");
    if (!f) return pool;
    
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(f); return pool; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    
    char *err = NULL;
    json_node_t *root = json_parse(buf, &err);
    free(buf);
    if (!root || err) {
        free(err);
        if (root) json_free(root);
        return pool;
    }
    
    json_node_t *providers = json_obj_get(root, "providers");
    if (providers && providers->type == JSON_OBJECT) {
        json_node_t *provider_obj = json_obj_get(providers, provider);
        if (provider_obj && provider_obj->type == JSON_ARRAY) {
            for (size_t i = 0; i < provider_obj->c.count && pool->entry_count < CREDENTIAL_POOL_MAX_KEYS; i++) {
                json_node_t *entry = provider_obj->c.items[i];
                if (!entry || entry->type != JSON_OBJECT) continue;
                
                json_node_t *api_key = json_obj_get(entry, "access_token");
                json_node_t *label = json_obj_get(entry, "label");
                
                const char *key_str = api_key && api_key->type == JSON_STRING ? api_key->str_val : "";
                const char *label_str = label && label->type == JSON_STRING ? label->str_val : "";
                
                if (key_str && *key_str) {
                    int idx = credential_pool_add_key(pool, key_str, label_str);
                    if (idx < 0) continue;
                    credential_entry_t *e = &pool->entries[idx];
                    json_node_t *v;
                    if ((v = json_obj_get(entry, "access_token")) && v->type == JSON_STRING)
                        snprintf(e->access_token, sizeof(e->access_token), "%s", v->str_val);
                    if ((v = json_obj_get(entry, "refresh_token")) && v->type == JSON_STRING)
                        snprintf(e->refresh_token, sizeof(e->refresh_token), "%s", v->str_val);
                    if ((v = json_obj_get(entry, "base_url")) && v->type == JSON_STRING)
                        snprintf(e->base_url, sizeof(e->base_url), "%s", v->str_val);
                    if ((v = json_obj_get(entry, "inference_base_url")) && v->type == JSON_STRING)
                        snprintf(e->inference_base_url, sizeof(e->inference_base_url), "%s", v->str_val);
                    if ((v = json_obj_get(entry, "scope")) && v->type == JSON_STRING)
                        snprintf(e->scope, sizeof(e->scope), "%s", v->str_val);
                    if ((v = json_obj_get(entry, "agent_key")) && v->type == JSON_STRING)
                        snprintf(e->agent_key, sizeof(e->agent_key), "%s", v->str_val);
                    if ((v = json_obj_get(entry, "source")) && v->type == JSON_STRING)
                        snprintf(e->source, sizeof(e->source), "%s", v->str_val);
                    if ((v = json_obj_get(entry, "expires_at_ms")) && v->type == JSON_NUMBER)
                        e->expires_at_ms = (long long)v->num_val;
                }
            }
        }
    }
    
    json_free(root);
    return pool;
}

/* Port of Python agent/credential_pool.py:select().
 * Select the next available credential from the pool using the configured strategy.
 * Returns the entry index or -1 if no usable key. */
int credential_pool_select(const credential_pool_t *pool) {
    if (!pool || pool->entry_count == 0) return -1;
    
    int idx;
    const char *key = credential_pool_next_key(pool, &idx);
    return key ? idx : -1;
}

/* Port of Python agent/credential_pool.py:peek().
 * Peek at the next credential without rotating.
 * Returns the entry index or -1 if no usable key. */
int peek(const credential_pool_t *pool) {
    if (!pool || pool->entry_count == 0) return -1;
    
    time_t now = time(NULL);
    for (int i = 0; i < pool->entry_count; i++) {
        int idx = (pool->current_index + i) % pool->entry_count;
        const credential_entry_t *e = &pool->entries[idx];
        
        /* Check if usable (CRED_OK and not rate limited) */
        if (e->status == CRED_OK) {
            if (e->rate_limit_reset > 0 && now >= e->rate_limit_reset) {
                /* Still rate limited */
                continue;
            }
            return idx;
        }
    }
    return -1;
}

/* Port of Python agent/credential_pool.py:current().
 * Get the currently selected credential entry index.
 * Returns the entry index or -1 if none selected. */
int current(const credential_pool_t *pool) {
    if (!pool || pool->entry_count == 0) return -1;
    if (pool->current_index < 0 || pool->current_index >= pool->entry_count) return -1;
    return pool->current_index;
}

/* Port of Python agent/credential_pool.py:mark_exhausted_and_rotate().
 * Mark the current entry as exhausted and rotate to the next available.
 * Returns the new current entry index or -1 if pool exhausted. */
int mark_exhausted_and_rotate(credential_pool_t *pool) {
    if (!pool || pool->entry_count == 0) return -1;
    
    int idx = pool->current_index;
    if (idx >= 0 && idx < pool->entry_count) {
        pool->entries[idx].status = CRED_EXHAUSTED;
    }
    
    int new_idx;
    const char *key = credential_pool_next_key(pool, &new_idx);
    return key ? new_idx : -1;
}

/* Port of Python agent/credential_pool.py:reset_statuses().
 * Reset all exhausted/failed/rate-limited entries back to CRED_OK.
 * Returns the number of entries reset. */
int reset_statuses(credential_pool_t *pool) {
    if (!pool) return 0;
    
    int count = 0;
    for (int i = 0; i < pool->entry_count; i++) {
        credential_entry_t *e = &pool->entries[i];
        if (e->status != CRED_OK) {
            e->status = CRED_OK;
            e->consecutive_failures = 0;
            e->rate_limit_remaining = -1;
            e->rate_limit_reset = 0;
            count++;
        }
    }
    return count;
}

/* Port of Python agent/credential_pool.py:has_available().
 * Check if the pool has any available (CRED_OK) entries.
 * Returns true if at least one entry is usable. */
bool has_available(const credential_pool_t *pool) {
    return credential_pool_has_available(pool);
}

/* Port of Python agent/credential_pool.py:add_entry().
 * Add a new credential entry to the pool.
 * Returns the entry index or -1 on failure. */
int add_entry(credential_pool_t *pool, const char *api_key, const char *label, const char *source) {
    if (!pool || !api_key) return -1;

    int idx = credential_pool_add_key(pool, api_key, label);
    if (idx >= 0 && source && source[0]) {
        /* Store provenance of the key in entry metadata. */
        snprintf(pool->entries[idx].source, sizeof(pool->entries[idx].source),
                 "%s", source);
    }
    return idx;
}

/* Port of Python agent/credential_pool.py:_upsert_entry().
 * Upsert an entry by source - update existing or add new.
 * Returns true if the pool was modified. */
bool _upsert_entry(credential_pool_t *pool, const char *source, const char *api_key, const char *label) {
    if (!pool || !source || !api_key) return false;

    /* Find an existing entry with the same provenance (source). */
    for (int i = 0; i < pool->entry_count; i++) {
        if (pool->entries[i].source[0] && strcmp(pool->entries[i].source, source) == 0) {
            /* Update in place — keep usage/status history intact. */
            snprintf(pool->entries[i].api_key, sizeof(pool->entries[i].api_key),
                     "%s", api_key);
            if (label && label[0])
                snprintf(pool->entries[i].label, sizeof(pool->entries[i].label),
                         "%s", label);
            return true;
        }
    }

    /* No match — add as a new entry. */
    int idx = credential_pool_add_key(pool, api_key, label);
    if (idx >= 0) {
        snprintf(pool->entries[idx].source, sizeof(pool->entries[idx].source),
                 "%s", source);
    }
    return idx >= 0;
}

/* Port of Python agent/credential_pool.py:get_pool_strategy().
 * Get the configured pool selection strategy for a provider.
 * Returns strategy string (e.g., "fill_first", "round_robin", "random", "least_used"). */
const char *get_pool_strategy(const char *provider) {
    (void)provider;
    /* The C config schema (hermes_core_types.h) exposes per-platform config
     * but no per-provider credential-pool strategy key, so the documented
     * default is returned. The Python original reads this from config.yaml;
     * wiring a new yaml key is out of scope until the schema gains one. */
    static const char *default_strategy = "fill_first";
    return default_strategy;
}


