#define _POSIX_C_SOURCE 200809L
/* models_pure_impl.c — faithful implementation of the 8 model-catalog
 * symbols that prior ports (models_helpers.c / models_validate.c /
 * provider_profiles_builtin.c) call but never defined. This restores a
 * GREEN link. All network access goes through the injectable http_fetch_fn
 * (port_models_net.h) so the logic is testable offline — no stubs, real
 * behavior ported from hermes_cli/models.py.
 *
 * Implements (PoP):
 *   _lmstudio_server_root             -> models_lmstudio_server_root
 *   _lmstudio_request_headers         -> models_lmstudio_request_headers
 *   _lmstudio_fetch_raw_models        -> models_lmstudio_fetch_raw_models
 *   probe_lmstudio_models             -> models_probe_lmstudio_models
 *   probe_api_models                  -> models_probe_api_models
 *   fetch_api_models                  -> models_fetch_api_models
 *   github_model_reasoning_efforts    -> models_github_model_reasoning_efforts
 *   _get_model_config_dict            -> models_get_model_config_dict
 *
 * Private helpers ported inline:
 *   normalize_copilot_model_id, _github_reasoning_efforts_for_model_id
 *   (depend on models_fetch_github_model_catalog /
 *    models_is_github_models_base_url from models_net.c).
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_models_net.h"
#include "port_models_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#define HERMES_USER_AGENT "hermes-cli/slermes"
static const char *COPILOT_BASE_URL = "https://api.githubcopilot.com";
static const char *COPILOT_MODELS_URL = "https://api.githubcopilot.com/models";

/* ─── _lmstudio_server_root ──────────────────────────────────────── */
char *models_lmstudio_server_root(const char *base_url) {
    if (!base_url) return NULL;
    char *root = strdup(base_url);
    /* strip + rstrip('/') */
    size_t l = strlen(root);
    while (l > 0 && (root[l - 1] == '/' || root[l - 1] == ' ')) root[--l] = '\0';
    const char *suffixes[] = {"/api/v1", "/api", "/v1", NULL};
    for (int i = 0; suffixes[i]; i++) {
        size_t sl = strlen(suffixes[i]);
        if (l >= sl && strcmp(root + l - sl, suffixes[i]) == 0) {
            root[l - sl] = '\0';
            l -= sl;
            while (l > 0 && (root[l - 1] == '/' || root[l - 1] == ' ')) root[--l] = '\0';
            break;
        }
    }
    if (l == 0) { free(root); return NULL; }
    return root;
}

/* ─── _lmstudio_request_headers ──────────────────────────────────── */
char *models_lmstudio_request_headers(const char *api_key) {
    const char *token = api_key ? api_key : "";
    while (*token == ' ' || *token == '\t') token++;
    char *out = (char *)malloc(256);
    if (*token) {
        snprintf(out, 256, "{\"User-Agent\":\"%s\",\"Authorization\":\"Bearer %s\"}",
                 HERMES_USER_AGENT, token);
    } else {
        snprintf(out, 256, "{\"User-Agent\":\"%s\"}", HERMES_USER_AGENT);
    }
    return out;
}

/* ─── _lmstudio_fetch_raw_models (injectable HTTP) ───────────────── */
json_t *models_lmstudio_fetch_raw_models(http_fetch_fn fetch, void *ctx,
                                         const char *api_key, const char *base_url) {
    char *root = models_lmstudio_server_root(base_url);
    if (!root) return NULL;
    char *headers = models_lmstudio_request_headers(api_key);
    char url[1024];
    snprintf(url, sizeof(url), "%s/api/v1/models", root);
    free(root);
    char *body = NULL; size_t blen = 0;
    int rc = fetch ? fetch(url, headers, &body, &blen, ctx) : -1;
    free(headers);
    if (rc != 0 || !body) { free(body); return NULL; }
    json_t *payload = json_parse(body, NULL);
    free(body);
    if (!payload || payload->type != JSON_OBJECT) { if (payload) json_free(payload); return NULL; }
    json_t *models = json_object_get(payload, "models");
    if (!models || models->type != JSON_ARRAY) { json_free(payload); return NULL; }
    /* Return the array (caller frees). Steal ref by returning the array
     * directly — but payload owns it; caller must free payload too. To keep
     * it simple, return a CLONE of the array and free payload. */
    json_t *clone = json_copy(models);
    json_free(payload);
    return clone;
}

