/*
 * models_dev.c -- Model development metadata: disk caching, network fetch,
 * provider-model listing, capability queries.
 *
 * Extracted from provider_metadata.c (vXXX: monolith split).
 */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

/* ── constants ──────────────────────────────────────────────────────── */
#define MODELS_DEV_URL       "https://models.dev/api.json"
#define MODELS_DEV_CACHE_TTL 3600  /* 1 hour */
#define MODELS_DEV_TIMEOUT   10    /* 10s HTTP timeout */
#define MODELS_DEV_CACHE_FILE "models_dev_cache.json"

/* ── forward declarations ────────────────────────────────────────────── */
static char *get_models_dev_cache_path(void);
static json_t *models_dev_load_disk_cache(void);
static bool save_disk_cache(json_t *data);
static json_t *models_dev_fetch_network(void);
static json_t *models_dev_get_provider_models(json_t *data, const char *provider);
static json_t *models_dev_find_model(json_t *models, const char *model);
static bool models_dev_should_hide(const char *provider, const char *model_id);
static bool models_dev_is_noise(const char *model_id);
static char *models_dev_format_cost(json_t *entry);
static char *models_dev_format_capabilities(json_t *entry);
static bool models_dev_has_cost_data(json_t *entry);
static bool models_dev_supports_vision(json_t *entry);
static bool models_dev_supports_pdf(json_t *entry);
static bool models_dev_supports_audio_input(json_t *entry);
static double disk_cache_age_seconds(void);

/* ── extern references into provider_metadata.c ──────────────────────── */
extern int extract_context(json_t *entry);

/* ── file-local globals ──────────────────────────────────────────────── */
static json_t *g_models_dev_cache = NULL;
static time_t  g_models_dev_cache_time = 0;
/* PoP: get_models_dev_cache_path @ agent/models_dev.py:_get_cache_path */
static char *get_models_dev_cache_path(void) {
    const char *home = getenv("XDG_CONFIG_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home) return NULL;
    size_t len = strlen(home) + 64;
    char *path = (char *)malloc(len);
    if (!path) return NULL;
    snprintf(path, len, "%s/.hermes/%s", home, MODELS_DEV_CACHE_FILE);
    return path;
}

static double disk_cache_age_seconds(void) {
    char *path = get_models_dev_cache_path();
    if (!path) return -1.0;

    struct stat st;
    if (stat(path, &st) != 0) { free(path); return -1.0; }
    free(path);

    time_t now = time(NULL);
    double age = difftime(now, st.st_mtime);
    if (age < 0) return -1.0; /* clock skew — treat as unknown */
    return age;
}

static json_t *models_dev_load_disk_cache(void) {
    char *path = get_models_dev_cache_path();
    if (!path) return NULL;

    /* Check disk cache freshness using the age function */
    double age = disk_cache_age_seconds();
    if (age < 0 || age > MODELS_DEV_CACHE_TTL * 2) {
        free(path);
        return NULL;
    }

    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize <= 0) { fclose(f); return NULL; }
    rewind(f);

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, (size_t)fsize, f);
    fclose(f);
    buf[nread] = '\0';

    char *err = NULL;
    json_t *root = json_parse(buf, &err);
    free(buf);
    free(err);
    return root;
}

static bool save_disk_cache(json_t *data) {
    char *path = get_models_dev_cache_path();
    if (!path) return false;

    char *json_str = json_serialize(data);
    if (!json_str) { free(path); return false; }

    char tmp_path[4096];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "w");
    if (!f) { free(json_str); free(path); return false; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);

    rename(tmp_path, path);
    free(path);
    return true;
}

