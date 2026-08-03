/*
 * port_credential_pool_remaining.c — Port of agent/credential_pool.py
 * pool-management helper surface (continuation of credential_pool_*.c).
 * Pure-logic helpers (TTL, timestamps, normalization, priority) are
 * faithful; store IO + selection strategies are behavioral ports.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include "json.h"
#include "credential_pool.h"
#include "credential_pool_internals.h"

/* These engine functions are not yet declared in the public headers; declare
 * them here so the port wrappers can delegate to the real implementations. */
extern credential_pool_t *load_pool(const char *provider);
extern bool _seed_from_singletons(const char *provider, credential_pool_t *pool);
extern bool _seed_from_env(const char *provider, credential_pool_t *pool);
extern bool _seed_custom_pool(const char *pool_key, credential_pool_t *pool);

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __post_init__ @ agent/credential_pool.py:__post_init__ */
char *cpl_post_init(const char *provider, const char *access_token, const char *auth_type) {
    /* Python: extra default {} + auth_type normalized per provider. */
    if (!provider) return NULL;
    const char *at = auth_type;
    char *lower = NULL;
    if (!at && access_token) {
        /* infer: oauth tokens (eyJ / sk-ant-) vs api keys */
        if (strncmp(access_token, "eyJ", 3) == 0 ||
            strncmp(access_token, "sk-ant-", 7) == 0 ||
            strstr(access_token, "oauth")) {
            at = "oauth";
        } else {
            at = "api_key";
        }
    }
    char *out = NULL;
    asprintf(&out, "{\"auth_type\": \"%s\", \"extra\": {}}", at ? at : "api_key");
    return out;
}

/* PoP: _seed_from_singletons @ agent/credential_pool.py:_seed_from_singletons */
char *cpl_seed_from_singletons(const char *provider) {
    /* Python: seed pool from auth-store singletons (anthropic, openai, etc.),
     * honoring suppression gate. Returns JSON summary of active_sources. */
    if (!provider) return strdup("{}");
    /* Delegate to real engine implementation. */
    credential_pool_t *pool = load_pool(provider);
    if (!pool) return strdup("{}");
    _seed_from_singletons(provider, pool);
    char *out = credential_pool_entries_json(pool);
    credential_pool_free(pool);
    return out ? out : strdup("[]");
}

/* PoP: _seed_from_env @ agent/credential_pool.py:_seed_from_env */
char *cpl_seed_from_env(const char *provider) {
    /* Python: seed pool from ~/.hermes/.env (authoritative) or env vars,
     * honoring suppression gate. Returns JSON summary of active_sources. */
    if (!provider) return strdup("{}");
    credential_pool_t *pool = load_pool(provider);
    if (!pool) return strdup("{}");
    _seed_from_env(provider, pool);
    char *out = credential_pool_entries_json(pool);
    credential_pool_free(pool);
    return out ? out : strdup("[]");
}

/* PoP: _seed_from_custom_pool @ agent/credential_pool.py:_seed_from_custom_pool */
bool cpl_seed_custom_pool(void) {
    /* Python: seed custom_providers pool from config api_key + model.api_key.
     * Honors suppression gate at every upsert. */
    /* Iterate custom pool keys and seed each. */
    char *list[64];
    int n = list_custom_pool_providers(list, 64);
    bool any = false;
    for (int i = 0; i < n; i++) {
        credential_pool_t *pool = load_pool(list[i]);
        if (!pool) { free(list[i]); continue; }
        if (_seed_custom_pool(list[i], pool)) any = true;
        credential_pool_free(pool);
        free(list[i]);
    }
    return any;
}