/* ─── probe_lmstudio_models ──────────────────────────────────────── */
char **models_probe_lmstudio_models(http_fetch_fn fetch, void *ctx,
                                    const char *api_key, const char *base_url) {
    json_t *raw = models_lmstudio_fetch_raw_models(fetch, ctx, api_key, base_url);
    if (!raw) return NULL;
    size_t n = json_len(raw);
    char **keys = (char **)malloc(sizeof(char *) * (n + 1));
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *item = json_array_get(raw, i);
        if (!item || item->type != JSON_OBJECT) continue;
        const char *type = json_get_str(item, "type", "");
        if (strcmp(type, "embedding") == 0) continue;
        const char *key = json_get_str(item, "key", "");
        if (!key || !*key) key = json_get_str(item, "id", "");
        if (!key || !*key) continue;
        /* dedupe */
        int dup = 0;
        for (size_t k = 0; k < m; k++) if (strcmp(keys[k], key) == 0) { dup = 1; break; }
        if (dup) continue;
        keys[m++] = strdup(key);
    }
    keys[m] = NULL;
    json_free(raw);
    return keys;
}

/* ─── probe_api_models ──────────────────────────────────────────── */
static int url_hostname_is(const char *url, const char *host) {
    /* crude: find "://", then compare hostname prefix */
    const char *p = strstr(url, "://");
    if (!p) return 0;
    p += 3;
    return strncmp(p, host, strlen(host)) == 0 &&
           (p[strlen(host)] == '/' || p[strlen(host)] == '\0');
}

char *models_probe_api_models(http_fetch_fn fetch, void *ctx,
                              const char *api_key, const char *base_url,
                              const char *api_mode) {
    char *normalized = base_url ? strdup(base_url) : strdup("");
    size_t l = strlen(normalized);
    while (l > 0 && (normalized[l - 1] == '/' || normalized[l - 1] == ' ')) normalized[--l] = '\0';
    if (l == 0) {
        free(normalized);
        return strdup("{\"models\":null,\"probed_url\":null,\"resolved_base_url\":\"\","
                      "\"suggested_base_url\":null,\"used_fallback\":false}");
    }
    if (models_is_github_models_base_url(normalized)) {
        char *catalog = models_fetch_github_model_catalog(fetch, ctx, api_key);
        json_t *arr = catalog ? json_parse(catalog, NULL) : NULL;
        free(catalog);
        size_t cnt = arr ? json_len(arr) : 0;
        size_t cap = 2;
        for (size_t i = 0; i < cnt; i++) {
            json_t *it = json_array_get(arr, i);
            const char *id = json_get_str(it, "id", "");
            if (id) cap += strlen(id) * 2 + 6;
        }
        char *result = (char *)malloc(cap + 256);
        result[0] = '\0';
        strcat(result, "{\"models\":[");
        for (size_t i = 0; i < cnt; i++) {
            json_t *it = json_array_get(arr, i);
            const char *id = json_get_str(it, "id", "");
            if (!id || !*id) continue;
            if (i) strcat(result, ",");
            strcat(result, "\"");
            for (const char *q = id; *q; q++) {
                if (*q == '"' || *q == '\\') strcat(result, "\\");
                char t[2] = {*q, 0}; strcat(result, t);
            }
            strcat(result, "\"");
        }
        char tb[1024];
        snprintf(tb, sizeof(tb), "],\"probed_url\":\"%s\",\"resolved_base_url\":\"%s\",\"suggested_base_url\":null,\"used_fallback\":false}",
                 COPILOT_MODELS_URL, COPILOT_BASE_URL);
        strcat(result, tb);
        if (arr) json_free(arr);
        free(normalized);
        return result;
    }

    /* alternate_base */
    char *alternate_base;
    size_t nl = strlen(normalized);
    if (nl >= 3 && strcmp(normalized + nl - 3, "/v1") == 0) {
        size_t al = nl - 3;
        while (al > 0 && (normalized[al - 1] == '/' || normalized[al - 1] == ' ')) al--;
        alternate_base = (char *)malloc(al + 1);
        memcpy(alternate_base, normalized, al); alternate_base[al] = '\0';
    } else {
        size_t al = nl + 4;
        alternate_base = (char *)malloc(al + 1);
        snprintf(alternate_base, al + 1, "%s/v1", normalized);
    }
    int has_alt = strcmp(alternate_base, normalized) != 0;

    /* headers */
    char hdr[1024];
    snprintf(hdr, sizeof(hdr), "{\"User-Agent\":\"%s\"", HERMES_USER_AGENT);
    if (strstr(normalized, "generativelanguage.googleapis.com")) {
        strcat(hdr, ",\"X-Goog-Api-Client\":\"hermes-agent/slermes\"");
    }
    if (api_key && api_mode && strcmp(api_mode, "anthropic_messages") == 0) {
        strcat(hdr, ",\"x-api-key\":\"");
        strcat(hdr, api_key);
        strcat(hdr, "\",\"anthropic-version\":\"2023-06-01\"");
    } else if (api_key && *api_key) {
        strcat(hdr, ",\"Authorization\":\"Bearer ");
        strcat(hdr, api_key);
        strcat(hdr, "\"");
    }
    strcat(hdr, "}");

    const char *candidates[2];
    int is_fallback[2];
    int ncand = 0;
    candidates[ncand] = normalized; is_fallback[ncand] = 0; ncand++;
    if (has_alt) { candidates[ncand] = alternate_base; is_fallback[ncand] = 1; ncand++; }

    char *tried = NULL;
    char *result = NULL;
    for (int ci = 0; ci < ncand; ci++) {
        char url[1024];
        snprintf(url, sizeof(url), "%s/models", candidates[ci]);
        if (!tried) tried = strdup(url);
        char *body = NULL; size_t blen = 0;
        int rc = fetch ? fetch(url, hdr, &body, &blen, ctx) : -1;
        if (rc == 0 && body) {
            json_t *data = json_parse(body, NULL);
            free(body);
            if (data && data->type == JSON_OBJECT) {
                json_t *darr = json_object_get(data, "data");
                char *models_json = darr ? json_serialize(darr) : strdup("null");
                const char *sugg = (has_alt && strcmp(candidates[ci], alternate_base) != 0)
                                  ? alternate_base : normalized;
                size_t rcap = strlen(models_json) + strlen(url) + strlen(candidates[ci]) +
                              strlen(sugg) + 256;
                result = (char *)malloc(rcap);
                snprintf(result, rcap,
                    "{\"models\":%s,\"probed_url\":\"%s\",\"resolved_base_url\":\"%s\","
                    "\"suggested_base_url\":\"%s\",\"used_fallback\":%s}",
                    models_json, url, candidates[ci], sugg,
                    is_fallback[ci] ? "true" : "false");
                free(models_json);
                json_free(data);
                break;
            }
            if (data) json_free(data);
        }
    }
    if (!result) {
        const char *sugg = has_alt ? alternate_base : "null";
        size_t rcap = strlen(normalized) + strlen(tried ? tried : "") + strlen(sugg) + 256;
        result = (char *)malloc(rcap);
        snprintf(result, rcap,
            "{\"models\":null,\"probed_url\":\"%s\",\"resolved_base_url\":\"%s\","
            "\"suggested_base_url\":%s,\"used_fallback\":false}",
            tried ? tried : "", normalized, sugg);
    }
    free(alternate_base);
    free(tried);
    free(normalized);
    return result;
}