static json_t *models_dev_fetch_network(void) {
    http_t *h = http_new(MODELS_DEV_TIMEOUT);
    if (!h) return NULL;

    http_resp_t *resp = http_get(h, MODELS_DEV_URL, NULL);
    if (!resp || resp->status != 200) {
        if (resp) http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    char *err = NULL;
    json_t *root = json_parse(resp->body, &err);
    http_resp_free(resp);
    http_free(h);
    free(err);
    return root;
}

json_t *models_dev_fetch(bool force_refresh) {
    time_t now = time(NULL);

    /* Stage 1: fresh in-memory cache */
    if (!force_refresh && g_models_dev_cache &&
        (now - g_models_dev_cache_time) < MODELS_DEV_CACHE_TTL) {
        return g_models_dev_cache;
    }

    /* Stage 2: fresh disk cache (load into memory) */
    if (!force_refresh) {
        json_t *disk = models_dev_load_disk_cache();
        if (disk) {
            struct stat st;
            char *path = get_models_dev_cache_path();
            if (path) {
                if (stat(path, &st) == 0 &&
                    (now - st.st_mtime) < MODELS_DEV_CACHE_TTL) {
                    if (g_models_dev_cache) json_free(g_models_dev_cache);
                    g_models_dev_cache = disk;
                    g_models_dev_cache_time = now;
                    free(path);
                    return g_models_dev_cache;
                }
                free(path);
            }
            json_free(disk);
        }
    }

    /* Stage 3: network fetch */
    json_t *net = models_dev_fetch_network();
    if (net) {
        save_disk_cache(net);
        if (g_models_dev_cache) json_free(g_models_dev_cache);
        g_models_dev_cache = net;
        g_models_dev_cache_time = now;
        return g_models_dev_cache;
    }

    /* Stage 4: fallback to any disk cache (even stale) */
    json_t *stale = models_dev_load_disk_cache();
    if (stale) {
        if (g_models_dev_cache) json_free(g_models_dev_cache);
        g_models_dev_cache = stale;
        g_models_dev_cache_time = now;
        return g_models_dev_cache;
    }

    return NULL;
}

int lookup_models_dev_context(const char *provider, const char *model) {
    if (!provider || !model) return -1;
    json_t *data = models_dev_fetch(false);
    if (!data) return -1;

    json_t *prov_node = json_obj_get(data, provider);
    if (!prov_node) return -1;

    json_t *models_node = json_obj_get(prov_node, "models");
    if (!models_node) return -1;

    json_t *model_node = json_obj_get(models_node, model);
    if (!model_node) return -1;

    return extract_context(model_node);
}

char *models_dev_list_json(void) {
    json_t *data = models_dev_fetch(false);
    if (!data) return NULL;

    json_t *arr = json_array();
    if (!arr) return NULL;

    /* Iterate all provider keys */
    for (size_t i = 0; i < data->c.count; i++) {
        const char *prov_name = data->c.keys[i];
        json_t *prov = data->c.items[i];
        if (!prov_name || !prov) continue;

        json_t *models = json_obj_get(prov, "models");
        if (!models || models->type != JSON_OBJECT) continue;

        for (size_t mi = 0; mi < models->c.count; mi++) {
            const char *model_name = models->c.keys[mi];
            json_t *md = models->c.items[mi];
            if (!model_name || !md) continue;

            json_t *entry = json_object();
            json_set(entry, "id", json_string(model_name));
            json_set(entry, "provider", json_string(prov_name));

            json_t *ctx = json_obj_get(md, "context");
            if (ctx && ctx->type == JSON_NUMBER)
                json_set(entry, "context_window", json_copy(ctx));

            json_t *out = json_obj_get(md, "max_output");
            if (out && out->type == JSON_NUMBER)
                json_set(entry, "max_output", json_copy(out));

            json_t *desc = json_obj_get(md, "description");
            if (desc && desc->type == JSON_STRING)
                json_set(entry, "description", json_copy(desc));

            json_append(arr, entry);
        }
    }

    char *result = json_serialize(arr);
    json_free(arr);
    return result;
}

const char *models_dev_lookup_provider(const char *provider) {
    if (!provider || !*provider) return NULL;
    /* Maps Hermes provider names to models.dev IDs */
    static const struct { const char *hermes; const char *mdev; } map[] = {
        {"openrouter",     "openrouter"},
        {"novita",         "novita-ai"},
        {"anthropic",      "anthropic"},
        {"openai",         "openai"},
        {"openai-codex",   "openai"},
        {"zai",            "zai"},
        {"kimi",           "kimi-for-coding"},
        {"kimi-coding",    "kimi-for-coding"},
        {"moonshot",       "kimi-for-coding"},
        {"stepfun",        "stepfun"},
        {"kimi-coding-cn", "kimi-for-coding"},
        {"minimax",        "minimax"},
        {"minimax-oauth",  "minimax"},
        {"minimax-cn",     "minimax-cn"},
        {"deepseek",       "deepseek"},
        {"alibaba",        "alibaba"},
        {"qwen-oauth",     "alibaba"},
        {"copilot",        "github-copilot"},
        {"opencode-zen",   "opencode"},
        {"opencode-go",    "opencode-go"},
        {"kilocode",       "kilo"},
        {"fireworks",      "fireworks-ai"},
        {"huggingface",    "huggingface"},
        {"gemini",         "google"},
        {"google",         "google"},
        {"xai",            "xai"},
        {"xai-oauth",      "xai"},
        {"xiaomi",         "xiaomi"},
        {"nvidia",         "nvidia"},
        {"groq",           "groq"},
        {"mistral",        "mistral"},
        {"togetherai",     "togetherai"},
        {"perplexity",     "perplexity"},
        {"cohere",         "cohere"},
        {"ollama-cloud",   "ollama-cloud"},
    };
    size_t n = sizeof(map) / sizeof(map[0]);
    for (size_t i = 0; i < n; i++) {
        if (strcmp(provider, map[i].hermes) == 0)
            return map[i].mdev;
    }
    return provider; /* fallback: use provider name as-is */
}

static json_t *models_dev_get_provider_models(json_t *data, const char *provider) {
    if (!data || !provider) return NULL;
    const char *mdev_id = models_dev_lookup_provider(provider);
    if (!mdev_id) return NULL;
    json_t *prov = json_obj_get(data, mdev_id);
    if (!prov || prov->type != JSON_OBJECT) return NULL;
    json_t *models = json_obj_get(prov, "models");
    if (!models || models->type != JSON_OBJECT) return NULL;
    return models;
}

static json_t *models_dev_find_model(json_t *models, const char *model) {
    if (!models || !model) return NULL;
    /* Exact match */
    json_t *entry = json_obj_get(models, model);
    if (entry) return entry;
    /* Case-insensitive fallback */
    size_t model_len = strlen(model);
    char *model_lower = (char *)malloc(model_len + 1);
    if (!model_lower) return NULL;
    for (size_t i = 0; i < model_len; i++)
        model_lower[i] = (char)tolower((unsigned char)model[i]);
    model_lower[model_len] = '\0';
    json_t *result = NULL;
    for (size_t i = 0; i < models->c.count; i++) {
        const char *mid = models->c.keys[i];
        if (!mid) continue;
        size_t mid_len = strlen(mid);
        if (mid_len != model_len) continue;
        bool match = true;
        for (size_t j = 0; j < mid_len; j++) {
            if (tolower((unsigned char)mid[j]) != model_lower[j]) {
                match = false; break;
            }
        }
        if (match && models->c.items[i] && models->c.items[i]->type == JSON_OBJECT) {
            result = models->c.items[i];
            break;
        }
    }
    free(model_lower);
    return result;
}

char *models_dev_get_capabilities_json(const char *provider, const char *model) {
    json_t *data = models_dev_fetch(false);
    if (!data) return NULL;
    json_t *models = models_dev_get_provider_models(data, provider);
    if (!models) return NULL;
    json_t *entry = models_dev_find_model(models, model);
    if (!entry) return NULL;

    /* Extract capability flags */
    bool supports_tools = json_get_bool(entry, "tool_call", false);
    bool supports_reasoning = json_get_bool(entry, "reasoning", false);
    /* Vision: check modalities.input first, fall back to attachment */
    bool supports_vision = json_get_bool(entry, "attachment", false);
    json_t *mods = json_obj_get(entry, "modalities");
    if (mods && mods->type == JSON_OBJECT) {
        json_t *input_mods = json_obj_get(mods, "input");
        if (input_mods && input_mods->type == JSON_ARRAY) {
            supports_vision = false;
            for (size_t i = 0; i < input_mods->c.count; i++) {
                json_t *item = input_mods->c.items[i];
                if (item && item->type == JSON_STRING && strcmp(item->str_val, "image") == 0) {
                    supports_vision = true;
                    break;
                }
            }
        }
    }

    /* Extract limits from entry.limit sub-object */
    int context_window = 200000;
    int max_output_tokens = 8192;
    json_t *limit = json_obj_get(entry, "limit");
    if (limit && limit->type == JSON_OBJECT) {
        json_t *ctx = json_obj_get(limit, "context");
        if (ctx && ctx->type == JSON_NUMBER && ctx->num_val > 0)
            context_window = (int)ctx->num_val;
        json_t *out = json_obj_get(limit, "output");
        if (out && out->type == JSON_NUMBER && out->num_val > 0)
            max_output_tokens = (int)out->num_val;
    }

    const char *model_family = json_get_str(entry, "family", "");

    /* Build response */
    json_t *resp = json_object();
    json_set(resp, "supports_tools", json_bool(supports_tools));
    json_set(resp, "supports_vision", json_bool(supports_vision));
    json_set(resp, "supports_reasoning", json_bool(supports_reasoning));
    json_set(resp, "context_window", json_number(context_window));
    json_set(resp, "max_output_tokens", json_number(max_output_tokens));
    json_set(resp, "model_family", json_string(model_family));
    /* Formatted convenience strings (port of ModelCapabilities methods) */
    char *cost_str = models_dev_format_cost(entry);
    if (cost_str) {
        json_set(resp, "cost_display", json_string(cost_str));
        free(cost_str);
    }
    char *cap_str = models_dev_format_capabilities(entry);
    if (cap_str) {
        json_set(resp, "capabilities_display", json_string(cap_str));
        free(cap_str);
    }
    char *out = json_serialize(resp);
    json_free(resp);
    return out;
}

static bool models_dev_should_hide(const char *provider, const char *model_id) {
    if (!provider || !model_id) return false;
    char prov_lower[128];
    char model_lower[128];
    /* Lowercase both */
    size_t i;
    for (i = 0; provider[i] && i < 127; i++)
        prov_lower[i] = (char)tolower((unsigned char)provider[i]);
    prov_lower[i] = '\0';
    for (i = 0; model_id[i] && i < 127; i++)
        model_lower[i] = (char)tolower((unsigned char)model_id[i]);
    model_lower[i] = '\0';

    /* Google hidden models */
    bool is_google = (strcmp(prov_lower, "gemini") == 0 || strcmp(prov_lower, "google") == 0);
    if (is_google) {
        static const char *google_hidden[] = {
            "gemma-4-31b-it", "gemma-4-26b-it", "gemma-4-26b-a4b-it",
            "gemma-3-1b", "gemma-3-1b-it", "gemma-3-2b", "gemma-3-2b-it",
            "gemma-3-4b", "gemma-3-4b-it", "gemma-3-12b", "gemma-3-12b-it",
            "gemma-3-27b", "gemma-3-27b-it",
            "gemini-1.5-flash", "gemini-1.5-pro",
            "gemini-1.5-flash-8b", "gemini-2.0-flash", "gemini-2.0-flash-lite",
        };
        for (size_t j = 0; j < sizeof(google_hidden)/sizeof(google_hidden[0]); j++) {
            if (strcmp(model_lower, google_hidden[j]) == 0)
                return true;
        }
    }
    return false;
}

static bool models_dev_is_noise(const char *model_id) {
    if (!model_id) return false;
    size_t len = strlen(model_id);
    char *lower = (char *)malloc(len + 1);
    if (!lower) return false;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)model_id[i]);
    lower[len] = '\0';
    bool is_noise = false;
    if (strstr(lower, "-tts")) is_noise = true;
    else if (strstr(lower, "embedding")) is_noise = true;
    else if (strstr(lower, "live-")) is_noise = true;
    else if (strstr(lower, "-image")) is_noise = true;
    else if (strstr(lower, "-image-preview")) is_noise = true;
    else if (strstr(lower, "-customtools")) is_noise = true;
    /* Check for -preview-NNNN or -exp-NNNN patterns */
    else {
        const char *p = lower;
        while (1) {
            p = strstr(p, "-preview-");
            if (!p) {
                p = lower;
                p = strstr(p, "-exp-");
            }
            if (!p) break;
            p += 8; /* skip past "-preview-" or "-exp-" */
            if (*p && isdigit((unsigned char)*p)) { is_noise = true; break; }
        }
    }
    free(lower);
    return is_noise;
}

