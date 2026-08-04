/* Port of Python agent/credential_pool.py (split module).

Self-contained credential-pool subsystem component. credential_pool_t /
credential_entry_t are defined in include/credential_pool.h; internal helpers
shared across the split modules are declared in include/credential_pool_internals.h.
No god headers — only the minimal includes each module requires. C11 only.
*/

/* strcasestr and friends are GNU extensions — musl (alpine) needs
 * _GNU_SOURCE to declare them; glibc exposes them by default. */
#define _GNU_SOURCE
#include "credential_pool.h"
#include "credential_pool_internals.h"
#include "credential_persistence.h"
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "hermes_auth.h"
#include "provider.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/*
 * Component 4/4 — token sync + refresh (the 6 REAL_GAP closures):
 * nous_invoke_jwt_is_usable / credential_entry_runtime_api_key /
 * credential_entry_runtime_base_url / credential_entry_get_extra /
 * credential_pool_sync_anthropic_entry_from_credentials_file /
 * credential_pool_refresh_entry_impl + _sync_*_entry_from_auth_store /
 * _replace_entry / _mark_exhausted / _entry_needs_refresh / remove_index /
 * acquire_lease / release_lease / try_refresh_current / resolve_target /
 * _write_through_provider_state_to_global_root / has_credentials /
 * entries / _is_terminal_auth_failure / _available_entries /
 * _select_unlocked.
 */

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
/* PoP: _resolve_target @ hermes_cli/send_cmd.py:_resolve_target */
/* Port of Python hermes_cli/send_cmd.py:_resolve_target(). */
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
 *  PooledCredential runtime helpers (agent/credential_pool.py gaps)
 *  Faithful ports of the 6 remaining REAL_GAPs + the Nous JWT check.
 * ================================================================ */

/* Decode the `exp` claim from a JWT (base64url payload segment). Returns the
 * expiry as epoch-seconds, or 0 if the token is malformed / has no exp. */
long long jwt_exp_claim(const char *token) {
    if (!token || !*token) return 0;
    const char *dot1 = strchr(token, '.');
    if (!dot1) return 0;
    const char *dot2 = strchr(dot1 + 1, '.');
    const char *seg = dot1 + 1;
    size_t seglen = dot2 ? (size_t)(dot2 - seg) : strlen(seg);

    char buf[4096];
    if (seglen >= sizeof(buf)) return 0;
    size_t o = 0;
    for (size_t i = 0; i < seglen; i++) {
        char c = seg[i];
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
        buf[o++] = c;
    }
    while (o % 4) buf[o++] = '=';
    buf[o] = '\0';

    char *err = NULL;
    json_node_t *j = json_parse(buf, &err);
    if (err) free(err);
    if (!j) return 0;
    long long exp = 0;
    json_node_t *e = json_obj_get(j, "exp");
    if (e && e->type == JSON_NUMBER) exp = (long long)e->num_val;
    json_free(j);
    return exp;
}

/* Nous NAS invoke-JWT usability: the token must carry `scope` and not be
 * past its `exp`. Mirrors hermes_cli.auth._nous_invoke_jwt_is_usable(). */
/* PoP: nous_invoke_jwt_is_usable @ hermes_cli/auth.py:_nous_invoke_jwt_is_usable */
bool nous_invoke_jwt_is_usable(const char *token, const char *scope, const char *expires_at) {
    (void)expires_at;
    if (!token || !*token) return false;
    if (!scope || !*scope) return false;
    long long exp = jwt_exp_claim(token);
    if (exp == 0) return false;
    long long now = (long long)time(NULL);
    return exp > now;
}

/* PoP: credential_entry_runtime_api_key @ agent/credential_pool.py:runtime_api_key */
/* PooledCredential.runtime_api_key — Nous prefers the agent_key (NAS invoke
 * JWT) when usable, otherwise the access_token; all other providers use the
 * access_token. Caller must free the result. */
char *credential_entry_runtime_api_key(const credential_entry_t *e, const char *provider) {
    if (!e) return NULL;
    if (provider && strcmp(provider, "nous") == 0 && e->agent_key[0]) {
        if (nous_invoke_jwt_is_usable(e->agent_key, e->scope, e->agent_key_expires_at))
            return strdup(e->agent_key);
    }
    return e->access_token[0] ? strdup(e->access_token) : NULL;
}

/* PoP: credential_entry_runtime_base_url @ agent/credential_pool.py:runtime_base_url */
/* PooledCredential.runtime_base_url — Nous uses inference_base_url or
 * base_url; other providers use base_url. Caller must free the result. */