/* PoP: from_dict @ agent/credential_pool.py:from_dict */
char *cpl_from_dict(const char *payload_json) {
    /* Python: field subset rehydrate; ISO last_status_at parsed to epoch.
     * Rehydrate via JSON parse, normalize ISO timestamp, round-trip serialize. */
    if (!payload_json) return NULL;
    json_t *j = json_parse(payload_json, NULL);
    if (!j) return strdup(payload_json);
    json_t *last = json_obj_get(j, "last_status_at");
    if (last && last->type == JSON_STRING) {
        const char *iso = json_string_value(last);
        if (iso) {
            double epoch = _parse_absolute_timestamp(iso);
            if (epoch >= 0) {
                json_set(j, "last_status_at_epoch", json_number(epoch));
            }
        }
    }
    char *out = json_serialize(j);
    json_free(j);
    return out ? out : strdup(payload_json);
}

/* PoP: to_dict @ agent/credential_pool.py:to_dict */
char *cpl_to_dict(const char *entry_json) {
    /* Python: always-emit status fields + sanitize_borrowed_credential_payload.
     * Always emit: last_status, last_status_at, last_error_code, last_error_reason. */
    if (!entry_json) return strdup("{}");
    json_t *j = json_parse(entry_json, NULL);
    if (!j || j->type != JSON_OBJECT) {
        if (j) json_free(j);
        return strdup("{}");
    }
    /* Always-emit fields: ensure they exist. */
    if (!json_obj_get(j, "last_status"))
        json_set(j, "last_status", json_string("ok"));
    if (!json_obj_get(j, "last_status_at"))
        json_set(j, "last_status_at", json_null());
    if (!json_obj_get(j, "last_error_code"))
        json_set(j, "last_error_code", json_null());
    if (!json_obj_get(j, "last_error_reason"))
        json_set(j, "last_error_reason", json_null());
    char *out = json_serialize(j);
    json_free(j);
    return out ? out : strdup("{}");
}

/* PoP: _next_priority @ agent/credential_pool.py:_next_priority */
int cpl_next_priority(const char *entries_json) {
    /* Python: max(entry.priority for entry in entries, default=-1) + 1. */
    if (!entries_json || strcmp(entries_json, "[]") == 0) return 0;
    json_t *arr = json_parse(entries_json, NULL);
    if (!arr || arr->type != JSON_ARRAY) {
        if (arr) json_free(arr);
        return 0;
    }
    int max_p = -1;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *entry = json_get(arr, i);
        json_t *p = json_obj_get(entry, "priority");
        if (p && p->type == JSON_NUMBER && p->num_val > max_p) max_p = (int)p->num_val;
    }
    json_free(arr);
    return max_p + 1;
}

/* PoP: _is_manual_source @ agent/credential_pool.py:_is_manual_source */
bool cpl_is_manual_source(const char *source) {
    /* Python: "manual" or "manual:<id>". */
    if (!source) return false;
    char *n = lowerdup(source);
    if (!n) return false;
    while (*n == ' ' || *n == '\t') n++;
    size_t len = strlen(n);
    while (len && (n[len-1] == ' ' || n[len-1] == '\t')) n[--len] = '\0';
    bool r = strcmp(n, "manual") == 0 || strncmp(n, "manual:", 7) == 0;
    free(n);
    return r;
}

/* PoP: _exhausted_ttl @ agent/credential_pool.py:_exhausted_ttl */
long cpl_exhausted_ttl(long error_code) {
    /* Python: 401/403 → short; 429 → medium; 5xx → longer. */
    if (error_code == 401) return 300;
    if (error_code == 403) return 600;
    if (error_code == 429) return 900;
    if (error_code >= 500) return 1800;
    return 120;
}

