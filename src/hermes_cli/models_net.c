/*
 * port_models_net.c — network-backed model catalog fetch ports of
 * hermes_cli/models.py, with an INJECTABLE HTTP transport so the logic is
 * faithfully testable offline (per hermes_agent AGENTS.md: exercise the real
 * path, never stub). Every fetch function receives an `http_fetch_fn` that
 * performs the real GET (the production adapter wraps hermes_http). Tests
 * pass a mock that returns canned JSON, so the parse/filter logic runs for
 * real without network access.
 */

#include "port_models_net.h"
#include "port_models_pure.h"
#include "port_models_helpers.h"
#include "model_catalog.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* ── Injectable transport ────────────────────────────────────────────────
 * typedef int (*http_fetch_fn)(const char *url, const char *headers_json,
 *                              char **out_body, size_t *out_len, void *ctx);
 * Returns 0 on success (fills *out_body, caller frees) or non-zero on failure.
 */

/* ── Pure parse helpers (faithful ports of models.py) ───────────────────── */

/* PoP: payload_items @ hermes_cli/models.py:_payload_items */
/* PoP: payload_items @ cron/scripts/classify_items.py:_load_items */
static void payload_items(const json_t *payload, json_t ***out_items, size_t *out_n) {
    size_t cap = 16, n = 0;
    json_t **items = malloc(cap * sizeof(json_t *));
    if (!items) { *out_n = 0; *out_items = NULL; return; }
    if (!payload) { *out_n = 0; *out_items = items; return; }
    json_t *arr = NULL;
    if (payload->type == JSON_ARRAY) arr = (json_t *)payload;
    else if (payload->type == JSON_OBJECT) arr = json_obj_get(payload, "data");
    if (arr && arr->type == JSON_ARRAY) {
        for (size_t i = 0; i < arr->c.count; i++) {
            json_t *it = arr->c.items[i];
            if (it->type != JSON_OBJECT) continue;
            if (n + 1 >= cap) { cap *= 2; items = realloc(items, cap * sizeof(json_t *)); }
            items[n++] = it;
        }
    }
    *out_n = n;
    *out_items = items;
}

/* PoP: models_copilot_item_is_text_model @ hermes_cli/models.py:_copilot_catalog_item_is_text_model */
int models_copilot_item_is_text_model(const json_t *item) {
    if (!item || item->type != JSON_OBJECT) return 0;
    json_t *id = json_obj_get(item, "id");
    if (!id || id->type != JSON_STRING || !id->str_val || !id->str_val[0]) return 0;
    json_t *picker = json_obj_get(item, "model_picker_enabled");
    if (picker && picker->type == JSON_BOOL && picker->bool_val == false) return 0;
    json_t *caps = json_obj_get(item, "capabilities");
    if (caps && caps->type == JSON_OBJECT) {
        const char *mt = json_get_str(caps, "type", "");
        if (mt && *mt && strcasecmp(mt, "chat") != 0) return 0;
    }
    return 1;
}

/* PoP: models_is_github_models_base_url @ hermes_cli/models.py:_is_github_models_base_url */
int models_is_github_models_base_url(const char *base_url) {
    if (!base_url) return 0;
    char buf[1024];
    size_t n = strlen(base_url);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        char c = base_url[i];
        if (c == ' ' || c == '\t') continue;
        buf[j++] = (char)tolower((unsigned char)c);
    }
    while (j > 0 && (buf[j-1] == '/' )) j--;
    buf[j] = '\0';
    return (strncmp(buf, "https://api.github.com", 19) == 0)
        || (strncmp(buf, "https://models.github.ai/inference", 33) == 0)
        || (strncmp(buf, "https://models.inference.ai.azure.com", 37) == 0);
}

/* ── Fetchers (injectable HTTP) ─────────────────────────────────────────── */

/* PoP: models_fetch_github_model_catalog @ hermes_cli/models.py:fetch_github_model_catalog */
/* Fetch the live GitHub Copilot model catalog. Injectable transport so the parse/filter logic is testable offline. Returns a JSON array string of */
char *models_fetch_github_model_catalog(http_fetch_fn fetch, void *ctx,
                                        const char *api_key) {
    if (!fetch) return NULL;
    char *headers = copilot_default_headers_json();   /* owned */
    if (!headers) headers = strdup("{}");
    if (api_key && *api_key) {
        /* merge Authorization into the headers object */
        json_t *h = json_parse(headers, NULL);
        if (h) {
            char bearer[1024];
            snprintf(bearer, sizeof(bearer), "Bearer %s", api_key);
            json_set(h, "Authorization", json_string(bearer));
            free(headers);
            headers = json_serialize(h);
            json_free(h);
        }
    }
    char *body = NULL; size_t blen = 0;
    int rc = fetch("https://api.github.com/copilot/models", headers, &body, &blen, ctx);
    free(headers);
    if (rc != 0 || !body) { free(body); return NULL; }

    json_t *root = json_parse(body, NULL);
    free(body);
    if (!root) return NULL;

    size_t n = 0;
    json_t **items = NULL; payload_items(root, &items, &n);
    json_t *outarr = json_array();
    int found = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *it = items[i];
        if (!models_copilot_item_is_text_model(it)) continue;
        json_t *idn = json_obj_get(it, "id");
        const char *mid = (idn && idn->type == JSON_STRING) ? idn->str_val : NULL;
        if (!mid || !*mid) continue;
        /* de-dup */
        int seen = 0;
        for (size_t k = 0; k < (size_t)outarr->c.count; k++) {
            json_t *e = outarr->c.items[k];
            json_t *eid = json_obj_get(e, "id");
            if (eid && eid->type == JSON_STRING && strcmp(eid->str_val, mid) == 0) { seen = 1; break; }
        }
        if (seen) continue;
        json_append(outarr, json_copy(it));
        found = 1;
    }
    free(items);
    json_free(root);
    if (!found) { json_free(outarr); return NULL; }
    char *res = json_serialize(outarr);
    json_free(outarr);
    return res;
}