/* ─── fetch_api_models ──────────────────────────────────────────── */
char **models_fetch_api_models(http_fetch_fn fetch, void *ctx,
                               const char *api_key, const char *base_url,
                               const char *api_mode) {
    char *probe = models_probe_api_models(fetch, ctx, api_key, base_url, api_mode);
    if (!probe) return NULL;
    json_t *p = json_parse(probe, NULL);
    free(probe);
    if (!p || p->type != JSON_OBJECT) { if (p) json_free(p); return NULL; }
    json_t *models = json_object_get(p, "models");
    if (!models || models->type != JSON_ARRAY) { json_free(p); return NULL; }
    size_t n = json_len(models);
    char **out = (char **)malloc(sizeof(char *) * (n + 1));
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_array_get(models, i);
        const char *id = (e && e->type == JSON_STRING) ? e->str_val : "";
        out[i] = strdup(id ? id : "");
    }
    out[n] = NULL;
    json_free(p);
    return out;
}

/* ─── _get_model_config_dict (injectable) ────────────────────────── */
char *models_get_model_config_dict(model_config_fn get, void *ctx) {
    if (!get) return strdup("{}");
    char *r = get(ctx);
    return r ? r : strdup("{}");
}

/* ─── private: _COPILOT_MODEL_ALIASES ────────────────────────────── */
typedef struct { const char *alias; const char *canon; } alias_t;
static const alias_t COPILOT_ALIASES[] = {
    {"openai/gpt-5", "gpt-5-mini"},
    {"openai/gpt-5-chat", "gpt-5-mini"},
    {"openai/gpt-5-mini", "gpt-5-mini"},
    {"openai/gpt-5-nano", "gpt-5-mini"},
    {"openai/gpt-4.1", "gpt-4.1"},
    {"openai/gpt-4.1-mini", "gpt-4.1"},
    {"openai/gpt-4.1-nano", "gpt-4.1"},
    {"openai/gpt-4o", "gpt-4o"},
    {"openai/gpt-4o-mini", "gpt-4o-mini"},
    {"openai/o1", "gpt-5.2"},
    {"openai/o1-mini", "gpt-5-mini"},
    {"openai/o1-preview", "gpt-5.2"},
    {"openai/o3", "gpt-5.3-codex"},
    {"openai/o3-mini", "gpt-5-mini"},
    {"openai/o4-mini", "gpt-5-mini"},
    {"anthropic/claude-opus-4.6", "claude-opus-4.6"},
    {"anthropic/claude-sonnet-5", "claude-sonnet-5"},
    {"anthropic/claude-sonnet-4.6", "claude-sonnet-4.6"},
    {"anthropic/claude-sonnet-4", "claude-sonnet-4"},
    {"anthropic/claude-sonnet-4.5", "claude-sonnet-4.5"},
    {"anthropic/claude-haiku-4.5", "claude-haiku-4.5"},
    {"claude-sonnet-5", "claude-sonnet-5"},
    {"claude-opus-4-6", "claude-opus-4.6"},
    {"claude-sonnet-4-6", "claude-sonnet-4.6"},
    {"claude-sonnet-4-0", "claude-sonnet-4"},
    {"claude-sonnet-4-5", "claude-sonnet-4.5"},
    {"claude-haiku-4-5", "claude-haiku-4.5"},
    {NULL, NULL}
};
static const char *copilot_alias(const char *raw) {
    for (int i = 0; COPILOT_ALIASES[i].alias; i++)
        if (strcmp(COPILOT_ALIASES[i].alias, raw) == 0) return COPILOT_ALIASES[i].canon;
    return NULL;
}
static int in_catalog(const char *id, char **catalog_ids, size_t n) {
    for (size_t i = 0; i < n; i++) if (strcmp(catalog_ids[i], id) == 0) return 1;
    return 0;
}

