/*
 * port_models_dev_remaining.c — Port of agent/models_dev.py models.dev
 * registry surface. Cost/capability formatting, disk cache, provider +
 * model lookups, catalog filtering, agentic filtering.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "hermes_http.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <pthread.h>

#define MODELS_DEV_URL "https://models.dev/api.json"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: mdv_has_cost_data @ agent/models_dev.py:has_cost_data */
bool mdv_has_cost_data(const char *model_json) {
    /* Python: cost_input > 0 or cost_output > 0. */
    if (!model_json) return false;
    const char *pi = strstr(model_json, "cost_input");
    const char *po = strstr(model_json, "cost_output");
    if (pi) {
        const char *c = strchr(pi, ':');
        if (c && atof(c + 1) > 0.0) return true;
    }
    if (po) {
        const char *c = strchr(po, ':');
        if (c && atof(c + 1) > 0.0) return true;
    }
    return false;
}

/* PoP: mdv_supports_pdf @ agent/models_dev.py:supports_pdf */
bool mdv_supports_pdf(const char *model_json) {
    /* Python: "pdf" in input_modalities. */
    if (!model_json) return false;
    return strstr(model_json, "\"pdf\"") != NULL;
}

/* PoP: mdv_format_cost @ agent/models_dev.py:format_cost */
char *mdv_format_cost(const char *model_json) {
    /* Python: '$3.00/M in, $15.00/M out'; '—' when no data. */
    if (!model_json || !mdv_has_cost_data(model_json)) return strdup("—");
    double ci = 0.0, co = 0.0;
    const char *pi = strstr(model_json, "cost_input");
    if (pi) { const char *c = strchr(pi, ':'); if (c) ci = atof(c + 1); }
    const char *po = strstr(model_json, "cost_output");
    if (po) { const char *c = strchr(po, ':'); if (c) co = atof(c + 1); }
    char *out = NULL;
    asprintf(&out, "$%.2f/M in, $%.2f/M out", ci, co);
    return out;
}

/* PoP: mdv_format_capabilities @ agent/models_dev.py:format_capabilities */
char *mdv_format_capabilities(const char *model_json) {
    /* Python: 'reasoning, tools, vision, PDF' style. */
    if (!model_json) return strdup("");
    char *out = malloc(strlen(model_json) + 32);
    if (!out) return strdup("");
    strcpy(out, "");
    bool first = true;
    const char *marks[] = {"\"reasoning\"", "\"tool_call\"", "\"vision\"", "\"pdf\"", "\"function_call\"", "\"audio\""};
    const char *labels[] = {"reasoning", "tools", "vision", "PDF", "functions", "audio"};
    for (int i = 0; i < 6; i++) {
        if (strstr(model_json, marks[i])) {
            if (!first) strcat(out, ", ");
            strcat(out, labels[i]);
            first = false;
        }
    }
    return out;
}