/* PoP: models_fetch_anthropic_models @ hermes_cli/models.py:_fetch_anthropic_models */
/* Fetch available models from the Anthropic /v1/models endpoint. Sorts opus > sonnet > haiku, latest first. Returns a malloc'd JSON array of */
char *models_fetch_anthropic_models(http_fetch_fn fetch, void *ctx,
                                    const char *base_url, const char *api_key) {
    if (!fetch) return NULL;
    char url[1024];
    if (base_url && *base_url) {
        size_t n = strlen(base_url);
        memcpy(url, base_url, n);
        if (url[n-1] != '/') url[n++] = '/';
        strcpy(url + n, "v1/models");
    } else {
        strcpy(url, "https://api.anthropic.com/v1/models");
    }
    char *headers = strdup("{\"anthropic-version\":\"2023-06-01\"}");
    if (api_key && *api_key) {
        json_t *h = json_parse(headers, NULL);
        if (h) {
            char bearer[1024];
            snprintf(bearer, sizeof(bearer), "Bearer %s", api_key);
            json_set(h, "Authorization", json_string(bearer));
            free(headers);
            headers = json_serialize(h);
            json_free(h);
        }
    }
    char *body = NULL; size_t blen = 0;
    int rc = fetch(url, headers, &body, &blen, ctx);
    free(headers);
    if (rc != 0 || !body) { free(body); return NULL; }
    json_t *root = json_parse(body, NULL);
    free(body);
    if (!root) return NULL;
    size_t n = 0;
    json_t **items = NULL; payload_items(root, &items, &n);
    /* collect ids */
    char **ids = malloc((n + 1) * sizeof(char *));
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *idn = json_obj_get(items[i], "id");
        if (idn && idn->type == JSON_STRING && idn->str_val && *idn->str_val)
            ids[m++] = strdup(idn->str_val);
    }
    free(items);
    json_free(root);
    /* sort: opus first, then sonnet, then haiku, then alphabetical */
    for (size_t a = 0; a + 1 < m; a++)
        for (size_t b = a + 1; b < m; b++) {
            int ra = (strstr(ids[a], "opus") ? 0 : strstr(ids[a], "sonnet") ? 1 :
                      strstr(ids[a], "haiku") ? 2 : 3);
            int rb = (strstr(ids[b], "opus") ? 0 : strstr(ids[b], "sonnet") ? 1 :
                      strstr(ids[b], "haiku") ? 2 : 3);
            if (ra > rb || (ra == rb && strcmp(ids[a], ids[b]) > 0)) {
                char *t = ids[a]; ids[a] = ids[b]; ids[b] = t;
            }
        }
    json_t *arr = json_array();
    for (size_t i = 0; i < m; i++) { json_append(arr, json_string(ids[i])); free(ids[i]); }
    free(ids);
    char *res = json_serialize(arr);
    json_free(arr);
    return res;
}

/* PoP: models_fetch_lmstudio_models @ hermes_cli/models.py:fetch_lmstudio_models */
/* Fetch the local LM Studio /v1/models catalog. Returns a malloc'd JSON */
char *models_fetch_lmstudio_models(http_fetch_fn fetch, void *ctx,
                                   const char *server_root) {
    if (!fetch) return NULL;
    char url[1024];
    if (!server_root || !*server_root) server_root = "http://localhost:1234";
    snprintf(url, sizeof(url), "%s/v1/models", server_root);
    char *headers = strdup("{\"Accept\":\"application/json\"}");
    char *body = NULL; size_t blen = 0;
    int rc = fetch(url, headers, &body, &blen, ctx);
    free(headers);
    if (rc != 0 || !body) { free(body); return NULL; }
    json_t *root = json_parse(body, NULL);
    free(body);
    if (!root) return NULL;
    size_t n = 0;
    json_t **items = NULL; payload_items(root, &items, &n);
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++) {
        json_t *idn = json_obj_get(items[i], "id");
        if (idn && idn->type == JSON_STRING && idn->str_val && *idn->str_val)
            json_append(arr, json_string(idn->str_val));
    }
    free(items);
    json_free(root);
    char *res = json_serialize(arr);
    json_free(arr);
    return res;
}

/* PoP: models_fetch_ollama_cloud_models @ hermes_cli/models.py:fetch_ollama_cloud_models */
/* Fetch the Ollama Cloud catalog. Returns a malloc'd JSON array of model-id */
char *models_fetch_ollama_cloud_models(http_fetch_fn fetch, void *ctx,
                                       const char *server_root) {
    if (!fetch) return NULL;
    char url[1024];
    if (!server_root || !*server_root) server_root = "https://api.ollama.com";
    snprintf(url, sizeof(url), "%s/api/tags", server_root);
    char *headers = strdup("{\"Accept\":\"application/json\"}");
    char *body = NULL; size_t blen = 0;
    int rc = fetch(url, headers, &body, &blen, ctx);
    free(headers);
    if (rc != 0 || !body) { free(body); return NULL; }
    json_t *root = json_parse(body, NULL);
    free(body);
    if (!root) return NULL;
    /* Ollama /api/tags returns {"models":[{"name":...}]} */
    json_t *arr = json_array();
    json_t *models = (root->type == JSON_OBJECT) ? json_obj_get(root, "models") : NULL;
    if (models && models->type == JSON_ARRAY) {
        for (size_t i = 0; i < models->c.count; i++) {
            json_t *it = models->c.items[i];
            if (it->type != JSON_OBJECT) continue;
            json_t *nm = json_obj_get(it, "name");
            if (nm && nm->type == JSON_STRING && nm->str_val && *nm->str_val)
                json_append(arr, json_string(nm->str_val));
        }
    }
    json_free(root);
    char *res = json_serialize(arr);
    json_free(arr);
    return res;
}

/* ── Pure model-tier / alias helpers (no network) ───────────────────────── */

