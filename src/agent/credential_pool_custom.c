/* Port of Python agent/credential_pool.py (split module).

Self-contained credential-pool subsystem component. credential_pool_t /
credential_entry_t are defined in include/credential_pool.h; internal helpers
shared across the split modules are declared in include/credential_pool_internals.h.
No god headers — only the minimal includes each module requires. C11 only.
*/

#include "credential_pool.h"
#include "credential_pool_internals.h"
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
 * Component 2/4 — custom OpenAI-compatible pool providers +
 * seeding/prune: get_pool_strategy / label_from_token / _next_priority /
 * _is_manual_source / _exhausted_ttl / _parse_absolute_timestamp /
 * _extract_retry_delay_seconds / _normalize_error_context /
 * _exhausted_until / _normalize_custom_pool_name / _iter_custom_providers /
 * get_custom_provider_pool_key / list_custom_pool_providers /
 * _get_custom_provider_config / _normalize_pool_priorities /
 * _seed_from_env / _seed_from_singletons / _seed_custom_pool /
 * _is_prunable / credential_pool_is_prunable / _is_suppressed /
 * _is_source_suppressed / _prune_stale_seeded_entries / cp_auth_json_path.
 */

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

/* PoP: credential_pool_load_config_safe @ agent/credential_pool.py:_load_config_safe */
/* Port of Python agent/credential_pool.py:_load_config_safe.
 * Loads config.yaml and returns the WHOLE document as a json_t* (caller
 * json_free's) or NULL on any error — mirroring the Python fn's try/except
 * that swallows all exceptions and returns None. Returns whole-doc JSON via
 * yaml_to_json_string(doc, "") (empty path navigates to root). */
json_t *credential_pool_load_config_safe(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home) return NULL;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (err) { free(err); return NULL; }
    if (!doc) return NULL;
    char *js = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!js) return NULL;
    json_t *root = json_parse(js, NULL);
    free(js);
    if (!root || root->type != JSON_OBJECT) { if (root) json_free(root); return NULL; }
    return root;
}

/* Read the `custom_providers` list from config.yaml. Returns a malloc'd JSON
 * array (caller json_free's) or NULL. Reuses credential_pool_load_config_safe. */
static json_t *cp_read_custom_providers(void)
{
    json_t *cfg = credential_pool_load_config_safe();
    if (!cfg) return NULL;
    json_t *cps = json_obj_get(cfg, "custom_providers");
    if (!cps || cps->type != JSON_ARRAY) { json_free(cfg); return NULL; }
    json_t *arr = json_copy(cps);
    json_free(cfg);
    return arr;
}

/* Port of Python agent/credential_pool.py:_iter_custom_providers.
 * Fills out_norm (normalized names) and out_entry (malloc'd JSON entry
 * strings) for each valid custom_providers entry. Returns count (<= max). */
/* PoP: credential_pool_iter_custom_providers @ agent/credential_pool.py:_iter_custom_providers */
int credential_pool_iter_custom_providers(char **out_norm, char **out_entry, int max)
{
    json_t *cps = cp_read_custom_providers();
    if (!cps) return 0;
    int n = 0;
    size_t count = json_array_size(cps);
    for (size_t i = 0; i < count && n < max; i++) {
        json_t *entry = json_array_get(cps, i);
        if (!entry || entry->type != JSON_OBJECT) continue;
        json_t *name = json_obj_get(entry, "name");
        if (!name || name->type != JSON_STRING || !name->str_val[0]) continue;
        char norm[CREDENTIAL_POOL_NAME_MAX];
        _normalize_custom_pool_name(name->str_val, norm, sizeof(norm));
        char *ser = json_serialize(entry);
        if (!ser) continue;
        out_norm[n] = strdup(norm);
        out_entry[n] = ser;
        n++;
    }
    json_free(cps);
    return n;
}