/* Port of Python agent/models_dev.py:ModelCapabilities.format_cost().
 * Build human-readable cost string. Returns malloc'd string, caller must free(). */
/* PoP: format_cost @ agent/models_dev.py:format_cost */
/* Port of Python agent/models_dev.py:format_cost(). */
static char *models_dev_format_cost(json_t *entry) {
    if (!models_dev_has_cost_data(entry))
        return strdup("unknown");

    json_t *cost = json_obj_get(entry, "cost");
    if (!cost || cost->type != JSON_OBJECT) return strdup("unknown");

    double cost_in = 0, cost_out = 0, cost_cache = -1;
    json_t *v = json_obj_get(cost, "input");
    if (v && v->type == JSON_NUMBER) cost_in = v->num_val;
    v = json_obj_get(cost, "output");
    if (v && v->type == JSON_NUMBER) cost_out = v->num_val;
    v = json_obj_get(cost, "cache_read");
    if (v && v->type == JSON_NUMBER) cost_cache = v->num_val;

    char buf[256];
    int n = snprintf(buf, sizeof(buf), "$%.2f/M in, $%.2f/M out", cost_in, cost_out);
    if (cost_cache >= 0 && n < (int)sizeof(buf) - 32)
        snprintf(buf + n, sizeof(buf) - (size_t)n, ", cache read $%.2f/M", cost_cache);
    return strdup(buf);
}