/* PoP: models_provider_keys @ hermes_cli/models.py:_provider_keys */
void models_provider_keys(const char *provider, char *out_key, size_t key_sz,
                          char *out_norm, size_t norm_sz) {
    char raw[256];
    size_t n = strlen(provider ? provider : "");
    size_t j = 0;
    for (size_t i = 0; i < n && j + 1 < sizeof(raw); i++) {
        char c = provider[i];
        if (c == ' ' || c == '\t') continue;
        raw[j++] = (char)tolower((unsigned char)c);
    }
    raw[j] = '\0';
    const char *norm = model_normalize_provider(raw);
    snprintf(out_key, key_sz, "%s", raw);
    snprintf(out_norm, norm_sz, "%s", norm);
}

/* PoP: model @ hermes_cli/models.py:_model_in_provider_catalog */
/* True when name_lower equals any model (lowercased) in any of providers' */
int models_model_in_provider_catalog(const char *name_lower,
                                     char *const *providers) {
    if (!name_lower || !providers) return 0;
    for (size_t p = 0; providers[p]; p++) {
        if (model_provider_has_model(providers[p], name_lower)) return 1;
    }
    return 0;
}

/* PoP: models_xai_promote_top @ hermes_cli/models.py:_xai_promote_top */
/* Pin _XAI_TOP_MODEL to the front of the list. ids is a NULL-terminated */
void models_xai_promote_top(char *const *ids, char **out, size_t cap) {
    const char *TOP = "grok-build-0.1";
    size_t n = 0;
    int top_idx = -1;
    for (size_t i = 0; ids[i]; i++) n++;
    size_t o = 0;
    for (size_t i = 0; i < n && o + 1 < cap; i++) {
        if (strcmp(ids[i], TOP) == 0) { top_idx = (int)i; break; }
    }
    if (top_idx >= 0 && o + 1 < cap) out[o++] = strdup(TOP);
    for (size_t i = 0; i < n && o + 1 < cap; i++) {
        if ((int)i == top_idx) continue;
        out[o++] = strdup(ids[i]);
    }
    out[o] = NULL;
}

/* PoP: models_xai_merge_curated_extras @ hermes_cli/models.py:_xai_merge_curated_extras */
/* Append Hermes-curated xAI extras that are missing. out is a NULL-terminated */
void models_xai_merge_curated_extras(char **ids, size_t cap) {
    static const char *EXTRAS[] = { "grok-composer-2.5-fast", NULL };
    const char *TOP = "grok-build-0.1";
    size_t n = 0; for (size_t i = 0; ids[i]; i++) n++;
    for (size_t e = 0; EXTRAS[e]; e++) {
        int present = 0;
        for (size_t i = 0; ids[i]; i++) if (strcmp(ids[i], EXTRAS[e]) == 0) { present = 1; break; }
        if (present) continue;
        size_t insert_at = (n > 0 && ids[0] && strcmp(ids[0], TOP) == 0) ? 1 : n;
        if (n + 2 >= cap) break; /* no room */
        for (size_t i = n; i > insert_at; i--) ids[i] = ids[i-1];
        ids[insert_at] = strdup(EXTRAS[e]);
        ids[n+1] = NULL;
        n++;
    }
}

/* PoP: models_xai_curated_models @ hermes_cli/models.py:_xai_curated_models */
/* Fills *out (caller frees each + array) with xAI curated model ids; *out_n is
 * the count. Static fallback (Python reads models.dev disk cache; live path
 * via the injectable fetch is layered on top). */
void models_xai_curated_models(char ***out, size_t *out_n) {
    static const char *FALLBACK[] = {
        "grok-build-0.1", "grok-4.3", "grok-4.20-0309-reasoning",
        "grok-4.20-0309-non-reasoning", "grok-4.20-multi-agent-0309", NULL
    };
    size_t n = 0; for (size_t i = 0; FALLBACK[i]; i++) n++;
    char **arr = calloc(n + 1, sizeof(char *));
    for (size_t i = 0; FALLBACK[i]; i++) arr[i] = strdup(FALLBACK[i]);
    arr[n] = NULL;
    models_xai_merge_curated_extras(arr, n + 1);
    *out = arr;
    *out_n = n;
}

/* PoP: models_codex_curated_models @ hermes_cli/models.py:_codex_curated_models */
/* Fills *out (caller frees each + array) with openai-codex curated model ids.
 * The Python derives from codex_models.DEFAULT_CODEX_MODELS; we port the
 * equivalent curated list. */
void models_codex_curated_models(char ***out, size_t *out_n) {
    static const char *CODEX[] = {
        "gpt-5.4-codex", "gpt-5.5-codex", "gpt-5.4-codex-mini",
        "o4-codex", "o4-mini-codex", NULL
    };
    size_t n = 0; for (size_t i = 0; CODEX[i]; i++) n++;
    char **arr = calloc(n + 1, sizeof(char *));
    for (size_t i = 0; CODEX[i]; i++) arr[i] = strdup(CODEX[i]);
    arr[n] = NULL;
    *out = arr;
    *out_n = n;
}

/* PoP: payload @ hermes_cli/models.py:get_nous_recommended_aux_model */
/* Extract the aux model id from a Nous recommended payload (dict with a "aux_model" or "auxiliary" key, else first model in the list). Returns */
char *models_nous_recommended_aux_model(const char *recommended_json) {
    if (!recommended_json) return NULL;
    json_t *root = json_parse(recommended_json, NULL);
    if (!root || root->type != JSON_OBJECT) { json_free(root); return NULL; }
    const char *aux = json_get_str(root, "aux_model", NULL);
    if (!aux) aux = json_get_str(root, "auxiliary", NULL);
    char *res = NULL;
    if (aux && *aux) {
        res = strdup(aux);
    } else {
        json_t *models = json_obj_get(root, "models");
        if (models && models->type == JSON_ARRAY && models->c.count > 0) {
            json_t *first = models->c.items[0];
            if (first->type == JSON_STRING && first->str_val)
                res = strdup(first->str_val);
        }
    }
    json_free(root);
    return res;
}

/* ── Nous recommended-models disk cache + curated list ──────────────────── */

/* PoP: data_json @ hermes_cli/models.py:_write_nous_recommended_disk */
/* Persist data_json (already JSON-encoded) as the last-known-good payload for
 * `base`, merged into any existing per-base map, written atomically. */
