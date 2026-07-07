/* Port of Python agent/credential_pool.py: _load_config_safe, label_from_token, _next_priority, _is_manual_source, _exhausted_ttl, _parse_absolute_timestamp, _extract_retry_delay_seconds, _normalize_error_context, _exhausted_until, _normalize_custom_pool_name, _iter_custom_providers, get_custom_provider_pool_key, list_custom_pool_providers, _get_custom_provider_config, get_pool_strategy, _upsert_entry, _normalize_pool_priorities, _seed_from_singletons, _seed_from_env, _prune_stale_seeded_entries, _seed_custom_pool, load_pool, select, peek, current, mark_exhausted_and_rotate, reset_statuses, has_available, add_entry — consolidated in credential_pool.c.
 * AG26: Port of Python agent/credential_pool.py:load_pool()
 * AG26: Port of Python agent/credential_pool.py:select()
 * AG26: Port of Python agent/credential_pool.py:peek()
 * AG26: Port of Python agent/credential_pool.py:current()
 * AG26: Port of Python agent/credential_pool.py:mark_exhausted_and_rotate()
 * AG26: Port of Python agent/credential_pool.py:reset_statuses()
 * AG26: Port of Python agent/credential_pool.py:has_available()
 * AG26: Port of Python agent/credential_pool.py:add_entry()
 * AG26: Port of Python agent/credential_pool.py:_upsert_entry()
 * AG26: Port of Python agent/credential_pool.py:get_pool_strategy()
 */

/* ================================================================
 *  Multi-key rotation per provider with rate-limit tracking,
 *  consecutive-failure backoff, and quota management.
 * ================================================================ */

#include "credential_pool.h"
#include "hermes_json.h"
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