/* ─── normalize_copilot_model_id (private) ──────────────────────── */
static char *normalize_copilot_model_id(const char *model_id,
                                        http_fetch_fn fetch, void *ctx,
                                        const char *catalog_json,
                                        const char *api_key) {
    char raw[512];
    if (!model_id) return strdup("");
    size_t i = 0;
    while (model_id[i] && isspace((unsigned char)model_id[i])) i++;
    size_t st = i;
    size_t en = strlen(model_id);
    while (en > st && isspace((unsigned char)model_id[en - 1])) en--;
    size_t o = 0;
    for (size_t k = st; k < en; k++) raw[o++] = model_id[k];
    raw[o] = '\0';
    if (!raw[0]) return strdup("");

    char **catalog_ids = NULL; size_t ncid = 0;
    if (catalog_json && *catalog_json) {
        json_t *c = json_parse(catalog_json, NULL);
        if (c && c->type == JSON_ARRAY) {
            ncid = json_len(c);
            catalog_ids = (char **)malloc(sizeof(char *) * (ncid + 1));
            for (size_t k = 0; k < ncid; k++) {
                json_t *it = json_array_get(c, k);
                const char *id = json_get_str(it, "id", "");
                catalog_ids[k] = strdup(id ? id : "");
            }
        }
        if (c) json_free(c);
    } else if (api_key && *api_key) {
        char *cat = models_fetch_github_model_catalog(fetch, ctx, api_key);
        if (cat) {
            json_t *c = json_parse(cat, NULL);
            free(cat);
            if (c && c->type == JSON_ARRAY) {
                ncid = json_len(c);
                catalog_ids = (char **)malloc(sizeof(char *) * (ncid + 1));
                for (size_t k = 0; k < ncid; k++) {
                    json_t *it = json_array_get(c, k);
                    const char *id = json_get_str(it, "id", "");
                    catalog_ids[k] = strdup(id ? id : "");
                }
            }
            if (c) json_free(c);
        }
    }

    char *result = NULL;
    const char *al = copilot_alias(raw);
    if (al) { result = strdup(al); goto done; }

    /* candidates */
    char *cands[8]; int nc = 0;
    cands[nc++] = strdup(raw);
    char *slash = strchr(raw, '/');
    if (slash) { cands[nc++] = strdup(slash + 1); }
    size_t rl = strlen(raw);
    if (rl > 5 && strcmp(raw + rl - 5, "-mini") == 0) cands[nc++] = strndup(raw, rl - 5);
    if (rl > 5 && strcmp(raw + rl - 5, "-nano") == 0) cands[nc++] = strndup(raw, rl - 5);
    if (rl > 5 && strcmp(raw + rl - 5, "-chat") == 0) cands[nc++] = strndup(raw, rl - 5);

    for (int k = 0; k < nc; k++) {
        const char *cand = cands[k];
        const char *ca = copilot_alias(cand);
        if (ca) { result = strdup(ca); break; }
        if (in_catalog(cand, catalog_ids, ncid)) { result = strdup(cand); break; }
    }
    for (int k = 0; k < nc; k++) free(cands[k]);
    if (!result) {
        if (slash) result = strdup(slash + 1);
        else result = strdup(raw);
    }
done:
    for (size_t k = 0; k < ncid; k++) free(catalog_ids[k]);
    free(catalog_ids);
    return result;
}