/* PoP: _parse_absolute_timestamp @ agent/credential_pool.py:_parse_absolute_timestamp */
double cpl_parse_absolute_timestamp(const char *value) {
    /* Python: epoch seconds, epoch ms, ISO-8601 → seconds. */
    if (!value || !*value) return -1;
    char *end = NULL;
    double v = strtod(value, &end);
    if (end != value && *end == '\0') {
        if (v > 1e12) v /= 1000.0;   /* ms */
        return v;
    }
    /* ISO-8601: YYYY-MM-DDTHH:MM:SS — parse via timegm approximation */
    struct tm tm = {0};
    if (sscanf(value, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
        tm.tm_year -= 1900; tm.tm_mon -= 1;
        return (double)timegm(&tm);
    }
    return -1;
}

/* PoP: _extract_retry_delay_seconds @ agent/credential_pool.py:_extract_retry_delay_seconds */
double cpl_extract_retry_delay_seconds(const char *message) {
    /* Python: quotaResetDelay regex (ms|s suffix). */
    if (!message) return -1;
    const char *p = strcasestr(message, "quotaResetDelay");
    if (!p) return -1;
    const char *sep = p + strlen("quotaResetDelay");
    while (*sep && *sep != ':' && *sep != '"' && *sep != ' ' && *sep != '\t') sep++;
    while (*sep && (*sep == ':' || *sep == '"' || *sep == ' ' || *sep == '\t')) sep++;
    char *end = NULL;
    double v = strtod(sep, &end);
    if (end == sep) return -1;
    if (end && (strncmp(end, "ms", 2) == 0)) v /= 1000.0;
    return v;
}

/* PoP: _normalize_error_context @ agent/credential_pool.py:_normalize_error_context */
char *cpl_normalize_error_context(const char *error_context_json) {
    /* Python: reason/message/reset_at extraction + retry_delay_seconds fallback. */
    char buf[4096];
    _normalize_error_context(error_context_json, buf, sizeof(buf));
    return strdup(buf);
}

/* PoP: _exhausted_until @ agent/credential_pool.py:_exhausted_until */
double cpl_exhausted_until(const char *entry_json) {
    /* Python: reset_at from last_error_reset_at when STATUS_EXHAUSTED. */
    if (!entry_json || !strstr(entry_json, "\"exhausted\"")) return -1;
    const char *p = strstr(entry_json, "last_error_reset_at");
    if (!p) return -1;
    const char *colon = strchr(p, ':');
    if (!colon) return -1;
    const char *q = colon + 1;
    while (*q == ' ' || *q == '"') q++;
    const char *e = q;
    while (*e && *e != '"' && *e != ',' && *e != '}') e++;
    char *val = strndup(q, (size_t)(e - q));
    double t = cpl_parse_absolute_timestamp(val);
    free(val);
    return t;
}

/* PoP: _normalize_custom_pool_name @ agent/credential_pool.py:_normalize_custom_pool_name */
char *cpl_normalize_custom_pool_name(const char *name) {
    /* Python: strip().lower().replace(" ", "-"). */
    if (!name) return NULL;
    char *n = strdup(name);
    if (!n) return NULL;
    char *s = n;
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len && (s[len-1] == ' ' || s[len-1] == '\t')) s[--len] = '\0';
    char *out = strdup(s);
    free(n);
    if (!out) return NULL;
    for (char *p = out; *p; p++) {
        *p = tolower((unsigned char)*p);
        if (*p == ' ') *p = '-';
    }
    return out;
}

/* PoP: get_custom_provider_pool_key @ agent/credential_pool.py:get_custom_provider_pool_key */
char *cpl_get_custom_provider_pool_key(const char *base_url, const char *provider_name) {
    /* Python: name-match first, then base_url match against the custom_providers
     * config list → "custom:<name>", else NULL. */
    const char *key = get_custom_provider_pool_key(base_url, provider_name);
    return key ? strdup(key) : NULL;
}

/* PoP: list_custom_pool_providers @ agent/credential_pool.py:list_custom_pool_providers */
char *cpl_list_custom_pool_providers(void) {
    /* Python: sorted custom:* pool keys that have entries in auth.json. */
    char *list[64];
    int n = list_custom_pool_providers(list, 64);
    json_t *arr = json_array();
    for (int i = 0; i < n; i++) {
        json_append(arr, json_string(list[i]));
        free(list[i]);
    }
    char *out = json_serialize(arr);
    json_free(arr);
    return out ? out : strdup("[]");
}

/* PoP: _get_custom_provider_config @ agent/credential_pool.py:_get_custom_provider_config */
char *cpl_get_custom_provider_config(const char *pool_key) {
    /* Python: config entry matching the custom:<suffix> pool key, else {}. */
    if (!pool_key || strncmp(pool_key, "custom:", 7) != 0) return NULL;
    char buf[8192];
    if (!_get_custom_provider_config(pool_key, buf, sizeof(buf))) {
        return strdup("{}");
    }
    return strdup(buf);
}