void models_write_nous_recommended_disk(const char *base, const char *data_json) {
    if (!data_json || !*data_json) return;
    char *path = nous_recommended_disk_path();
    if (!path) return;
    /* Load existing blob (best-effort). */
    json_t *blob = NULL;
    char *existing = read_nous_recommended_disk(path);
    if (existing) {
        blob = json_parse(existing, NULL);
        free(existing);
    }
    if (!blob || blob->type != JSON_OBJECT) {
        if (blob) json_free(blob);
        blob = json_object();
    }
    json_t *entry = json_object();
    json_t *data = json_parse(data_json, NULL);
    if (!data) data = json_object();
    json_set(entry, "data", data);
    json_set(blob, base ? base : "", entry);

    char *serialized = json_serialize(blob);
    json_free(blob);
    if (!serialized) { free(path); return; }

    /* Atomic write: write to <path>.tmp then rename. */
    size_t plen = strlen(path);
    char *tmp = malloc(plen + 8);
    snprintf(tmp, plen + 8, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (f) {
        fputs(serialized, f);
        fclose(f);
        rename(tmp, path);
    } else {
        /* Fall back to direct write. */
        FILE *f2 = fopen(path, "w");
        if (f2) { fputs(serialized, f2); fclose(f2); }
    }
    free(tmp);
    free(serialized);
    free(path);
}

/* PoP: out @ hermes_cli/models.py:get_curated_nous_model_ids */
/* Fills *out (caller frees each + array) with the Nous curated list, preferring
 * the live catalog manifest (injectable) and falling back to the static
 * _PROVIDER_MODELS["nous"] snapshot. */
void models_get_curated_nous_model_ids(http_fetch_fn fetch, void *ctx,
                                       char ***out, size_t *out_n) {
    char **res = NULL; size_t rn = 0;
    /* Prefer the live catalog manifest (injectable). */
    if (fetch) {
        char *body = NULL; size_t blen = 0;
        int rc = fetch("https://portal.nousresearch.com/static/api/model-catalog.json",
                       "{\"Accept\":\"application/json\"}", &body, &blen, ctx);
        if (rc == 0 && body) {
            json_t *root = json_parse(body, NULL);
            free(body);
            if (root && root->type == JSON_OBJECT) {
                json_t *models = json_obj_get(root, "models");
                if (models && models->type == JSON_ARRAY && models->c.count > 0) {
                    size_t n = models->c.count;
                    res = calloc(n + 1, sizeof(char *));
                    for (size_t i = 0; i < n; i++) {
                        json_t *m = models->c.items[i];
                        if (m->type == JSON_STRING && m->str_val)
                            res[rn++] = strdup(m->str_val);
                    }
                    res[rn] = NULL;
                    json_free(root);
                    *out = res; *out_n = rn;
                    return;
                }
            }
            if (root) json_free(root);
        }
    }
    /* Fallback to the static _PROVIDER_MODELS["nous"] snapshot. */
    rn = (size_t)model_provider_model_count("nous");
    res = calloc(rn + 1, sizeof(char *));
    for (size_t i = 0; i < rn; i++) {
        const char *m = model_provider_model_at("nous", (int)i);
        res[i] = m ? strdup(m) : strdup("");
    }
    res[rn] = NULL;
    *out = res; *out_n = rn;
}

/* PoP: endpoint @ hermes_cli/models.py:fetch_nous_recommended_models */
/* Hits the Portal recommended-models endpoint (injectable HTTP) and returns the
 * parsed JSON dict string, or falls back to the disk cache on any failure. */
char *models_fetch_nous_recommended_models(http_fetch_fn fetch, void *ctx,
                                            const char *portal_base_url,
                                            int force_refresh) {
    char base[1024];
    const char *b = portal_base_url && *portal_base_url ? portal_base_url
                                                        : "https://portal.nousresearch.com";
    size_t n = strlen(b);
    if (n >= sizeof(base)) n = sizeof(base) - 1;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) { char c = b[i]; if (c==' '||c=='\t') continue; base[j++]=c; }
    while (j > 0 && base[j-1] == '/') j--;
    base[j] = '\0';

    char url[1100];
    snprintf(url, sizeof(url), "%s/api/nous/recommended-models", base);

    if (fetch && !force_refresh) {
        char *body = NULL; size_t blen = 0;
        int rc = fetch(url, "{\"Accept\":\"application/json\"}", &body, &blen, ctx);
        if (rc == 0 && body) {
            json_t *data = json_parse(body, NULL);
            free(body);
            if (data && data->type == JSON_OBJECT) {
                char *res = json_serialize(data);
                json_free(data);
                /* Persist last-known-good to disk. */
                models_write_nous_recommended_disk(base, res);
                return res;
            }
            if (data) json_free(data);
        }
    }
    /* Fall back to the last-known-good disk copy. */
    char *disk = read_nous_recommended_disk(base);
    return disk;
}

/* ── Reasoning-capability catalogs (OpenRouter / Nous Portal) ──────────── */

#define MODELS_REASONING_CAPS_DISK_TTL_SECONDS (24.0 * 3600.0)
#define MODELS_REASONING_CAPS_FAIL_TTL_SECONDS 60.0
#define MODELS_DEFAULT_NOUS_INFERENCE_BASE "https://inference-api.nousresearch.com"

/* Per-provider reasoning-caps state (memory cache + failure backoff). */
typedef struct {
    char  *cache;        /* serialized {model_id: caps-or-null} map, or NULL */
    int    disk_checked; /* one disk-mirror attempt per process              */
    double failed_at;    /* monotonic timestamp of last failed fetch, or -1  */
} caps_state;
static caps_state g_or_caps_state;
static caps_state g_nous_caps_state;

static double caps_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* PoP: models_reasoning_caps_disk_path @ hermes_cli/models.py:_reasoning_caps_disk_path */
char *models_reasoning_caps_disk_path(void) {
    char home[4096];
    hermes_home_dir(home, sizeof(home));
    size_t n = strlen(home) + strlen("/cache/reasoning_caps.json") + 1;
    char *p = malloc(n);
    if (p) snprintf(p, n, "%s/cache/reasoning_caps.json", home);
    return p;
}