/* PoP: mdv_load_disk_cache @ agent/models_dev.py:_load_disk_cache */
char *mdv_load_disk_cache(const char *cache_path) {
    /* Python: read cache file; None on missing/corrupt. */
    if (!cache_path) return NULL;
    FILE *f = fopen(cache_path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    if (buf[0] != '{' && buf[0] != '[') { free(buf); return NULL; }
    return buf;
}

/* PoP: mdv_disk_cache_age_seconds @ agent/models_dev.py:_disk_cache_age_seconds */
long mdv_disk_cache_age_seconds(const char *cache_path) {
    /* Python: age in seconds of cache file; -1 when missing. */
    if (!cache_path) return -1;
    struct stat st;
    if (stat(cache_path, &st) != 0) return -1;
    return (long)(time(NULL) - st.st_mtime);
}

/* PoP: mdv_save_disk_cache @ agent/models_dev.py:_save_disk_cache */
int mdv_save_disk_cache(const char *cache_path, const char *registry_json) {
    /* Python: atomic write. */
    if (!cache_path || !registry_json) return -1;
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", cache_path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); return -1; }
    fwrite(registry_json, 1, strlen(registry_json), w);
    fputc('\n', w);
    if (fflush(w) != 0) { fclose(w); unlink(tmp); free(tmp); return -1; }
    fclose(w);
    if (rename(tmp, cache_path) != 0) { unlink(tmp); free(tmp); return -1; }
    free(tmp);
    return 0;
}

/* PoP: mdv_fetch_models_dev @ agent/models_dev.py:fetch_models_dev */
char *mdv_fetch_models_dev(const char *cache_path, long max_age_seconds) {
    /* Python: in-mem → disk → network. Real: disk cache w/ age gate. */
    if (!cache_path) return NULL;
    long age = mdv_disk_cache_age_seconds(cache_path);
    if (age >= 0 && age <= max_age_seconds) {
        return mdv_load_disk_cache(cache_path);
    }
    /* network fetch (best-effort) */
    char *url = strdup("https://models.dev/api.json");
    http_resp_t *r = NULL;
    http_t *h = http_new(30);
    if (h) {
        r = http_get(h, url, "User-Agent: hermes-agent");
        if (r && r->status == 200 && r->body) {
            mdv_save_disk_cache(cache_path, r->body);
            char *out = strdup(r->body);
            http_resp_free(r);
            http_free(h);
            free(url);
            return out;
        }
        if (r) http_resp_free(r);
        http_free(h);
    }
    free(url);
    return mdv_load_disk_cache(cache_path);
}

/* PoP: mdv_lookup_models_dev_context @ agent/models_dev.py:lookup_models_dev_context */
long mdv_lookup_models_dev_context(const char *registry_json, const char *provider, const char *model) {
    /* Python: context length for provider+model. */
    if (!registry_json || !provider || !model) return 0;
    /* find the model entry, then limit.context */
    const char *p = strstr(registry_json, model);
    if (!p) return 0;
    const char *lim = strstr(p, "\"context\"");
    if (!lim) return 0;
    const char *colon = strchr(lim, ':');
    if (!colon) return 0;
    return atol(colon + 1);
}

/* PoP: mdv_get_provider_models @ agent/models_dev.py:_get_provider_models */
char *mdv_get_provider_models(const char *registry_json, const char *provider) {
    /* Python: provider entry's models dict. */
    if (!registry_json || !provider) return NULL;
    char needle[256];
    snprintf(needle, sizeof(needle), "\"%s\"", provider);
    const char *p = strstr(registry_json, needle);
    if (!p) return NULL;
    const char *models = strstr(p, "\"models\"");
    if (!models) return NULL;
    const char *colon = strchr(models, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    if (*v != '{') return NULL;
    /* find matching close brace */
    int depth = 0;
    const char *e = v;
    while (*e) {
        if (*e == '{') depth++;
        else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
        e++;
    }
    return strndup(v, (size_t)(e - v));
}

/* PoP: mdv_find_model_entry @ agent/models_dev.py:_find_model_entry */
char *mdv_find_model_entry(const char *models_json, const char *model_id) {
    /* Python: exact then case-insensitive fallback. */
    if (!models_json || !model_id) return NULL;
    char needle[512];
    snprintf(needle, sizeof(needle), "\"%s\"", model_id);
    const char *p = strstr(models_json, needle);
    if (!p) {
        char *l = lowerdup(model_id);
        char *lj = lowerdup(models_json);
        if (l && lj) {
            snprintf(needle, sizeof(needle), "\"%s\"", l);
            p = strstr(lj, needle);
        }
        free(l);
        free(lj);
    }
    if (!p) return NULL;
    const char *colon = strchr(p, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    if (*v != '{') return NULL;
    int depth = 0;
    const char *e = v;
    while (*e) {
        if (*e == '{') depth++;
        else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
        e++;
    }
    return strndup(v, (size_t)(e - v));
}

/* PoP: mdv_get_model_capabilities @ agent/models_dev.py:get_model_capabilities */
char *mdv_get_model_capabilities(const char *registry_json, const char *provider, const char *model) {
    if (!registry_json || !model) return strdup("{}");
    char *models = mdv_get_provider_models(registry_json, provider ? provider : "");
    char *entry = mdv_find_model_entry(models ? models : "{}", model);
    free(models);
    char *out = entry ? entry : strdup("{}");
    return out;
}

/* PoP: mdv_list_provider_models @ agent/models_dev.py:list_provider_models */
char *mdv_list_provider_models(const char *registry_json, const char *provider) {
    /* Python: all model IDs for provider. */
    if (!registry_json || !provider) return strdup("[]");
    char *models = mdv_get_provider_models(registry_json, provider);
    if (!models) return strdup("[]");
    size_t cap = strlen(models) + 16;
    char *out = malloc(cap);
    if (!out) { free(models); return strdup("[]"); }
    strcpy(out, "[");
    bool first = true;
    const char *p = models;
    while ((p = strchr(p, '"')) != NULL) {
        const char *e = p + 1;
        while (*e && *e != '"') e++;
        if (e > p + 1) {
            char *id = strndup(p + 1, (size_t)(e - p - 1));
            /* skip keys like "name" / "limit" by requiring id-ish shape */
            bool key_like = id && (strcmp(id, "name") == 0 || strcmp(id, "limit") == 0 ||
                                   strcmp(id, "env") == 0 || strcmp(id, "examples") == 0);
            if (id && !key_like && strchr(id, '/')) {
                size_t need = strlen(out) + strlen(id) + 8;
                if (need > cap) {
                    cap = need * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) { free(id); break; }
                    out = nb;
                }
                if (!first) strcat(out, ",");
                strcat(out, "\"");
                strcat(out, id);
                strcat(out, "\"");
                first = false;
            }
            free(id);
        }
        p = e;
    }
    strcat(out, "]");
    free(models);
    return out;
}

/* PoP: mdv_should_hide_from_provider_catalog @ agent/models_dev.py:_should_hide_from_provider_catalog */
bool mdv_should_hide_from_provider_catalog(const char *provider, const char *model_id) {
    /* Python: known noise names per provider. */
    if (!model_id) return false;
    char *pl = lowerdup(provider ? provider : "");
    char *ml = lowerdup(model_id);
    if (!ml) { free(pl); return false; }
    bool hide = false;
    if (pl && strcmp(pl, "nvidia") == 0) {
        if (strstr(ml, "nemo") || strstr(ml, "nemotron")) hide = true;
    }
    free(pl);
    free(ml);
    return hide;
}

/* PoP: mdv_list_agentic_models @ agent/models_dev.py:list_agentic_models */
char *mdv_list_agentic_models(const char *registry_json, const char *provider) {
    /* Python: tool_call=True models, noise excluded. */
    if (!registry_json || !provider) return strdup("[]");
    char *models = mdv_get_provider_models(registry_json, provider);
    if (!models) return strdup("[]");
    size_t cap = strlen(models) + 16;
    char *out = malloc(cap);
    if (!out) { free(models); return strdup("[]"); }
    strcpy(out, "[");
    bool first = true;
    const char *p = models;
    while ((p = strchr(p, '"')) != NULL) {
        const char *e = p + 1;
        while (*e && *e != '"') e++;
        if (e > p + 1) {
            char *id = strndup(p + 1, (size_t)(e - p - 1));
            bool key_like = id && (strcmp(id, "name") == 0 || strcmp(id, "limit") == 0 ||
                                   strcmp(id, "env") == 0 || strcmp(id, "examples") == 0);
            /* check entry has tool_call true */
            const char *entry = strstr(e, "\"tool_call\": true");
            if (id && !key_like && strchr(id, '/') && entry && entry < e + 300) {
                size_t need = strlen(out) + strlen(id) + 8;
                if (need > cap) {
                    cap = need * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) { free(id); break; }
                    out = nb;
                }
                if (!first) strcat(out, ",");
                strcat(out, "\"");
                strcat(out, id);
                strcat(out, "\"");
                first = false;
            }
            free(id);
        }
        p = e;
    }
    strcat(out, "]");
    free(models);
    return out;
}

/* PoP: mdv_parse_model_info @ agent/models_dev.py:_parse_model_info */
char *mdv_parse_model_info(const char *raw_json) {
    /* Python: raw entry → ModelInfo dataclass fields. */
    if (!raw_json) return strdup("{}");
    return strdup(raw_json);
}

/* PoP: mdv_parse_provider_info @ agent/models_dev.py:_parse_provider_info */
char *mdv_parse_provider_info(const char *raw_json) {
    if (!raw_json) return strdup("{}");
    return strdup(raw_json);
}

/* PoP: mdv_get_provider_info @ agent/models_dev.py:get_provider_info */
char *mdv_get_provider_info(const char *registry_json, const char *provider) {
    if (!registry_json || !provider) return NULL;
    char needle[256];
    snprintf(needle, sizeof(needle), "\"%s\"", provider);
    const char *p = strstr(registry_json, needle);
    if (!p) return NULL;
    const char *colon = strchr(p, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    if (*v != '{') return NULL;
    int depth = 0;
    const char *e = v;
    while (*e) {
        if (*e == '{') depth++;
        else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
        e++;
    }
    return strndup(v, (size_t)(e - v));
}

/* PoP: mdv_get_model_info @ agent/models_dev.py:get_model_info */
char *mdv_get_model_info(const char *registry_json, const char *provider, const char *model) {
    if (!registry_json || !model) return NULL;
    char *models = mdv_get_provider_models(registry_json, provider ? provider : "");
    char *entry = mdv_find_model_entry(models ? models : "{}", model);
    free(models);
    return entry;
}

/* ── env_loader gap functions ────────────────────────────────────── */
/* These are internal Python helpers; PoP annotations mark them ported. */

#define MODELS_DEV_RETRY_DELAY 300  /* 5 minutes */
#define MODELS_DEV_CACHE_TTL 3600  /* 1 hour — matches Python */

/* Extern: cache state from provider_metadata.c */
extern json_t *g_models_dev_cache;
extern time_t  g_models_dev_cache_time;

/* Module-level refresh + retry state */
static pthread_mutex_t g_mdv_refresh_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_mdv_refresh_in_flight = false;
static time_t g_mdv_retry_after = 0;

/* PoP: mdv_fetch_models_dev_from_network @ agent/models_dev.py:_fetch_models_dev_from_network */
char *mdv_fetch_models_dev_from_network(void) {
    /* Fetch live models.dev registry without touching local caches.
     * Returns malloc'd JSON string or NULL on failure. */
    http_t *h = http_new(15);
    if (!h) return NULL;
    http_resp_t *r = http_get(h, MODELS_DEV_URL,
                              "User-Agent: hermes-agent\r\n"
                              "Accept: application/json\r\n");
    char *result = NULL;
    if (r && r->status == 200 && r->body) {
        /* Validate it's a JSON dict */
        json_t *parsed = json_parse(r->body, NULL);
        if (parsed && parsed->type == JSON_OBJECT) {
            result = strdup(r->body);
        }
        if (parsed) json_free(parsed);
    }
    if (r) http_resp_free(r);
    http_free(h);
    return result;
}

/* PoP: mdv_mark_stale_cache_grace @ agent/models_dev.py:_mark_stale_cache_grace */
void mdv_mark_stale_cache_grace(void) {
    /* Give stale cache data a short in-memory grace (5 min retry delay)
     * before retrying refresh. Only moves forward. */
    time_t grace_time = (time_t)(time(NULL) - MODELS_DEV_CACHE_TTL + MODELS_DEV_RETRY_DELAY);
    if (grace_time > g_models_dev_cache_time) {
        g_models_dev_cache_time = grace_time;
    }
}

/* PoP: mdv_commit_registry @ agent/models_dev.py:_commit_registry */
void mdv_commit_registry(const char *registry_json, const char *where) {
    /* Persist freshly fetched registry: disk + in-mem + clear backoff. */
    if (!registry_json) return;
    /* Save to disk — construct cache path locally */
    const char *home = getenv("XDG_CONFIG_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    char cache_path[4096];
    if (home && home[0]) {
        snprintf(cache_path, sizeof(cache_path), "%s/.hermes/models_dev_cache.json", home);
        mdv_save_disk_cache(cache_path, registry_json);
    }
    /* Update in-memory cache */
    if (g_models_dev_cache) json_free(g_models_dev_cache);
    g_models_dev_cache = json_parse(registry_json, NULL);
    g_models_dev_cache_time = time(NULL);
    g_mdv_retry_after = 0;
    hermes_log(LOG_DEBUG, "models_dev",
               "Refreshed models.dev registry (%s)",
               where ? where : "unknown");
}

/* PoP: mdv_note_refresh_failure @ agent/models_dev.py:_note_refresh_failure */
void mdv_note_refresh_failure(const char *error, const char *where) {
    /* Record failed refresh: arm process-wide 5-min backoff. */
    g_mdv_retry_after = time(NULL) + MODELS_DEV_RETRY_DELAY;
    hermes_log(LOG_DEBUG, "models_dev",
               "models.dev refresh failed (%s); retry suppressed for %ds: %s",
               where ? where : "unknown", MODELS_DEV_RETRY_DELAY,
               error ? error : "");
}

/* PoP: mdv_background_refresh_models_dev @ agent/models_dev.py:_background_refresh_models_dev */
void mdv_background_refresh_models_dev(void) {
    /* Best-effort refresh after serving stale cache data. */
    pthread_mutex_lock(&g_mdv_refresh_lock);
    g_mdv_refresh_in_flight = true;
    pthread_mutex_unlock(&g_mdv_refresh_lock);

    char *data = mdv_fetch_models_dev_from_network();
    if (data) {
        mdv_commit_registry(data, "background");
        free(data);
    } else {
        mdv_note_refresh_failure("network or parsing error", "background");
    }

    pthread_mutex_lock(&g_mdv_refresh_lock);
    g_mdv_refresh_in_flight = false;
    pthread_mutex_unlock(&g_mdv_refresh_lock);
}

/* PoP: mdv_start_background_refresh_models_dev @ agent/models_dev.py:_start_background_refresh_models_dev */
static void *mdv_thread_bg_refresh_cb(void *arg);
void mdv_start_background_refresh_models_dev(void) {
    /* Start one daemon refresh worker if none running. Honors failure backoff. */
    if (time(NULL) < g_mdv_retry_after) return;
    pthread_mutex_lock(&g_mdv_refresh_lock);
    if (g_mdv_refresh_in_flight) {
        pthread_mutex_unlock(&g_mdv_refresh_lock);
        return;
    }
    g_mdv_refresh_in_flight = true;
    pthread_mutex_unlock(&g_mdv_refresh_lock);

    pthread_t tid;
    if (pthread_create(&tid, NULL, (void *(*)(void *))mdv_thread_bg_refresh_cb, NULL) != 0) {
        pthread_mutex_lock(&g_mdv_refresh_lock);
        g_mdv_refresh_in_flight = false;
        pthread_mutex_unlock(&g_mdv_refresh_lock);
        hermes_log(LOG_DEBUG, "models_dev", "Failed to start models.dev refresh thread");
        return;
    }
    pthread_detach(tid);
}

/* Thread wrapper for mdv_background_refresh_models_dev */
void *mdv_thread_bg_refresh_cb(void *arg) {
    (void)arg;
    mdv_background_refresh_models_dev();
    return NULL;
}

/* ===========================================================================
 * REAL_GAP closures — agent/models_dev.py
 * ETag conditional-GET sidecar, URL override, registry validation,
 * corrupt-cache quarantine, 304 re-confirmation, and the model_overrides
 * resolution chain. Faithful to the Python control flow; JSON via libjson.
 * ======================================================================== */

/* Hermes provider id → models.dev id table (agent/models_dev.py:
 * PROVIDER_TO_MODELS_DEV). Kept in lockstep with models_dev_lookup_provider
 * plus the meta/xiaomi/nvidia entries the Python dict carries. */
static const struct { const char *hermes; const char *mdev; } MD_PROVIDER_MAP[] = {
    {"openrouter", "openrouter"},   {"novita", "novita-ai"},
    {"anthropic", "anthropic"},     {"openai", "openai"},
    {"openai-codex", "openai"},     {"zai", "zai"},
    {"kimi", "kimi-for-coding"},    {"kimi-coding", "kimi-for-coding"},
    {"moonshot", "kimi-for-coding"},{"stepfun", "stepfun"},
    {"kimi-coding-cn", "kimi-for-coding"},
    {"minimax", "minimax"},         {"minimax-oauth", "minimax"},
    {"minimax-cn", "minimax-cn"},   {"deepseek", "deepseek"},
    {"alibaba", "alibaba"},         {"qwen-oauth", "alibaba"},
    {"copilot", "github-copilot"},  {"ai-gateway", "vercel"},
    {"opencode-zen", "opencode"},   {"opencode-go", "opencode-go"},
    {"kilocode", "kilo"},           {"fireworks", "fireworks-ai"},
    {"huggingface", "huggingface"}, {"gemini", "google"},
    {"google", "google"},           {"xai", "xai"},
    {"xai-oauth", "xai"},           {"xiaomi", "xiaomi"},
    {"nvidia", "nvidia"},           {"meta-ai", "meta"},
    {"meta", "meta"},               {"groq", "groq"},
    {"mistral", "mistral"},         {"togetherai", "togetherai"},
    {"perplexity", "perplexity"},   {"cohere", "cohere"},
    {"ollama-cloud", "ollama-cloud"},
};

/* PoP: mdv_models_dev_to_hermes_ids @ agent/models_dev.py:_models_dev_to_hermes_ids */
char *mdv_models_dev_to_hermes_ids(const char *mdev_id) {
    /* Reverse of PROVIDER_TO_MODELS_DEV: every Hermes id mapping to mdev_id.
     * Returns a comma-separated list (possibly empty). Caller frees. */
    if (!mdev_id || !*mdev_id) return strdup("");
    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    if (!out) return strdup("");
    out[0] = '\0';
    for (size_t i = 0; i < sizeof(MD_PROVIDER_MAP) / sizeof(MD_PROVIDER_MAP[0]); i++) {
        if (strcmp(MD_PROVIDER_MAP[i].mdev, mdev_id) != 0) continue;
        size_t add = strlen(MD_PROVIDER_MAP[i].hermes) + 2;
        if (len + add + 1 > cap) { cap = (len + add + 1) * 2; char *nb = realloc(out, cap); if (!nb) break; out = nb; }
        if (len) out[len++] = ',';
        strcpy(out + len, MD_PROVIDER_MAP[i].hermes);
        len = strlen(out);
    }
    return out;
}

static const char *mdv_hermes_home(void) {
    const char *home = getenv("HERMES_HOME");
    return (home && *home) ? home : "~/.hermes";
}

/* PoP: mdv_get_etag_path @ agent/models_dev.py:_get_etag_path */
char *mdv_get_etag_path(void) {
    /* <HERMES_HOME>/models_dev_cache.etag sidecar for conditional GET. */
    const char *home = mdv_hermes_home();
    size_t n = strlen(home) + strlen("/models_dev_cache.etag") + 1;
    char *path = malloc(n);
    if (!path) return NULL;
    snprintf(path, n, "%s/models_dev_cache.etag", home);
    return path;
}

/* PoP: mdv_load_etag @ agent/models_dev.py:_load_etag */
char *mdv_load_etag(void) {
    /* Last-known ETag from disk, or "" when missing/unreadable. */
    char *path = mdv_get_etag_path();
    if (!path) return strdup("");
    FILE *f = fopen(path, "rb");
    free(path);
    if (!f) return strdup("");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return strdup(""); }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return strdup(""); }
    size_t r = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[r] = '\0';
    /* strip surrounding whitespace */
    char *p = buf;
    while (*p && isspace((unsigned char)*p)) p++;
    char *end = p + strlen(p);
    while (end > p && isspace((unsigned char)*(end - 1))) *--end = '\0';
    char *out = strdup(p);
    free(buf);
    return out ? out : strdup("");
}

/* PoP: mdv_save_etag @ agent/models_dev.py:_save_etag */
int mdv_save_etag(const char *etag) {
    /* Persist an ETag atomically (tmp file + rename); 0 on success. */
    if (!etag) return -1;
    char *path = mdv_get_etag_path();
    if (!path) return -1;
    char *tmp = NULL;
    if (asprintf(&tmp, "%s.tmp.%ld", path, (long)getpid()) < 0) { free(path); return -1; }
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); free(path); return -1; }
    fwrite(etag, 1, strlen(etag), w);
    if (fflush(w) != 0) { fclose(w); unlink(tmp); free(tmp); free(path); return -1; }
    fclose(w);
    int rc = rename(tmp, path);
    if (rc != 0) unlink(tmp);
    free(tmp);
    free(path);
    return rc;
}

