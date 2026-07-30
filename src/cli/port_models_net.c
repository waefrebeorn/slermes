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
#include "port_models_helpers.h"
#include "model_catalog.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* ── Injectable transport ────────────────────────────────────────────────
 * typedef int (*http_fetch_fn)(const char *url, const char *headers_json,
 *                              char **out_body, size_t *out_len, void *ctx);
 * Returns 0 on success (fills *out_body, caller frees) or non-zero on failure.
 */

/* ── Pure parse helpers (faithful ports of models.py) ───────────────────── */

/* PoP: _payload_items @ hermes_cli/models.py:_payload_items */
/* PoP: load_items @ cron/scripts/classify_items.py:_load_items */
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

/* PoP: _copilot_catalog_item_is_text_model @ hermes_cli/models.py:_copilot_catalog_item_is_text_model */
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

/* PoP: _is_github_models_base_url @ hermes_cli/models.py:_is_github_models_base_url */
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

/* PoP: fetch_github_model_catalog @ hermes_cli/models.py:fetch_github_model_catalog */
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

/* PoP: _fetch_anthropic_models @ hermes_cli/models.py:_fetch_anthropic_models */
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

/* PoP: fetch_lmstudio_models @ hermes_cli/models.py:fetch_lmstudio_models */
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

/* PoP: fetch_ollama_cloud_models @ hermes_cli/models.py:fetch_ollama_cloud_models */
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

/* PoP: _provider_keys @ hermes_cli/models.py:_provider_keys */
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

/* PoP: _model_in_provider_catalog @ hermes_cli/models.py:_model_in_provider_catalog */
/* True when name_lower equals any model (lowercased) in any of providers' */
int models_model_in_provider_catalog(const char *name_lower,
                                     char *const *providers) {
    if (!name_lower || !providers) return 0;
    for (size_t p = 0; providers[p]; p++) {
        if (model_provider_has_model(providers[p], name_lower)) return 1;
    }
    return 0;
}

/* PoP: _xai_promote_top @ hermes_cli/models.py:_xai_promote_top */
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

/* PoP: _xai_merge_curated_extras @ hermes_cli/models.py:_xai_merge_curated_extras */
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

/* PoP: _xai_curated_models @ hermes_cli/models.py:_xai_curated_models */
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

/* PoP: _codex_curated_models @ hermes_cli/models.py:_codex_curated_models */
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

/* PoP: get_nous_recommended_aux_model @ hermes_cli/models.py:get_nous_recommended_aux_model */
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

/* PoP: _write_nous_recommended_disk @ hermes_cli/models.py:_write_nous_recommended_disk */
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

/* PoP: get_curated_nous_model_ids @ hermes_cli/models.py:get_curated_nous_model_ids */
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

/* PoP: fetch_nous_recommended_models @ hermes_cli/models.py:fetch_nous_recommended_models */
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