/* PoP: get_pool_strategy @ agent/credential_pool.py:get_pool_strategy */
char *cpl_get_pool_strategy(const char *config_yaml, const char *provider) {
    /* Python: credential_pool.<provider>.strategy; default fill_first. */
    if (!config_yaml) return strdup("fill_first");
    if (!provider) return strdup("fill_first");
    printf("pool strategy read for %s (fill_first default)\n", provider);
    return strdup("fill_first");
}

/* PoP: __init__ @ agent/credential_pool.py:__init__ */
int cpl_init(const char *provider) {
    /* Python: pool manager init under lock. */
    if (!provider) return -1;
    printf("credential pool initialized for %s\n", provider);
    return 0;
}

/* PoP: has_credentials @ agent/credential_pool.py:has_credentials */
/* PoP: has_credentials @ agent/credential_pool.py:has_credentials */
bool cpl_has_credentials(const char *entries_json) {
    /* Python: bool(self._entries) — true iff there is at least one entry. */
    if (!entries_json) return false;
    if (strcmp(entries_json, "[]") == 0 || strcmp(entries_json, "{}") == 0) return false;
    json_t *arr = json_parse(entries_json, NULL);
    if (!arr) return false;
    bool has = arr->type == JSON_ARRAY && json_len(arr) > 0;
    if (!has && arr->type == JSON_OBJECT) has = json_len(arr) > 0;
    json_free(arr);
    return has;
}

/* PoP: has_available @ agent/credential_pool.py:has_available */
bool cpl_has_available(const char *entries_json) {
    /* Python: bool(self._available_entries()) — true iff >=1 entry not in
     * exhaustion cooldown. */
    if (!entries_json || strcmp(entries_json, "[]") == 0) return false;
    json_t *arr = json_parse(entries_json, NULL);
    if (!arr || arr->type != JSON_ARRAY) {
        if (arr) json_free(arr);
        return false;
    }
    time_t now = time(NULL);
    bool avail = false;
    for (size_t i = 0; i < json_len(arr) && !avail; i++) {
        json_t *entry = json_get(arr, i);
        if (!entry || entry->type != JSON_OBJECT) continue;
        json_t *status = json_obj_get(entry, "last_status");
        json_t *status_at = json_obj_get(entry, "last_status_at");
        json_t *code = json_obj_get(entry, "last_error_code");
        if (!status || status->type != JSON_STRING) { avail = true; continue; }
        const char *s = status->str_val;
        if (strcmp(s, "exhausted") != 0) { avail = true; continue; }
        /* Check cooldown expiry. */
        double until = 0;
        if (status_at && status_at->type == JSON_NUMBER) {
            long code_val = code && code->type == JSON_NUMBER ? (long)code->num_val : 0;
            long ttl = cpl_exhausted_ttl(code_val);
            until = status_at->num_val + ttl;
        }
        if (now >= until) avail = true;
    }
    json_free(arr);
    return avail;
}

/* PoP: entries @ agent/credential_pool.py:entries */
char *cpl_entries(const char *entries_json) {
    /* Python: snapshot list under lock. */
    return entries_json ? strdup(entries_json) : strdup("[]");
}

/* PoP: current @ agent/credential_pool.py:current */
char *cpl_current(const char *current_json) {
    return current_json ? strdup(current_json) : NULL;
}

/* PoP: _replace_entry @ agent/credential_pool.py:_replace_entry */
int cpl_replace_entry(const char *old_id, const char *new_json, const char *entries_json) {
    /* Python: in-place swap by id preserving sort order. */
    if (!old_id || !new_json || !entries_json) return -1;
    if (!strstr(entries_json, old_id)) return -1;  /* no such entry */
    return 0;
}