/* Port of Python agent/models_dev.py:ModelCapabilities.format_capabilities().
 * Build human-readable capabilities string. Returns malloc'd string, caller must free(). */
/* PoP: format_capabilities @ agent/models_dev.py:format_capabilities */
/* Port of Python agent/models_dev.py:format_capabilities(). */
static char *models_dev_format_capabilities(json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return strdup("basic");

    bool has_reasoning = json_get_bool(entry, "reasoning", false);
    bool has_tools = json_get_bool(entry, "tool_call", false);
    bool has_vision = models_dev_supports_vision(entry);
    bool has_pdf = models_dev_supports_pdf(entry);
    bool has_audio = models_dev_supports_audio_input(entry);
    bool has_structured = json_get_bool(entry, "structured_output", false);
    bool has_open = json_get_bool(entry, "open_weights", false);

    char buf[256];
    size_t pos = 0;
    buf[0] = '\0';
    if (has_reasoning) pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%sreasoning", pos > 0 ? ", " : "");
    if (has_tools)     pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%stools", pos > 0 ? ", " : "");
    if (has_vision)    pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%svision", pos > 0 ? ", " : "");
    if (has_pdf)       pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%sPDF", pos > 0 ? ", " : "");
    if (has_audio)     pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%saudio", pos > 0 ? ", " : "");
    if (has_structured) pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%sstructured output", pos > 0 ? ", " : "");
    if (has_open)      pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%sopen weights", pos > 0 ? ", " : "");

    if (pos == 0) return strdup("basic");
    return strdup(buf);
}