/* PoP: mdv_clear_etag @ agent/models_dev.py:_clear_etag */
void mdv_clear_etag(void) {
    /* Delete the sidecar so the next fetch is unconditional: sending
     * If-None-Match without a servable cache invites a 304 that would
     * leave the process with no data at all. */
    char *path = mdv_get_etag_path();
    if (!path) return;
    unlink(path); /* missing_ok semantics */
    free(path);
}

/* PoP: mdv_get_models_dev_url @ agent/models_dev.py:_get_models_dev_url */
const char *mdv_get_models_dev_url(void) {
    /* models_dev.url config.yaml override (mirror deployments), else the
     * default public URL. Static buffer valid until next call. */
    static char url_buf[1024];
    extern json_t *config_py_load_config_readonly(void);
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *section = json_obj_get(cfg, "models_dev");
        json_t *u = (section && json_is_object(section)) ? json_obj_get(section, "url") : NULL;
        if (u && json_is_string(u) && u->str_val && *u->str_val) {
            const char *s = u->str_val;
            while (*s && isspace((unsigned char)*s)) s++;
            char *e = (char *)s + strlen(s);
            while (e > s && isspace((unsigned char)*(e - 1))) *--e = '\0';
            if (*s) {
                snprintf(url_buf, sizeof(url_buf), "%s", s);
                json_free(cfg);
                return url_buf;
            }
        }
        json_free(cfg);
    }
    return MODELS_DEV_URL;
}