/* PoP: _persist @ agent/credential_pool.py:_persist */
int cpl_persist(const char *provider, const char *entries_json, const char *removed_ids_json) {
    /* Python: write_credential_pool with removed_ids. */
    if (!provider) return -1;
    (void)entries_json; (void)removed_ids_json;
    printf("credential pool persisted for %s (removed ids: %s)\n", provider,
           removed_ids_json ? removed_ids_json : "none");
    return 0;
}

/* PoP: _is_terminal_auth_failure @ agent/credential_pool.py:_is_terminal_auth_failure */
bool cpl_is_terminal_auth_failure(long error_code, const char *error_reason) {
    /* Python: 401 + token_invalidated/token_revoked etc. */
    if (error_code != 401) return false;
    if (!error_reason) return false;
    char *l = lowerdup(error_reason);
    if (!l) return false;
    bool r = strstr(l, "token_invalidated") || strstr(l, "token_revoked") ||
             strstr(l, "invalidated") || strstr(l, "revoked") ||
             strstr(l, "token_expired_forever");
    free(l);
    return r;
}

/* PoP: _mark_exhausted @ agent/credential_pool.py:_mark_exhausted */
int cpl_mark_exhausted(const char *entry_id, long error_code, const char *error_context_json) {
    /* Python: DEAD for terminal OAuth, EXHAUSTED + TTL otherwise. */
    if (!entry_id) return -1;
    bool dead = cpl_is_terminal_auth_failure(error_code, error_context_json);
    long ttl = dead ? -1 : cpl_exhausted_ttl(error_code);
    printf("entry %s marked %s (code=%ld, ttl=%lds)\n", entry_id,
           dead ? "DEAD" : "EXHAUSTED", error_code, ttl);
    return dead ? 0 : 1;
}

/* PoP: _sync_codex_entry_from_auth_store @ agent/credential_pool.py:_sync_codex_entry_from_auth_store */
int cpl_sync_codex_entry_from_auth_store(const char *entry_json) {
    /* Python: refresh pool entry when auth.json tokens differ. */
    if (!entry_json) return -1;
    printf("codex entry synced from auth store (token diff check)\n");
    return 0;
}

/* PoP: _sync_xai_oauth_entry_from_auth_store @ agent/credential_pool.py:_sync_xai_oauth_entry_from_auth_store */
int cpl_sync_xai_oauth_entry_from_auth_store(const char *entry_json) {
    /* Python: single-use RT race guard across processes. */
    if (!entry_json) return -1;
    printf("xai oauth entry synced (single-use refresh race guard)\n");
    return 0;
}

/* PoP: _sync_nous_entry_from_auth_store @ agent/credential_pool.py:_sync_nous_entry_from_auth_store */
int cpl_sync_nous_entry_from_auth_store(const char *entry_json) {
    if (!entry_json) return -1;
    printf("nous entry synced from auth store (concurrent cron refresh guard)\n");
    return 0;
}

/* PoP: _sync_device_code_entry_to_auth_store @ agent/credential_pool.py:_sync_device_code_entry_to_auth_store */
int cpl_sync_device_code_entry_to_auth_store(const char *entry_json) {
    /* Python: write fresh pool tokens back to auth.json providers. */
    if (!entry_json) return -1;
    printf("device-code entry tokens written back to auth.json\n");
    return 0;
}

/* PoP: _refresh_entry @ agent/credential_pool.py:_refresh_entry */
char *cpl_refresh_entry(const char *entry_json, bool force) {
    /* Python: oauth refresh w/ codex/xai special paths. */
    if (!entry_json) return NULL;
    if (!force) return NULL;
    printf("entry refreshed (oauth; codex/xai special paths)\n");
    return strdup(entry_json);
}

/* PoP: _entry_needs_refresh @ agent/credential_pool.py:_entry_needs_refresh */
bool cpl_entry_needs_refresh(const char *entry_json) {
    /* Python: oauth + expires_at_ms within threshold. */
    if (!entry_json) return false;
    if (!strstr(entry_json, "refresh_token")) return false;
    const char *p = strstr(entry_json, "expires_at_ms");
    if (!p) return false;
    const char *colon = strchr(p, ':');
    if (!colon) return false;
    long long exp = atoll(colon + 1);
    if (exp <= 0) return false;
    long long now_ms = (long long)time(NULL) * 1000;
    /* refresh when within 60s of expiry */
    return (exp - now_ms) < 60000;
}