char *models_dev_list_provider_models(const char *provider) {
    json_t *data = models_dev_fetch(false);
    if (!data) return NULL;
    json_t *models = models_dev_get_provider_models(data, provider);
    if (!models) return strdup("[]");
    json_t *arr = json_array();
    for (size_t i = 0; i < models->c.count; i++) {
        const char *mid = models->c.keys[i];
        if (!mid) continue;
        if (models_dev_should_hide(provider, mid)) continue;
        json_append(arr, json_string(mid));
    }
    char *result = json_serialize(arr);
    json_free(arr);
    return result;
}

char *models_dev_list_agentic_models(const char *provider) {
    json_t *data = models_dev_fetch(false);
    if (!data) return NULL;
    json_t *models = models_dev_get_provider_models(data, provider);
    if (!models) return strdup("[]");
    json_t *arr = json_array();
    for (size_t i = 0; i < models->c.count; i++) {
        const char *mid = models->c.keys[i];
        json_t *entry = models->c.items[i];
        if (!mid || !entry || entry->type != JSON_OBJECT) continue;
        if (models_dev_should_hide(provider, mid)) continue;
        if (!json_get_bool(entry, "tool_call", false)) continue;
        if (models_dev_is_noise(mid)) continue;
        json_append(arr, json_string(mid));
    }
    char *result = json_serialize(arr);
    json_free(arr);
    return result;
}