/* Strip whitespace in place-ish helper for effort strings. */
static char *caps_strip_dup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    char *q = d;
    while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
    size_t L = strlen(q);
    while (L > 0 && (q[L-1]==' '||q[L-1]=='\t'||q[L-1]=='\n'||q[L-1]=='\r')) q[--L] = 0;
    memmove(d, q, L + 1);
    return d;
}

/* PoP: models_parse_openrouter_reasoning_capabilities_node @ hermes_cli/models.py:parse_openrouter_reasoning_capabilities */
/* Core parser over a parsed catalog item. Returns a new JSON node:
 *   {"supports_reasoning":true,"supported_efforts":[...]|null,"mandatory":bool}
 * or {"supports_reasoning":false} when the entry omits "reasoning" from
 * supported_parameters, or NULL when capability can't be determined. */
static json_t *models_parse_openrouter_reasoning_capabilities_node(const json_t *item)
{
    if (!item || item->type != JSON_OBJECT) return NULL;
    json_t *params = json_obj_get(item, "supported_parameters");
    if (!params || params->type != JSON_ARRAY)
        return NULL; /* field absent/malformed — unknown capability */
    int has_reasoning = 0;
    for (size_t i = 0; i < params->c.count && !has_reasoning; i++) {
        json_t *e = params->c.items[i];
        if (e->type == JSON_STRING && e->str_val && strcmp(e->str_val, "reasoning") == 0)
            has_reasoning = 1;
    }
    if (!has_reasoning) {
        json_t *neg = json_object();
        if (neg) json_set(neg, "supports_reasoning", json_bool(false));
        return neg;
    }
    json_t *reasoning = json_obj_get(item, "reasoning");
    int mandatory = 0;
    json_t *efforts = NULL;
    if (reasoning && reasoning->type == JSON_OBJECT) {
        json_t *mnd = json_obj_get(reasoning, "mandatory");
        mandatory = mnd && mnd->type == JSON_BOOL && mnd->bool_val;
        json_t *raw = json_obj_get(reasoning, "supported_efforts");
        if (raw && raw->type == JSON_ARRAY) {
            /* list(dict.fromkeys(str(effort).strip().lower() ...)): dedupe
             * preserving first-seen order; empty list stays an empty list. */
            efforts = json_array();
            for (size_t i = 0; efforts && i < raw->c.count; i++) {
                json_t *e2 = raw->c.items[i];
                if (!e2 || e2->type != JSON_STRING || !e2->str_val) continue;
                char *s = caps_strip_dup(e2->str_val);
                if (!s || !*s) { free(s); continue; }
                for (char *p = s; *p; p++) *p = (char)tolower((unsigned char)*p);
                int dup = 0;
                for (size_t j = 0; j < efforts->c.count && !dup; j++) {
                    json_t *x = efforts->c.items[j];
                    if (x->type == JSON_STRING && x->str_val && strcmp(x->str_val, s) == 0)
                        dup = 1;
                }
                if (!dup) json_append(efforts, json_string(s));
                free(s);
            }
            if (!efforts) return NULL; /* OOM */
        }
    }
    json_t *out = json_object();
    if (!out) { if (efforts) json_free(efforts); return NULL; }
    json_set(out, "supports_reasoning", json_bool(true));
    json_set(out, "supported_efforts", efforts ? efforts : json_null());
    json_set(out, "mandatory", json_bool(mandatory ? true : false));
    return out;
}

/* PoP: models_parse_openrouter_reasoning_capabilities @ hermes_cli/models.py:parse_openrouter_reasoning_capabilities */
/* Normalize one OpenRouter catalog entry's reasoning metadata. item_json is
 * one entry of the /v1/models data array. Returns a malloc'd JSON object
 * string per the tri-state contract, or NULL when capability can't be
 * determined (missing/malformed supported_parameters). Caller frees. */
char *models_parse_openrouter_reasoning_capabilities(const char *item_json)
{
    if (!item_json) return NULL;
    json_t *item = json_parse(item_json, NULL);
    json_t *caps = item ? models_parse_openrouter_reasoning_capabilities_node(item) : NULL;
    char *out = caps ? json_serialize(caps) : NULL;
    if (caps) json_free(caps);
    if (item) json_free(item);
    return out;
}

/* PoP: models_read_reasoning_caps_disk @ hermes_cli/models.py:_read_reasoning_caps_disk */
static json_t *models_read_reasoning_caps_disk(void) {
    char *path = models_reasoning_caps_disk_path();
    if (!path) return NULL;
    char *err = NULL;
    json_t *doc = json_parse_file(path, &err);
    if (err) free(err);
    free(path);
    if (!doc || doc->type != JSON_OBJECT) { if (doc) json_free(doc); return NULL; }
    return doc;
}

/* PoP: models_load_reasoning_caps_disk @ hermes_cli/models.py:_load_reasoning_caps_disk */
/* Returns the serialized caps map for url (malloc'd) and fills *age_seconds,
 * or NULL. A missing/bad ts counts as fully stale, like Python. */
static char *models_load_reasoning_caps_disk(const char *url, double *age_seconds) {
    json_t *data = models_read_reasoning_caps_disk();
    if (!data) return NULL;
    json_t *entry = json_obj_get(data, url);
    json_t *caps = entry ? json_obj_get(entry, "caps") : NULL;
    if (!entry || entry->type != JSON_OBJECT || !caps || caps->type != JSON_OBJECT ||
        caps->c.count == 0) {
        json_free(data);
        return NULL;
    }
    json_t *ts = json_obj_get(entry, "ts");
    double saved = (ts && ts->type == JSON_NUMBER) ? ts->num_val : 0.0;
    double now = (double)time(NULL);
    double age = now - saved;
    if (!(age >= 0.0)) age = MODELS_REASONING_CAPS_DISK_TTL_SECONDS; /* bad ts -> stale */
    if (age < 0.0) age = 0.0;
    if (age_seconds) *age_seconds = age;
    char *out = json_serialize(caps);
    json_free(data);
    return out;
}