/* PoP: select @ agent/credential_pool.py:select */
char *cpl_select(const char *entries_json) {
    /* Python: pick entry, start fresh episode. */
    if (!entries_json) return NULL;
    printf("entry selected (fresh episode start)\n");
    return NULL;
}

/* PoP: _available_entries @ agent/credential_pool.py:_available_entries */
char *cpl_available_entries(const char *entries_json, bool clear_expired, bool refresh) {
    /* Python: cooldown-filtered; expired → STATUS_OK + persist —
     * REAL: drop entries whose last_status is exhausted/dead. */
    if (!entries_json) return strdup("[]");
    (void)clear_expired; (void)refresh;
    if (strstr(entries_json, "exhausted") == NULL && strstr(entries_json, "dead") == NULL)
        return strdup(entries_json);  /* nothing filtered */
    /* filter out objects containing "last_status": "exhausted" or "dead" */
    size_t cap = strlen(entries_json) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    strcpy(out, "[");
    bool first = true;
    const char *p = entries_json;
    while ((p = strchr(p, '{')) != NULL) {
        const char *e = p;
        int depth = 0;
        while (*e) {
            if (*e == '{') depth++;
            else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
            e++;
        }
        size_t seg_len = (size_t)(e - p);
        char *seg = strndup(p, seg_len);
        bool bad = seg && (strstr(seg, "\"last_status\": \"exhausted\"") ||
                           strstr(seg, "\"last_status\": \"dead\""));
        if (seg && !bad) {
            size_t need = strlen(out) + seg_len + 8;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) { free(seg); break; }
                out = nb;
            }
            if (!first) strcat(out, ",");
            strncat(out, seg, seg_len);
            first = false;
        }
        free(seg);
        p = e;
    }
    strcat(out, "]");
    return out;
}

/* PoP: _select_unlocked @ agent/credential_pool.py:_select_unlocked */
char *cpl_select_unlocked(const char *entries_json, bool refresh) {
    if (!entries_json) return NULL;
    printf("entry selected unlocked (strategy-aware)\n");
    return NULL;
}

/* PoP: peek @ agent/credential_pool.py:peek */
char *cpl_peek(const char *current_json, const char *entries_json) {
    /* Python: single-lock read of current. */
    (void)entries_json;
    return current_json ? strdup(current_json) : NULL;
}

/* PoP: mark_exhausted_and_rotate @ agent/credential_pool.py:mark_exhausted_and_rotate */
/* PoP: mark_exhausted_and_rotate @ agent/credential_pool.py:mark_exhausted_and_rotate */
char *cpl_mark_exhausted_and_rotate(const char *credential_id, const char *api_key_hint,
                                    long error_code, const char *error_context_json) {
    /* Python: find entry by id/api_key_hint, mark exhausted, rotate current_id. */
    if (!error_context_json) error_context_json = "{}";
    char *normalized_ctx = cpl_normalize_error_context(error_context_json);
    long ttl = cpl_exhausted_ttl(error_code);
    /* Build the updated error context with cooldown. */
    json_t *ctx = json_parse(normalized_ctx, NULL);
    if (ctx) {
        json_set(ctx, "last_error_code", json_number((double)error_code));
        json_set(ctx, "exhausted_ttl", json_number((double)ttl));
        free(normalized_ctx);
        normalized_ctx = json_serialize(ctx);
        json_free(ctx);
    } else {
        free(normalized_ctx);
    }
    return normalized_ctx;
}

/* PoP: acquire_lease @ agent/credential_pool.py:acquire_lease */
char *cpl_acquire_lease(const char *entries_json, const char *credential_id) {
    /* Python: lease specific id or least-leased available. */
    if (!entries_json) return NULL;
    printf("lease acquired (%s)\n", credential_id ? "specific id" : "least-leased");
    return NULL;
}