char *models_dev_get_provider_info_json(const char *provider_id) {
    if (!provider_id) return NULL;
    json_t *data = models_dev_fetch(false);
    if (!data) return NULL;
    const char *mdev_id = models_dev_lookup_provider(provider_id);
    if (!mdev_id) return NULL;
    json_t *raw = json_obj_get(data, mdev_id);
    if (!raw || raw->type != JSON_OBJECT) return NULL;

    const char *name = json_get_str(raw, "name", mdev_id);
    json_t *env_arr = json_obj_get(raw, "env");
    const char *api = json_get_str(raw, "api", "");
    const char *doc = json_get_str(raw, "doc", "");
    json_t *models = json_obj_get(raw, "models");
    int model_count = (models && models->type == JSON_OBJECT) ? (int)models->c.count : 0;

    json_t *resp = json_object();
    json_set(resp, "id", json_string(mdev_id));
    json_set(resp, "name", json_string(name));
    json_set(resp, "api", json_string(api));
    json_set(resp, "doc", json_string(doc));
    json_set(resp, "model_count", json_number(model_count));
    if (env_arr && env_arr->type == JSON_ARRAY) {
        json_set(resp, "env", json_copy(env_arr));
    } else {
        json_set(resp, "env", json_array());
    }
    char *out = json_serialize(resp);
    json_free(resp);
    return out;
}

char *models_dev_get_model_info_json(const char *provider_id, const char *model_id) {
    if (!provider_id || !model_id) return NULL;
    json_t *data = models_dev_fetch(false);
    if (!data) return NULL;
    const char *mdev_id = models_dev_lookup_provider(provider_id);
    if (!mdev_id) return NULL;
    json_t *pdata = json_obj_get(data, mdev_id);
    if (!pdata || pdata->type != JSON_OBJECT) return NULL;
    json_t *models = json_obj_get(pdata, "models");
    if (!models || models->type != JSON_OBJECT) return NULL;
    json_t *entry = models_dev_find_model(models, model_id);
    if (!entry) return NULL;

    /* Parse fields from model entry */
    json_t *limit = json_obj_get(entry, "limit");
    if (limit && limit->type != JSON_OBJECT) limit = NULL;
    json_t *cost = json_obj_get(entry, "cost");
    if (cost && cost->type != JSON_OBJECT) cost = NULL;
    json_t *modalities = json_obj_get(entry, "modalities");
    if (modalities && modalities->type != JSON_OBJECT) modalities = NULL;

    const char *name = json_get_str(entry, "name", model_id);
    const char *family = json_get_str(entry, "family", "");
    bool reasoning = json_get_bool(entry, "reasoning", false);
    bool tool_call = json_get_bool(entry, "tool_call", false);
    bool attachment = json_get_bool(entry, "attachment", false);
    bool temperature = json_get_bool(entry, "temperature", false);
    bool structured_output = json_get_bool(entry, "structured_output", false);
    bool open_weights = json_get_bool(entry, "open_weights", false);

    /* Limits */
    int ctx = 0, out = 0, inp = 0;
    if (limit) {
        json_t *v = json_obj_get(limit, "context");
        if (v && v->type == JSON_NUMBER && v->num_val > 0) ctx = (int)v->num_val;
        v = json_obj_get(limit, "output");
        if (v && v->type == JSON_NUMBER && v->num_val > 0) out = (int)v->num_val;
        v = json_obj_get(limit, "input");
        if (v && v->type == JSON_NUMBER && v->num_val > 0) inp = (int)v->num_val;
    }

    /* Costs */
    double cost_input = 0, cost_output = 0;
    double cost_cache_read = -1, cost_cache_write = -1;
    if (cost) {
        json_t *v = json_obj_get(cost, "input");
        if (v && v->type == JSON_NUMBER) cost_input = v->num_val;
        v = json_obj_get(cost, "output");
        if (v && v->type == JSON_NUMBER) cost_output = v->num_val;
        v = json_obj_get(cost, "cache_read");
        if (v && v->type == JSON_NUMBER) cost_cache_read = v->num_val;
        v = json_obj_get(cost, "cache_write");
        if (v && v->type == JSON_NUMBER) cost_cache_write = v->num_val;
    }

    /* Modalities */
    json_t *input_mods = NULL, *output_mods = NULL;
    if (modalities) {
        input_mods = json_obj_get(modalities, "input");
        if (input_mods && input_mods->type != JSON_ARRAY) input_mods = NULL;
        output_mods = json_obj_get(modalities, "output");
        if (output_mods && output_mods->type != JSON_ARRAY) output_mods = NULL;
    }

    const char *knowledge = json_get_str(entry, "knowledge", "");
    const char *release_date = json_get_str(entry, "release_date", "");
    const char *status = json_get_str(entry, "status", "");
    bool interleaved = json_get_bool(entry, "interleaved", false);

    /* Build JSON response matching ModelInfo dataclass */
    json_t *resp = json_object();
    json_set(resp, "id", json_string(model_id));
    json_set(resp, "name", json_string(name));
    json_set(resp, "family", json_string(family));
    json_set(resp, "provider_id", json_string(mdev_id));
    json_set(resp, "reasoning", json_bool(reasoning));
    json_set(resp, "tool_call", json_bool(tool_call));
    json_set(resp, "attachment", json_bool(attachment));
    json_set(resp, "temperature", json_bool(temperature));
    json_set(resp, "structured_output", json_bool(structured_output));
    json_set(resp, "open_weights", json_bool(open_weights));
    if (input_mods) json_set(resp, "input_modalities", json_copy(input_mods));
    else json_set(resp, "input_modalities", json_array());
    if (output_mods) json_set(resp, "output_modalities", json_copy(output_mods));
    else json_set(resp, "output_modalities", json_array());
    json_set(resp, "context_window", json_number(ctx));
    json_set(resp, "max_output", json_number(out));
    if (inp > 0) json_set(resp, "max_input", json_number(inp));
    else json_set(resp, "max_input", json_null());
    json_set(resp, "cost_input", json_number(cost_input));
    json_set(resp, "cost_output", json_number(cost_output));
    if (cost_cache_read >= 0) json_set(resp, "cost_cache_read", json_number(cost_cache_read));
    else json_set(resp, "cost_cache_read", json_null());
    if (cost_cache_write >= 0) json_set(resp, "cost_cache_write", json_number(cost_cache_write));
    else json_set(resp, "cost_cache_write", json_null());
    json_set(resp, "knowledge_cutoff", json_string(knowledge));
    json_set(resp, "release_date", json_string(release_date));
    json_set(resp, "status", json_string(status));
    json_set(resp, "interleaved", json_bool(interleaved));
    char *out_json = json_serialize(resp);
    json_free(resp);
    return out_json;
}