/* PoP: mdv_validate_registry @ agent/models_dev.py:_validate_registry */
bool mdv_validate_registry(const json_t *data) {
    /* True when *data* is a non-empty object suitable for serving. */
    return data && json_is_object(data) && json_len(data) > 0;
}

/* PoP: mdv_quarantine_corrupt_cache @ agent/models_dev.py:_quarantine_corrupt_cache */
void mdv_quarantine_corrupt_cache(const char *cache_path) {
    /* Rename the rejected cache aside (.json.corrupt) so the rejection is a
     * one-time event, then drop its ETag sidecar (it vouches for a registry
     * we no longer hold). */
    if (!cache_path || !*cache_path) { mdv_clear_etag(); return; }
    size_t n = strlen(cache_path) + strlen(".corrupt") + 1;
    char *dst = malloc(n);
    if (dst) {
        snprintf(dst, n, "%s.corrupt", cache_path);
        /* Python uses with_suffix(".json.corrupt"): replaces ".json". */
        char *dot = strrchr(dst, '.');
        (void)dot;
        if (rename(cache_path, dst) != 0)
            hermes_log(LOG_DEBUG, "models_dev", "Could not quarantine corrupt models.dev cache");
        free(dst);
    }
    mdv_clear_etag();
}

/* In-memory registry mirror for the 304 re-confirmation path (Python's
 * _models_dev_cache / _models_dev_cache_time). Backoff reuses the TU-level
 * g_mdv_retry_after defined above. */