/* PoP: models_save_reasoning_caps_disk @ hermes_cli/models.py:_save_reasoning_caps_disk */
/* Merge url's caps map into the shared disk mirror, atomically. Best-effort. */
static void models_save_reasoning_caps_disk(const char *url, const json_t *map) {
    char *path = models_reasoning_caps_disk_path();
    if (!path || !url || !map) { free(path); return; }
    /* Ensure the cache directory exists (parents are created by callers of
     * hermes_home_dir; only the trailing component is ours). */
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s/cache", "");
    {
        /* derive dir from path by trimming the filename */
        snprintf(dir, sizeof(dir), "%.*s", (int)(strlen(path) - strlen("/reasoning_caps.json")), path);
    #ifdef S_IRWXU
        mkdir(dir, 0755); /* ignore EEXIST */
    #endif
    }
    json_t *data = models_read_reasoning_caps_disk();
    if (!data) data = json_object();
    if (!data) { free(path); return; }
    json_t *entry = json_object();
    if (entry) {
        json_set(entry, "ts", json_number((double)time(NULL)));
        json_set(entry, "caps", json_copy(map));
        json_set(data, url, entry);
    }
    char *ser = json_serialize(data);
    json_free(data);
    if (!ser) { free(path); return; }
    size_t plen = strlen(path);
    char *tmp = malloc(plen + 8);
    if (tmp) {
        snprintf(tmp, plen + 8, "%s.tmp", path);
        FILE *f = fopen(tmp, "w");
        if (f) {
            fputs(ser, f);
            fclose(f);
            if (rename(tmp, path) != 0) unlink(tmp);
        } else {
            free(tmp);
            tmp = NULL;
        }
    }
    if (!tmp) { FILE *f2 = fopen(path, "w"); if (f2) { fputs(ser, f2); fclose(f2); } }
    free(tmp);
    free(ser);
    free(path);
}

/* PoP: models_seed_reasoning_caps @ hermes_cli/models.py:_seed_reasoning_caps */
/* Parse a /v1/models data array into a per-model caps map, mirror it to disk
 * for url, and return the serialized map (malloc'd) — or NULL when the array
 * has no usable entries (callers remember that as a failure, not empty). */
static char *models_seed_reasoning_caps(const json_t *items, const char *url) {
    if (!items || items->type != JSON_ARRAY || !url) return NULL;
    json_t *map = json_object();
    if (!map) return NULL;
    for (size_t i = 0; i < items->c.count; i++) {
        json_t *item = items->c.items[i];
        if (!item || item->type != JSON_OBJECT) continue;
        json_t *idn = json_obj_get(item, "id");
        if (!idn || idn->type != JSON_STRING || !idn->str_val) continue;
        char *mid = caps_strip_dup(idn->str_val);
        if (!mid || !*mid) { free(mid); continue; }
        json_set(map, mid, models_parse_openrouter_reasoning_capabilities_node(item));
        free(mid);
    }
    if (map->c.count == 0) { json_free(map); return NULL; }
    models_save_reasoning_caps_disk(url, map);
    char *out = json_serialize(map);
    json_free(map);
    return out;
}

/* Shared fetch of one OpenRouter-shaped /v1/models catalog → caps map.
 * Sends Accept + User-Agent because the Portal 403s anonymous reads. The
 * HTTP call itself is the injectable boundary (http_fetch_fn). */
static char *fetch_reasoning_caps_catalog(const char *url, http_fetch_fn fetch, void *ctx) {
    if (!url || !fetch) return NULL;
    char *body = NULL; size_t blen = 0;
    int rc = fetch(url,
                   "{\"Accept\":\"application/json\",\"User-Agent\":\"hermes-cli\"}",
                   &body, &blen, ctx);
    if (rc != 0 || !body) { if (body) free(body); return NULL; }
    json_t *payload = json_parse(body, NULL);
    free(body);
    if (!payload) return NULL;
    json_t *data = (payload->type == JSON_OBJECT) ? json_obj_get(payload, "data") : NULL;
    char *map = models_seed_reasoning_caps(data, url);
    json_free(payload);
    return map;
}

/* Cache-only lookup: memory, else the disk mirror (once per process). Never
 * blocks on HTTP; stale disk copies still count (a stale verdict beats no
 * verdict). Python's background warmers (_warm_reasoning_caps_async /
 * warm_*_async) are concurrency machinery around this same refresh closure
 * and are intentionally not ported here. */
static char *caps_cached(caps_state *st, const char *url) {
    if (!st->cache && !st->disk_checked) {
        st->disk_checked = 1;
        st->cache = models_load_reasoning_caps_disk(url, NULL);
    }
    return st->cache;
}

/* Fetch path with the 60-second failure backoff so hot per-turn callers
 * don't pay an HTTP round-trip while offline. force refetches past the cache. */
static char *caps_fetch(caps_state *st, const char *url, http_fetch_fn fetch,
                        void *ctx, int force) {
    if (st->cache && !force) return st->cache;
    if (st->failed_at >= 0 &&
        (caps_monotonic() - st->failed_at) < MODELS_REASONING_CAPS_FAIL_TTL_SECONDS)
        return NULL;
    char *map = fetch_reasoning_caps_catalog(url, fetch, ctx);
    if (!map) {
        st->failed_at = caps_monotonic();
        return NULL;
    }
    free(st->cache);
    st->cache = map;
    st->failed_at = -1;
    return st->cache;
}

/* PoP: models_resolve_nous_pricing_credentials @ hermes_cli/models.py:_resolve_nous_pricing_credentials */
/* Return (api_key, base_url) for Nous Portal pricing/inference. Base URL
 * precedence: NOUS_INFERENCE_BASE_URL env override → injected runtime
 * credential resolver → production default. Strips a trailing "/v1" so
 * callers append their own path. api_key is best-effort (""). */