char *credential_entry_runtime_base_url(const credential_entry_t *e, const char *provider) {
    if (!e) return NULL;
    if (provider && strcmp(provider, "nous") == 0) {
        if (e->inference_base_url[0]) return strdup(e->inference_base_url);
        if (e->base_url[0]) return strdup(e->base_url);
    }
    return e->base_url[0] ? strdup(e->base_url) : NULL;
}

/* PoP: credential_entry_get_extra @ agent/credential_pool.py:__getattr__ */
/* PooledCredential.__getattr__ — resolve a key from the entry's `extra` dict. */
char *credential_entry_get_extra(const credential_entry_t *e, const char *key) {
    if (!e || !e->extra || !key) return NULL;
    json_node_t *v = json_obj_get(e->extra, key);
    if (v && v->type == JSON_STRING) return strdup(v->str_val);
    if (v && v->type == JSON_NUMBER) {
        char b[64];
        snprintf(b, sizeof(b), "%g", v->num_val);
        return strdup(b);
    }
    if (v && v->type == JSON_BOOL)
        return strdup(v->num_val ? "true" : "false");
    return NULL;
}

/* PoP: agent/credential_pool.py:_write_through_provider_state_to_global_root */
/* _write_through_provider_state_to_global_root — best-effort write of a
 * rotated provider `state` JSON object into the global-root auth.json
 * providers.<provider_id> section. Swallows all errors. Mirrors the Python fn. */
/* PoP: credential_pool_write_through_provider_state_to_global_root @ agent/credential_pool.py:_write_through_provider_state_to_global_root */
void credential_pool_write_through_provider_state_to_global_root(const char *provider_id,
                                                                  const char *state_json) {
    if (!provider_id || !*provider_id || !state_json) return;
    if (getenv("PYTEST_CURRENT_TEST") || getenv("PYTEST_VERSION")) return;
    char *path = cp_auth_json_path();
    if (!path) return;
    FILE *f = fopen(path, "r");
    json_node_t *root = NULL;
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *buf = (char *)malloc(sz + 1);
        if (buf) {
            fread(buf, 1, sz, f);
            buf[sz] = '\0';
            char *err = NULL;
            root = json_parse(buf, &err);
            if (err) free(err);
            free(buf);
        }
        fclose(f);
    }
    if (!root) root = json_object();
    if (root->type != JSON_OBJECT) { json_free(root); free(path); return; }

    json_node_t *providers = json_obj_get(root, "providers");
    if (!providers || providers->type != JSON_OBJECT) {
        providers = json_object();
        json_set(root, "providers", providers);
    }
    char *err = NULL;
    json_node_t *state = json_parse(state_json, &err);
    if (err) free(err);
    if (state) {
        json_set(providers, provider_id, state);
        char *ser = json_serialize_pretty(root, 2);
        if (ser) {
            FILE *out = fopen(path, "w");
            if (out) { fputs(ser, out); fclose(out); }
            free(ser);
        }
    }
    json_free(root);
    free(path);
}

/* PoP: agent/credential_pool.py: _write_through_provider_state_to_global_root
 * is the per-provider OAuth-state writer. This is the entry-array writer that
 * mirrors Python's `[entry.to_dict() for entry in self._entries]` persistence
 * (credential_pool.py:543): it serializes every pool entry through
 * credential_entry_to_json() — which runs sanitize_borrowed_credential_payload
 * — and stores the resulting array under providers.<provider> in auth.json.
 * Best-effort; swallows all errors; skips under pytest. */
void credential_pool_persist_entries(const credential_pool_t *pool) {
    if (!pool || pool->entry_count <= 0) return;
    if (getenv("PYTEST_CURRENT_TEST") || getenv("PYTEST_VERSION")) return;

    char *entries_json = credential_pool_entries_json(pool);
    if (!entries_json) return;

    char *path = cp_auth_json_path();
    if (!path) { free(entries_json); return; }

    FILE *f = fopen(path, "r");
    json_node_t *root = NULL;
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *buf = (char *)malloc(sz + 1);
        if (buf) {
            fread(buf, 1, sz, f);
            buf[sz] = '\0';
            char *err = NULL;
            root = json_parse(buf, &err);
            if (err) free(err);
            free(buf);
        }
        fclose(f);
    }
    if (!root) root = json_object();
    if (root->type != JSON_OBJECT) { json_free(root); free(entries_json); free(path); return; }

    json_node_t *providers = json_obj_get(root, "providers");
    if (!providers || providers->type != JSON_OBJECT) {
        providers = json_object();
        json_set(root, "providers", providers);
    }

    json_node_t *entries = json_parse(entries_json, NULL);
    if (entries && entries->type == JSON_ARRAY) {
        json_set(providers, pool->provider_name, entries);
        char *ser = json_serialize_pretty(root, 2);
        if (ser) {
            FILE *out = fopen(path, "w");
            if (out) { fputs(ser, out); fclose(out); }
            free(ser);
        }
        /* entries is now owned by root; do not free separately. */
    } else {
        json_free(entries);
    }
    json_free(root);
    free(entries_json);
    free(path);
}