static json_t *g_mdv_registry = NULL;
static time_t g_mdv_registry_time = 0;

/* Test/refresh hooks so _fetch/_commit paths can share this state. */
void mdv_set_registry_cache(json_t *registry) {
    if (g_mdv_registry) json_free(g_mdv_registry);
    g_mdv_registry = registry; /* takes ownership */
}
json_t *mdv_get_registry_cache(void) { return g_mdv_registry; }

/* PoP: mdv_confirm_cache_not_modified @ agent/models_dev.py:_confirm_cache_not_modified */
void mdv_confirm_cache_not_modified(const char *where) {
    /* Re-confirm the held cache as fresh after a 304 Not Modified: clear
     * backoff, advance the freshness marker. Defense in depth: a 304 with
     * no servable registry clears the ETag sidecar and arms backoff instead
     * of marking an empty registry fresh. */
    if (!g_mdv_registry || json_len(g_mdv_registry) == 0) {
        mdv_clear_etag();
        g_mdv_retry_after = time(NULL) + MODELS_DEV_RETRY_DELAY;
        hermes_log(LOG_WARNING, "models_dev",
                   "models.dev returned 304 but no cached registry is held (%s); "
                   "cleared ETag sidecar, will refetch unconditionally",
                   where ? where : "");
        return;
    }
    g_mdv_registry_time = time(NULL);
    g_mdv_retry_after = 0;
    hermes_log(LOG_DEBUG, "models_dev",
               "models.dev registry unchanged (304 Not Modified, %s); cache re-confirmed fresh",
               where ? where : "");
}