void models_resolve_nous_pricing_credentials(nous_creds_fn resolve, void *ctx,
                                             char **out_key, char **out_base)
{
    char *env_base = NULL, *key = NULL, *creds_base = NULL;
    const char *raw = getenv("NOUS_INFERENCE_BASE_URL");
    if (raw && *raw) {
        env_base = caps_strip_dup(raw);
        if (env_base) {
            size_t L = strlen(env_base);
            while (L > 0 && env_base[L-1] == '/') env_base[--L] = 0;
            if (!*env_base) { free(env_base); env_base = NULL; }
        }
    }
    if (resolve) resolve(&key, &creds_base, ctx);
    if (creds_base) {
        char *s = caps_strip_dup(creds_base);
        free(creds_base);
        creds_base = s;
        if (creds_base) {
            size_t L = strlen(creds_base);
            while (L > 0 && creds_base[L-1] == '/') creds_base[--L] = 0;
        }
    }
    const char *pick = env_base ? env_base
                     : (creds_base && *creds_base) ? creds_base
                     : MODELS_DEFAULT_NOUS_INFERENCE_BASE;
    size_t L = strlen(pick);
    if (L >= 3 && strcmp(pick + L - 3, "/v1") == 0) L -= 3;
    char *base = malloc(L + 1);
    if (base) { memcpy(base, pick, L); base[L] = 0; }
    free(env_base);
    free(creds_base);
    if (out_key)  *out_key  = key ? key : strdup("");
    else free(key);
    if (out_base) *out_base = base ? base : strdup("");
    else free(base);
}

/* PoP: models_nous_catalog_url @ hermes_cli/models.py:nous_catalog_url */
/* The Portal /v1/models URL for the endpoint we actually talk to, resolved
 * through the documented ladder (env override → credential base → prod).
 * Returns a malloc'd URL string. Caller frees. */
char *models_nous_catalog_url(void) {
    char *key = NULL, *base = NULL;
    models_resolve_nous_pricing_credentials(NULL, NULL, &key, &base);
    free(key);
    if (!base) base = strdup(MODELS_DEFAULT_NOUS_INFERENCE_BASE);
    size_t n = strlen(base) + strlen("/v1/models") + 1;
    char *out = malloc(n);
    if (out) snprintf(out, n, "%s/v1/models", base);
    free(base);
    return out;
}

/* PoP: models_openrouter_model_reasoning_capabilities @ hermes_cli/models.py:openrouter_model_reasoning_capabilities */
/* Tri-state contract: malloc'd caps JSON with supports_reasoning true/false,
 * or NULL when unknown (catalog not loaded, model not listed, malformed).
 * Cache-only unless allow_fetch; the HTTP transport is injectable. Caller
 * frees a non-NULL result. */
char *models_openrouter_model_reasoning_capabilities(http_fetch_fn fetch, void *ctx,
                                                     const char *model_id,
                                                     int allow_fetch)
{
    static const char *URL = "https://openrouter.ai/api/v1/models";
    if (!model_id) return NULL;
    char *model = caps_strip_dup(model_id);
    if (!model || !*model) { free(model); return NULL; }
    char *map = caps_cached(&g_or_caps_state, URL);
    if (!map && allow_fetch) map = caps_fetch(&g_or_caps_state, URL, fetch, ctx, 0);
    char *out = NULL;
    if (map) {
        json_t *m = json_parse(map, NULL);
        json_t *e = m ? json_obj_get(m, model) : NULL;
        out = e ? json_serialize(e) : NULL;
        if (m) json_free(m);
    }
    free(model);
    return out;
}

/* PoP: models_nous_model_reasoning_capabilities @ hermes_cli/models.py:nous_model_reasoning_capabilities */
/* Nous Portal counterpart of models_openrouter_model_reasoning_capabilities;
 * same tri-state contract and cache-only default. */
char *models_nous_model_reasoning_capabilities(http_fetch_fn fetch, void *ctx,
                                               const char *model_id,
                                               int allow_fetch)
{
    if (!model_id) return NULL;
    char *model = caps_strip_dup(model_id);
    if (!model || !*model) { free(model); return NULL; }
    char *url = models_nous_catalog_url();
    char *out = NULL;
    if (url) {
        char *map = caps_cached(&g_nous_caps_state, url);
        if (!map && allow_fetch) map = caps_fetch(&g_nous_caps_state, url, fetch, ctx, 0);
        if (map) {
            json_t *m = json_parse(map, NULL);
            json_t *e = m ? json_obj_get(m, model) : NULL;
            out = e ? json_serialize(e) : NULL;
            if (m) json_free(m);
        }
        free(url);
    }
    free(model);
    return out;
}

/* ── Nous Portal recommended-model union helpers ────────────────────────── */

/* Collect modelName strings from a payload block (list of dicts). Returns a
 * malloc'd array of malloc'd strings; *out_n receives the count. */
static char **portal_block_names(const json_t *block, size_t *out_n) {
    size_t cap = 8, n = 0;
    char **names = malloc(cap * sizeof(char *));
    if (!names) { *out_n = 0; return NULL; }
    if (block && block->type == JSON_ARRAY) {
        for (size_t i = 0; i < block->c.count; i++) {
            json_t *entry = block->c.items[i];
            if (!entry || entry->type != JSON_OBJECT) continue;
            json_t *mn = json_obj_get(entry, "modelName");
            if (!mn || mn->type != JSON_STRING || !mn->str_val || !*mn->str_val) continue;
            if (n + 1 >= cap) { cap *= 2; names = realloc(names, cap * sizeof(char *)); if (!names) break; }
            names[n++] = strdup(mn->str_val);
        }
    }
    *out_n = n;
    return names;
}
static void free_names(char **names, size_t n) {
    for (size_t i = 0; i < n; i++) free(names[i]);
    free(names);
}

/* Shared core of union_with_portal_{free,paid}_recommendations.
 * synthesize_free_pricing adds {"prompt":"0","completion":"0"} entries for
 * portal ids missing from pricing (free side only — the paid side never
 * fabricates numbers). Returns {"ids":[...],"pricing":{...}} serialized, or
 * an echo of the inputs on any failure (silent degradation, like Python). */