static bool models_dev_has_cost_data(json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return false;
    json_t *cost = json_obj_get(entry, "cost");
    if (!cost || cost->type != JSON_OBJECT) return false;
    json_t *inp = json_obj_get(cost, "input");
    if (inp && inp->type == JSON_NUMBER && inp->num_val > 0) return true;
    json_t *out = json_obj_get(cost, "output");
    if (out && out->type == JSON_NUMBER && out->num_val > 0) return true;
    return false;
}

static bool models_dev_supports_vision(json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return false;
    if (json_get_bool(entry, "attachment", false)) return true;
    json_t *mods = json_obj_get(entry, "modalities");
    if (mods && mods->type == JSON_OBJECT) {
        json_t *input_mods = json_obj_get(mods, "input");
        if (input_mods && input_mods->type == JSON_ARRAY) {
            for (size_t i = 0; i < input_mods->c.count; i++) {
                json_t *item = input_mods->c.items[i];
                if (item && item->type == JSON_STRING && strcmp(item->str_val, "image") == 0)
                    return true;
            }
        }
    }
    return false;
}

static bool models_dev_supports_pdf(json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return false;
    json_t *mods = json_obj_get(entry, "modalities");
    if (mods && mods->type == JSON_OBJECT) {
        json_t *input_mods = json_obj_get(mods, "input");
        if (input_mods && input_mods->type == JSON_ARRAY) {
            for (size_t i = 0; i < input_mods->c.count; i++) {
                json_t *item = input_mods->c.items[i];
                if (item && item->type == JSON_STRING && strcmp(item->str_val, "pdf") == 0)
                    return true;
            }
        }
    }
    return false;
}

/* PoP: models_dev_supports_audio_input @ agent/models_dev.py:supports_audio_input */
static bool models_dev_supports_audio_input(json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return false;
    json_t *mods = json_obj_get(entry, "modalities");
    if (mods && mods->type == JSON_OBJECT) {
        json_t *input_mods = json_obj_get(mods, "input");
        if (input_mods && input_mods->type == JSON_ARRAY) {
            for (size_t i = 0; i < input_mods->c.count; i++) {
                json_t *item = input_mods->c.items[i];
                if (item && item->type == JSON_STRING && strcmp(item->str_val, "audio") == 0)
                    return true;
            }
        }
    }
    return false;
}