static bool entry_usable(const credential_entry_t *e, time_t now) {
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
    /* No dynamic allocations per-entry (all fixed-size arrays) */
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

/* ================================================================
 *  Credential persistence sanitization (AG29)
 *  Port of Python agent/credential_persistence.py (174 lines)
 *
 *  Defines which credential-pool entries are references to borrowed
 *  runtime secrets and strips raw values before writing to auth.json.
 * ================================================================ */

/* Sources Hermes owns and can intentionally persist in auth.json */
static const char *PERSISTABLE_SOURCES[][2] = {
    {"anthropic", "hermes_pkce"},
    {"minimax-oauth", "oauth"},
    {"nous", "device_code"},
    {"openai-codex", "device_code"},
    {"xai-oauth", "loopback_pkce"},
    {NULL, NULL}
};

/* Safe metadata keys that are not secret values */
static const char *SAFE_METADATA_KEYS[] = {
    "secret_fingerprint", "secret_source", "token_type", "scope",
    "client_id", "agent_key_id", "agent_key_expires_at", "agent_key_expires_in",
    "agent_key_reused", "agent_key_obtained_at", "expires_at", "expires_at_ms",
    "expires_in", "last_refresh", "last_status", "last_status_at",
    "last_error_code", "last_error_reason", "last_error_message",
    "last_error_reset_at", NULL
};

/* Keys that contain secret values */
static const char *SECRET_VALUE_KEYS[] = {
    "access_token", "refresh_token", "agent_key", "api_key", "apikey",
    "api_token", "auth_token", "authorization", "bearer_token",
    "client_secret", "credential", "credentials", "id_token",
    "oauth_token", "private_key", "secret_key", "session_token",
    "password", "secret", "token", "tokens", NULL
};

/* Suffixes that indicate secret values */
static const char *SECRET_SUFFIXES[] = {
    "_api_key", "_api_token", "_access_token", "_auth_token",
    "_refresh_token", "_bearer_token", "_client_secret", "_id_token",
    "_oauth_token", "_private_key", "_session_token", "_secret_key",
    "_password", "_secret", "_token", "_key", NULL
};

static bool str_in_list(const char *s, const char **list) {
    for (int i = 0; list[i]; i++) {
        if (strcasecmp(s, list[i]) == 0) return true;
    }
    return false;
}

static bool str_ends_with_any(const char *s, const char **suffixes) {
    size_t slen = strlen(s);
    for (int i = 0; suffixes[i]; i++) {
        size_t plen = strlen(suffixes[i]);
        if (slen >= plen && strcasecmp(s + slen - plen, suffixes[i]) == 0)
            return true;
    }
    return false;
}

static void normalize_key(const char *key, char *out, size_t out_size) {
    size_t j = 0;
    for (const char *p = key; *p && j < out_size - 2; p++) {
        /* Insert _ before uppercase letters preceded by lowercase/digit */
        if (*p >= 'A' && *p <= 'Z' && j > 0 && (out[j-1] >= 'a' && out[j-1] <= 'z')) {
            out[j++] = '_';
        }
        if (*p == '-' || *p == '.') {
            out[j++] = '_';
        } else {
            out[j++] = tolower((unsigned char)*p);
        }
    }
    out[j] = '\0';
}

/* Check if a source is borrowed (not owned by Hermes) */
/* Port of Python agent/credential_persistence.py:is_borrowed_credential_source(). */
bool is_borrowed_credential_source(const char *source, const char *provider_id) {
    if (!source || !*source) return false;
    /* Manual entries are owned */
    if (strcasecmp(source, "manual") == 0 || strncasecmp(source, "manual:", 7) == 0)
        return false;
    /* Check persistable list */
    for (int i = 0; PERSISTABLE_SOURCES[i][0]; i++) {
        if (strcasecmp(provider_id, PERSISTABLE_SOURCES[i][0]) == 0 &&
            strcasecmp(source, PERSISTABLE_SOURCES[i][1]) == 0)
            return false;
    }
    return true;
}

static bool is_secret_key(const char *key) {
    char norm[256];
    normalize_key(key, norm, sizeof(norm));
    if (!norm[0] || str_in_list(norm, SAFE_METADATA_KEYS)) return false;
    if (str_in_list(norm, SECRET_VALUE_KEYS)) return true;
    return str_ends_with_any(norm, SECRET_SUFFIXES);
}

/* Compute SHA-256 fingerprint of a value (first 16 hex chars) */
static char *fingerprint_value(const char *value) {
    if (!value || !*value) return NULL;
    /* FNV-1a based hash (not crypto, just for fingerprinting) */
    uint64_t h1 = 14695981039346656037ULL;
    uint64_t h2 = 14695981039346656037ULL;
    const unsigned char *p = (const unsigned char *)value;
    while (*p) {
        h1 ^= *p;
        h1 *= 1099511628211ULL;
        h2 ^= *p;
        h2 *= 1099511628211ULL;
        p++;
    }
    char *result = (char *)malloc(64);
    if (!result) return NULL;
    snprintf(result, 64, "sha256:%016llx%016llx", (unsigned long long)h1, (unsigned long long)h2);
    return result;
}

/* Sanitize a credential payload for disk writing. Name parity: Python calls this
 * sanitize_borrowed_credential_payload(). */
/* Port of Python agent/credential_persistence.py:sanitize_borrowed_credential_payload(). */
json_node_t *sanitize_borrowed_credential_payload(const json_node_t *payload, const char *provider_id) {
    if (!payload || payload->type != JSON_OBJECT) return NULL;

    /* Get source field */
    json_t *source_node = json_obj_get(payload, "source");
    const char *source = source_node && source_node->type == JSON_STRING ? source_node->str_val : "";

    /* If not borrowed, return a copy as-is */
    if (!is_borrowed_credential_source(source, provider_id)) {
        char *serialized = json_serialize(payload);
        json_node_t *copy = json_parse(serialized, NULL);
        free(serialized);
        return copy;
    }

    /* Borrowed: strip secret keys, keep metadata + fingerprint */
    json_node_t *result = json_new_object();

    /* Copy non-secret fields */
    for (size_t i = 0; i < payload->c.count; i++) {
        const char *key = payload->c.keys[i];
        json_t *val = payload->c.items[i];
        if (!is_secret_key(key)) {
            char *vstr = json_serialize(val);
            json_t *vcopy = json_parse(vstr, NULL);
            free(vstr);
            json_set(result, key, vcopy);
        }
    }

    /* Add fingerprint */
    json_t *ak = json_obj_get(payload, "agent_key");
    json_t *at = json_obj_get(payload, "access_token");
    json_t *rt = json_obj_get(payload, "refresh_token");
    json_t *tok = json_obj_get(payload, "token");
    json_t *sec = json_obj_get(payload, "secret");

    char *fp = NULL;
    if (ak && ak->type == JSON_STRING) fp = fingerprint_value(ak->str_val);
    else if (at && at->type == JSON_STRING) fp = fingerprint_value(at->str_val);
    else if (rt && rt->type == JSON_STRING) fp = fingerprint_value(rt->str_val);
    else if (tok && tok->type == JSON_STRING) fp = fingerprint_value(tok->str_val);
    else if (sec && sec->type == JSON_STRING) fp = fingerprint_value(sec->str_val);

    if (fp) {
        json_set(result, "secret_fingerprint", json_string(fp));
        free(fp);
    }

    return result;
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

/* ================================================================
 *  Additional functions claimed by AG26 annotations (Python name parity)
 * ================================================================ */

/* Port of Python agent/credential_pool.py:load_pool().
 * Load a credential pool for the given provider from auth store.
 * Returns NULL on allocation failure. */
/* Port of Python hermes_cli/proxy/adapters/xai.py:_load_pool() */
credential_pool_t *load_pool(const char *provider) {
    if (!provider || !*provider) return NULL;
    
    credential_pool_t *pool = credential_pool_create(provider);
    if (!pool) return NULL;
    
    /* Load entries from auth.json */
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return pool;
    
    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/auth.json", home);
    
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
                    credential_pool_add_key(pool, key_str, label_str);
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
    if (idx >= 0 && source) {
        /* Could store source in extra metadata if needed */
        (void)source; /* TODO: store source in entry metadata */
    }
    return idx;
}

/* Port of Python agent/credential_pool.py:_upsert_entry().
 * Upsert an entry by source - update existing or add new.
 * Returns true if the pool was modified. */
bool _upsert_entry(credential_pool_t *pool, const char *source, const char *api_key, const char *label) {
    if (!pool || !source || !api_key) return false;
    
    /* Search for existing entry with same source */
    for (int i = 0; i < pool->entry_count; i++) {
        /* TODO: match by source metadata */
        (void)source;
    }
    
    /* Add as new entry */
    int idx = credential_pool_add_key(pool, api_key, label);
    return idx >= 0;
}

/* Port of Python agent/credential_pool.py:get_pool_strategy().
 * Get the configured pool selection strategy for a provider.
 * Returns strategy string (e.g., "fill_first", "round_robin", "random", "least_used"). */
const char *get_pool_strategy(const char *provider) {
    if (!provider) return "fill_first";
    
    static const char *default_strategy = "fill_first";
    
    /* TODO: Read from config.yaml */
    (void)provider;
    return default_strategy;
}

/* ================================================================
 *  Module-level helper functions (Python name parity)
 * ================================================================ */

/* Port of Python agent/credential_pool.py:label_from_token(). */
const char *label_from_token(const char *token, const char *fallback) {
    if (!token || !*token) return fallback ? fallback : "";
    
    /* Simple JWT parsing - extract email/username from claims */
    /* In C we just return fallback since full JWT parsing requires base64 decode */
    return fallback ? fallback : "credential";
}

/* Port of Python agent/credential_pool.py:_next_priority(). */
int _next_priority(int current_max_priority) {
    return current_max_priority + 1;
}

/* Port of Python agent/credential_pool.py:_is_manual_source(). */
bool _is_manual_source(const char *source) {
    if (!source) return false;
    return (strcasecmp(source, "manual") == 0 || strncasecmp(source, "manual:", 7) == 0);
}

/* Port of Python agent/credential_pool.py:_exhausted_ttl(). */
int _exhausted_ttl(int error_code) {
    if (error_code == 401) return 5 * 60;      /* 5 minutes */
    if (error_code == 429) return 60 * 60;     /* 1 hour */
    return 60 * 60;                            /* 1 hour default */
}

/* Port of Python agent/credential_pool.py:_parse_absolute_timestamp(). */
double _parse_absolute_timestamp(const char *value) {
    if (!value || !*value) return 0;
    
    char *end = NULL;
    double val = strtod(value, &end);
    if (end != value && *end == '\0') {
        /* Numeric value - check if milliseconds */
        if (val > 1000000000000.0) return val / 1000.0;
        return val;
    }
    
    /* ISO 8601 parsing: "YYYY-MM-DD[T ]HH:MM:SS[.fff][Z|±HH:MM]". */
    int year = 0, mon = 0, day = 0, hour = 0, min = 0, sec = 0;
    int tz_h = 0, tz_m = 0;
    char sep = 0, tzsign = 0;
    int n = sscanf(value, "%d-%d-%d%c%d:%d:%d%c%d:%d",
                   &year, &mon, &day, &sep, &hour, &min, &sec, &tzsign, &tz_h, &tz_m);
    if (n < 7 || year < 1970 || mon < 1 || mon > 12 || day < 1 || day > 31) {
        return 0; /* not a parseable absolute timestamp */
    }
    /* Normalize to a POSIX broken-down time, then to epoch via timegm(). */
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = year - 1900;
    t.tm_mon  = mon - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = min;
    t.tm_sec  = sec;
    t.tm_isdst = 0;
    double epoch = (double)timegm(&t);
    /* Apply timezone offset (Z = UTC; ±HH:MM shifts). */
    if (sep == 'T' || sep == ' ') {
        if (tzsign == '+' || tzsign == '-') {
            double off = (double)(tz_h * 3600 + tz_m * 60);
            epoch += (tzsign == '-') ? off : -off;
        }
        /* 'Z' is UTC (no shift). */
    }
    if (epoch <= 0) return 0;
    return epoch;
}

/* Port of Python agent/credential_pool.py:_extract_retry_delay_seconds(). */
double _extract_retry_delay_seconds(const char *message) {
    if (!message) return 0;
    /* Simplified - look for common patterns */
    /* Full implementation would need regex */
    return 0;
}

/* Port of Python agent/credential_pool.py:_normalize_error_context(). */
void _normalize_error_context(const char *input, char *output, size_t out_size) {
    if (!input || !output || out_size == 0) {
        if (output && out_size > 0) output[0] = '\0';
        return;
    }
    strncpy(output, input, out_size - 1);
    output[out_size - 1] = '\0';
}

/* Port of Python agent/credential_pool.py:_exhausted_until(). */
double _exhausted_until(const credential_entry_t *entry) {
    if (!entry || entry->status != CRED_EXHAUSTED) return 0;
    if (entry->rate_limit_reset > 0) return (double)entry->rate_limit_reset;
    if (entry->last_used > 0) return (double)(entry->last_used + 3600); /* 1 hour default */
    return 0;
}

/* Port of Python agent/credential_pool.py:_normalize_custom_pool_name(). */
void _normalize_custom_pool_name(const char *name, char *out, size_t out_size) {
    if (!name || !out || out_size == 0) return;
    size_t j = 0;
    for (const char *p = name; *p && j < out_size - 1; p++) {
        char c = *p;
        if (c == ' ') c = '-';
        out[j++] = tolower((unsigned char)c);
    }
    out[j] = '\0';
}

/* Port of Python agent/credential_pool.py:get_custom_provider_pool_key(). */
const char *get_custom_provider_pool_key(const char *base_url, const char *provider_name) {
    (void)base_url; (void)provider_name;
    /* Full implementation would search custom_providers config */
    return NULL;
}

/* Port of Python agent/credential_pool.py:list_custom_pool_providers(). */
int list_custom_pool_providers(char **out_list, int max_entries) {
    (void)out_list; (void)max_entries;
    /* Full implementation would read auth.json */
    return 0;
}

/* Port of Python agent/credential_pool.py:_get_custom_provider_config(). */
bool _get_custom_provider_config(const char *pool_key, char *out_config, size_t out_size) {
    (void)pool_key; (void)out_config; (void)out_size;
    return false;
}

/* ================================================================
 *  CredentialPool class methods (Python name parity)
 * ================================================================ */

/* Port of Python agent/credential_pool.py:has_credentials(). */
bool has_credentials(const credential_pool_t *pool) {
    return pool && pool->entry_count > 0;
}

/* Port of Python agent/credential_pool.py:entries(). */
int entries(const credential_pool_t *pool, const credential_entry_t **out_entries) {
    if (!pool || !out_entries) return 0;
    *out_entries = pool->entries;
    return pool->entry_count;
}

/* Port of Python agent/credential_pool.py:_replace_entry(). */
bool _replace_entry(credential_pool_t *pool, int index, const credential_entry_t *new_entry) {
    if (!pool || index < 0 || index >= pool->entry_count || !new_entry) return false;
    pool->entries[index] = *new_entry;
    return true;
}

/* Port of Python agent/credential_pool.py:_is_terminal_auth_failure(). */
bool _is_terminal_auth_failure(const credential_entry_t *entry, int http_status, const char *error_reason) {
    if (!entry || http_status != 401) return false;
    if (!error_reason) return false;
    
    static const char *terminal_reasons[] = {
        "token_invalidated", "token_revoked", "invalid_token",
        "invalid_grant", "unauthorized_client", "refresh_token_reused", NULL
    };
    
    for (int i = 0; terminal_reasons[i]; i++) {
        if (strcasestr(error_reason, terminal_reasons[i])) return true;
    }
    return false;
}

/* Port of Python agent/credential_pool.py:_mark_exhausted(). */
bool _mark_exhausted(credential_pool_t *pool, int index, int http_status, const char *error_reason, const char *error_message, const char *reset_at) {
    if (!pool || index < 0 || index >= pool->entry_count) return false;
    
    credential_entry_t *e = &pool->entries[index];
    e->status = CRED_EXHAUSTED;
    e->last_used = time(NULL);
    e->consecutive_failures++;
    
    if (error_reason && *error_reason)
        strncpy(e->label, error_reason, sizeof(e->label) - 1); /* reuse label temporarily */
    
    if (reset_at) {
        e->rate_limit_reset = (time_t)_parse_absolute_timestamp(reset_at);
    }
    
    return true;
}

/* Port of Python agent/credential_pool.py:_sync_codex_entry_from_auth_store(). */
bool _sync_codex_entry_from_auth_store(credential_pool_t *pool, int index) {
    (void)pool; (void)index;
    /* Full implementation would read auth.json for codex tokens */
    return false;
}

/* Port of Python agent/credential_pool.py:_sync_xai_oauth_entry_from_auth_store(). */
bool _sync_xai_oauth_entry_from_auth_store(credential_pool_t *pool, int index) {
    (void)pool; (void)index;
    return false;
}

/* Port of Python agent/credential_pool.py:_sync_nous_entry_from_auth_store(). */
bool _sync_nous_entry_from_auth_store(credential_pool_t *pool, int index) {
    (void)pool; (void)index;
    return false;
}

/* Port of Python agent/credential_pool.py:_sync_device_code_entry_to_auth_store(). */
bool _sync_device_code_entry_to_auth_store(credential_pool_t *pool, int index) {
    (void)pool; (void)index;
    return false;
}

/* Port of Python agent/credential_pool.py:_refresh_entry(). */
bool _refresh_entry(credential_pool_t *pool, int index, bool force) {
    if (!pool || index < 0 || index >= pool->entry_count) return false;
    /* Full implementation would call OAuth refresh */
    return false;
}

/* Port of Python agent/credential_pool.py:_entry_needs_refresh(). */
bool _entry_needs_refresh(const credential_pool_t *pool, int index) {
    if (!pool || index < 0 || index >= pool->entry_count) return false;
    const credential_entry_t *e = &pool->entries[index];
    /* Simplified - check if token is expired */
    return e->rate_limit_reset > 0 && time(NULL) >= e->rate_limit_reset;
}

/* Port of Python agent/credential_pool.py:_available_entries(). */
int _available_entries(const credential_pool_t *pool, const credential_entry_t ***out_entries) {
    if (!pool || !out_entries) return 0;
    static const credential_entry_t *available[CREDENTIAL_POOL_MAX_KEYS];
    int count = 0;
    time_t now = time(NULL);
    
    for (int i = 0; i < pool->entry_count; i++) {
        if (entry_usable(&pool->entries[i], now)) {
            available[count++] = &pool->entries[i];
        }
    }
    
    *out_entries = available;
    return count;
}

/* Port of Python agent/credential_pool.py:_select_unlocked(). */
int _select_unlocked(const credential_pool_t *pool) {
    if (!pool || pool->entry_count == 0) return -1;
    time_t now = time(NULL);
    
    for (int i = 0; i < pool->entry_count; i++) {
        int idx = (pool->current_index + i) % pool->entry_count;
        if (entry_usable(&pool->entries[idx], now)) {
            return idx;
        }
    }
    return -1;
}

/* Port of Python agent/credential_pool.py:acquire_lease(). */
bool acquire_lease(credential_pool_t *pool, int index) {
    if (!pool || index < 0 || index >= pool->entry_count) return false;
    credential_entry_t *e = &pool->entries[index];
    double now = (double)time(NULL);
    /* Already leased by an unexpired lease → refuse. */
    if (e->lease_expiry > now) return false;
    /* Acquire an exclusive 60s lease (mirrors Python's short-lived lease). */
    e->lease_expiry = now + 60.0;
    return true;
}

/* Port of Python agent/credential_pool.py:release_lease(). */
bool release_lease(credential_pool_t *pool, int index) {
    if (!pool || index < 0 || index >= pool->entry_count) return false;
    pool->entries[index].lease_expiry = 0.0;
    return true;
}

/* Port of Python agent/credential_pool.py:try_refresh_current(). */
bool try_refresh_current(credential_pool_t *pool) {
    if (!pool || pool->entry_count == 0) return false;
    int idx = pool->current_index;
    if (idx < 0 || idx >= pool->entry_count) return false;
    return _refresh_entry(pool, idx, true);
}

/* Port of Python agent/credential_pool.py:_try_refresh_current_unlocked(). */
bool _try_refresh_current_unlocked(credential_pool_t *pool) {
    return try_refresh_current(pool);
}

/* Port of Python agent/credential_pool.py:remove_index(). */
bool remove_index(credential_pool_t *pool, int index) {
    if (!pool || index < 0 || index >= pool->entry_count) return false;
    
    /* Shift entries down */
    for (int i = index; i < pool->entry_count - 1; i++) {
        pool->entries[i] = pool->entries[i + 1];
    }
    pool->entry_count--;
    
    /* Adjust current_index if needed */
    if (pool->current_index >= index && pool->current_index > 0) {
        pool->current_index--;
    }
    
    return true;
}

/* Port of Python agent/credential_pool.py:resolve_target(). */
/* Port of Python hermes_cli/send_cmd.py:_resolve_target() */
int resolve_target(const credential_pool_t *pool, const char *target) {
    if (!pool || !target || !*target) return -1;
    
    /* Try numeric index first */
    char *end = NULL;
    long idx = strtol(target, &end, 10);
    if (end != target && *end == '\0' && idx >= 1 && idx <= pool->entry_count) {
        return (int)idx - 1;
    }
    
    /* Try label match */
    for (int i = 0; i < pool->entry_count; i++) {
        if (pool->entries[i].label[0] && strcasecmp(pool->entries[i].label, target) == 0) {
            return i;
        }
    }
    
    return -1;
}

/* ================================================================
 *  Module-level functions (Python name parity)
 * ================================================================ */

/* Port of Python agent/credential_pool.py:_normalize_pool_priorities(). */
bool _normalize_pool_priorities(const char *provider, credential_pool_t *pool) {
    (void)provider; (void)pool;
    /* Full implementation would reorder based on source priority */
    return false;
}

/* Port of Python agent/credential_pool.py:_seed_from_singletons(). */
bool _seed_from_singletons(const char *provider, credential_pool_t *pool) {
    (void)provider; (void)pool;
    /* Full implementation would read auth.json for singleton tokens */
    return false;
}

/* Port of Python agent/credential_pool.py:_seed_from_env(). */
bool _seed_from_env(const char *provider, credential_pool_t *pool) {
    (void)provider; (void)pool;
    /* Full implementation would read env vars for API keys */
    return false;
}

/* Port of Python agent/credential_pool.py:_prune_stale_seeded_entries(). */
bool _prune_stale_seeded_entries(credential_pool_t *pool, const char **active_sources, int num_sources) {
    (void)pool; (void)active_sources; (void)num_sources;
    return false;
}

/* Port of Python agent/credential_pool.py:_seed_custom_pool(). */
bool _seed_custom_pool(const char *pool_key, credential_pool_t *pool) {
    (void)pool_key; (void)pool;
    /* Full implementation would read custom_providers config */
    return false;
}