static char *union_with_portal_block(http_fetch_fn fetch, void *ctx,
                                     const char *curated_ids_json,
                                     const char *pricing_json,
                                     const char *portal_base_url,
                                     int force_refresh,
                                     const char *block_key,
                                     int synthesize_free_pricing)
{
    /* Fallback builder: echo inputs unchanged. */
    json_t *ids_fb = json_parse(curated_ids_json, NULL);
    json_t *pr_fb = (pricing_json && *pricing_json) ? json_parse(pricing_json, NULL) : NULL;

    char *pj = models_fetch_nous_recommended_models(fetch, ctx, portal_base_url, force_refresh);
    json_t *root = pj ? json_parse(pj, NULL) : NULL;
    free(pj);
    json_t *block = (root && root->type == JSON_OBJECT) ? json_obj_get(root, block_key) : NULL;

    size_t nn = 0;
    char **names = block ? portal_block_names(block, &nn) : NULL;
    if (!names || nn == 0) {
        if (names) free_names(names, nn);
        if (root) json_free(root);
        /* silent degrade to inputs */
        json_t *out = json_object();
        json_set(out, "ids", ids_fb ? ids_fb : json_array());
        json_set(out, "pricing", pr_fb ? pr_fb : json_object());
        char *res = json_serialize(out);
        json_free(out);
        return res;
    }

    json_t *aug_ids = ids_fb ? ids_fb : json_array(); /* reuse parsed curated copy */
    size_t curated_n = aug_ids->type == JSON_ARRAY ? aug_ids->c.count : 0;
    /* Append portal recommendations not already curated; curated shows first. */
    for (size_t i = 0; i < nn; i++) {
        int seen = 0;
        for (size_t j = 0; j < curated_n && !seen; j++) {
            json_t *e = aug_ids->c.items[j]; /* note: grows as we append */
            if (j < aug_ids->c.count && e && e->type == JSON_STRING && e->str_val &&
                strcmp(e->str_val, names[i]) == 0) seen = 1;
        }
        if (!seen) json_append(aug_ids, json_string(names[i]));
    }

    json_t *pricing_out = pr_fb ? pr_fb : json_object();
    if (synthesize_free_pricing && pricing_out->type == JSON_OBJECT) {
        json_t *synthetic = json_object();
        json_set(synthetic, "prompt", json_string("0"));
        json_set(synthetic, "completion", json_string("0"));
        for (size_t i = 0; i < nn; i++) {
            if (!json_has(pricing_out, names[i]))
                json_set(pricing_out, names[i], json_copy(synthetic));
        }
        json_free(synthetic);
    }

    json_t *out = json_object();
    json_set(out, "ids", aug_ids);
    json_set(out, "pricing", pricing_out);
    char *res = json_serialize(out);
    json_free(out);
    if (root) json_free(root);
    free_names(names, nn);
    return res;
}

/* PoP: models_union_with_portal_free_recommendations @ hermes_cli/models.py:union_with_portal_free_recommendations */
/* Augment curated ids + pricing with the Portal's freeRecommendedModels.
 * Returns {"ids":[...],"pricing":{...}} (malloc'd JSON); failures degrade to
 * echoing the inputs. Caller frees. */
char *models_union_with_portal_free_recommendations(http_fetch_fn fetch, void *ctx,
                                                    const char *curated_ids_json,
                                                    const char *pricing_json,
                                                    const char *portal_base_url,
                                                    int force_refresh)
{
    return union_with_portal_block(fetch, ctx, curated_ids_json, pricing_json,
                                   portal_base_url, force_refresh,
                                   "freeRecommendedModels", 1);
}

/* PoP: models_union_with_portal_paid_recommendations @ hermes_cli/models.py:union_with_portal_paid_recommendations */
/* Mirror for paid-tier users over paidRecommendedModels; pricing is left
 * untouched (no fabricated numbers on the paid side). Caller frees. */
char *models_union_with_portal_paid_recommendations(http_fetch_fn fetch, void *ctx,
                                                    const char *curated_ids_json,
                                                    const char *pricing_json,
                                                    const char *portal_base_url,
                                                    int force_refresh)
{
    return union_with_portal_block(fetch, ctx, curated_ids_json, pricing_json,
                                   portal_base_url, force_refresh,
                                   "paidRecommendedModels", 0);
}

/* ── Nous free-tier check (cached entitlement probe) ────────────────────── */

#define MODELS_FREE_TIER_CACHE_TTL_SECONDS 180.0

typedef int (*nous_free_tier_probe_fn)(int force_fresh, void *ctx);
static nous_free_tier_probe_fn g_ft_probe;
static void *g_ft_probe_ctx;

/* Register the runtime probe used to fetch Nous Portal account info (the
 * Python imports hermes_cli.nous_account.get_nous_portal_account_info here;
 * in C the network-backed account layer plugs in via this setter). Passing
 * NULL restores the default unknown-state probe. */
void models_set_nous_free_tier_probe(int (*fn)(int force_fresh, void *ctx), void *ctx) {
    g_ft_probe = fn;
    g_ft_probe_ctx = ctx;
}

/* PoP: models_check_nous_free_tier @ hermes_cli/models.py:check_nous_free_tier */
/* Cached (180s TTL) free-tier check against the Portal account info. True
 * only when entitlement is known-free; unknown/error states cache False so
 * this wrapper never blocks users. */
int models_check_nous_free_tier(int force_fresh)
{
    static int cached_result = 0;
    static int have_cache = 0;
    static double cached_at = 0.0;
    double now = caps_monotonic();
    if (!force_fresh && have_cache && (now - cached_at) < MODELS_FREE_TIER_CACHE_TTL_SECONDS)
        return cached_result;

    int result = 0;
    if (g_ft_probe) result = g_ft_probe(force_fresh, g_ft_probe_ctx) ? 1 : 0;
    /* Error/unknown states cache as False — default to paid, don't block. */
    cached_result = result;
    cached_at = now;
    have_cache = 1;
    return result;
}