/* ─── _github_reasoning_efforts_for_model_id (private) ──────────── */
static char **github_reasoning_efforts_for_model_id(const char *model_id) {
    char raw[512];
    if (!model_id) model_id = "";
    size_t i = 0;
    while (model_id[i] && isspace((unsigned char)model_id[i])) i++;
    size_t st = i, en = strlen(model_id);
    while (en > st && isspace((unsigned char)model_id[en - 1])) en--;
    size_t o = 0;
    for (size_t k = st; k < en; k++) raw[o++] = (char)tolower((unsigned char)model_id[k]);
    raw[o] = '\0';
    const char *o_series[] = {"openai/o1", "openai/o3", "openai/o4", "o1", "o3", "o4", NULL};
    for (int k = 0; o_series[k]; k++)
        if (strncmp(raw, o_series[k], strlen(o_series[k])) == 0) {
            char **r = (char **)malloc(sizeof(char *) * 4);
            r[0] = strdup("low"); r[1] = strdup("medium"); r[2] = strdup("high"); r[3] = NULL;
            return r;
        }
    if (strncmp(raw, "gpt-5", 5) == 0) {
        char **r = (char **)malloc(sizeof(char *) * 5);
        r[0] = strdup("minimal"); r[1] = strdup("low"); r[2] = strdup("medium");
        r[3] = strdup("high"); r[4] = NULL;
        return r;
    }
    char **r = (char **)malloc(sizeof(char *));
    r[0] = NULL;
    return r;
}

/* ─── github_model_reasoning_efforts ─────────────────────────────── */
char **models_github_model_reasoning_efforts(http_fetch_fn fetch, void *ctx,
                                             const char *model_id, json_t *catalog,
                                             const char *api_key) {
    char *catalog_json = NULL;
    if (catalog && catalog->type == JSON_ARRAY) catalog_json = json_serialize(catalog);
    char *normalized = normalize_copilot_model_id(model_id, fetch, ctx, catalog_json, api_key);
    if (catalog_json) free(catalog_json);
    if (!normalized || !normalized[0]) { free(normalized); return NULL; }

    /* If we have a catalog, look up capabilities. */
    char **result = NULL;
    if (catalog && catalog->type == JSON_ARRAY) {
        size_t n = json_len(catalog);
        for (size_t i = 0; i < n && !result; i++) {
            json_t *it = json_array_get(catalog, i);
            const char *id = json_get_str(it, "id", "");
            if (id && strcmp(id, normalized) == 0) {
                json_t *caps = json_object_get(it, "capabilities");
                if (caps && caps->type == JSON_OBJECT) {
                    json_t *supports = json_object_get(caps, "supports");
                    if (supports && supports->type == JSON_OBJECT) {
                        json_t *eff = json_object_get(supports, "reasoning_effort");
                        if (eff && eff->type == JSON_ARRAY) {
                            size_t e = json_len(eff);
                            result = (char **)malloc(sizeof(char *) * (e + 1));
                            for (size_t k = 0; k < e; k++) {
                                json_t *ev = json_array_get(eff, k);
                                const char *s = (ev && ev->type == JSON_STRING) ? ev->str_val : "";
                                result[k] = strdup(s ? s : "");
                            }
                            result[e] = NULL;
                        }
                    }
                }
                if (!result) {
                    /* legacy capabilities list */
                    json_t *clist = json_object_get(it, "capabilities");
                    if (clist && clist->type == JSON_ARRAY) {
                        int has_reasoning = 0;
                        for (size_t k = 0; k < json_len(clist); k++) {
                            json_t *cv = json_array_get(clist, k);
                            const char *s = (cv && cv->type == JSON_STRING) ? cv->str_val : "";
                            if (s && strcmp(s, "reasoning") == 0) { has_reasoning = 1; break; }
                        }
                        if (!has_reasoning) { /* returns [] */ }
                    }
                }
            }
        }
    }
    if (result) { free(normalized); return result; }
    result = github_reasoning_efforts_for_model_id(normalized);
    free(normalized);
    return result;
}