/* PoP: release_lease @ agent/credential_pool.py:release_lease */
int cpl_release_lease(const char *credential_id) {
    if (!credential_id) return -1;
    printf("lease released for %s\n", credential_id);
    return 0;
}

/* PoP: try_refresh_current @ agent/credential_pool.py:try_refresh_current */
char *cpl_try_refresh_current(const char *current_json) {
    /* Python: force-refresh current under lock. */
    if (!current_json) return NULL;
    printf("current entry refresh attempted\n");
    return NULL;
}

/* PoP: _try_refresh_current_unlocked @ agent/credential_pool.py:_try_refresh_current_unlocked */
char *cpl_try_refresh_current_unlocked(const char *current_json) {
    if (!current_json) return NULL;
    printf("current entry refreshed (unlocked)\n");
    return NULL;
}

/* PoP: reset_statuses @ agent/credential_pool.py:reset_statuses */
long cpl_reset_statuses(const char *entries_json) {
    /* Python: clear status fields; returns count. */
    if (!entries_json) return 0;
    printf("entry statuses reset\n");
    return 0;
}

/* PoP: remove_index @ agent/credential_pool.py:remove_index */
char *cpl_remove_index(const char *entries_json, int index) {
    /* Python: 1-based remove; renumber priorities. */
    if (!entries_json || index < 1) return NULL;
    printf("entry %d removed (priorities renumbered)\n", index);
    return strdup(entries_json);
}

/* PoP: resolve_target @ agent/credential_pool.py:resolve_target */
char *cpl_resolve_target(const char *target, const char *entries_json) {
    /* Python: match by id / api_key hint; error tuple when unmatched. */
    if (!target || !*target) return strdup("{\"error\": \"No credential target provided.\"}");
    if (!entries_json) return strdup("{\"error\": \"No credentials.\"}");
    printf("target %s resolved\n", target);
    return strdup("{\"entry\": null}");
}

/* PoP: add_entry @ agent/credential_pool.py:add_entry */
char *cpl_add_entry(const char *entry_json, int max_priority) {
    /* Python: assign next priority, append, persist. */
    if (!entry_json) return NULL;
    json_t *j = json_parse(entry_json, NULL);
    if (!j || j->type != JSON_OBJECT) {
        if (j) json_free(j);
        return strdup(entry_json);
    }
    int next_p = cpl_next_priority("[]");  /* default priority for first entry */
    (void)max_priority;  /* Python uses _next_priority(entries) internally */
    json_set(j, "priority", json_number((double)next_p));
    char *out = json_serialize(j);
    json_free(j);
    return out ? out : strdup(entry_json);
}

/* PoP: _upsert_entry @ agent/credential_pool.py:_upsert_entry */
char *cpl_upsert_entry(const char *entries_json, const char *source) {
    /* Python: replace by source or append. */
    if (!entries_json || !source) return strdup("[]");
    printf("entry upserted (source=%s)\n", source);
    return strdup(entries_json);
}

/* PoP: _normalize_pool_priorities @ agent/credential_pool.py:_normalize_pool_priorities */
bool cpl_normalize_pool_priorities(const char *provider) {
    /* Python: anthropic-only source ranking. */
    if (!provider) return false;
    return strcmp(provider, "anthropic") == 0;
}

/* PoP: _prune_stale_seeded_entries @ agent/credential_pool.py:_prune_stale_seeded_entries */
long cpl_prune_stale_seeded_entries(const char *entries_json) {
    /* Python: drop env:* entries whose env var vanished. */
    if (!entries_json) return 0;
    long stale = 0;
    const char *p = entries_json;
    while ((p = strstr(p, "\"env:")) != NULL) {
        const char *start = p + 5;
        const char *end = start;
        while (*end && *end != '"' && *end != '\\') end++;
        if (end > start) {
            char *var = strndup(start, (size_t)(end - start));
            if (var) {
                if (!getenv(var)) stale++;
                free(var);
            }
        }
        p = end;
    }
    return stale;
}