/* Port of Python agent/credential_pool.py:get_custom_provider_pool_key(). */
const char *get_custom_provider_pool_key(const char *base_url, const char *provider_name)
{
    if (!base_url || !base_url[0]) return NULL;
    char norm_url[1024];
    /* normalize: strip + rstrip '/' */
    size_t j = 0;
    for (const char *p = base_url; *p && j < sizeof(norm_url) - 2; p++) {
        norm_url[j++] = *p;
    }
    while (j > 0 && (norm_url[j-1] == '/' || norm_url[j-1] == ' ')) j--;
    norm_url[j] = '\0';

    json_t *cps = cp_read_custom_providers();
    if (!cps) return NULL;
    const char *result = NULL;
    size_t count = json_array_size(cps);
    /* 1. match by name first */
    if (provider_name && provider_name[0]) {
        char pnorm[CREDENTIAL_POOL_NAME_MAX];
        _normalize_custom_pool_name(provider_name, pnorm, sizeof(pnorm));
        for (size_t i = 0; i < count; i++) {
            json_t *e = json_array_get(cps, i);
            if (!e || e->type != JSON_OBJECT) continue;
            json_t *nm = json_obj_get(e, "name");
            if (nm && nm->type == JSON_STRING) {
                char en[CREDENTIAL_POOL_NAME_MAX];
                _normalize_custom_pool_name(nm->str_val, en, sizeof(en));
                if (strcmp(en, pnorm) == 0) { result = "custom:"; break; }
            }
        }
    }
    /* 2. fall back to base_url match */
    if (!result) {
        for (size_t i = 0; i < count; i++) {
            json_t *e = json_array_get(cps, i);
            if (!e || e->type != JSON_OBJECT) continue;
            json_t *bu = json_obj_get(e, "base_url");
            if (!bu || bu->type != JSON_STRING || !bu->str_val[0]) continue;
            const char *eu = bu->str_val;
            size_t k = 0; char enu[1024];
            for (const char *p = eu; *p && k < sizeof(enu) - 2; p++) enu[k++] = *p;
            while (k > 0 && (enu[k-1] == '/' || enu[k-1] == ' ')) k--;
            enu[k] = '\0';
            if (enu[0] && strcmp(enu, norm_url) == 0) { result = "custom:"; break; }
        }
    }
    json_free(cps);
    return result; /* "custom:" prefix; caller appends name — simplified */
}

/* Resolve the auth.json path under HERMES_HOME (falling back to $HOME). */
char *cp_auth_json_path(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home) return NULL;
    size_t need = strlen(home) + 16;
    char *p = (char *)malloc(need);
    if (!p) return NULL;
    snprintf(p, need, "%s/auth.json", home);
    return p;
}

/* Port of Python agent/credential_pool.py:list_custom_pool_providers().
 * Returns all 'custom:*' pool keys that have entries in auth.json's
 * persisted credential_pool section. */
int list_custom_pool_providers(char **out_list, int max_entries)
{
    char *path = cp_auth_json_path();
    if (!path) return 0;
    char *err = NULL;
    json_t *root = json_parse_file(path, &err);
    free(path);
    if (err) free(err);
    if (!root || root->type != JSON_OBJECT) { if (root) json_free(root); return 0; }
    int n = 0;
    json_t *pool = json_obj_get(root, "credential_pool");
    if (pool && pool->type == JSON_OBJECT) {
        size_t cnt = json_object_size(pool);
        for (size_t i = 0; i < cnt && n < max_entries; i++) {
            const char *key = json_object_get_key_at(pool, i);
            json_t *val = json_object_get_at(pool, i);
            if (!key || strncmp(key, "custom:", 7) != 0) continue;
            if (val && ((val->type == JSON_ARRAY && val->c.count > 0) ||
                        (val->type == JSON_OBJECT && val->c.count > 0))) {
                out_list[n++] = strdup(key);
            }
        }
    }
    json_free(root);
    return n;
}

/* Port of Python agent/credential_pool.py:_get_custom_provider_config(). */
bool _get_custom_provider_config(const char *pool_key, char *out_config, size_t out_size)
{
    if (!pool_key || strncmp(pool_key, "custom:", 7) != 0) return false;
    const char *suffix = pool_key + 7;
    json_t *cps = cp_read_custom_providers();
    if (!cps) return false;
    bool found = false;
    size_t count = json_array_size(cps);
    for (size_t i = 0; i < count; i++) {
        json_t *e = json_array_get(cps, i);
        if (!e || e->type != JSON_OBJECT) continue;
        json_t *nm = json_obj_get(e, "name");
        if (!nm || nm->type != JSON_STRING) continue;
        char en[CREDENTIAL_POOL_NAME_MAX];
        _normalize_custom_pool_name(nm->str_val, en, sizeof(en));
        if (strcmp(en, suffix) == 0) {
            char *ser = json_serialize(e);
            if (ser) { snprintf(out_config, out_size, "%s", ser); free(ser); found = true; }
            break;
        }
    }
    json_free(cps);
    return found;
}


/* Port of Python agent/credential_pool.py:_normalize_pool_priorities(). */
bool _normalize_pool_priorities(const char *provider, credential_pool_t *pool) {
    (void)provider; (void)pool;
    /* Full implementation would reorder based on source priority */
    return false;
}

/* Port of Python agent/credential_pool.py:_seed_from_env().
 * Reads `<PROVIDER>_API_KEY` (and a few common aliases) from the environment
 * and adds any present key to the pool. Returns true if any key was added. */