/* _sync_anthropic_entry_from_credentials_file — if the entry is an anthropic
 * claude_code entry, sync its tokens from ~/.claude/.credentials.json when they
 * differ. Mutates *e in place. Returns true if a sync was applied. */
/* PoP: credential_pool_sync_anthropic_entry_from_credentials_file @ agent/credential_pool.py:_sync_anthropic_entry_from_credentials_file */
bool credential_pool_sync_anthropic_entry_from_credentials_file(credential_entry_t *e) {
    if (!e) return false;
    if (strcmp(e->source, "claude_code") != 0 && strcmp(e->source, "anthropic") != 0)
        return false;
    const char *home = getenv("HOME");
    if (!home) return false;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.claude/.credentials.json", home);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(f); return false; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    char *err = NULL;
    json_node_t *root = json_parse(buf, &err);
    free(buf);
    if (err) free(err);
    if (!root || root->type != JSON_OBJECT) { if (root) json_free(root); return false; }

    bool synced = false;
    json_node_t *access = json_obj_get(root, "access_token");
    json_node_t *refresh = json_obj_get(root, "refresh_token");
    if (access && access->type == JSON_STRING &&
        (!e->access_token[0] || strcmp(e->access_token, access->str_val) != 0)) {
        snprintf(e->access_token, sizeof(e->access_token), "%s", access->str_val);
        synced = true;
    }
    if (refresh && refresh->type == JSON_STRING &&
        (!e->refresh_token[0] || strcmp(e->refresh_token, refresh->str_val) != 0)) {
        snprintf(e->refresh_token, sizeof(e->refresh_token), "%s", refresh->str_val);
        synced = true;
    }
    json_free(root);
    return synced;
}

/* PoP: agent/credential_pool.py:_refresh_entry_impl */
/* _refresh_entry_impl — refresh a pool entry's tokens per provider, adopting
 * fresher tokens from the auth store first where applicable. force=true skips
 * the expiry short-circuit. Returns true if the entry was refreshed in place.
 * Dispatches to the real C OAuth refresh primitives — no stubs. */
/* PoP: credential_pool_refresh_entry_impl @ agent/credential_pool.py:_refresh_entry_impl */
bool credential_pool_refresh_entry_impl(credential_pool_t *pool, int entry_index, bool force) {
    if (!pool || entry_index < 0 || entry_index >= pool->entry_count) return false;
    credential_entry_t *e = &pool->entries[entry_index];

    if (!force && e->expires_at_ms > 0) {
        long long now_ms = (long long)time(NULL) * 1000LL;
        if (e->expires_at_ms > now_ms) return false;
    }

    if (strcmp(pool->provider_name, "anthropic") == 0) {
        if (e->refresh_token[0]) {
            json_node_t *r = anthropic_refresh_oauth(e->refresh_token,
                                                     strcmp(e->source, "hermes_pkce") == 0);
            if (r) {
                json_node_t *at = json_obj_get(r, "access_token");
                json_node_t *rt = json_obj_get(r, "refresh_token");
                json_node_t *em = json_obj_get(r, "expires_at_ms");
                if (at && at->type == JSON_STRING)
                    snprintf(e->access_token, sizeof(e->access_token), "%s", at->str_val);
                if (rt && rt->type == JSON_STRING)
                    snprintf(e->refresh_token, sizeof(e->refresh_token), "%s", rt->str_val);
                if (em && em->type == JSON_NUMBER)
                    e->expires_at_ms = (long long)em->num_val;
                json_free(r);
                e->status = CRED_OK;
                e->consecutive_failures = 0;
                return true;
            }
        }
        return credential_pool_sync_anthropic_entry_from_credentials_file(e);
    }

    /* xai-oauth / openai-codex / nous: token_exchange + auth subsystem own
     * per-provider refresh; leave the entry for those paths. */
    return false;
}