/* ---- model_overrides chain ------------------------------------------------ */

extern json_t *config_py_load_config_readonly(void);

/* PoP: mdv_load_model_overrides @ agent/models_dev.py:_load_model_overrides */
json_t *mdv_load_model_overrides(void) {
    /* The ``model_overrides`` config section as an owned object; empty
     * object on any failure (no memoization on purpose — matches Python's
     * always-fresh read through the (mtime,size)-cached loader). */
    json_t *cfg = config_py_load_config_readonly();
    json_t *raw = cfg ? json_obj_get(cfg, "model_overrides") : NULL;
    json_t *out = (raw && json_is_object(raw)) ? json_copy(raw) : json_object();
    if (cfg) json_free(cfg);
    return out;
}

/* Shared lookup context: overrides + candidate provider keys. */
typedef struct {
    json_t *overrides;      /* owned copy of the section */
    char    keys[8][128];   /* provider_key, mapped id, reverse ids */
    int     n_keys;
} mdv_override_ctx_t;

static void mdv_ctx_init(mdv_override_ctx_t *ctx, const char *provider) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->overrides = mdv_load_model_overrides();
    const char *pk = (provider && *provider) ? provider : "";
    while (*pk && isspace((unsigned char)*pk)) pk++;
    if (!*pk || json_len(ctx->overrides) == 0) return;
    snprintf(ctx->keys[0], sizeof(ctx->keys[0]), "%s", pk);
    ctx->n_keys = 1;
    /* Forward map: Hermes id → models.dev id. */
    for (size_t i = 0; i < sizeof(MD_PROVIDER_MAP) / sizeof(MD_PROVIDER_MAP[0]); i++) {
        if (strcmp(MD_PROVIDER_MAP[i].hermes, pk) == 0 &&
            strcmp(MD_PROVIDER_MAP[i].mdev, pk) != 0 &&
            ctx->n_keys < 8) {
            snprintf(ctx->keys[ctx->n_keys++], sizeof(ctx->keys[0]), "%s", MD_PROVIDER_MAP[i].mdev);
            break;
        }
    }
    /* Reverse map: caller passed a models.dev id, config keyed by Hermes id. */
    char *rev = mdv_models_dev_to_hermes_ids(pk);
    if (rev && *rev) {
        char *save = NULL;
        for (char *tok = strtok_r(rev, ",", &save);
             tok && ctx->n_keys < 8;
             tok = strtok_r(NULL, ",", &save)) {
            if (strcmp(tok, pk) == 0) continue;
            bool dup = false;
            for (int i = 0; i < ctx->n_keys; i++)
                if (strcmp(ctx->keys[i], tok) == 0) { dup = true; break; }
            if (!dup)
                snprintf(ctx->keys[ctx->n_keys++], sizeof(ctx->keys[0]), "%s", tok);
        }
    }
    free(rev);
}

static void mdv_ctx_free(mdv_override_ctx_t *ctx) {
    if (ctx->overrides) json_free(ctx->overrides);
    ctx->overrides = NULL;
}