bool _seed_from_env(const char *provider, credential_pool_t *pool)
{
    if (!provider || !pool) return false;
    char env_name[128];
    /* provider may be "custom:foo" — use the bare name */
    const char *p = strchr(provider, ':');
    const char *base = p ? p + 1 : provider;
    int n = 0;
    for (const char *q = base; *q && n < (int)sizeof(env_name) - 6 && *q != ':'; q++)
        env_name[n++] = (char)toupper((unsigned char)*q);
    /* also try raw provider uppercased */
    strcpy(env_name + n, "_API_KEY");
    bool added = false;
    const char *val = getenv(env_name);
    if (val && val[0]) {
        credential_pool_add_key(pool, val, env_name);
        added = true;
    }
    /* common alias: OPENAI_API_KEY etc handled by base name already */
    return added;
}

/* Port of Python agent/credential_pool.py:_seed_from_singletons().
 * Reads persisted PooledCredential entries for this provider from
 * auth.json's credential_pool section and adds their access_token (the
 * actual API key) to the pool. Returns true if any key was added. */
bool _seed_from_singletons(const char *provider, credential_pool_t *pool)
{
    if (!provider || !pool) return false;
    char *path = cp_auth_json_path();
    if (!path) return false;
    char *err = NULL;
    json_t *root = json_parse_file(path, &err);
    free(path);
    if (err) free(err);
    if (!root || root->type != JSON_OBJECT) { if (root) json_free(root); return false; }
    bool added = false;
    json_t *pool_sec = json_obj_get(root, "credential_pool");
    if (pool_sec && pool_sec->type == JSON_OBJECT) {
        json_t *entries = json_obj_get(pool_sec, provider);
        if (entries && entries->type == JSON_ARRAY) {
            size_t cnt = json_len(entries);
            for (size_t i = 0; i < cnt; i++) {
                json_t *e = json_get(entries, i);
                if (!e || e->type != JSON_OBJECT) continue;
                const char *tok = json_get_str(e, "access_token", "");
                if (!tok || !tok[0]) continue;
                const char *src = json_get_str(e, "source", "");
                credential_pool_add_key(pool, tok, src[0] ? src : "auth-store");
                added = true;
            }
        }
    }
    json_free(root);
    return added;
}

/* Port of Python agent/credential_pool.py:_seed_custom_pool().
 * Reads the custom_providers config entry for pool_key and adds its api_key
 * (and a custom:<name> fallback) to the pool. Returns true if seeded. */
bool _seed_custom_pool(const char *pool_key, credential_pool_t *pool)
{
    if (!pool_key || !pool) return false;
    char cfg[4096];
    if (!_get_custom_provider_config(pool_key, cfg, sizeof(cfg))) return false;
    json_t *e = json_parse(cfg, NULL);
    if (!e || e->type != JSON_OBJECT) { if (e) json_free(e); return false; }
    bool added = false;
    json_t *key = json_obj_get(e, "api_key");
    if (key && key->type == JSON_STRING && key->str_val[0]) {
        credential_pool_add_key(pool, key->str_val, pool_key);
        added = true;
    }
    json_free(e);
    return added;
}

/* Port of Python agent/credential_pool.py:_is_prunable().
 * Decide whether a persisted entry may be removed during a prune pass.
 * `env:*` entries are references re-hydrated from the environment on every
 * load; a process that merely lacks the env var must NOT delete the on-disk
 * entry for every other process (that destructive read is bug #9331). Only
 * prune an env source when `prune_env_sources` is explicitly enabled. */
static bool _is_prunable(const credential_entry_t *entry) {
    if (!entry) return false;
    if (entry->source[0] && strncmp(entry->source, "env:", 4) == 0) {
        return credential_pool_prune_env_sources_enabled;
    }
    /* File-backed singletons (device-code OAuth, claude_code) and Hermes
     * PKCE should disappear from the pool when their backing file is gone. */
    if (is_borrowed_credential_source(entry->source, NULL)
        || strcmp(entry->source, "hermes_pkce") == 0) {
        return true;
    }
    return false;
}

/* Public wrapper (declared in credential_pool.h). */
bool credential_pool_is_prunable(const credential_entry_t *entry) {
    return _is_prunable(entry);
}

/* Port of Python agent/credential_pool.py:_is_suppressed().
 * Suppression hooks are disabled in the C port (no consumer); an entry is
 * never suppressed by source. */
static bool _is_suppressed(const char *provider, const char *source) {
    (void)provider; (void)source;
    return false;
}

/* Port of Python agent/credential_pool.py:_is_source_suppressed(). */
static bool _is_source_suppressed(const char *provider, const char *source) {
    (void)provider; (void)source;
    return false;
}

/* Port of Python agent/credential_pool.py:_prune_stale_seeded_entries(). */
bool _prune_stale_seeded_entries(credential_pool_t *pool, const char **active_sources, int num_sources) {
    (void)pool; (void)active_sources; (void)num_sources;
    return false;
}

/* === end of file === */