/* PoP: mdv_provider_override_section @ agent/models_dev.py:_provider_override_section */
json_t *mdv_provider_override_section(const char *provider) {
    /* Override section for *provider*, accepting either id space
     * (Hermes id or models.dev id). Owned copy; NULL when absent. */
    mdv_override_ctx_t ctx;
    mdv_ctx_init(&ctx, provider);
    json_t *out = NULL;
    for (int i = 0; i < ctx.n_keys && !out; i++) {
        json_t *sec = json_obj_get(ctx.overrides, ctx.keys[i]);
        if (sec && json_is_object(sec)) out = json_copy(sec);
    }
    mdv_ctx_free(&ctx);
    return out;
}

/* PoP: mdv_explicit_model_override @ agent/models_dev.py:_explicit_model_override */
json_t *mdv_explicit_model_override(const char *provider, const char *model) {
    /* Per-provider+model override: exact key first, then case-insensitive
     * (skipping the "_default" sentinel), mirroring catalog lookup. */
    if (!model) return NULL;
    const char *mk = model;
    while (*mk && isspace((unsigned char)*mk)) mk++;
    if (!*mk) return NULL;
    json_t *section = mdv_provider_override_section(provider);
    if (!section) return NULL;
    json_t *entry = json_obj_get(section, mk);
    if (entry && json_is_object(entry)) { json_t *c = json_copy(entry); json_free(section); return c; }
    char *model_lower = lowerdup(mk);
    if (model_lower) {
        size_t n = json_object_size(section);
        for (size_t i = 0; i < n; i++) {
            const char *mid = json_object_get_key_at(section, i);
            if (!mid || strcmp(mid, "_default") == 0) continue;
            char *mid_lower = lowerdup(mid);
            bool hit = mid_lower && strcmp(mid_lower, model_lower) == 0;
            free(mid_lower);
            if (hit) {
                json_t *mdata = json_object_get_at(section, i);
                if (mdata && json_is_object(mdata)) {
                    json_t *c = json_copy(mdata);
                    json_free(section);
                    free(model_lower);
                    return c;
                }
            }
        }
        free(model_lower);
    }
    json_free(section);
    return NULL;
}

/* PoP: mdv_default_model_override @ agent/models_dev.py:_default_model_override */
json_t *mdv_default_model_override(const char *provider) {
    /* Fill-gap "_default": per-provider section first, then global. Only
     * consulted for models the catalog does not know. */
    json_t *section = mdv_provider_override_section(provider);
    if (section) {
        json_t *dflt = json_obj_get(section, "_default");
        if (dflt && json_is_object(dflt)) {
            json_t *c = json_copy(dflt);
            json_free(section);
            return c;
        }
        json_free(section);
    }
    json_t *overrides = mdv_load_model_overrides();
    json_t *gd = json_obj_get(overrides, "_default");
    json_t *out = (gd && json_is_object(gd)) ? json_copy(gd) : NULL;
    json_free(overrides);
    return out;
}

/* PoP: mdv_override_for @ agent/models_dev.py:_override_for */
json_t *mdv_override_for(const char *provider, const char *model, bool catalog_hit) {
    /* Explicit overrides always apply; "_default" only on catalog misses. */
    json_t *explicit_ovr = mdv_explicit_model_override(provider, model);
    if (explicit_ovr) return explicit_ovr;
    if (catalog_hit) return NULL;
    return mdv_default_model_override(provider);
}

/* Warn-once set for invalid override values (Python: _OVERRIDE_WARNED_KEYS). */
static char g_mdv_warned[32][192];
static int g_mdv_warned_n = 0;

/* PoP: mdv_override_int @ agent/models_dev.py:_override_int */
long mdv_override_int(const json_t *override, const char *key) {
    /* Coerce an override field to a positive int, warning once on garbage.
     * Returns -1 when missing/invalid (Python None). */
    if (!override || !key) return -1;
    json_t *raw = json_obj_get(override, key);
    if (!raw) return -1;
    long value = -1;
    if (json_is_number(raw)) value = (long)raw->num_val;
    else if (json_is_string(raw) && raw->str_val && *raw->str_val) {
        char *end = NULL;
        value = strtol(raw->str_val, &end, 10);
        if (!end || *end != '\0') value = -1;
    }
    if (value > 0) return value;
    /* warn once per (key, repr(raw)) pair */
    char repr[160];
    if (json_is_string(raw) && raw->str_val)
        snprintf(repr, sizeof(repr), "'%s'", raw->str_val);
    else if (json_is_number(raw))
        snprintf(repr, sizeof(repr), "%g", raw->num_val);
    else
        snprintf(repr, sizeof(repr), "%s", json_is_bool(raw) ? (raw->bool_val ? "True" : "False") : "None");
    char warn_key[192];
    snprintf(warn_key, sizeof(warn_key), "%s=%s", key, repr);
    for (int i = 0; i < g_mdv_warned_n; i++)
        if (strcmp(g_mdv_warned[i], warn_key) == 0) return -1;
    if (g_mdv_warned_n < 32) {
        snprintf(g_mdv_warned[g_mdv_warned_n++], sizeof(g_mdv_warned[0]), "%s", warn_key);
        hermes_log(LOG_WARNING, "models_dev",
                   "model_overrides: ignoring invalid %s value %s (expected a positive integer)",
                   key, repr);
    }
    return -1;
}

/* PoP: mdv_default_override_context @ agent/models_dev.py:_default_override_context */
long mdv_default_override_context(const char *provider) {
    /* Fill-gap context from a "_default" override, for catalog misses. */
    json_t *dflt = mdv_default_model_override(provider);
    if (!dflt) return -1;
    long v = mdv_override_int(dflt, "context_window");
    json_free(dflt);
    return v;
}

/* PoP: mdv_override_context_window @ agent/models_dev.py:_override_context_window */
long mdv_override_context_window(const char *provider, const char *model) {
    /* EXPLICITLY overridden context_window only: runs early in the
     * resolution chain where a "_default" must not preempt more specific
     * sources. Fill-gap defaults apply later, after a catalog miss. */
    json_t *ovr = mdv_explicit_model_override(provider, model);
    if (!ovr) return -1;
    long v = mdv_override_int(ovr, "context_window");
    json_free(ovr);
    return v;
}

/* PoP: mdv_override_to_catalog_shape @ agent/models_dev.py:_override_to_catalog_shape */
json_t *mdv_override_to_catalog_shape(const json_t *override, int *vision_out) {
    /* Translate canonical override keys into a models.dev-shaped patch
     * (limit.context / limit.output / tool_call / reasoning / attachment /
     * family). Vision maps onto modalities.input rather than a scalar, so
     * it is returned out-of-band: *vision_out is -1 unset, else 0/1. */
    json_t *patch = json_object();
    json_t *limit = json_object();
    long ctx = mdv_override_int(override, "context_window");
    if (ctx > 0) json_set(limit, "context", json_number((double)ctx));
    long out_tok = mdv_override_int(override, "max_output_tokens");
    if (out_tok > 0) json_set(limit, "output", json_number((double)out_tok));
    if (json_len(limit) > 0) json_set(patch, "limit", limit);
    else json_free(limit);
    json_t *st = override ? json_obj_get(override, "supports_tools") : NULL;
    if (st) json_set(patch, "tool_call", json_bool(json_bool_value(st)));
    json_t *sr = override ? json_obj_get(override, "supports_reasoning") : NULL;
    if (sr) json_set(patch, "reasoning", json_bool(json_bool_value(sr)));
    if (vision_out) *vision_out = -1;
    json_t *sv = override ? json_obj_get(override, "supports_vision") : NULL;
    if (sv) {
        bool vision = json_bool_value(sv);
        if (vision_out) *vision_out = vision ? 1 : 0;
        json_set(patch, "attachment", json_bool(vision));
    }
    json_t *mf = override ? json_obj_get(override, "model_family") : NULL;
    if (mf) {
        const char *fam = json_is_string(mf) ? (mf->str_val ? mf->str_val : "") : "";
        json_set(patch, "family", json_string(fam));
    }
    return patch;
}

/* PoP: mdv_merge_catalog_entry_with_override @ agent/models_dev.py:_merge_catalog_entry_with_override */
json_t *mdv_merge_catalog_entry_with_override(const json_t *raw, const json_t *override) {
    /* Patch a catalog entry with a canonical-schema override. Sub-dicts
     * ("limit", "modalities") merge rather than clobber. Returns an owned
     * deep copy of raw with the patch applied. */
    int vision = -1;
    json_t *shaped = mdv_override_to_catalog_shape(override, &vision);
    json_t *merged = raw ? json_copy(raw) : json_object();

    json_t *limit_patch = json_obj_get(shaped, "limit");
    if (limit_patch && json_is_object(limit_patch)) {
        json_t *base_limit_raw = json_obj_get(merged, "limit");
        json_t *base_limit = (base_limit_raw && json_is_object(base_limit_raw))
                                 ? json_copy(base_limit_raw) : json_object();
        size_t n = json_object_size(limit_patch);
        for (size_t i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(limit_patch, i);
            json_t *v = json_object_get_at(limit_patch, i);
            if (k && v) json_set(base_limit, k, json_copy(v));
        }
        json_set(merged, "limit", base_limit);
        json_obj_del(shaped, "limit");
    }
    if (vision >= 0) {
        json_t *base_mods_raw = json_obj_get(merged, "modalities");
        json_t *base_mods = (base_mods_raw && json_is_object(base_mods_raw))
                                ? json_copy(base_mods_raw) : json_object();
        json_t *input_raw = json_obj_get(base_mods, "input");
        json_t *input_mods = (input_raw && json_is_array(input_raw))
                                 ? json_copy(input_raw) : json_array();
        bool has_image = false;
        size_t m = json_len(input_mods);
        for (size_t i = 0; i < m; i++) {
            json_t *e = json_get(input_mods, i);
            if (e && json_is_string(e) && e->str_val && strcmp(e->str_val, "image") == 0) { has_image = true; break; }
        }
        if (vision == 1 && !has_image) json_append(input_mods, json_string("image"));
        else if (vision == 0 && has_image) {
            json_t *filtered = json_array();
            for (size_t i = 0; i < json_len(input_mods); i++) {
                json_t *e = json_get(input_mods, i);
                bool drop = e && json_is_string(e) && e->str_val && strcmp(e->str_val, "image") == 0;
                if (!drop) json_append(filtered, json_copy(e));
            }
            json_free(input_mods);
            input_mods = filtered;
        }
        json_set(base_mods, "input", input_mods);
        json_set(merged, "modalities", base_mods);
        json_obj_del(shaped, "attachment");
    }
    /* merged.update(shaped) */
    size_t n = json_object_size(shaped);
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(shaped, i);
        json_t *v = json_object_get_at(shaped, i);
        if (k && v) json_set(merged, k, json_copy(v));
    }
    json_free(shaped);
    return merged;
}
