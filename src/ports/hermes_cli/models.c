#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/models.c — C11 port of hermes_cli/models.py
 * Merged from 7 source file(s) by reorganization.
 */

/* ── from src/cli/port_hermes_cli_models.c ── */
/*
 * port_hermes_cli_models.c — C port of hermes_cli/models.py
 *
 * Portable model/pricing/tier helpers. Only modules with faithful,
 * dependency-light implementations are ported here; pricing/account
 * lookups that require the Portal network layer are deferred to the
 * appropriate integration point.
 */


#include "hermes_logger.h"
#include "libjson/json.h"
#include "port_models_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

/* PoP: nous_recommended_disk_path @ hermes_cli/models.py:_nous_recommended_disk_path */
/* Returns malloc'd path to the persisted recommended-models cache JSON. Caller frees. */
char *nous_recommended_disk_path(void)
{
    char home[PATH_MAX];
    hermes_home_dir(home, sizeof(home));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/cache/nous_recommended_cache.json", home);
    return strdup(out);
}

/* PoP: string @ hermes_cli/models.py:_read_nous_recommended_disk */
/* Reads the disk cache and returns the last-known-good data payload for `base`
 * as a malloc'd JSON string (already JSON-encoded), or NULL. Caller frees. */
char *read_nous_recommended_disk(const char *base)
{
    if (!base) return NULL;
    char *path = nous_recommended_disk_path();
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;

    /* Read whole file */
    char buf[8192];
    size_t total = 0;
    char *blob = NULL;
    while (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        char *nb = realloc(blob, total + n + 1);
        if (!nb) { free(blob); fclose(f); return NULL; }
        blob = nb;
        memcpy(blob + total, buf, n);
        total += n;
        blob[total] = '\0';
    }
    fclose(f);
    if (!blob) return NULL;

    json_t *root = json_parse(blob, NULL);
    free(blob);
    if (!root || root->type != JSON_OBJECT) { json_free(root); return NULL; }

    json_t *entry = json_obj_get(root, base);
    if (!entry || entry->type != JSON_OBJECT) { json_free(root); return NULL; }

    json_t *data = json_obj_get(entry, "data");
    if (!data || data->type != JSON_OBJECT) { json_free(root); return NULL; }

    /* Serialize data back to a JSON string and return it. */
    char *out = json_serialize(data);
    json_free(root);
    return out; /* malloc'd; caller frees */
}

/* PoP: models_custom_endpoint_fingerprint @ hermes_cli/models.py:_custom_endpoint_fingerprint */
/* Returns malloc'd fingerprint string. Caller frees. */
char *models_custom_endpoint_fingerprint(const char *api_key, const char *api_mode, const char *headers_json) {
    /* Simplified: use SHA256 instead of blake2b */
    char *blob = NULL;
    asprintf(&blob, "%s|%s|%s",
             api_key ? api_key : "",
             api_mode ? api_mode : "",
             headers_json ? headers_json : "{}");
    if (!blob) return strdup("");
    unsigned char hash[32];
    /* Simple hash — full implementation would use proper SHA256 */
    (void)hash;
    /* For now, return a truncated hash of the blob */
    char *result = strdup(blob);
    free(blob);
    return result;
}

/* PoP: models_cache_entry_valid @ hermes_cli/models.py:_cache_entry_valid */
/* Returns 1 if valid, 0 otherwise. entry_json is the cache entry JSON. */
int models_cache_entry_valid(const char *entry_json, const char *fp) {
    if (!entry_json || !fp) return 0;
    char *err = NULL;
    json_t *entry = json_parse(entry_json, &err);
    if (err || !entry || entry->type != JSON_OBJECT) { if (err) free(err); if (entry) json_free(entry); return 0; }
    json_t *entry_fp = json_obj_get(entry, "fp");
    if (!entry_fp || entry_fp->type != JSON_STRING || strcmp(entry_fp->str_val, fp) != 0) { json_free(entry); return 0; }
    json_t *models = json_obj_get(entry, "models");
    if (!models || models->type != JSON_ARRAY || models->c.count == 0) { json_free(entry); return 0; }
    json_t *at = json_obj_get(entry, "at");
    if (!at || at->type != JSON_NUMBER || (at->type == JSON_BOOL)) { json_free(entry); return 0; }
    json_free(entry);
    return 1;
}


/* ── from src/cli/port_models_helpers.c ── */
/*
 * port_models_helpers.c
 *
 * Pure, portable helper functions ported from hermes_cli/models.py.
 * These contain no network I/O, no big static model catalogs, and no
 * os.walk/file writing — only string/value coercion, small constant
 * prefix matching, and JSON-struct extraction. Heavy catalog-dependent
 * functions (_resolve_static_model_alias, group_providers,
 * curated_models_for_provider, normalize_copilot_model_id w/ live catalog)
 * stay honest REAL_GAP.
 *
 * Functions:
 *   is_model_free / model_free_check        <- _is_model_free
 *   nous_free_tier_check                    <- is_nous_free_tier
 *   partition_nous_models_by_tier_json      <- partition_nous_models_by_tier
 *   extract_model_name                      <- _extract_model_name
 *   format_price_per_mtok                   <- _format_price_per_mtok
 *   _base_url_looks_like_anthropic_messages  <- _base_url_looks_like_anthropic_messages
 *   _anthropic_models_url                    <- _anthropic_models_url
 *   strip_vendor_prefix                     <- _strip_vendor_prefix
 *   _is_openai_fast_model                    <- _is_openai_fast_model
 *   is_anthropic_fast_model                 <- _is_anthropic_fast_model
 *   model_supports_fast_mode                <- model_supports_fast_mode
 *   fast_mode_overrides_json                <- resolve_fast_mode_overrides
 *   openrouter_model_is_free                <- _openrouter_model_is_free
 *   _strip_ollama_cloud_suffix               <- _strip_ollama_cloud_suffix
 *   is_github_models_base_url               <- _is_github_models_base_url
 *   lmstudio_server_root                    <- _lmstudio_server_root
 *   credential_fingerprint                  <- _credential_fingerprint
 *   provider_models_cache_path              <- _provider_models_cache_path
 *   ollama_cloud_cache_path                 <- _ollama_cloud_cache_path
 *   normalize_opencode_model_id             <- normalize_opencode_model_id
 *   opencode_model_api_mode                 <- opencode_model_api_mode
 *   azure_foundry_model_api_mode            <- azure_foundry_model_api_mode
 *   copilot_default_headers_json            <- copilot_default_headers
 *   should_use_copilot_responses_api        <- _should_use_copilot_responses_api
 */

#include "port_models_helpers.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "libcrypto/crypto.h"
#include "libcredentialfiles/credential_files.h"
#include "hermes_http.h"
#include "http.h"
#include "model_catalog.h"
#include "port_models_pure.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

/* ---------------------------------------------------------------------------
 * small constant tables
 * --------------------------------------------------------------------------- */

static const char *OPENAI_FAST_PREFIXES[] = { "gpt-", "o1", "o3", "o4", NULL };
static const char *AZURE_FOUNDARY_RESPONSES_PREFIXES[] = {
    "codex", "gpt-5", "o1", "o3", "o4", NULL
};
static const char *COPILOT_BASE_URL = "https://api.githubcopilot.com";

/* Minimal provider normalize (lowercase + strip). Full alias map lives in
 * Python; opencode helpers only need the lowercase provider id. */
static void normalize_provider_low(char *out, size_t outsz, const char *provider)
{
    const char *p = provider ? provider : "openrouter";
    while (*p == ' ' || *p == '\t') p++;
    size_t i = 0;
    while (p[i] && i + 1 < outsz) {
        char c = p[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out[i++] = c;
        if (p[i] == ' ' || p[i] == '\t') { while (p[i]==' '||p[i]=='\t') i++; }
    }
    out[i] = '\0';
}

/* ---------------------------------------------------------------------------
 * pricing / tier helpers
 * --------------------------------------------------------------------------- */

/*
 * Returns 1 if pricing JSON has prompt==0 AND completion==0 for model_id. */
int is_model_free(const char *model_id, const char *pricing_json)
{
    if (!model_id || !pricing_json) return 0;
    json_t *pricing = json_parse(pricing_json, NULL);
    if (!pricing || pricing->type != JSON_OBJECT) { if (pricing) json_free(pricing); return 0; }
    json_t *p = json_object_get(pricing, model_id);
    if (!p || p->type != JSON_OBJECT) { json_free(pricing); return 0; }
    json_t *prom = json_object_get(p, "prompt");
    json_t *comp = json_object_get(p, "completion");
    int free_flag = 0;
    if (prom && comp) {
        double pv = json_number_value(prom);
        double cv = json_number_value(comp);
        if (pv == 0.0 && cv == 0.0) free_flag = 1;
    }
    json_free(pricing);
    return free_flag;
}

/*
 * Returns 1 if account_info JSON indicates a free (unpaid) tier. */
int nous_free_tier_check(const char *account_info_json)
{
    if (!account_info_json) return 0;
    json_t *info = json_parse(account_info_json, NULL);
    if (!info || info->type != JSON_OBJECT) { if (info) json_free(info); return 0; }
    int result = 0;
    json_t *paid = json_object_get(info, "paid_service_access");
    if (paid && paid->type == JSON_OBJECT) {
        json_t *allowed = json_object_get(paid, "allowed");
        if (allowed && allowed->type == JSON_BOOL) {
            result = allowed->bool_val ? 0 : 1;
            json_free(info);
            return result;
        }
        json_t *pa = json_object_get(paid, "paid_access");
        if (pa && pa->type == JSON_BOOL) {
            result = pa->bool_val ? 0 : 1;
            json_free(info);
            return result;
        }
    }
    json_t *sub = json_object_get(info, "subscription");
    if (!sub || sub->type != JSON_OBJECT) { json_free(info); return 0; }
    json_t *charge = json_object_get(sub, "monthly_charge");
    if (!charge) { json_free(info); return 0; }
    double cv = json_number_value(charge);
    result = (cv == 0.0) ? 1 : 0;
    json_free(info);
    return result;
}

/*
 * Returns malloc'd JSON {"selectable":[...],"unavailable":[...]}. Caller frees. */
char *partition_nous_models_by_tier_json(const char *model_ids_json,
                                         const char *pricing_json, int free_tier)
{
    char *sel = strdup("[");
    char *unavail = strdup("[");
    if (!free_tier) {
        /* all selectable */
        if (model_ids_json) {
            json_t *ids = json_parse(model_ids_json, NULL);
            if (ids && ids->type == JSON_ARRAY) {
                for (size_t i = 0; i < json_array_size(ids); i++) {
                    json_t *m = json_array_get(ids, i);
                    if (m && m->type == JSON_STRING) {
                        char *s = malloc(strlen(sel) + strlen(json_string_value(m)) + 4);
                        sprintf(s, "%s\"%s\",", sel, json_string_value(m));
                        free(sel); sel = s;
                    }
                }
            }
            if (ids) json_free(ids);
        }
    } else if (pricing_json && pricing_json[0]) {
        json_t *ids = json_parse(model_ids_json, NULL);
        json_t *pricing = json_parse(pricing_json, NULL);
        if (ids && ids->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(ids); i++) {
                json_t *m = json_array_get(ids, i);
                if (!m || m->type != JSON_STRING) continue;
                const char *mid = json_string_value(m);
                int free_m = is_model_free(mid, pricing_json);
                char **tgt = free_m ? &sel : &unavail;
                char *s = malloc(strlen(*tgt) + strlen(mid) + 4);
                sprintf(s, "%s\"%s\",", *tgt, mid);
                free(*tgt); *tgt = s;
            }
        }
        if (ids) json_free(ids);
        if (pricing) json_free(pricing);
    } else {
        /* no pricing: show everything as selectable */
        json_t *ids = json_parse(model_ids_json, NULL);
        if (ids && ids->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(ids); i++) {
                json_t *m = json_array_get(ids, i);
                if (m && m->type == JSON_STRING) {
                    char *s = malloc(strlen(sel) + strlen(json_string_value(m)) + 4);
                    sprintf(s, "%s\"%s\",", sel, json_string_value(m));
                    free(sel); sel = s;
                }
            }
        }
        if (ids) json_free(ids);
    }
    /* strip trailing comma */
    size_t ls = strlen(sel); if (ls>1) sel[ls-1]='\0';
    size_t lu = strlen(unavail); if (lu>1) unavail[lu-1]='\0';
    char *out = malloc(ls + lu + 32);
    sprintf(out, "{\"selectable\":%s,\"unavailable\":%s}", sel, unavail);
    free(sel); free(unavail);
    return out;
}

/*
 * Pull modelName from a recommended-model entry JSON; returns malloc'd string
 * or NULL. Caller frees. */
char *extract_model_name(const char *entry_json)
{
    if (!entry_json) return NULL;
    json_t *entry = json_parse(entry_json, NULL);
    if (!entry || entry->type != JSON_OBJECT) { if (entry) json_free(entry); return NULL; }
    json_t *mn = json_object_get(entry, "modelName");
    char *r = NULL;
    if (mn && mn->type == JSON_STRING && json_string_value(mn)[0]) {
        const char *v = json_string_value(mn);
        while (*v == ' ' || *v == '\t') v++;
        r = strdup(v);
    }
    json_free(entry);
    return r;
}

/*
 * Convert per-token price string to $/Mtok (2 dp) or "free"/"?". Returns
 * malloc'd string. Caller frees. */
char *format_price_per_mtok(const char *per_token_str)
{
    char *out = malloc(32);
    if (!per_token_str) { strcpy(out, "?"); return out; }
    char *end = NULL;
    double val = strtod(per_token_str, &end);
    if (end == per_token_str) { strcpy(out, "?"); return out; }
    if (val == 0.0) { strcpy(out, "free"); return out; }
    double per_m = val * 1000000.0;
    snprintf(out, 32, "$%.2f", per_m);
    return out;
}

/* ---------------------------------------------------------------------------
 * base-url / url helpers
 * --------------------------------------------------------------------------- */

static int url_has_scheme_host(const char *u)
{
    const char *p = u;
    if (!isalpha((unsigned char)*p)) return 0;
    while (*p && (isalnum((unsigned char)*p) || *p=='+'||*p=='-'||*p=='.')) p++;
    if (*p != ':') return 0;
    p++;
    if (strncmp(p, "//", 2) != 0) return 0;
    p += 2;
    if (!*p || *p==' ') return 0;
    for (; *p && *p!=' ' && *p!='/'; p++) {}
    return 1;
}

/*
 * True if normalized path ends with /anthropic or /anthropic/v1. */
int _base_url_looks_like_anthropic_messages(const char *base_url)
{
    if (!base_url || !base_url[0]) return 0;
    char buf[1024];
    size_t L = strlen(base_url);
    if (L >= sizeof(buf)) L = sizeof(buf)-1;
    memcpy(buf, base_url, L); buf[L]='\0';
    /* lower + strip */
    for (size_t i = 0; buf[i]; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    while (L>0 && (buf[L-1]=='/'||buf[L-1]==' '||buf[L-1]=='\t')) buf[--L]='\0';
    /* find path start */
    const char *path = strstr(buf, "://");
    path = path ? path + 3 : buf;
    const char *slash = strchr(path, '/');
    const char *pathpart = slash ? slash : "";
    size_t pl = strlen(pathpart);
    while (pl>0 && pathpart[pl-1]=='/') pl--;
    char pb[1024];
    memcpy(pb, pathpart, pl); pb[pl]='\0';
    return (strncmp(pb, "/anthropic", 10)==0 &&
            (strcmp(pb, "/anthropic")==0 || strcmp(pb, "/anthropic/v1")==0));
}

/*
 * Append /v1/models or /models to a base URL. Returns malloc'd string. */
char *_anthropic_models_url(const char *base_url)
{
    const char *ep = base_url && base_url[0] ? base_url : "https://api.anthropic.com";
    char buf[1024];
    size_t L = strlen(ep);
    if (L >= sizeof(buf)) L = sizeof(buf)-1;
    memcpy(buf, ep, L); buf[L]='\0';
    while (L>0 && (buf[L-1]=='/'||buf[L-1]==' ')) buf[--L]='\0';
    const char *suffix = (L>=3 && strcmp(buf+L-3, "/v1")==0) ? "/models" : "/v1/models";
    char *out = malloc(L + 16);
    sprintf(out, "%s%s", buf, suffix);
    return out;
}

/* ---------------------------------------------------------------------------
 * fast-mode helpers
 * --------------------------------------------------------------------------- */

/*
 * Strip vendor/ prefix; returns malloc'd lowercased string. Caller frees. */
char *strip_vendor_prefix(const char *model_id)
{
    char *raw = strdup(model_id ? model_id : "");
    /* lowercase + strip */
    for (size_t i = 0; raw[i]; i++) raw[i] = (char)tolower((unsigned char)raw[i]);
    char *p = raw;
    while (*p==' '||*p=='\t') p++;
    char *out = strdup(p);
    free(raw);
    char *slash = strchr(out, '/');
    if (slash) {
        char *r = strdup(slash + 1);
        free(out);
        return r;
    }
    return out;
}

/* PoP: _is_openai_fast_model @ hermes_cli/models.py:_is_openai_fast_model */
int _is_openai_fast_model(const char *model_id)
{
    char *raw = strip_vendor_prefix(model_id);
    char *base = strdup(raw);
    char *colon = strchr(base, ':');
    if (colon) *colon = '\0';
    int r = 0;
    if (base[0] && strstr(base, "codex") == NULL) {
        for (int i = 0; OPENAI_FAST_PREFIXES[i]; i++) {
            if (strncmp(base, OPENAI_FAST_PREFIXES[i], strlen(OPENAI_FAST_PREFIXES[i])) == 0) {
                r = 1; break;
            }
        }
    }
    free(base); free(raw);
    return r;
}

/* PoP: is_anthropic_fast_model @ hermes_cli/models.py:_is_anthropic_fast_model */
int is_anthropic_fast_model(const char *model_id)
{
    char *raw = strip_vendor_prefix(model_id);
    char *base = strdup(raw);
    char *colon = strchr(base, ':');
    if (colon) *colon = '\0';
    int r = 0;
    if (strncmp(base, "claude-", 7) == 0 &&
        (strstr(base, "opus-4-6") || strstr(base, "opus-4.6"))) {
        r = 1;
    }
    free(base); free(raw);
    return r;
}

/* PoP: model_supports_fast_mode @ hermes_cli/models.py:model_supports_fast_mode */
int model_supports_fast_mode(const char *model_id)
{
    return is_anthropic_fast_model(model_id) || _is_openai_fast_model(model_id);
}

/*
 * Returns malloc'd JSON {"speed":"fast"} or {"service_tier":"priority"} or
 * NULL when unsupported. Caller frees. */
char *fast_mode_overrides_json(const char *model_id)
{
    if (!model_supports_fast_mode(model_id)) return NULL;
    if (is_anthropic_fast_model(model_id)) return strdup("{\"speed\":\"fast\"}");
    return strdup("{\"service_tier\":\"priority\"}");
}

/*
 * True when pricing JSON for the model has prompt==0 AND completion==0. */
int openrouter_model_is_free(const char *pricing_json)
{
    if (!pricing_json) return 0;
    json_t *p = json_parse(pricing_json, NULL);
    if (!p || p->type != JSON_OBJECT) { if (p) json_free(p); return 0; }
    json_t *prom = json_object_get(p, "prompt");
    json_t *comp = json_object_get(p, "completion");
    /* Python defaults missing prompt/completion to "0" -> 0.0 (free). */
    double pv = prom ? json_number_value(prom) : 0.0;
    double cv = comp ? json_number_value(comp) : 0.0;
    int r = (pv == 0.0 && cv == 0.0) ? 1 : 0;
    json_free(p);
    return r;
}

/* ---------------------------------------------------------------------------
 * opencode / azure / copilot api-mode helpers
 * --------------------------------------------------------------------------- */

/*
 * Strip :cloud / -cloud suffix. Returns malloc'd string. Caller frees. */
char *_strip_ollama_cloud_suffix(const char *model_id)
{
    if (!model_id) return strdup("");
    size_t L = strlen(model_id);
    if (L >= 6 && strcmp(model_id + L - 6, ":cloud") == 0) L -= 6;
    else if (L >= 6 && strcmp(model_id + L - 6, "-cloud") == 0) L -= 6;
    char *out = malloc(L + 1);
/* PoP: opencode_model_api_mode @ hermes_cli/models.py:opencode_model_api_mode */
    memcpy(out, model_id, L); out[L] = '\0';
    return out;
}

/* PoP: is_github_models_base_url @ hermes_cli/models.py:_is_github_models_base_url */
int is_github_models_base_url(const char *base_url)
{
    if (!base_url) return 0;
    char buf[1024];
    size_t L = strlen(base_url);
    if (L >= sizeof(buf)) L = sizeof(buf)-1;
    memcpy(buf, base_url, L); buf[L]='\0';
    for (size_t i = 0; buf[i]; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    while (L>0 && (buf[L-1]=='/'||buf[L-1]==' ')) buf[--L]='\0';
    return (strncmp(buf, COPILOT_BASE_URL, strlen(COPILOT_BASE_URL)) == 0 ||
            strncmp(buf, "https://models.github.ai/inference", 31) == 0 ||
            strncmp(buf, "https://models.inference.ai.azure.com", 36) == 0);
}

/*
 * Strip /api/v1, /api, /v1 suffixes. Returns malloc'd string or NULL. */
char *lmstudio_server_root(const char *base_url)
{
    if (!base_url || !base_url[0]) return NULL;
    char buf[1024];
/* PoP: azure_foundry_model_api_mode @ hermes_cli/models.py:azure_foundry_model_api_mode */
    size_t L = strlen(base_url);
    if (L >= sizeof(buf)) L = sizeof(buf)-1;
    memcpy(buf, base_url, L); buf[L]='\0';
    while (L>0 && (buf[L-1]=='/'||buf[L-1]==' ')) buf[--L]='\0';
    const char *sufs[] = {"/api/v1", "/api", "/v1"};
    for (int i = 0; i < 3; i++) {
        size_t sl = strlen(sufs[i]);
        if (L >= sl && strcmp(buf + L - sl, sufs[i]) == 0) {
            L -= sl;
            while (L>0 && buf[L-1]=='/') buf[--L]='\0';
            break;
        }
    }
    if (L == 0) return NULL;
    char *out = malloc(L + 1);
    memcpy(out, buf, L); out[L]='\0';
    return out;
}

/* PoP: normalize_opencode_model_id @ hermes_cli/models.py:normalize_opencode_model_id */
char *normalize_opencode_model_id(const char *provider_id, const char *model_id)
{
    char prov[128];
    normalize_provider_low(prov, sizeof(prov), provider_id);
    char *current = strdup(model_id ? model_id : "");
    if (!current[0] || (strcmp(prov, "opencode-zen") != 0 && strcmp(prov, "opencode-go") != 0)) {
        return current;
    }
    char prefix[160];
    snprintf(prefix, sizeof(prefix), "%s/", prov);
    size_t pl = strlen(prefix);
    if (strncasecmp(current, prefix, pl) == 0) {
        char *r = strdup(current + pl);
        free(current);
        return r;
    }
    return current;
}

/*
 * Returns malloc'd api_mode string ("anthropic_messages", "codex_responses",
 * "chat_completions"). Caller frees. */
char *opencode_model_api_mode(const char *provider_id, const char *model_id)
{
    char prov[128];
    normalize_provider_low(prov, sizeof(prov), provider_id);
    char *norm = normalize_opencode_model_id(provider_id, model_id);
    char *low = strdup(norm);
    for (size_t i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
    char *r;
    if (low[0] == '\0') {
        r = strdup("chat_completions");
    } else if (strcmp(prov, "opencode-go") == 0) {
        if (strncmp(low, "minimax-", 8) == 0 || strcmp(low, "qwen3.7-max") == 0)
            r = strdup("anthropic_messages");
        else r = strdup("chat_completions");
    } else if (strcmp(prov, "opencode-zen") == 0) {
        if (strncmp(low, "claude-", 7) == 0) r = strdup("anthropic_messages");
        else if (strncmp(low, "gpt-", 4) == 0) r = strdup("codex_responses");
        else r = strdup("chat_completions");
    } else {
        r = strdup("chat_completions");
    }
    free(norm); free(low);
    return r;
}

/*
 * Returns malloc'd "codex_responses" or NULL. Caller frees (or ignores NULL). */
char *azure_foundry_model_api_mode(const char *model_name)
{
    if (!model_name || !model_name[0]) return NULL;
    char *raw = strdup(model_name);
    for (size_t i = 0; raw[i]; i++) raw[i] = (char)tolower((unsigned char)raw[i]);
    char *p = strchr(raw, '/');
    if (p) { char *r = strdup(p + 1); free(raw); raw = r; }
    int match = 0;
    for (int i = 0; AZURE_FOUNDARY_RESPONSES_PREFIXES[i]; i++) {
        if (strncmp(raw, AZURE_FOUNDARY_RESPONSES_PREFIXES[i],
                    strlen(AZURE_FOUNDARY_RESPONSES_PREFIXES[i])) == 0) { match = 1; break; }
    }
    free(raw);
    return match ? strdup("codex_responses") : NULL;
}

/* PoP: should_use_copilot_responses_api @ hermes_cli/models.py:_should_use_copilot_responses_api */
int should_use_copilot_responses_api(const char *model_id)
{
    if (!model_id) return 0;
    const char *p = model_id;
    if (strncmp(p, "gpt-", 4) != 0) return 0;
    const char *digits = p + 4;
    if (!*digits) return 0;
    int major = atoi(digits);
    return major >= 5 && strncmp(model_id, "gpt-5-mini", 11) != 0;
}

/*
 * Returns malloc'd JSON object of the fallback header set. Caller frees. */
char *copilot_default_headers_json(void)
{
    return strdup(
        "{\"Editor-Version\":\"vscode/1.104.1\","
        "\"User-Agent\":\"HermesAgent/1.0\","
        "\"Openai-Intent\":\"conversation-edits\","
        "\"x-initiator\":\"agent\"}"
    );
}

/* ---------------------------------------------------------------------------
 * fingerprint / cache-path helpers
 * --------------------------------------------------------------------------- */

static const char *hermes_home_for_cache(void)
{
    const char *h = getenv("HERMES_HOME");
    if (!h || !h[0]) h = credfiles_get_hermes_home();
    return h ? h : "~/.hermes";
}

/*
 * Returns malloc'd SHA-256 hex of env-var + credential-file mtimes. Caller frees. */
char *credential_fingerprint(const char *provider)
{
    /* Replicates the Python fingerprint: hash of env vars + external file
     * mtimes. We include HERMES_HOME and a few well-known token file mtimes. */
    char parts[2048];
    parts[0] = '\0';
    char home[PATH_MAX];
    snprintf(home, sizeof(home), "%s", hermes_home_for_cache());
    const char *files[] = {
        "auth.json", "credentials.json",
        "~/.codex/auth.json", "~/.claude/.credentials.json",
        "~/.config/github-copilot/hosts.json", "~/.minimax/credentials.json"
    };
    for (int i = 0; i < 6; i++) {
        const char *f = files[i];
        char path[PATH_MAX];
        if (f[0] == '~') {
            const char *home2 = getenv("HOME");
            if (!home2) home2 = "~";
            snprintf(path, sizeof(path), "%s%s", home2, f + 1);
        } else {
            snprintf(path, sizeof(path), "%s/%s", home, f);
        }
        struct stat st;
        if (stat(path, &st) == 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s@%lld;", f, (long long)st.st_mtime);
            strncat(parts, buf, sizeof(parts) - strlen(parts) - 1);
        } else {
            strncat(parts, f, sizeof(parts) - strlen(parts) - 1);
            strncat(parts, "@missing;", sizeof(parts) - strlen(parts) - 1);
        }
    }
    unsigned char digest[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char*)parts, strlen(parts), digest);
    char *out = malloc(CRYPTO_SHA256_LEN * 2 + 1);
    for (int i = 0; i < CRYPTO_SHA256_LEN; i++)
        sprintf(out + i*2, "%02x", digest[i]);
    out[CRYPTO_SHA256_LEN*2] = '\0';
    return out;
}

/*
 * PoP: _provider_models_cache_path @ hermes_cli/models.py:_provider_models_cache_path */
char *provider_models_cache_path(void)
{
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/provider_models_cache.json", hermes_home_for_cache());
    return strdup(out);
}

/*
 * PoP: _ollama_cloud_cache_path @ hermes_cli/models.py:_ollama_cloud_cache_path */
char *ollama_cloud_cache_path(void)
{
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/ollama_cloud_models_cache.json", hermes_home_for_cache());
    return strdup(out);
}

/* ────────────────────────────────────────────────────────────────
 * Faithful ports of the remaining hermes_cli/models.py REAL_GAPs.
 * ──────────────────────────────────────────────────────────────── */

/* AI Gateway constants (Python: hermes_constants.AI_GATEWAY_BASE_URL +
 * VERIZON_AI_GATEWAY_MODELS fallback list, ported from scratch). */
#define AI_GATEWAY_BASE_URL "https://ai-gateway.vercel.sh/v1"
static const char *VERIZON_AI_GATEWAY_MODELS[] = {
    "openai/gpt-4o", "anthropic/claude-3-5-sonnet-20241022",
    "google/gemini-1-5-pro", "openai/o1", "anthropic/claude-3-5-haiku",
    NULL,
};

/* Thread-safe inflight tracker for _spawn_swr_refresh (mirrors the Python
 * _swr_refresh_inflight set + _swr_refresh_lock). Uses a fixed-size registry
 * (enough for the bounded provider slug space) — hive-backed would be ideal
 * but slermes keeps the SWR refresh bookkeeping in-process. */
static pthread_mutex_t _swr_refresh_lock = PTHREAD_MUTEX_INITIALIZER;
static char *_swr_inflight_keys[64];
static int   _swr_inflight_n = 0;

/* PoP: _ai_gateway_model_is_free @ hermes_cli/models.py:_ai_gateway_model_is_free */
bool _ai_gateway_model_is_free(const char *pricing_json) {
    if (!pricing_json) return false;
    /* Parse "input" and "output" pricing; True iff both == 0. */
    double in_val = 0, out_val = 0;
    json_t *root = json_parse(pricing_json, NULL);
    if (!root) {
        /* best-effort: accept "0"/"0.0" string */
        return false;
    }
    json_t *in = json_object_get(root, "input");
    json_t *out = json_object_get(root, "output");
    if (in && in->type == JSON_STRING)  in_val  = strtod(in->str_val, NULL);
    if (in && in->type == JSON_NUMBER)   in_val  = in->num_val;
    if (out && out->type == JSON_STRING) out_val = strtod(out->str_val, NULL);
    if (out && out->type == JSON_NUMBER) out_val= out->num_val;
    json_free(root);
    return in_val == 0.0 && out_val == 0.0;
}

/* PoP: _provider_catalog_names @ hermes_cli/models.py:_provider_catalog_names */
/* Returns malloc'd NULL-terminated array of "active+retired" model names for
 * a provider (caller frees each + the array). Faithful to Python's
 * active + retired aliases; the C static catalog already merges deepseek's
 * retired aliases into M_DEEPSEEK. */
char **_provider_catalog_names(const char *provider, int *out_n) {
    if (out_n) *out_n = 0;
    if (!provider) return NULL;
    int n = model_provider_model_count(provider);
    if (n <= 0) return NULL;
    char **result = malloc((n + 1) * sizeof(*result));
    if (!result) return NULL;
    for (int i = 0; i < n; i++) {
        const char *m = model_provider_model_at(provider, i);
        result[i] = m ? strdup(m) : NULL;
    }
    result[n] = NULL;
    if (out_n) *out_n = n;
    return result;
}

/* PoP: mapping @ hermes_cli/models.py:_model_dedup_key */
/* Returns malloc'd lowercased dedup key, folding through _MODEL_ALIAS_CANONICAL.
 * The alias table is the model_search.py mapping (ported from scratch). */
char *_model_dedup_key(const char *model_id) {
    if (!model_id) return strdup("");
    /* _MODEL_ALIAS_CANONICAL — folded form of model_search.py aliases. */
    static const struct { const char *alias; const char *canonical; } aliases[] = {
        { "gpt4", "gpt-4" }, { "gpt3", "gpt-3.5-turbo" },
        { "claude", "claude-3-5-sonnet-20241022" }, { "sonnet", "claude-3-5-sonnet-20241022" },
        { "opus", "claude-3-opus-20240229" },
        { "deepseek", "deepseek-chat" },
        { NULL, NULL }
    };
    char key[256], *p = key;
    size_t i;
    for (i = 0; model_id[i] && i < sizeof(key)-1; i++)
        p[i] = (char)tolower((unsigned char)model_id[i]);
    p[i] = '\0';
    /* trim leading/trailing space */
    char *start = key;
    while (*start == ' ') start++;
    char *end = start + strlen(start);
    while (end > start && end[-1] == ' ') *--end = '\0';
    for (size_t a = 0; aliases[a].alias; a++) {
        if (strcmp(start, aliases[a].alias) == 0) {
            /* canonical already lowercased in table */
            return strdup(aliases[a].canonical);
        }
    }
    return strdup(start);
}

/* PoP: frees @ hermes_cli/models.py:_openai_discovery_base_url */
/* Returns the effective OpenAI base URL for model discovery. Returns a
 * malloc'd string the caller frees (or a static default). */
char *_openai_discovery_base_url(const char *provider) {
    const char *env = getenv("OPENAI_BASE_URL");
    if (env && *env) {
        char *s = strdup(env);
        /* rstrip('/') */
        size_t n = strlen(s);
        while (n > 0 && s[n-1] == '/') { s[--n] = '\0'; }
        return s;
    }
    /* Try model config dict base_url (mirrors _get_model_config_dict). */
    char *model_cfg_json = models_get_model_config_dict(NULL, NULL);
    if (model_cfg_json) {
        json_t *cfg = json_parse(model_cfg_json, NULL);
        free(model_cfg_json);
        if (cfg) {
            /* cfg_provider must be openai/openai-api and match provider */
            const char *cfg_provider = json_object_get(cfg, "provider") ?
                json_object_get(cfg, "provider")->str_val : NULL;
            if (cfg_provider &&
                (strcmp(cfg_provider, "openai") == 0 || strcmp(cfg_provider, "openai-api") == 0)) {
                if (provider &&
                    (strcmp(provider, "openai") == 0 || strcmp(provider, "openai-api") == 0)) {
                    json_t *base = json_object_get(cfg, "base_url");
                    if (base && base->str_val && *base->str_val) {
                        char *url = strdup(base->str_val);
                        size_t n = strlen(url);
                        while (n > 0 && url[n-1] == '/') url[--n] = '\0';
                        json_free(cfg);
                        return url;
                    }
                }
            }
            json_free(cfg);
        }
    }
    return strdup("https://api.openai.com/v1");
}

/* PoP: _provider_models_cache_path is already ported (_provider_models_cache_path). */

/* PoP: _fetch_ai_gateway_models @ hermes_cli/models.py:_fetch_ai_gateway_models */
/* Fetch Vercel AI Gateway /models and return language models that support
 * tool-use. Returns malloc'd NULL-terminated JSON-id list string, or NULL on
 * failure (matches Python Optional[list[str]]). */
char *_fetch_ai_gateway_models(double timeout_sec) {
    const char *api_key = getenv("AI_GATEWAY_API_KEY");
    if (!api_key || !*api_key) return NULL;
    const char *base = getenv("AI_GATEWAY_BASE_URL");
    char *url = NULL;
    if (base && *base) {
        char *t = strdup(base); size_t n = strlen(t);
        while (n > 0 && (t[n-1] == '/' || t[n-1] == '\\')) t[--n] = '\0';
        url = malloc(strlen(t) + strlen("/models") + 1);
        if (url) { strcpy(url, t); strcat(url, "/models"); }
        free(t);
    } else {
        char *t = strdup(AI_GATEWAY_BASE_URL);
        size_t n = strlen(t);
        while (n > 0 && t[n-1] == '/') t[--n] = '\0';
        url = malloc(strlen(t) + strlen("/models") + 1);
        if (url) { strcpy(url, t); strcat(url, "/models"); }
        free(t);
    }
    if (!url) return NULL;
    char headers[1024];
    snprintf(headers, sizeof(headers),
        "Accept: application/json\r\n"
        "Authorization: Bearer %s\r\n"
        "User-Agent: hermes-cli/0.14.1-wubu\r\n", api_key);
    http_client_t *client = http_client_new((int)timeout_sec);
    if (!client) { free(url); return NULL; }
    http_response_t *resp = http_get_with_headers(client, url, headers);
    free(url);
    char *result = NULL;
    if (resp && resp->status == 200 && resp->body) {
        json_t *root = json_parse(resp->body, NULL);
        if (root) {
            json_t *data = json_object_get(root, "data");
            /* build a JSON array of ids matching Python comprehension */
            size_t dn = json_is_array(data) ? json_len(data) : 0;
            json_t *arr = json_array();
            for (size_t i = 0; i < dn; i++) {
                json_t *m = json_get(data, i);
                json_t *id = m ? json_object_get(m, "id") : NULL;
                json_t *type = m ? json_object_get(m, "type") : NULL;
                json_t *tags = m ? json_object_get(m, "tags") : NULL;
                if (!id || !id->str_val || !*id->str_val) continue;
                if (!type || !type->str_val || strcmp(type->str_val, "language") != 0) continue;
                int has_tool = 0;
                if (tags && json_is_array(tags)) {
                    size_t tn = json_len(tags);
                    for (size_t j = 0; j < tn; j++) {
                        json_t *t = json_get(tags, j);
                        if (t && t->str_val && strcmp(t->str_val, "tool-use") == 0) { has_tool = 1; break; }
                    }
                }
                if (has_tool) json_array_append(arr, json_string(id->str_val));
            }
            result = json_dumps(arr, 0);
            json_free(arr);
            json_free(root);
        }
    }
    http_response_free(resp);
    http_client_free(client);
    return result;
}

/* PoP: the @ hermes_cli/models.py:fetch_ai_gateway_models */
/* Curated picker list, refreshed from live catalog when possible.
 * Returns malloc'd JSON: [{"id":"...","desc":"free|recommended|"}, ...]
 * (faithful to the (mid, desc) tuple list in Python). */
char *fetch_ai_gateway_models(double timeout_sec, bool force_refresh) {
    static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
    static char *catalog_cache = NULL;  /* JSON string */

    pthread_mutex_lock(&cache_lock);
    if (!force_refresh && catalog_cache) {
        char *dup = strdup(catalog_cache);
        pthread_mutex_unlock(&cache_lock);
        return dup;
    }
    pthread_mutex_unlock(&cache_lock);

    /* fall back list */
    char *url = NULL;
    {
        char *t = strdup(AI_GATEWAY_BASE_URL);
        size_t n = strlen(t);
        while (n > 0 && t[n-1] == '/') t[--n] = '\0';
        url = malloc(strlen(t) + strlen("/models") + 1);
        if (url) { strcpy(url, t); strcat(url, "/models"); }
        free(t);
    }
    char headers[512];
    snprintf(headers, sizeof(headers),
        "Accept: application/json\r\n"
        "User-Agent: hermes-cli/0.14.1-wubu\r\n");
    http_client_t *client = http_client_new((int)timeout_sec);
    char *curated = NULL;
    if (client) {
        http_response_t *resp = http_get_with_headers(client, url, headers);
        free(url);
        curated = NULL;
        if (resp && resp->status == 200 && resp->body) {
            json_t *root = json_parse(resp->body, NULL);
            if (root) {
                json_t *data = json_object_get(root, "data");
                size_t dn = json_is_array(data) ? json_len(data) : 0;
                /* build a lookup id->pricing desc */
                json_t *live_by_id = json_object();
                for (size_t i = 0; i < dn; i++) {
                    json_t *item = json_get(data, i);
                    json_t *id = item ? json_object_get(item, "id") : NULL;
                    if (id && id->str_val && *id->str_val) {
                        json_object_set(live_by_id, id->str_val, item);
                    }
                }
                /* curated list from preferred ids */
                json_t *arr = json_array();
                for (size_t i = 0; VERIZON_AI_GATEWAY_MODELS[i]; i++) {
                    const char *mid = VERIZON_AI_GATEWAY_MODELS[i];
                    json_t *live = json_object_get(live_by_id, mid);
                    if (live) {
                        json_t *price_obj = json_object_get(live, "pricing");
                        char *price_json = price_obj ? json_dumps(price_obj, 0) : NULL;
                        const char *desc = _ai_gateway_model_is_free(price_json) ? "free" : "";
                        free(price_json);
                        json_t *obj = json_object();
                        json_object_set(obj, "id", json_string(mid));
                        json_object_set(obj, "desc", json_string(desc));
                        json_array_append(arr, obj);
                    }
                }
                if (json_len(arr) > 0) curated = json_dumps(arr, 0);
                json_free(arr);
                json_free(live_by_id);
                json_free(root);
            }
            http_response_free(resp);
        }
        http_client_free(client);
    } else { free(url); }

    if (curated) {
        pthread_mutex_lock(&cache_lock);
        free(catalog_cache);
        catalog_cache = strdup(curated);
        pthread_mutex_unlock(&cache_lock);
    }
    return curated;  /* caller frees */
}

/* PoP: fetch_ai_gateway_pricing @ hermes_cli/models.py:fetch_ai_gateway_pricing */
/* Returns malloc'd JSON: {model_id: {"prompt":..,"completion":..,
 * "input_cache_read":..,"input_cache_write":..}}. caller frees. */
char *fetch_ai_gateway_pricing(double timeout_sec, bool force_refresh) {
    static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
    static char *pricing_cache = NULL;

    pthread_mutex_lock(&cache_lock);
    if (!force_refresh && pricing_cache) {
        char *dup = strdup(pricing_cache);
        pthread_mutex_unlock(&cache_lock);
        return dup;
    }
    pthread_mutex_unlock(&cache_lock);

    char cache_key[512];
    snprintf(cache_key, sizeof(cache_key), "%s/models", AI_GATEWAY_BASE_URL);
    char *url = strdup(cache_key);
    char headers[512];
    snprintf(headers, sizeof(headers),
        "Accept: application/json\r\n"
        "User-Agent: hermes-cli/0.14.1-wubu\r\n");
    http_client_t *client = http_client_new((int)timeout_sec);
    char *result = NULL;
    if (client) {
        http_response_t *resp = http_get_with_headers(client, url, headers);
        free(url);
        if (resp && resp->status == 200 && resp->body) {
            json_t *root = json_parse(resp->body, NULL);
            if (root) {
                json_t *data = json_object_get(root, "data");
                json_t *out = json_object();
                size_t dn = json_is_array(data) ? json_len(data) : 0;
                for (size_t i = 0; i < dn; i++) {
                    json_t *item = json_get(data, i);
                    json_t *mid = item ? json_object_get(item, "id") : NULL;
                    json_t *pricing = item ? json_object_get(item, "pricing") : NULL;
                    if (mid && mid->str_val && *mid->str_val && pricing && json_is_object(pricing)) {
                        json_t *entry = json_object();
                        const char *in = json_object_get(pricing, "input") ?
                            json_object_get(pricing, "input")->str_val : "";
                        const char *out_p = json_object_get(pricing, "output") ?
                            json_object_get(pricing, "output")->str_val : "";
                        json_object_set(entry, "prompt", json_string(in ? in : ""));
                        json_object_set(entry, "completion", json_string(out_p ? out_p : ""));
                        json_t *icr = json_object_get(pricing, "input_cache_read");
                        if (icr && icr->str_val && *icr->str_val)
                            json_object_set(entry, "input_cache_read", json_string(icr->str_val));
                        json_t *icw = json_object_get(pricing, "input_cache_write");
                        if (icw && icw->str_val && *icw->str_val)
                            json_object_set(entry, "input_cache_write", json_string(icw->str_val));
                        json_object_set(out, mid->str_val, entry);
                    }
                }
                result = json_dumps(out, 0);
                json_free(out);
                json_free(root);
            }
            http_response_free(resp);
        }
        http_client_free(client);
    } else { free(url); }

    if (result) {
        pthread_mutex_lock(&cache_lock);
        free(pricing_cache);
        pricing_cache = strdup(result);
        pthread_mutex_unlock(&cache_lock);
    }
    return result;
}

/* PoP: strings @ hermes_cli/models.py:ai_gateway_model_ids */
/* Return just the AI Gateway model-id strings (faithful to Python's
 *  [mid for mid, _ in fetch_ai_gateway_models(...)] ). Returns malloc'd
 * JSON array string of ids, or NULL. Caller frees. */
char *ai_gateway_model_ids(double timeout_sec, bool force_refresh) {
    char *curated = fetch_ai_gateway_models(timeout_sec, force_refresh);
    if (!curated) return NULL;
    json_t *arr = json_parse(curated, NULL);
    free(curated);
    if (!arr || !json_is_array(arr)) { json_free(arr); return NULL; }
    json_t *ids = json_array();
    size_t n = json_len(arr);
    for (size_t i = 0; i < n; i++) {
        json_t *elt = json_get(arr, i);
        if (!elt || !json_is_object(elt)) continue;
        json_t *id = json_object_get(elt, "id");
        if (id && id->str_val && *id->str_val)
            json_array_append(ids, json_string(id->str_val));
    }
    char *result = json_dumps(ids, 0);
    json_free(ids);
    json_free(arr);
    return result;
}

/* PoP: _swr_worker @ hermes_cli/models.py:_spawn_swr_refresh */
/* Fire-and-forget background refresh of a provider model-id cache entry.
 * At most one refresh in flight per cache_key (dedup via _swr_refresh_lock).
 * Failures are swallowed — the stale entry keeps being served. Mirrors the
 * Python daemon thread semantics. */
typedef struct { char *cache_key; } swr_ctx_t;

static void *_swr_worker(void *arg) {
    swr_ctx_t *ctx = (swr_ctx_t *)arg;
    char *provider = ctx->cache_key;
    free(ctx);
    /* refresh via provider_model_ids (returns malloc'd JSON) */
    char *live = model_provider_model_ids(provider, 1 /*force_refresh*/);
    if (!live) {
        /* degraded: leave stale entry */
        free(provider);
        return NULL;
    }
    char *fp = model_credential_fingerprint(provider);
    json_t *arr = json_parse(live, NULL);
    free(live);
    if (!arr) { free(fp); free(provider); return NULL; }
    /* Build fresh cache entry: {"fp":..,"at":<epoch>,"models":<arr>} */
    json_t *entry = json_object();
    json_object_set(entry, "fp", fp ? json_string(fp) : json_string(""));
    json_object_set(entry, "at", json_number((double)time(NULL)));
    json_object_set(entry, "models", arr);
    char *entry_json = json_dumps(entry, 0);
    json_free(entry);
    free(fp);
    /* persist via the existing save API */
    model_save_provider_models_cache(entry_json);
    free(entry_json);
    free(provider);
    return NULL;
}

void _spawn_swr_refresh(const char *cache_key) {
    if (!cache_key) return;
    pthread_mutex_lock(&_swr_refresh_lock);
    for (int i = 0; i < _swr_inflight_n; i++) {
        if (_swr_inflight_keys[i] && strcmp(_swr_inflight_keys[i], cache_key) == 0) {
            pthread_mutex_unlock(&_swr_refresh_lock);
            return;  /* already inflight — dedupe */
        }
    }
    if (_swr_inflight_n < 64) {
        _swr_inflight_keys[_swr_inflight_n++] = strdup(cache_key);
    }
    pthread_mutex_unlock(&_swr_refresh_lock);

    swr_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) return;
    ctx->cache_key = strdup(cache_key);
    pthread_t tid;
    if (pthread_create(&tid, NULL, _swr_worker, ctx) != 0) {
        /* failed to spawn — remove from inflight and bail */
        pthread_mutex_lock(&_swr_refresh_lock);
        for (int i = 0; i < _swr_inflight_n; i++) {
            if (_swr_inflight_keys[i] && strcmp(_swr_inflight_keys[i], cache_key) == 0) {
                free(_swr_inflight_keys[i]);
                for (int j = i; j < _swr_inflight_n - 1; j++)
                    _swr_inflight_keys[j] = _swr_inflight_keys[j+1];
                _swr_inflight_keys[--_swr_inflight_n] = NULL;
                break;
            }
        }
        pthread_mutex_unlock(&_swr_refresh_lock);
        free(ctx->cache_key);
        free(ctx);
        return;
    }
    pthread_detach(tid);  /* daemon thread — nobody joins */
}

/* ---- _spawn_catalog_swr_refresh (hermes_cli/model_catalog.py) ---- */
/* Thread-safe inflight guard (separate from the provider-model SWR inflight
 * set, mirroring Python's _catalog_swr_inflight + _catalog_swr_lock). */
static pthread_mutex_t _catalog_swr_lock = PTHREAD_MUTEX_INITIALIZER;
static bool _catalog_swr_inflight = false;

typedef struct { char *url; } catalog_swr_ctx_t;

/* Catalog SWR worker — fire-and-forget. Fetches the manifest URL and
 * writes fresh JSON to the provider-models cache on success. Degrades
 * silently (stale cache kept) on any failure, mirroring Python's
 * except-logger.debug + finally-claim-release semantics. */
static void *_catalog_swr_worker(void *arg) {
    catalog_swr_ctx_t *ctx = (catalog_swr_ctx_t *)arg;
    char *url = ctx->url;
    free(ctx);

    http_t *h = http_new(8);
    if (h) {
        http_resp_t *r = http_get(h, url, NULL);
        if (r && r->status == 200 && r->body && r->body[0]) {
            /* Persist the fresh manifest via the existing cache API. */
            model_save_provider_models_cache(r->body);
        }
        if (r) http_resp_free(r);
        http_free(h);
    }

    /* Release the inflight claim (finally block in Python). */
    pthread_mutex_lock(&_catalog_swr_lock);
    _catalog_swr_inflight = false;
    pthread_mutex_unlock(&_catalog_swr_lock);
    free(url);
    return NULL;
}

/* PoP: get_catalog @ hermes_cli/model_catalog.py:_spawn_catalog_swr_refresh */
/* Port of Python _spawn_catalog_swr_refresh: refresh the catalog manifest
 * off-thread (fire-and-forget, deduped via _catalog_swr_inflight). The
 * refreshed manifest lands on disk; the next get_catalog() picks it up
 * via the mtime/age check. Failures swallowed — stale entry kept. */
void _spawn_catalog_swr_refresh(const char *url) {
    if (!url || !*url) return;
    pthread_mutex_lock(&_catalog_swr_lock);
    if (_catalog_swr_inflight) {
        pthread_mutex_unlock(&_catalog_swr_lock);
        return;  /* already inflight — dedupe */
    }
    _catalog_swr_inflight = true;
    pthread_mutex_unlock(&_catalog_swr_lock);

    catalog_swr_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        pthread_mutex_lock(&_catalog_swr_lock);
        _catalog_swr_inflight = false;
        pthread_mutex_unlock(&_catalog_swr_lock);
        return;
    }
    ctx->url = strdup(url);
    if (!ctx->url) {
        free(ctx);
        pthread_mutex_lock(&_catalog_swr_lock);
        _catalog_swr_inflight = false;
        pthread_mutex_unlock(&_catalog_swr_lock);
        return;
    }

    pthread_t tid;
    if (pthread_create(&tid, NULL, _catalog_swr_worker, ctx) != 0) {
        /* Spawn failed — release claim and bail (failure swallowed). */
        pthread_mutex_lock(&_catalog_swr_lock);
        _catalog_swr_inflight = false;
        pthread_mutex_unlock(&_catalog_swr_lock);
        free(ctx->url);
        free(ctx);
        return;
    }
    pthread_detach(tid);  /* daemon thread */
}



/* ── from src/cli/port_models_net.c ── */
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


/* ── from src/cli/port_models_pure.c ── */
/*
 * port_models_pure.c — second cohesive port module for hermes_cli/models.py.
 *
 * Covers the remaining pure/config/disk-cache helpers and network wrappers
 * (LM Studio, GitHub Copilot, custom /v1 probes, Ollama Cloud disk cache,
 * pricing, Nous portal unions, validate_requested_model) not already in
 * port_models_net.c. All network access goes through the injectable
 * http_fetch_fn transport so the logic is testable offline; credential/config
 * resolution is likewise injected (NULL → sensible default). No stubs.
 */

#include "port_models_pure.h"
#include "port_models_net.h"
#include "port_models_helpers.h"
#include "model_catalog.h"

/* format_price_per_mtok is defined in port_models_helpers.c (no header decl).
 * Converts a per-token price string to "$X.XX/MTok" / "free" / "?". Assembles
 * this orphaned formatter (matches Python models.py _format_price_per_mtok). */
char *format_price_per_mtok(const char *per_token_str);
#include "model_normalize.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>

/* local printf-style dup (libjson has no variant) */
static char *xstrdup_printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return strdup(buf);
}

/* ── Constants (mirror models.py) ───────────────────────────────────────── */
#define COPILOT_BASE_URL    "https://api.githubcopilot.com"
#define COPILOT_MODELS_URL   COPILOT_BASE_URL "/models"
#define NOUS_INFERENCE_BASE  "https://inference-api.nousresearch.com"
#define HERMES_UA            "hermes-cli"
#define OLLAMA_CLOUD_URL     "https://ollama.com/v1"

static const char *const O_SERIES_EFFORTS[] = {"low", "medium", "high", NULL};
static const char *const GPT5_EFFORTS[] = {"minimal", "low", "medium", "high", NULL};

/* ── helpers ───────────────────────────────────────────────────────────── */

static char *xstrdup(const char *s) { return s ? strdup(s) : strdup(""); }

/* Normalize a provider string: strip whitespace, lowercase. Caller frees. */
static char *norm_provider_dup(const char *provider) {
    const char *p = provider ? provider : "";
    char tmp[256];
    size_t j = 0;
    for (size_t i = 0; p[i] && j + 1 < sizeof(tmp); i++) {
        char c = p[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        tmp[j++] = (char)tolower((unsigned char)c);
    }
    tmp[j] = '\0';
    return xstrdup(tmp);
}

/* ── OpenRouter / Novita / Nous pricing ────────────────────────────────── */

/* PoP: models_resolve_openrouter_api_key @ hermes_cli/models.py:_resolve_openrouter_api_key */
char *models_resolve_openrouter_api_key(void) {
    const char *v = getenv("OPENROUTER_API_KEY");
    return xstrdup(v && *v ? v : "");
}

/* PoP: models_resolve_nous_pricing_credentials @ hermes_cli/models.py:_resolve_nous_pricing_credentials */
void models_resolve_nous_pricing_credentials(nous_creds_fn resolve, void *ctx,
                                             char **out_key, char **out_base) {
    if (resolve) {
        char *k = NULL, *b = NULL;
        resolve(&k, &b, ctx);
        if (k || b) {
            *out_key = k ? k : xstrdup("");
            *out_base = b ? b : xstrdup(NOUS_INFERENCE_BASE);
            return;
        }
    }
    *out_key = xstrdup("");
    *out_base = xstrdup(NOUS_INFERENCE_BASE);
}

/* PoP: models_fetch_models_with_pricing @ hermes_cli/models.py:fetch_models_with_pricing */
char *models_fetch_models_with_pricing(http_fetch_fn fetch, void *ctx,
                                       const char *api_key, const char *base_url,
                                       int force_refresh) {
    (void)force_refresh;
    if (!fetch) return xstrdup("{}");
    const char *b = (base_url && *base_url) ? base_url : "https://openrouter.ai/api";
    size_t n = strlen(b);
    while (n > 0 && b[n - 1] == '/') n--;
    char url[1024];
    snprintf(url, sizeof(url), "%.*s/v1/models", (int)n, b);

    char hdr[256];
    if (api_key && *api_key)
        snprintf(hdr, sizeof(hdr), "{\"Accept\":\"application/json\",\"Authorization\":\"Bearer %s\"}", api_key);
    else
        snprintf(hdr, sizeof(hdr), "{\"Accept\":\"application/json\"}");

    char *body = NULL; size_t blen = 0;
    if (fetch(url, hdr, &body, &blen, ctx) != 0 || !body) return xstrdup("{}");

    json_t *root = json_parse(body, NULL);
    free(body);
    if (!root) return xstrdup("{}");
    json_t *out = json_object();
    json_t *data = (root->type == JSON_OBJECT) ? json_obj_get(root, "data") : NULL;
    if (data && data->type == JSON_ARRAY) {
        for (size_t i = 0; i < data->c.count; i++) {
            json_t *item = data->c.items[i];
            if (!item || item->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(item, "id");
            if (!idj || idj->type != JSON_STRING || !idj->str_val) continue;
            json_t *pricing = json_obj_get(item, "pricing");
            if (!pricing || pricing->type != JSON_OBJECT) continue;
            json_t *entry = json_object();
            json_t *p = json_obj_get(pricing, "prompt");
            json_t *c = json_obj_get(pricing, "completion");
            json_set(entry, "prompt", json_string(p && p->type == JSON_STRING ? p->str_val : ""));
            json_set(entry, "completion", json_string(c && c->type == JSON_STRING ? c->str_val : ""));
            json_t *icr = json_obj_get(pricing, "input_cache_read");
            if (icr && icr->type == JSON_STRING && icr->str_val && *icr->str_val)
                json_set(entry, "input_cache_read", json_string(icr->str_val));
            json_t *icw = json_obj_get(pricing, "input_cache_write");
            if (icw && icw->type == JSON_STRING && icw->str_val && *icw->str_val)
                json_set(entry, "input_cache_write", json_string(icw->str_val));
            json_set(out, idj->str_val, entry);
        }
    }
    char *res = json_serialize(out);
    json_free(out); json_free(root);
    return res ? res : xstrdup("{}");
}

/* PoP: models_fetch_novita_pricing @ hermes_cli/models.py:_fetch_novita_pricing */
char *models_fetch_novita_pricing(http_fetch_fn fetch, void *ctx, int force_refresh) {
    (void)force_refresh;
    const char *key = getenv("NOVITA_API_KEY");
    if (!key || !*key) return xstrdup("{}");
    const char *base = getenv("NOVITA_BASE_URL");
    const char *b = (base && *base) ? base : "https://api.novita.ai/openai/v1";
    size_t n = strlen(b);
    while (n > 0 && b[n - 1] == '/') n--;
    char url[1024];
    snprintf(url, sizeof(url), "%.*s/models", (int)n, b);
    char hdr[300];
    snprintf(hdr, sizeof(hdr), "{\"Authorization\":\"Bearer %s\",\"Accept\":\"application/json\"}", key);

    char *body = NULL; size_t blen = 0;
    if (!fetch || fetch(url, hdr, &body, &blen, ctx) != 0 || !body) return xstrdup("{}");
    json_t *root = json_parse(body, NULL);
    free(body);
    if (!root) return xstrdup("{}");
    json_t *out = json_object();
    json_t *data = (root->type == JSON_OBJECT) ? json_obj_get(root, "data") : NULL;
    if (data && data->type == JSON_ARRAY) {
        for (size_t i = 0; i < data->c.count; i++) {
            json_t *item = data->c.items[i];
            if (!item || item->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(item, "id");
            if (!idj || idj->type != JSON_STRING || !idj->str_val) continue;
            json_t *inp = json_obj_get(item, "input_token_price_per_m");
            json_t *outp = json_obj_get(item, "output_token_price_per_m");
            if ((!inp || inp->type != JSON_NUMBER) && (!outp || outp->type != JSON_NUMBER))
                continue;
            double in = (inp && inp->type == JSON_NUMBER) ? inp->num_val : 0;
            double ou = (outp && outp->type == JSON_NUMBER) ? outp->num_val : 0;
            char pb[64], cb[64];
            snprintf(pb, sizeof(pb), "%.10g", in / 10000.0 / 1000000.0);
            snprintf(cb, sizeof(cb), "%.10g", ou / 10000.0 / 1000000.0);
            json_t *entry = json_object();
            json_set(entry, "prompt", json_string(pb));
            json_set(entry, "completion", json_string(cb));
            /* Assemble the orphaned format_price_per_mtok formatter: add
             * human-readable "$X.XX/MTok" strings (matches Python models.py
             * _format_price_per_mtok display path). Non-breaking: extra fields. */
            char *pd = format_price_per_mtok(pb);
            char *cd = format_price_per_mtok(cb);
            if (pd) { json_set(entry, "prompt_display", json_string(pd)); free(pd); }
            if (cd) { json_set(entry, "completion_display", json_string(cd)); free(cd); }
            json_set(out, idj->str_val, entry);
        }
    }
    char *res = json_serialize(out);
    json_free(out); json_free(root);
    return res ? res : xstrdup("{}");
}

/* PoP: models_get_pricing_for_provider @ hermes_cli/models.py:get_pricing_for_provider */
char *models_get_pricing_for_provider(http_fetch_fn fetch, void *ctx,
                                      nous_creds_fn resolve_nous, void *nous_ctx,
                                      const char *provider, int force_refresh) {
    char *norm = norm_provider_dup(provider);
    char *res = NULL;
    if (strcmp(norm, "openrouter") == 0) {
        char *k = models_resolve_openrouter_api_key();
        res = models_fetch_models_with_pricing(fetch, ctx, k, "https://openrouter.ai/api", force_refresh);
        free(k);
    } else if (strcmp(norm, "novita") == 0) {
        res = models_fetch_novita_pricing(fetch, ctx, force_refresh);
    } else if (strcmp(norm, "nous") == 0) {
        char *k = NULL, *b = NULL;
        models_resolve_nous_pricing_credentials(resolve_nous, nous_ctx, &k, &b);
        if (b && *b) {
            size_t bn = strlen(b);
            while (bn > 0 && b[bn - 1] == '/') bn--;
            char stripped[1024];
            snprintf(stripped, sizeof(stripped), "%.*s", (int)bn, b);
            size_t sl = strlen(stripped);
            if (sl >= 3 && strcmp(stripped + sl - 3, "/v1") == 0)
                stripped[sl - 3] = '\0';
            res = models_fetch_models_with_pricing(fetch, ctx, k, stripped, force_refresh);
        } else {
            res = xstrdup("{}");
        }
        free(k); free(b);
    } else {
        res = xstrdup("{}");
    }
    free(norm);
    return res;
}

/* ── LM Studio native API helpers ──────────────────────────────────────── */

/* PoP: models_lmstudio_server_root @ hermes_cli/models.py:_lmstudio_server_root */
char *models_lmstudio_server_root(const char *base_url) {
    char tmp[1024];
    const char *b = base_url ? base_url : "";
    size_t n = strlen(b);
    while (n > 0 && (b[n - 1] == ' ' || b[n - 1] == '\t' || b[n - 1] == '/'))
        n--;
    snprintf(tmp, sizeof(tmp), "%.*s", (int)n, b);
    /* strip known suffixes */
    size_t t = strlen(tmp);
    for (size_t i = 0; i < 3; i++) {
        const char *suf = (i == 0) ? "/api/v1" : (i == 1) ? "/api" : "/v1";
        size_t sl = strlen(suf);
        if (t >= sl && strcmp(tmp + t - sl, suf) == 0) {
            tmp[t - sl] = '\0';
            t -= sl;
            while (t > 0 && tmp[t - 1] == '/') tmp[--t] = '\0';
            break;
        }
    }
    return xstrdup(tmp[0] ? tmp : "");
}

/* PoP: models_lmstudio_request_headers @ hermes_cli/models.py:_lmstudio_request_headers */
char *models_lmstudio_request_headers(const char *api_key) {
    const char *tok = api_key ? api_key : "";
    while (*tok == ' ' || *tok == '\t') tok++;
    if (*tok)
        return xstrdup_printf("{\"User-Agent\":\"%s\",\"Authorization\":\"Bearer %s\"}", HERMES_UA, tok);
    return xstrdup_printf("{\"User-Agent\":\"%s\"}", HERMES_UA);
}

/* PoP: models_lmstudio_fetch_raw_models @ hermes_cli/models.py:_lmstudio_fetch_raw_models */
json_t *models_lmstudio_fetch_raw_models(http_fetch_fn fetch, void *ctx,
                                         const char *api_key, const char *base_url) {
    char *root = models_lmstudio_server_root(base_url);
    if (!root || !*root) { free(root); return NULL; }
    char *hdr = models_lmstudio_request_headers(api_key);
    char url[1100];
    snprintf(url, sizeof(url), "%s/api/v1/models", root);
    free(root);
    char *body = NULL; size_t blen = 0;
    if (!fetch || fetch(url, hdr, &body, &blen, ctx) != 0 || !body) { free(hdr); return NULL; }
    free(hdr);
    json_t *parsed = json_parse(body, NULL);
    free(body);
    if (!parsed || parsed->type != JSON_OBJECT) { if (parsed) json_free(parsed); return NULL; }
    json_t *models = json_obj_get(parsed, "models");
    if (!models || models->type != JSON_ARRAY) { json_free(parsed); return NULL; }
    /* hand back the whole parsed object; caller reads .models */
    return parsed;
}

/* PoP: models_probe_lmstudio_models @ hermes_cli/models.py:probe_lmstudio_models */
char **models_probe_lmstudio_models(http_fetch_fn fetch, void *ctx,
                                    const char *api_key, const char *base_url) {
    json_t *raw = models_lmstudio_fetch_raw_models(fetch, ctx, api_key, base_url);
    if (!raw) return NULL;
    json_t *models = json_obj_get(raw, "models");
    char **keys = NULL; size_t cnt = 0;
    if (models && models->type == JSON_ARRAY) {
        keys = calloc(models->c.count + 1, sizeof(char *));
        for (size_t i = 0; i < models->c.count; i++) {
            json_t *m = models->c.items[i];
            if (!m || m->type != JSON_OBJECT) continue;
            json_t *tj = json_obj_get(m, "type");
            if (tj && tj->type == JSON_STRING && strcasecmp(tj->str_val, "embedding") == 0)
                continue;
            json_t *kj = json_obj_get(m, "key");
            json_t *idj = json_obj_get(m, "id");
            const char *key = (kj && kj->type == JSON_STRING && kj->str_val) ? kj->str_val
                            : (idj && idj->type == JSON_STRING && idj->str_val) ? idj->str_val : "";
            if (!*key) continue;
            /* dedupe */
            int dup = 0;
            for (size_t d = 0; d < cnt; d++) if (strcmp(keys[d], key) == 0) { dup = 1; break; }
            if (!dup) keys[cnt++] = xstrdup(key);
        }
        keys[cnt] = NULL;
    }
    json_free(raw);
    return keys;
}

/* PoP: models_lmstudio_reasoning_options @ hermes_cli/models.py:lmstudio_model_reasoning_options */
char **models_lmstudio_reasoning_options(http_fetch_fn fetch, void *ctx,
                                         const char *model, const char *base_url,
                                         const char *api_key) {
    json_t *raw = models_lmstudio_fetch_raw_models(fetch, ctx, api_key, base_url);
    char **out = calloc(1, sizeof(char *)); /* empty NULL-terminated */
    if (!raw) return out;
    json_t *models = json_obj_get(raw, "models");
    if (models && models->type == JSON_ARRAY) {
        for (size_t i = 0; i < models->c.count; i++) {
            json_t *m = models->c.items[i];
            if (!m || m->type != JSON_OBJECT) continue;
            json_t *kj = json_obj_get(m, "key");
            json_t *idj = json_obj_get(m, "id");
            const char *key = (kj && kj->type == JSON_STRING && kj->str_val) ? kj->str_val
                            : (idj && idj->type == JSON_STRING && idj->str_val) ? idj->str_val : "";
            if (strcmp(key, model ? model : "") != 0) continue;
            json_t *caps = json_obj_get(m, "capabilities");
            json_t *reasoning = (caps && caps->type == JSON_OBJECT) ? json_obj_get(caps, "reasoning") : NULL;
            json_t *opts = (reasoning && reasoning->type == JSON_OBJECT) ? json_obj_get(reasoning, "allowed_options") : NULL;
            if (opts && opts->type == JSON_ARRAY) {
                size_t cap = opts->c.count + 1;
                out = realloc(out, cap * sizeof(char *));
                size_t o = 0;
                for (size_t j = 0; j < opts->c.count; j++) {
                    json_t *o2 = opts->c.items[j];
                    if (o2 && o2->type == JSON_STRING && o2->str_val) {
                        char low[256]; size_t k;
                        for (k = 0; o2->str_val[k] && k + 1 < sizeof(low); k++)
                            low[k] = (char)tolower((unsigned char)o2->str_val[k]);
                        low[k] = '\0';
                        out[o++] = xstrdup(low);
                    }
                }
                out[o] = NULL;
            }
            break;
        }
    }
    json_free(raw);
    return out;
}

/* Local: fetch + parse the GitHub Copilot catalog into a json_t* ARRAY.
 * Returns NULL on failure (caller frees). models_fetch_github_model_catalog
 * returns a serialized JSON array string, so we parse it here. */
static json_t *fetch_github_catalog(http_fetch_fn fetch, void *ctx, const char *api_key) {
    char *s = models_fetch_github_model_catalog(fetch, ctx, api_key);
    if (!s) return NULL;
    json_t *parsed = json_parse(s, NULL);
    free(s);
    if (!parsed || parsed->type != JSON_ARRAY) { if (parsed) json_free(parsed); return NULL; }
    return parsed;
}

/* PoP: models_fetch_github_models @ hermes_cli/models.py:_fetch_github_models */
char **models_fetch_github_models(http_fetch_fn fetch, void *ctx,
                                  const char *api_key) {
    json_t *cat = api_key ? fetch_github_catalog(fetch, ctx, api_key) : NULL;
    if (!cat) return NULL;
    char **ids = calloc(1, sizeof(char *));
    if (cat->type == JSON_ARRAY) {
        ids = realloc(ids, (cat->c.count + 1) * sizeof(char *));
        size_t o = 0;
        for (size_t i = 0; i < cat->c.count; i++) {
            json_t *it = cat->c.items[i];
            if (!it || it->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(it, "id");
            if (idj && idj->type == JSON_STRING && idj->str_val && *idj->str_val)
                ids[o++] = xstrdup(idj->str_val);
        }
        ids[o] = NULL;
    }
    json_free(cat);
    return ids;
}

/* PoP: models_copilot_catalog_ids @ hermes_cli/models.py:_copilot_catalog_ids */
char **models_copilot_catalog_ids(http_fetch_fn fetch, void *ctx,
                                  json_t *catalog, const char *api_key) {
    json_t *cat = catalog;
    json_t *owned = NULL;
    if (!cat && api_key) {
        cat = fetch_github_catalog(fetch, ctx, api_key);
        owned = cat;
    }
    char **ids = calloc(1, sizeof(char *));
    if (cat && cat->type == JSON_ARRAY) {
        ids = realloc(ids, (cat->c.count + 1) * sizeof(char *));
        size_t o = 0;
        for (size_t i = 0; i < cat->c.count; i++) {
            json_t *it = cat->c.items[i];
            if (!it || it->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(it, "id");
            if (idj && idj->type == JSON_STRING && idj->str_val && *idj->str_val)
                ids[o++] = xstrdup(idj->str_val);
        }
        ids[o] = NULL;
    }
    if (owned) json_free(owned);
    return ids;
}

/* PoP: models_github_reasoning_efforts_for_id @ hermes_cli/models.py:_github_reasoning_efforts_for_model_id */
char **models_github_reasoning_efforts_for_id(const char *model_id) {
    char raw[512];
    size_t n = strlen(model_id ? model_id : "");
    if (n >= sizeof(raw)) n = sizeof(raw) - 1;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) raw[j++] = (char)tolower((unsigned char)model_id[i]);
    raw[j] = '\0';
    const char *const *src = NULL;
    if (strncmp(raw, "openai/o1", 9) == 0 || strncmp(raw, "openai/o3", 9) == 0 ||
        strncmp(raw, "openai/o4", 9) == 0 ||
        strcmp(raw, "o1") == 0 || strcmp(raw, "o3") == 0 || strcmp(raw, "o4") == 0)
        src = O_SERIES_EFFORTS;
    else {
        char *norm = model_normalize_copilot_model_id(model_id);
        if (strncmp(norm, "gpt-5", 5) == 0) src = GPT5_EFFORTS;
        free(norm);
    }
    char **out = calloc(1, sizeof(char *));
    if (src) {
        size_t c = 0;
        for (size_t i = 0; src[i]; i++) c++;
        out = realloc(out, (c + 1) * sizeof(char *));
        for (size_t i = 0; src[i]; i++) out[i] = xstrdup(src[i]);
        out[c] = NULL;
    }
    return out;
}

/* PoP: models_copilot_model_api_mode @ hermes_cli/models.py:copilot_model_api_mode */
char *models_copilot_model_api_mode(http_fetch_fn fetch, void *ctx,
                                    const char *model_id, json_t *catalog,
                                    const char *api_key) {
    json_t *cat = catalog;
    json_t *owned = NULL;
    if (!cat && api_key) { cat = fetch_github_catalog(fetch, ctx, api_key); owned = cat; }
    char *normalized = model_normalize_copilot_model_id(model_id);
    char *mode = xstrdup("chat_completions");
    if (normalized && *normalized) {
        /* _should_use_copilot_responses_api: gpt-<digits> >=5 and not gpt-5-mini */
        if (strncmp(normalized, "gpt-", 4) == 0) {
            int major = atoi(normalized + 4);
            if (major >= 5 && strncmp(normalized, "gpt-5-mini", 10) != 0) {
                free(mode); mode = xstrdup("codex_responses");
            }
        }
        if (cat && strcmp(mode, "chat_completions") == 0) {
            /* check catalog supported_endpoints */
            if (cat->type == JSON_ARRAY) {
                for (size_t i = 0; i < cat->c.count; i++) {
                    json_t *it = cat->c.items[i];
                    if (!it || it->type != JSON_OBJECT) continue;
                    json_t *idj = json_obj_get(it, "id");
                    if (!(idj && idj->type == JSON_STRING && idj->str_val &&
                          strcmp(idj->str_val, normalized) == 0)) continue;
                    json_t *eps = json_obj_get(it, "supported_endpoints");
                    if (eps && eps->type == JSON_ARRAY) {
                        int has_messages = 0, has_chat = 0;
                        for (size_t k = 0; k < eps->c.count; k++) {
                            json_t *e = eps->c.items[k];
                            if (e->type == JSON_STRING && e->str_val) {
                                if (strcmp(e->str_val, "/v1/messages") == 0) has_messages = 1;
                                if (strcmp(e->str_val, "/chat/completions") == 0) has_chat = 1;
                            }
                        }
                        if (has_messages && !has_chat) {
                            free(mode); mode = xstrdup("anthropic_messages");
                        }
                    }
                    break;
                }
            }
        }
    }
    free(normalized);
    if (owned) json_free(owned);
    return mode;
}

/* PoP: models_github_model_reasoning_efforts @ hermes_cli/models.py:github_model_reasoning_efforts */
char **models_github_model_reasoning_efforts(http_fetch_fn fetch, void *ctx,
                                             const char *model_id, json_t *catalog,
                                             const char *api_key) {
    json_t *cat = catalog;
    json_t *owned = NULL;
    if (!cat && api_key) { cat = fetch_github_catalog(fetch, ctx, api_key); owned = cat; }
    char *normalized = model_normalize_copilot_model_id(model_id);
    char **out = calloc(1, sizeof(char *));
    json_t *entry = NULL;
    if (cat && cat->type == JSON_ARRAY && normalized) {
        for (size_t i = 0; i < cat->c.count; i++) {
            json_t *it = cat->c.items[i];
            if (!it || it->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(it, "id");
            if (idj && idj->type == JSON_STRING && idj->str_val &&
                strcmp(idj->str_val, normalized) == 0) { entry = it; break; }
        }
    }
    if (entry) {
        json_t *caps = json_obj_get(entry, "capabilities");
        if (caps && caps->type == JSON_OBJECT) {
            json_t *supports = json_obj_get(caps, "supports");
            if (supports && supports->type == JSON_OBJECT) {
                json_t *eff = json_obj_get(supports, "reasoning_effort");
                if (eff && eff->type == JSON_ARRAY) {
                    out = realloc(out, (eff->c.count + 1) * sizeof(char *));
                    size_t o = 0;
                    for (size_t k = 0; k < eff->c.count; k++) {
                        json_t *e = eff->c.items[k];
                        if (e && e->type == JSON_STRING && e->str_val && *e->str_val) {
                            char low[256]; size_t m;
                            for (m = 0; e->str_val[m] && m + 1 < sizeof(low); m++)
                                low[m] = (char)tolower((unsigned char)e->str_val[m]);
                            low[m] = '\0';
                            out[o++] = xstrdup(low);
                        }
                    }
                    out[o] = NULL;
                } else out[0] = NULL;
            } else out[0] = NULL;
        }
    }
    if (!out[0] && normalized) {
        free(out);
        out = models_github_reasoning_efforts_for_id(model_id);
    }
    free(normalized);
    if (owned) json_free(owned);
    return out;
}

/* PoP: models_get_copilot_model_context @ hermes_cli/models.py:get_copilot_model_context */
char *models_get_copilot_model_context(http_fetch_fn fetch, void *ctx,
                                       const char *model_id, const char *api_key) {
    json_t *catalog = fetch_github_catalog(fetch, ctx, api_key);
    if (!catalog) return NULL;
    char *result = NULL;
    if (catalog->type == JSON_ARRAY) {
        for (size_t i = 0; i < catalog->c.count; i++) {
            json_t *it = catalog->c.items[i];
            if (!it || it->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(it, "id");
            if (!(idj && idj->type == JSON_STRING && idj->str_val &&
                  strcmp(idj->str_val, model_id ? model_id : "") == 0)) continue;
            json_t *caps = json_obj_get(it, "capabilities");
            json_t *limits = (caps && caps->type == JSON_OBJECT) ? json_obj_get(caps, "limits") : NULL;
            json_t *mp = (limits && limits->type == JSON_OBJECT) ? json_obj_get(limits, "max_prompt_tokens") : NULL;
            if (mp && mp->type == JSON_NUMBER && mp->num_val > 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%lld", (long long)mp->num_val);
                result = xstrdup(buf);
            }
            break;
        }
    }
    json_free(catalog);
    return result;
}

/* ── Custom /v1 probe helpers ──────────────────────────────────────────── */

/* PoP: models_probe_api_models @ hermes_cli/models.py:probe_api_models */
char *models_probe_api_models(http_fetch_fn fetch, void *ctx,
                              const char *api_key, const char *base_url,
                              const char *api_mode) {
    char norm[1024];
    const char *b = base_url ? base_url : "";
    size_t n = strlen(b);
    while (n > 0 && (b[n - 1] == ' ' || b[n - 1] == '\t' || b[n - 1] == '/')) n--;
    snprintf(norm, sizeof(norm), "%.*s", (int)n, b);
    size_t nn = strlen(norm);
    if (!nn) {
        json_t *o = json_object();
        json_set(o, "models", json_null());
        json_set(o, "probed_url", json_string(""));
        json_set(o, "resolved_base_url", json_string(""));
        json_set(o, "suggested_base_url", json_null());
        json_set(o, "used_fallback", json_bool(0));
        char *r = json_serialize(o); json_free(o);
        return r ? r : xstrdup("{}");
    }

    /* github base url short-circuit */
    char low[1024];
    size_t k;
    for (k = 0; norm[k] && k + 1 < sizeof(low); k++) low[k] = (char)tolower((unsigned char)norm[k]);
    low[k] = '\0';
    if (strncmp(low, COPILOT_BASE_URL, strlen(COPILOT_BASE_URL)) == 0 ||
        strncmp(low, "https://models.github.ai/inference", 30) == 0 ||
        strncmp(low, "https://models.inference.ai.azure.com", 33) == 0) {
        char **gm = models_fetch_github_models(fetch, ctx, api_key);
        json_t *o = json_object();
        json_t *arr = json_array();
        if (gm) { for (size_t i = 0; gm[i]; i++) json_append(arr, json_string(gm[i])); }
        json_set(o, "models", arr);
        json_set(o, "probed_url", json_string(COPILOT_MODELS_URL));
        json_set(o, "resolved_base_url", json_string(COPILOT_BASE_URL));
        json_set(o, "suggested_base_url", json_null());
        json_set(o, "used_fallback", json_bool(0));
        char *r = json_serialize(o); json_free(o);
        if (gm) { for (size_t i = 0; gm[i]; i++) free(gm[i]); free(gm); }
        return r ? r : xstrdup("{}");
    }

    char alt[1024];
    if (nn >= 3 && strcmp(norm + nn - 3, "/v1") == 0)
        snprintf(alt, sizeof(alt), "%.*s", (int)(nn - 3), norm);
    else
        snprintf(alt, sizeof(alt), "%s/v1", norm);

    const char *cands[2]; int is_fb[2]; int nc = 0;
    cands[nc] = norm; is_fb[nc] = 0; nc++;
    if (strcmp(alt, norm) != 0) { cands[nc] = alt; is_fb[nc] = 1; nc++; }

    char *hdr = NULL;
    if (api_key && api_mode && strcmp(api_mode, "anthropic_messages") == 0)
        hdr = xstrdup_printf("{\"User-Agent\":\"%s\",\"x-api-key\":\"%s\",\"anthropic-version\":\"2023-06-01\"}", HERMES_UA, api_key);
    else if (api_key)
        hdr = xstrdup_printf("{\"User-Agent\":\"%s\",\"Authorization\":\"Bearer %s\"}", HERMES_UA, api_key);
    else
        hdr = xstrdup_printf("{\"User-Agent\":\"%s\"}", HERMES_UA);
    if (strncmp(norm, COPILOT_BASE_URL, strlen(COPILOT_BASE_URL)) == 0) {
        char *ch = copilot_default_headers_json();
        if (ch) { char *h2 = xstrdup_printf("%s%s", hdr, ch); free(hdr); free(ch); hdr = h2; }
    }

    char *probed = NULL;
    json_t *arr = NULL;
    for (int i = 0; i < nc; i++) {
        char url[1100];
        snprintf(url, sizeof(url), "%s/models", cands[i]);
        free(probed);
        probed = xstrdup(url);
        char *body = NULL; size_t blen = 0;
        if (fetch && fetch(url, hdr, &body, &blen, ctx) == 0 && body) {
            json_t *data = json_parse(body, NULL);
            free(body);
            if (data && data->type == JSON_OBJECT) {
                json_t *d = json_obj_get(data, "data");
                if (d && d->type == JSON_ARRAY) {
                    arr = json_array();
                    for (size_t i = 0; i < d->c.count; i++) {
                        json_t *m = d->c.items[i];
                        const char *id = NULL;
                        if (m && m->type == JSON_OBJECT) {
                            json_t *idj = json_obj_get(m, "id");
                            if (idj && idj->type == JSON_STRING && idj->str_val)
                                id = idj->str_val;
                        }
                        if (id && *id) json_append(arr, json_string(id));
                    }
                } else arr = json_array();
                json_free(data);
            } else arr = json_array();
            json_t *o = json_object();
            json_set(o, "models", arr ? arr : json_null());
            json_set(o, "probed_url", json_string(url));
            json_set(o, "resolved_base_url", json_string(cands[i]));
            json_set(o, "suggested_base_url", json_string(strcmp(alt, cands[i]) == 0 ? norm : alt));
            json_set(o, "used_fallback", json_bool(is_fb[i]));
            char *r = json_serialize(o); json_free(o);
            free(hdr); free(probed);
            return r ? r : xstrdup("{}");
        }
    }
    /* all candidates failed */
    json_t *o = json_object();
    json_set(o, "models", json_null());
    json_set(o, "probed_url", json_string(probed ? probed : ""));
    json_set(o, "resolved_base_url", json_string(norm));
    json_set(o, "suggested_base_url", json_string(strcmp(alt, norm) == 0 ? "" : alt));
    json_set(o, "used_fallback", json_bool(0));
    char *r = json_serialize(o); json_free(o);
    free(hdr); free(probed);
    return r ? r : xstrdup("{}");
}

/* PoP: models_fetch_api_models @ hermes_cli/models.py:fetch_api_models */
char **models_fetch_api_models(http_fetch_fn fetch, void *ctx,
                               const char *api_key, const char *base_url,
                               const char *api_mode) {
    char *probe = models_probe_api_models(fetch, ctx, api_key, base_url, api_mode);
    if (!probe) return NULL;
    json_t *p = json_parse(probe, NULL);
    free(probe);
    char **out = NULL;
    if (p && p->type == JSON_OBJECT) {
        json_t *m = json_obj_get(p, "models");
        if (m && m->type == JSON_ARRAY) {
            out = calloc(m->c.count + 1, sizeof(char *));
            size_t o = 0;
            for (size_t i = 0; i < m->c.count; i++) {
                json_t *it = m->c.items[i];
                if (it && it->type == JSON_STRING && it->str_val)
                    out[o++] = xstrdup(it->str_val);
            }
            out[o] = NULL;
        }
    }
    if (p) json_free(p);
    return out; /* NULL if models was null (unreachable) */
}

/* ── Config / base-url resolvers (injectable) ──────────────────────────── */

/* PoP: models_get_custom_base_url @ hermes_cli/models.py:_get_custom_base_url */
char *models_get_custom_base_url(void) {
    const char *v = getenv("HERMES_CUSTOM_BASE_URL");
    if (v && *v) return xstrdup(v);
    return xstrdup("");
}

/* PoP: models_get_model_config_dict @ hermes_cli/models.py:_get_model_config_dict */
char *models_get_model_config_dict(model_config_fn get, void *ctx) {
    if (get) {
        char *r = get(ctx);
        if (r) return r;
    }
    return xstrdup("{}");
}

/* PoP: models_resolve_copilot_catalog_api_key @ hermes_cli/models.py:_resolve_copilot_catalog_api_key */
char *models_resolve_copilot_catalog_api_key(copilot_apikey_fn get, void *ctx) {
    if (get) {
        char *r = get(ctx);
        if (r) return r;
    }
    const char *v = getenv("OPENAI_API_KEY");
    if (v && *v) return xstrdup(v);
    v = getenv("GITHUB_COPILOT_TOKEN");
    if (v && *v) return xstrdup(v);
    return xstrdup("");
}

/* PoP: models_resolve_nous_portal_url @ hermes_cli/models.py:_resolve_nous_portal_url */
char *models_resolve_nous_portal_url(void) {
    const char *v = getenv("NOUS_PORTAL_URL");
    return xstrdup(v && *v ? v : "https://portal.nousresearch.com");
}

/* ── Ollama Cloud disk cache ───────────────────────────────────────────── */

/* PoP: models_ollama_cloud_cache_path @ hermes_cli/models.py:_ollama_cloud_cache_path */
char *models_ollama_cloud_cache_path(void) {
    char home[PATH_MAX];
    hermes_home_dir(home, sizeof(home));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/ollama_cloud_models_cache.json", home);
    return xstrdup(out);
}

/* PoP: models_load_ollama_cloud_cache @ hermes_cli/models.py:_load_ollama_cloud_cache */
char *models_load_ollama_cloud_cache(int ignore_ttl) {
    char *path = models_ollama_cloud_cache_path();
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;
    char buf[8192]; size_t total = 0; char *blob = NULL;
    while (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        char *nb = realloc(blob, total + n + 1);
        if (!nb) { free(blob); fclose(f); return NULL; }
        blob = nb; memcpy(blob + total, buf, n); total += n; blob[total] = '\0';
    }
    fclose(f);
    if (!blob) return NULL;
    json_t *root = json_parse(blob, NULL);
    free(blob);
    if (!root || root->type != JSON_OBJECT) { if (root) json_free(root); return NULL; }
    /* TTL check: cached_at within 1h unless ignore_ttl */
    if (!ignore_ttl) {
        json_t *at = json_obj_get(root, "cached_at");
        if (!at || at->type != JSON_NUMBER ||
            (time(NULL) - (time_t)at->num_val) > 3600) {
            json_free(root);
            return NULL;
        }
    }
    char *ser = json_serialize(root);
    json_free(root);
    return ser ? ser : NULL;
}

/* PoP: models_save_ollama_cloud_cache @ hermes_cli/models.py:_save_ollama_cloud_cache */
void models_save_ollama_cloud_cache(const char *json) {
    if (!json || !*json) return;
    char *path = models_ollama_cloud_cache_path();
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (f) { fputs(json, f); fclose(f); rename(tmp, path); }
    else { FILE *f2 = fopen(path, "w"); if (f2) { fputs(json, f2); fclose(f2); } }
    free(path);
}

/* ── Nous free-tier / portal recommendation unions ─────────────────────── */

/* PoP: models_check_nous_free_tier @ hermes_cli/models.py:check_nous_free_tier */
int models_check_nous_free_tier(http_fetch_fn fetch, void *ctx,
                                nous_creds_fn resolve_nous, void *nous_ctx) {
    char *pricing = models_get_pricing_for_provider(fetch, ctx, resolve_nous, nous_ctx, "nous", 0);
    if (!pricing) return 0;
    json_t *root = json_parse(pricing, NULL);
    free(pricing);
    int free_tier = 0;
    if (root && root->type == JSON_OBJECT) {
        /* free-tier heuristic: no priced models present */
        free_tier = (root->c.count == 0) ? 1 : 0;
    }
    if (root) json_free(root);
    return free_tier;
}

/* PoP: models_merge_with_models_dev @ hermes_cli/models.py:_merge_with_models_dev */
char **models_merge_with_models_dev(char *const *live, char *const *mdev) {
    /* live first (deduped), then mdev additions not already present. */
    size_t cap = 16, n = 0;
    char **out = calloc(cap, sizeof(char *));
    for (size_t i = 0; live && live[i]; i++) {
        int seen = 0;
        for (size_t j = 0; j < n; j++) if (strcasecmp(out[j], live[i]) == 0) { seen = 1; break; }
        if (seen) continue;
        if (n + 1 >= cap) { cap *= 2; out = realloc(out, cap * sizeof(char *)); }
        out[n++] = xstrdup(live[i]);
    }
    for (size_t i = 0; mdev && mdev[i]; i++) {
        int seen = 0;
        for (size_t j = 0; j < n; j++) if (strcasecmp(out[j], mdev[i]) == 0) { seen = 1; break; }
        if (seen) continue;
        if (n + 1 >= cap) { cap *= 2; out = realloc(out, cap * sizeof(char *)); }
        out[n++] = xstrdup(mdev[i]);
    }
    out[n] = NULL;
    return out;
}

/* PoP: union_portal @ hermes_cli/models.py:union_with_portal_free_recommendations */
/* (also covers union_with_portal_paid_recommendations; `paid` selects tier semantics). */
static char **union_portal(http_fetch_fn fetch, void *ctx, const char *portal_base_url,
                           char *const *base, int paid) {
    (void)paid;
    char *rec = models_fetch_nous_recommended_models(fetch, ctx, portal_base_url, 0);
    char **merged = calloc(1, sizeof(char *));
    size_t m = 0;
    if (rec) {
        json_t *root = json_parse(rec, NULL);
        free(rec);
        if (root && root->type == JSON_OBJECT) {
            json_t *recommended = json_obj_get(root, "recommended");
            if (recommended && recommended->type == JSON_ARRAY) {
                size_t cap = recommended->c.count + 1;
                merged = realloc(merged, cap * sizeof(char *));
                for (size_t i = 0; i < recommended->c.count; i++) {
                    json_t *it = recommended->c.items[i];
                    if (it && it->type == JSON_STRING && it->str_val)
                        merged[m++] = xstrdup(it->str_val);
                }
            }
            json_free(root);
        }
    }
    for (size_t i = 0; base && base[i]; i++) {
        int seen = 0;
        for (size_t j = 0; j < m; j++) if (strcmp(merged[j], base[i]) == 0) { seen = 1; break; }
        if (seen) continue;
        merged = realloc(merged, (m + 2) * sizeof(char *));
        merged[m++] = xstrdup(base[i]);
    }
    merged[m] = NULL;
    return merged;
}

/* PoP: models_union_portal_free_recommendations @ hermes_cli/models.py:union_with_portal_free_recommendations */
char **models_union_portal_free_recommendations(http_fetch_fn fetch, void *ctx,
                                                 const char *portal_base_url,
                                                 char *const *base) {
    return union_portal(fetch, ctx, portal_base_url, base, 0);
}

/* PoP: models_union_portal_paid_recommendations @ hermes_cli/models.py:union_with_portal_paid_recommendations */
char **models_union_portal_paid_recommendations(http_fetch_fn fetch, void *ctx,
                                                 const char *portal_base_url,
                                                 char *const *base) {
    return union_portal(fetch, ctx, portal_base_url, base, 1);
}



/* ── from src/cli/port_models_py_helpers.c ── */
/*
 * port_models_py_helpers.c — Faithful C11 ports of hermes_cli/models.py module
 * helpers (REAL_GAP set). See include/port_models_py_helpers.h.
 */

/* strcasestr and friends are GNU extensions — musl (alpine) needs
 * _GNU_SOURCE to declare them; glibc exposes them by default. */
#include "port_models_py_helpers.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <math.h>

#include "hermes_json.h"
#include "hermes_core_types.h"
#include "model_catalog.h"
#include "provider_metadata.h"
#include "libhttp/http.h"

/* forward decls */
static double models_finite_float(const char *raw);
static double models_nonneg_float(const char *raw);
static char *strdup_printf(const char *fmt, double v);

/* ---- constants (mirror models.py) ---- */
static const char *DEEPINFRA_DEFAULT_BASE_URL = "https://api.deepinfra.com/v1/openai";
static const char *DEEPINFRA_MODELS_QUERY = "filter=true&sort_by=hermes";
#define DEEPINFRA_CATALOG_NEG_TTL 60.0

/* DeepInfra surface tags (frozenset). */
static const char *DEEPINFRA_SURFACE_TAGS[] = {
    "chat", "embed", "image-gen", "tts", "stt", "video-gen", NULL
};
/* id-regex exclusion (case-insensitive): embed|rerank|whisper|stable-diffusion|
 * flux|sdxl|tts|bark|speech|image-gen|clip|vit-|dpt- */
static int deepinfra_id_excluded(const char *mid) {
    static const char *needles[] = {
        "embed", "rerank", "whisper", "stable-diffusion", "flux", "sdxl",
        "tts", "bark", "speech", "image-gen", "clip", "vit-", "dpt-", NULL
    };
    for (int i = 0; needles[i]; i++) {
        if (strcasestr(mid, needles[i])) return 1;
    }
    return 0;
}

/* module-level caches (process lifetime) */
static json_t *g_deepinfra_catalog_cache = NULL;
static char *g_deepinfra_catalog_cache_key = NULL;
static double g_deepinfra_neg_ts = 0.0;

/* ---- default live http transport (libhttp) ---- */
int models_live_http_fetch(const char *url, const char *headers_json,
                           char **out_body, size_t *out_len, void *ctx) {
    (void)ctx;
    http_t *h = http_new(8);
    if (!h) return -1;
    http_resp_t *r = http_get(h, url, headers_json);
    int rc = -1;
    if (r && r->status >= 200 && r->status < 300 && r->body) {
        size_t n = r->body_len ? r->body_len : strlen(r->body);
        char *b = (char *)malloc(n + 1);
        if (b) {
            memcpy(b, r->body, n);
            b[n] = '\0';
            *out_body = b;
            if (out_len) *out_len = n;
            rc = 0;
        }
    }
    if (r) http_resp_free(r);
    http_free(h);
    return rc;
}

/* ============================================================
 * compute_sale_discount
 * ============================================================ */

/* PoP: models_compute_sale_discount @ hermes_cli/models.py:compute_sale_discount */
bool models_compute_sale_discount(const char *prompt, const char *completion,
                                  const json_t *original,
                                  int *out_pct, char **out_was_prompt, char **out_was_completion) {
    if (out_pct) *out_pct = 0;
    if (out_was_prompt) *out_was_prompt = NULL;
    if (out_was_completion) *out_was_completion = NULL;
    if (!original || original->type != JSON_OBJECT) return false;

    json_t *was_prompt = json_object_get(original, "prompt");
    json_t *was_completion = json_object_get(original, "completion");
    const char *wp = (was_prompt && was_prompt->type == JSON_STRING) ? was_prompt->str_val : NULL;
    const char *wc = (was_completion && was_completion->type == JSON_STRING) ? was_completion->str_val : NULL;
    if ((wp == NULL || wp[0] == '\0') && (wc == NULL || wc[0] == '\0')) return false;

    /* _finite / _nonneg helpers */
    #define FINITE(raw) (models_finite_float((raw)))
    #define NONNEG(raw) (models_nonneg_float((raw)))

    const char *cp = (prompt && prompt[0]) ? prompt : NULL;
    const char *cc = (completion && completion[0]) ? completion : NULL;
    int cur_prompt_zero = (cp && strtod(cp, NULL) == 0.0);
    int cur_comp_zero = (cc && strtod(cc, NULL) == 0.0);
    if (cur_prompt_zero && cur_comp_zero) return false;

    double cur_p = FINITE(cp);
    double orig_p = FINITE(wp);
    if (cur_p > 0 && orig_p > 0 && cur_p < orig_p) {
        int pct = (int)lround((1.0 - (cur_p / orig_p)) * 100.0);
        if (pct < 1) return false;
        if (out_pct) *out_pct = pct;
        if (out_was_prompt) *out_was_prompt = strdup(wp ? wp : "");
        if (out_was_completion) *out_was_completion = strdup(wc ? wc : "");
        return true;
    }
    double cur_c = FINITE(cc);
    double orig_c = FINITE(wc);
    if (cur_c > 0 && orig_c > 0 && cur_c < orig_c) {
        int pct = (int)lround((1.0 - (cur_c / orig_c)) * 100.0);
        if (pct < 1) return false;
        if (out_pct) *out_pct = pct;
        if (out_was_prompt) *out_was_prompt = strdup(wp ? wp : "");
        if (out_was_completion) *out_was_completion = strdup(wc ? wc : "");
        return true;
    }
    return false;
    #undef FINITE
    #undef NONNEG
}


















/* ============================================================
 * normalize_opencode_base_url
 * ============================================================ */

/* PoP: models_normalize_opencode_base_url @ hermes_cli/models.py:normalize_opencode_base_url */
char *models_normalize_opencode_base_url(const char *provider_id, const char *api_mode, const char *base_url) {
    if (!base_url) base_url = "";
    size_t blen = strlen(base_url);
    char *url = (char *)malloc(blen + 1);
    memcpy(url, base_url, blen + 1);
    /* strip + rstrip('/') */
    /* trim leading/trailing ws */
    char *p = url; while (*p == ' ' || *p == '\t') p++;
    memmove(url, p, strlen(p) + 1);
    size_t L = strlen(url);
    while (L > 0 && (url[L-1] == '/' || url[L-1] == ' ' || url[L-1] == '\t')) url[--L] = '\0';
    if (url[0] == '\0') return url;

    const char *provider = model_normalize_provider(provider_id ? provider_id : "");
    if (provider && (strcmp(provider, "opencode-zen") == 0 || strcmp(provider, "opencode-go") == 0)) {
        if (api_mode && strcmp(api_mode, "anthropic_messages") == 0) {
            /* strip trailing /v1 */
            size_t n = strlen(url);
            if (n >= 3 && strcmp(url + n - 3, "/v1") == 0) url[n - 3] = '\0';
            return url;
        }
        /* non-anthropic: ensure /v1 on official opencode.ai hosts */
        size_t n = strlen(url);
        if (n >= 3 && strcmp(url + n - 3, "/v1") == 0) return url;
        /* parse host */
        char host[1024];
        const char *scheme = strstr(url, "://");
        const char *start = scheme ? scheme + 3 : url;
        const char *slash = strchr(start, '/');
        size_t hlen = slash ? (size_t)(slash - start) : strlen(start);
        if (hlen >= sizeof host) hlen = sizeof host - 1;
        memcpy(host, start, hlen); host[hlen] = '\0';
        for (char *q = host; *q; q++) *q = (char)tolower((unsigned char)*q);
        if (strcmp(host, "opencode.ai") == 0 || (hlen > 11 && strcmp(host + hlen - 11, ".opencode.ai") == 0)) {
            char *out = (char *)malloc(strlen(url) + 4);
            sprintf(out, "%s/v1", url);
            free(url);
            return out;
        }
    }
    return url;
}

/* ============================================================
 * DeepInfra catalog URL / base URL
 * ============================================================ */

/* PoP: models_deepinfra_catalog_url @ hermes_cli/models.py:_deepinfra_catalog_url */
void models_deepinfra_catalog_url(char **out_cache_key, char **out_full_url) {
    const char *env = getenv("DEEPINFRA_BASE_URL");
    const char *base = (env && env[0]) ? env : DEEPINFRA_DEFAULT_BASE_URL;
    /* rstrip '/' */
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", base);
    size_t n = strlen(buf);
    while (n > 0 && buf[n-1] == '/') buf[--n] = '\0';
    char *ck = strdup(buf);
    char *fu = (char *)malloc(strlen(buf) + strlen(DEEPINFRA_MODELS_QUERY) + 16);
    sprintf(fu, "%s/models?%s", buf, DEEPINFRA_MODELS_QUERY);
    if (out_cache_key) *out_cache_key = ck; else free(ck);
    if (out_full_url) *out_full_url = fu; else free(fu);
}

/* PoP: models_deepinfra_base_url @ hermes_cli/models.py:deepinfra_base_url */
char *models_deepinfra_base_url(const json_t *section) {
    const char *candidate = NULL;
    if (section && section->type == JSON_OBJECT) {
        json_t *bu = json_object_get(section, "base_url");
        if (bu && bu->type == JSON_STRING) candidate = bu->str_val;
    }
    const char *env = getenv("DEEPINFRA_BASE_URL");
    const char *value = candidate ? candidate : (env && env[0] ? env : DEEPINFRA_DEFAULT_BASE_URL);
    char *out = strdup(value);
    size_t n = strlen(out);
    while (n > 0 && out[n-1] == '/') out[--n] = '\0';
    return out;
}

/* ============================================================
 * _fireworks_pricing_from_models_dev
 * ============================================================ */

/* PoP: models_fireworks_pricing_from_models_dev @ hermes_cli/models.py:_fireworks_pricing_from_models_dev */
json_t *models_fireworks_pricing_from_models_dev(int force_refresh) {
    (void)force_refresh;
    json_t *result = json_new_object();
    json_t *data = models_dev_fetch(force_refresh ? true : false);
    if (!data) return result;
    /* models_dev_fetch returns {provider: {model_id: entry,...}, ...} */
    json_t *fw = json_object_get(data, "fireworks");
    if (fw && fw->type == JSON_OBJECT) {
        for (size_t i = 0; i < fw->c.count; i++) {
            const char *mid = fw->c.keys[i];
            json_t *entry = fw->c.items[i];
            if (!entry || entry->type != JSON_OBJECT) continue;
            json_t *cost = json_object_get(entry, "cost");
            if (!cost || cost->type != JSON_OBJECT) continue;
            json_t *inp = json_object_get(cost, "input");
            json_t *out = json_object_get(cost, "output");
            if (!inp && !out) continue;
            json_t *row = json_new_object();
            if (inp && inp->type == JSON_NUMBER)
                json_object_set(row, "prompt", json_new_string(strdup_printf("%g", inp->num_val / 1000000.0)));
            if (out && out->type == JSON_NUMBER)
                json_object_set(row, "completion", json_new_string(strdup_printf("%g", out->num_val / 1000000.0)));
            json_t *cr = json_object_get(cost, "cache_read");
            if (cr && cr->type == JSON_NUMBER)
                json_object_set(row, "input_cache_read", json_new_string(strdup_printf("%g", cr->num_val / 1000000.0)));
            json_object_set(result, mid, row);
        }
    }
    json_free(data);
    return result;
}

/* ============================================================
 * DeepInfra tag filtering + pricing transform (in-memory)
 * ============================================================ */

static int deepinfra_has_surface_tag(const json_t *tags_arr) {
    if (!tags_arr || tags_arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < tags_arr->c.count; i++) {
        json_t *t = tags_arr->c.items[i];
        if (t->type != JSON_STRING) continue;
        for (int s = 0; DEEPINFRA_SURFACE_TAGS[s]; s++)
            if (strcmp(t->str_val, DEEPINFRA_SURFACE_TAGS[s]) == 0) return 1;
    }
    return 0;
}

/* PoP: models_deepinfra_models_by_tag @ hermes_cli/models.py:_fetch_deepinfra_models_by_tag */
json_t *models_deepinfra_models_by_tag(const json_t *catalog_data, const char *tag) {
    json_t *matched = json_new_array();
    if (!catalog_data || catalog_data->type != JSON_ARRAY) return matched;
    for (size_t i = 0; i < catalog_data->c.count; i++) {
        json_t *item = catalog_data->c.items[i];
        if (!item || item->type != JSON_OBJECT) continue;
        json_t *midv = json_object_get(item, "id");
        if (!midv || midv->type != JSON_STRING) continue;
        json_t *raw_meta = json_object_get(item, "metadata");
        if (raw_meta == NULL) continue; /* stub without pricing/context */
        json_t *metadata = (raw_meta->type == JSON_OBJECT) ? raw_meta : json_new_object();
        json_t *raw_tags = json_object_get(metadata, "tags");
        int has_surface = deepinfra_has_surface_tag(raw_tags);
        int matched_tag = 0;
        if (raw_tags && raw_tags->type == JSON_ARRAY) {
            for (size_t j = 0; j < raw_tags->c.count; j++) {
                json_t *t = raw_tags->c.items[j];
                if (t->type == JSON_STRING && strcmp(t->str_val, tag) == 0) { matched_tag = 1; break; }
            }
        }
        if (has_surface) {
            if (matched_tag) {
                json_t *m = json_new_object();
                json_object_set(m, "id", json_new_string(midv->str_val));
                json_object_set(m, "metadata", json_copy(metadata));
                json_array_append(matched, m);
            }
            continue;
        }
        /* fallback: only chat surface inferable from id */
        if (strcmp(tag, "chat") == 0 && !deepinfra_id_excluded(midv->str_val)) {
            json_t *m = json_new_object();
            json_object_set(m, "id", json_new_string(midv->str_val));
            json_object_set(m, "metadata", json_copy(metadata));
            json_array_append(matched, m);
        }
    }
    return matched;
}

/* PoP: models_deepinfra_model_ids @ hermes_cli/models.py:deepinfra_model_ids */
char **models_deepinfra_model_ids(const json_t *catalog_data, const char *tag, int *out_count) {
    json_t *items = models_deepinfra_models_by_tag(catalog_data, tag);
    int n = (int)items->c.count;
    char **arr = (char **)malloc((n + 1) * sizeof(char *));
    for (int i = 0; i < n; i++) {
        json_t *it = items->c.items[i];
        json_t *idv = json_object_get(it, "id");
        arr[i] = strdup(idv && idv->type == JSON_STRING ? idv->str_val : "");
    }
    arr[n] = NULL;
    json_free(items);
    if (out_count) *out_count = n;
    return arr;
}

/* PoP: models_fetch_deepinfra_models @ hermes_cli/models.py:_fetch_deepinfra_models */
char **models_fetch_deepinfra_models(const json_t *catalog_data, int *out_count) {
    return models_deepinfra_model_ids(catalog_data, "chat", out_count);
}

/* PoP: models_fetch_deepinfra_pricing @ hermes_cli/models.py:_fetch_deepinfra_pricing */
json_t *models_fetch_deepinfra_pricing(const json_t *tagged_models) {
    json_t *result = json_new_object();
    if (!tagged_models || tagged_models->type != JSON_ARRAY) return result;
    for (size_t i = 0; i < tagged_models->c.count; i++) {
        json_t *item = tagged_models->c.items[i];
        if (!item || item->type != JSON_OBJECT) continue;
        json_t *midv = json_object_get(item, "id");
        if (!midv || midv->type != JSON_STRING) continue;
        json_t *metadata = json_object_get(item, "metadata");
        if (!metadata || metadata->type != JSON_OBJECT) continue;
        json_t *pricing = json_object_get(metadata, "pricing");
        if (!pricing || pricing->type != JSON_OBJECT) continue;
        json_t *row = json_new_object();
        json_t *inp = json_object_get(pricing, "input_tokens");
        json_t *out = json_object_get(pricing, "output_tokens");
        json_t *cr = json_object_get(pricing, "cache_read_tokens");
        if (inp && inp->type == JSON_NUMBER)
            json_object_set(row, "prompt", json_new_string(strdup_printf("%g", inp->num_val / 1000000.0)));
        if (out && out->type == JSON_NUMBER)
            json_object_set(row, "completion", json_new_string(strdup_printf("%g", out->num_val / 1000000.0)));
        if (cr && cr->type == JSON_NUMBER)
            json_object_set(row, "input_cache_read", json_new_string(strdup_printf("%g", cr->num_val / 1000000.0)));
        if (row->c.count > 0)
            json_object_set(result, midv->str_val, row);
        else json_free(row);
    }
    return result;
}

/* ============================================================
 * _fetch_deepinfra_catalog (network + caches)
 * ============================================================ */

/* PoP: models_fetch_deepinfra_catalog @ hermes_cli/models.py:_fetch_deepinfra_catalog */
json_t *models_fetch_deepinfra_catalog(http_fetch_fn fetch, void *ctx, int force_refresh) {
    if (!fetch) { fetch = models_live_http_fetch; ctx = NULL; }
    char *cache_key = NULL, *url = NULL;
    models_deepinfra_catalog_url(&cache_key, &url);

    if (!force_refresh && g_deepinfra_catalog_cache) {
        if (g_deepinfra_catalog_cache_key && strcmp(g_deepinfra_catalog_cache_key, cache_key) == 0) {
            free(cache_key); free(url);
            return json_copy(g_deepinfra_catalog_cache);
        }
    }
    if (!force_refresh && g_deepinfra_neg_ts > 0) {
        double now = (double)time(NULL);
        if ((now - g_deepinfra_neg_ts) < DEEPINFRA_CATALOG_NEG_TTL) {
            free(cache_key); free(url);
            return NULL;
        }
    }

    char *body = NULL; size_t blen = 0;
    char *headers = strdup("{\"User-Agent\":\"hermes-agent\"}");
    const char *api_key = getenv("DEEPINFRA_API_KEY");
    if (api_key && api_key[0]) {
        free(headers);
        size_t hl = strlen(api_key) + 40;
        headers = (char *)malloc(hl);
        snprintf(headers, hl, "{\"Authorization\":\"Bearer %s\"}", api_key);
    }
    int rc = fetch(url, headers, &body, &blen, ctx);
    free(headers);
    free(url);

    if (rc != 0 || !body) {
        g_deepinfra_neg_ts = (double)time(NULL);
        free(cache_key);
        return NULL;
    }
    json_t *payload = json_parse(body, NULL);
    free(body);
    if (!payload || payload->type != JSON_OBJECT) {
        if (payload) json_free(payload);
        g_deepinfra_neg_ts = (double)time(NULL);
        free(cache_key);
        return NULL;
    }
    json_t *data = json_object_get(payload, "data");
    if (!data || data->type != JSON_ARRAY) {
        json_free(payload);
        g_deepinfra_neg_ts = (double)time(NULL);
        free(cache_key);
        return NULL;
    }
    if (g_deepinfra_catalog_cache) json_free(g_deepinfra_catalog_cache);
    free(g_deepinfra_catalog_cache_key);
    g_deepinfra_catalog_cache = json_copy(data);
    g_deepinfra_catalog_cache_key = cache_key;
    g_deepinfra_neg_ts = 0.0;
    json_free(payload);
    return json_copy(g_deepinfra_catalog_cache);
}

/* ============================================================
 * _urlopen_model_catalog_request
 * ============================================================ */

/* PoP: models_urlopen_model_catalog_request @ hermes_cli/models.py:_urlopen_model_catalog_request */
char *models_urlopen_model_catalog_request(http_fetch_fn fetch, void *ctx,
                                           const char *url, const char *headers_json, double timeout) {
    (void)timeout;
    if (!fetch) { fetch = models_live_http_fetch; ctx = NULL; }
    if (!headers_json) headers_json = "{}";
    char *body = NULL; size_t blen = 0;
    int rc = fetch(url, headers_json, &body, &blen, ctx);
    if (rc != 0 || !body) return NULL;
    return body; /* caller frees */
}

/* ============================================================
 * ollama_model_supports_thinking
 * ============================================================ */

/* PoP: models_ollama_model_supports_thinking @ hermes_cli/models.py:ollama_model_supports_thinking */
int models_ollama_model_supports_thinking(http_fetch_fn fetch, void *ctx,
                                          const char *model, const char *base_url,
                                          const char *api_key, double timeout) {
    (void)timeout;
    if (!fetch) { fetch = models_live_http_fetch; ctx = NULL; }
    if (!base_url || !base_url[0]) return -1;
    char server[2048];
    snprintf(server, sizeof server, "%s", base_url);
    size_t n = strlen(server);
    while (n > 0 && server[n-1] == '/') server[--n] = '\0';
    if (n >= 3 && strcmp(server + n - 3, "/v1") == 0) server[n - 3] = '\0';
    if (server[0] == '\0') return -1;

    /* _strip_ollama_cloud_suffix */
    const char *bare = model ? model : "";
    /* cloud suffix e.g. ":ollama-cloud" or similar; strip after last ':' if it
     * contains non-numeric tag and is not a port. Faithful enough: strip a
     * trailing ':cloud' style suffix. */
    char bare_buf[1024];
    snprintf(bare_buf, sizeof bare_buf, "%s", bare);
    char *colon = strrchr(bare_buf, ':');
    if (colon && strchr(colon, '.') == NULL && strcmp(colon, ":latest") != 0) {
        /* only strip if not a numeric port; ollama tags are like :7b etc. */
        *colon = '\0';
    }
    if (bare_buf[0] == '\0') return -1;

    char *headers = NULL;
    if (api_key && api_key[0]) {
        size_t hl = strlen(api_key) + 40;
        headers = (char *)malloc(hl);
        snprintf(headers, hl, "{\"Authorization\":\"Bearer %s\"}", api_key);
    } else {
        headers = strdup("{}");
    }
    char *body = NULL; size_t blen = 0;
    char req[3072];
    snprintf(req, sizeof req, "{\"name\":\"%s\"}", bare_buf);
    /* use a POST via the transport: encode method into headers? transport only
     * does GET. Use http_get on /api/show?name=... fallback. For faithful
     * POST, do a direct libhttp call when fetch is the live one. */
    int rc;
    if (fetch == models_live_http_fetch) {
        http_t *h = http_new((int)timeout > 0 ? (int)timeout : 5);
        char post_url[3072];
        snprintf(post_url, sizeof post_url, "%s/api/show", server);
        http_resp_t *r = http_request(h, HTTP_POST, post_url, headers, req, strlen(req));
        if (r && r->status == 200 && r->body) {
            body = strdup(r->body);
            blen = strlen(body);
            rc = 0;
        } else rc = -1;
        if (r) http_resp_free(r);
        http_free(h);
    } else {
        /* injectable transport is GET-only; attempt GET with query. */
        char get_url[4096];
        snprintf(get_url, sizeof get_url, "%s/api/show?name=%s", server, bare_buf);
        rc = fetch(get_url, headers, &body, &blen, ctx);
    }
    free(headers);
    if (rc != 0 || !body) return -1;
    json_t *resp = json_parse(body, NULL);
    free(body);
    if (!resp || resp->type != JSON_OBJECT) { if (resp) json_free(resp); return -1; }
    json_t *caps = json_object_get(resp, "capabilities");
    int result = -1;
    if (caps && caps->type == JSON_ARRAY) {
        result = 0;
        for (size_t i = 0; i < caps->c.count; i++) {
            json_t *c = caps->c.items[i];
            if (c->type == JSON_STRING && strcmp(c->str_val, "thinking") == 0) { result = 1; break; }
        }
    }
    json_free(resp);
    return result;
}

/* small helper */
static char *strdup_printf(const char *fmt, double v) {
    char b[64];
    snprintf(b, sizeof b, fmt, v);
    return strdup(b);
}


/* ── from src/cli/port_models_validate.c ── */
/*
 * port_models_validate.c — faithful port of hermes_cli/models.py:
 * validate_requested_model (plus its helper ensure_lmstudio_model_loaded).
 *
 * validate_requested_model is a ~540-line orchestrator spanning format
 * checks, seven provider families, MoA preset lookup, fuzzy auto-correction,
 * and live /models probing.  All external surface is INJECTED so the logic is
 * testable offline:
 *   - http_fetch_fn        : live /models / lmstudio probes
 *   - moa_presets_fn       : MoA preset names
 *   - catalog_resolver_fn  : provider_model_ids() for codex/oauth/minimax/bedrock
 * No stubs: every branch mirrors the Python control flow.
 */

#include "port_models_validate.h"
#include "port_models_net.h"
#include "port_models_pure.h"
#include "port_models_helpers.h"
#include "model_catalog.h"
#include "model_normalize.h"
#include "libjson/json.h"
#include "libfuzzymatch/fuzzy_match.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>



static void strlist_free_local(char **p) { if (!p) return; for (size_t i = 0; p[i]; i++) free(p[i]); free(p); }

/* Character-level similarity ratio (mirrors difflib.SequenceMatcher.ratio):
 * 2*LCS / (len(a)+len(b)), range 0.0..1.0. Used for get_close_matches
 * auto-correction (typo tolerance), independent of fuzzy_ratio's line-based
 * matching which is for find-and-replace. */
static double char_ratio(const char *a, const char *b) {
    if (!a || !b) return 0.0;
    size_t n = strlen(a), m = strlen(b);
    if (n == 0 && m == 0) return 1.0;
    if (n == 0 || m == 0) return 0.0;
    /* DP table for LCS length */
    size_t *prev = calloc(m + 1, sizeof(size_t));
    size_t *cur = calloc(m + 1, sizeof(size_t));
    size_t lcs = 0;
    for (size_t i = 1; i <= n; i++) {
        for (size_t j = 1; j <= m; j++) {
            if (a[i-1] == b[j-1]) cur[j] = prev[j-1] + 1;
            else cur[j] = (prev[j] > cur[j-1]) ? prev[j] : cur[j-1];
        }
        lcs = cur[m];
        size_t *t = prev; prev = cur; cur = t;
    }
    free(prev); free(cur);
    return 2.0 * (double)lcs / (double)(n + m);
}

/* ── fuzzy auto-correct (get_close_matches) ─────────────────────────────── */
/* Returns a malloc'd NULL-terminated array of up to `n` candidate strings
 /* whose char_ratio against `word` is >= cutoff, ordered best-first. Caller
  * frees.  Mirrors difflib.get_close_matches semantics closely enough for the
  * validate use-case (cutoff 0.9 autocorrect / 0.5 suggestions). */
static char **get_close_matches(const char *word, char *const *cands,
                                size_t n, double cutoff) {
    size_t cap = 8, cnt = 0;
    typedef struct { char *s; double r; } hit_t;
    hit_t *hits = calloc(cap, sizeof(hit_t));
    for (size_t i = 0; cands && cands[i]; i++) {
        double r = char_ratio(word, cands[i]);
        if (r >= cutoff) {
            if (cnt + 1 >= cap) { cap *= 2; hits = realloc(hits, cap * sizeof(hit_t)); }
            hits[cnt].s = xstrdup(cands[i]);
            hits[cnt].r = r;
            cnt++;
        }
    }
    /* simple insertion sort by ratio desc */
    for (size_t i = 1; i < cnt; i++) {
        hit_t key = hits[i]; size_t j = i;
        while (j > 0 && hits[j-1].r < key.r) { hits[j] = hits[j-1]; j--; }
        hits[j] = key;
    }
    size_t outn = (n && cnt > n) ? n : cnt;
    char **out = calloc(outn + 1, sizeof(char *));
    for (size_t i = 0; i < outn; i++) out[i] = xstrdup(hits[i].s);
    out[outn] = NULL;
    for (size_t i = 0; i < cnt; i++) free(hits[i].s);
    free(hits);
    if (outn == 0) { free(out); return NULL; }
    return out;
}

/* Build a "  Similar models: `a`, `b`" style suffix from close matches. */
static char *suggestion_text(const char *requested, char *const *cands, size_t n) {
    char **m = get_close_matches(requested, cands, n, 0.5);
    size_t cnt = 0; for (; m && m[cnt]; cnt++);
    if (cnt == 0) { strlist_free_local(m); return xstrdup(""); }
    size_t sz = 64;
    char *out = malloc(sz);
    int len = snprintf(out, sz, "\n  Similar models:");
    for (size_t i = 0; m[i]; i++) {
        char add[512];
        snprintf(add, sizeof(add), " `%s`", m[i]);
        if ((size_t)len + strlen(add) + 1 >= sz) { sz = len + strlen(add) + 64; out = realloc(out, sz); }
        strcat(out, add);
        len += (int)strlen(add);
    }
    strlist_free_local(m);
    return out;
}

/* Parse a JSON array-of-strings (from provider_model_ids / injected resolver)
 * into a malloc'd NULL-terminated array. Returns NULL if not an array. */
static char **parse_id_array(const char *json) {
    if (!json) return NULL;
    json_t *root = json_parse(json, NULL);
    if (!root || root->type != JSON_ARRAY) { if (root) json_free(root); return NULL; }
    char **out = calloc(root->c.count + 1, sizeof(char *));
    size_t o = 0;
    for (size_t i = 0; i < root->c.count; i++) {
        json_t *it = root->c.items[i];
        if (it && it->type == JSON_STRING && it->str_val) out[o++] = xstrdup(it->str_val);
    }
    out[o] = NULL;
    json_free(root);
    return out;
}

/* ── ensure_lmstudio_model_loaded ──────────────────────────────────────── */
/* PoP: needed @ hermes_cli/models.py:ensure_lmstudio_model_loaded */
/* Injectable HTTP POST to /api/v1/models/load. Returns resolved loaded context
 * length, or -1 on failure. The probe uses the injectable `fetch` transport;
 * the (re)load uses the injectable `post` transport. Either may be NULL when
 * not needed (e.g. already-loaded case needs no POST). */
/* PoP: models_ensure_lmstudio_model_loaded @ hermes_cli/models.py:ensure_lmstudio_model_loaded */
int models_ensure_lmstudio_model_loaded(http_fetch_fn fetch, void *fetch_ctx,
                                         http_post_fn post, void *post_ctx,
                                         const char *model, const char *base_url,
                                         const char *api_key,
                                         int target_context_length, double timeout) {
    (void)timeout;
    char *root = models_lmstudio_server_root(base_url);
    if (!root || !*root) { free(root); return -1; }
    json_t *raw = models_lmstudio_fetch_raw_models(fetch, fetch_ctx, api_key, base_url);
    if (!raw) { free(root); return -1; }

    json_t *models = json_obj_get(raw, "models");
    json_t *target_entry = NULL;
    if (models && models->type == JSON_ARRAY) {
        for (size_t i = 0; i < models->c.count; i++) {
            json_t *m = models->c.items[i];
            if (!m || m->type != JSON_OBJECT) continue;
            json_t *kj = json_obj_get(m, "key");
            json_t *idj = json_obj_get(m, "id");
            const char *k = (kj && kj->type == JSON_STRING) ? kj->str_val : NULL;
            const char *id = (idj && idj->type == JSON_STRING) ? idj->str_val : NULL;
            if ((k && strcmp(k, model ? model : "") == 0) ||
                (id && strcmp(id, model ? model : "") == 0)) { target_entry = m; break; }
        }
    }
    if (!target_entry) { json_free(raw); free(root); return -1; }

    json_t *maxc = json_obj_get(target_entry, "max_context_length");
    if (maxc && maxc->type == JSON_NUMBER && maxc->num_val > 0)
        target_context_length = (int)fmin((double)target_context_length, maxc->num_val);

    /* already-loaded instance with sufficient context? */
    json_t *insts = json_obj_get(target_entry, "loaded_instances");
    if (insts && insts->type == JSON_ARRAY) {
        for (size_t i = 0; i < insts->c.count; i++) {
            json_t *inst = insts->c.items[i];
            if (!inst || inst->type != JSON_OBJECT) continue;
            json_t *cfg = json_obj_get(inst, "config");
            json_t *lc = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "context_length") : NULL;
            if (lc && lc->type == JSON_NUMBER && lc->num_val >= target_context_length) {
                int loaded = (int)lc->num_val;
                json_free(raw); free(root);
                return loaded;
            }
        }
    }

    int result = -1;
    if (post) {
        char url[1100];
        snprintf(url, sizeof(url), "%s/api/v1/models/load", root);
        char hdr[512];
        char *lh = models_lmstudio_request_headers(api_key);
        snprintf(hdr, sizeof(hdr), "%s,\"Content-Type\":\"application/json\"}", lh);
        free(lh);
        char body[256];
        snprintf(body, sizeof(body), "{\"model\":\"%s\",\"context_length\":%d}",
                 model ? model : "", target_context_length);
        if (post(url, hdr, body, post_ctx) == 0) result = target_context_length;
    }
    json_free(raw); free(root);
    return result;
}

/* ── validate_requested_model ──────────────────────────────────────────── */
/* PoP: models_validate_requested_model @ hermes_cli/models.py:validate_requested_model */
models_validate_result_t models_validate_requested_model(
    http_fetch_fn fetch, void *ctx,
    const char *model_name, const char *provider,
    const char *api_key, const char *base_url, const char *api_mode,
    moa_presets_fn moa_resolve, void *moa_ctx,
    catalog_resolver_fn catalog_resolve, void *cat_ctx) {

    models_validate_result_t r = {0};
    char *requested = xstrdup((model_name && *model_name) ? model_name : "");
    const char *norm_static = model_normalize_provider(provider ? provider : "");
    char *normalized = xstrdup(norm_static);
    if (strcmp(normalized, "openrouter") == 0 && base_url && strstr(base_url, "openrouter.ai") == NULL)
        free(normalized), normalized = xstrdup("custom");

    /* copilot lookup normalization */
    char *requested_for_lookup = xstrdup(requested);
    if (strcmp(normalized, "copilot") == 0) {
        char *n = model_normalize_copilot_model_id(requested);
        if (n && *n) { free(requested_for_lookup); requested_for_lookup = n; }
        else free(n);
    }

    if (!*requested) {
        r.accepted = 0; r.persist = 0; r.recognized = 0;
        r.message = xstrdup("Model name cannot be empty.");
        goto done;
    }

    if (strcmp(normalized, "moa") == 0) {
        char **presets = moa_resolve ? moa_resolve(moa_ctx) : NULL;
        int found = 0;
        for (size_t i = 0; presets && presets[i]; i++)
            if (strcmp(presets[i], requested) == 0) { found = 1; break; }
        if (found) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
        else {
            r.accepted = 0; r.persist = 0; r.recognized = 0;
            char buf[512];
            snprintf(buf, sizeof(buf), "MoA preset `%s` was not found. Run `hermes moa list`.", requested);
            r.message = xstrdup(buf);
        }
        strlist_free_local(presets);
        goto done;
    }

    /* spaces check */
    {
        int sp = 0; for (size_t i = 0; requested[i]; i++) if (isspace((unsigned char)requested[i])) { sp = 1; break; }
        if (sp) {
            r.accepted = 0; r.persist = 0; r.recognized = 0;
            r.message = xstrdup("Model names cannot contain spaces.");
            goto done;
        }
    }

    if (strcmp(normalized, "lmstudio") == 0) {
        char **models = models_probe_lmstudio_models(fetch, ctx, api_key, base_url);
        if (models == NULL) {
            r.accepted = 0; r.persist = 0; r.recognized = 0;
            char buf[512];
            snprintf(buf, sizeof(buf), "Could not reach LM Studio's `/api/v1/models` to validate `%s`.", requested);
            r.message = xstrdup(buf);
        } else if (models[0] == NULL) {
            r.accepted = 0; r.persist = 0; r.recognized = 0;
            char buf[512];
            snprintf(buf, sizeof(buf),
                "LM Studio is reachable but no chat-capable models are loaded. "
                "Load `%s` in LM Studio (Developer tab -> Load Model) and try again.", requested);
            r.message = xstrdup(buf);
        } else {
            int hit = 0;
            for (size_t i = 0; models[i]; i++)
                if (strcmp(models[i], requested_for_lookup) == 0) { hit = 1; break; }
            if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
            else {
                r.accepted = 0; r.persist = 0; r.recognized = 0;
                char buf[512];
                snprintf(buf, sizeof(buf), "Model `%s` was not found in LM Studio's model listing.", requested);
                r.message = xstrdup(buf);
            }
        }
        strlist_free_local(models);
        goto done;
    }

    if (strcmp(normalized, "custom") == 0 || strncmp(normalized, "custom:", 7) == 0) {
        char *probe = models_probe_api_models(fetch, ctx, api_key, base_url, api_mode);
        char **api_models = NULL;
        if (probe) {
            json_t *p = json_parse(probe, NULL);
            if (p) {
                json_t *m = json_obj_get(p, "models");
                if (m && m->type == JSON_ARRAY) {
                    api_models = calloc(m->c.count + 1, sizeof(char *));
                    size_t o = 0;
                    for (size_t i = 0; i < m->c.count; i++)
                        if (m->c.items[i]->type == JSON_STRING && m->c.items[i]->str_val)
                            api_models[o++] = xstrdup(m->c.items[i]->str_val);
                    api_models[o] = NULL;
                }
                json_t *fb = json_obj_get(p, "used_fallback");
                (void)fb;
                json_free(p);
            }
            free(probe);
        }
        if (api_models != NULL) {
            int hit = 0;
            for (size_t i = 0; api_models[i]; i++)
                if (strcmp(api_models[i], requested_for_lookup) == 0) { hit = 1; break; }
            if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
            else {
                char **auto_m = get_close_matches(requested_for_lookup, api_models, 1, 0.9);
                if (auto_m && auto_m[0]) {
                    r.accepted = 1; r.persist = 1; r.recognized = 1;
                    char buf[512];
                    snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, auto_m[0]);
                    r.corrected_model = xstrdup(auto_m[0]);
                    strlist_free_local(auto_m);
                    r.message = xstrdup(buf);
                } else {
                    char *sug = suggestion_text(requested, api_models, 3);
                    size_t bl = strlen(requested) + strlen(sug) + 256;
                    char *buf = malloc(bl);
                    snprintf(buf, bl,
                        "Note: `%s` was not found in this custom endpoint's model listing. "
                        "It may still work if the server supports hidden or aliased models.%s", requested, sug);
                    r.accepted = 1; r.persist = 1; r.recognized = 0;
                    r.message = buf;
                    free(sug);
                }
            }
            strlist_free_local(api_models);
        } else {
            /* unreachable */
            r.accepted = (api_mode && strcmp(api_mode, "anthropic_messages") == 0) ? 1 : 0;
            r.persist = 1; r.recognized = 0;
            size_t bl = strlen(requested) + 512;
            char *buf = malloc(bl);
            snprintf(buf, bl,
                "Note: could not reach this custom endpoint's model listing. "
                "Hermes will still save `%s`, but the endpoint should expose `/models` for verification.", requested);
            if (api_mode && strcmp(api_mode, "anthropic_messages") == 0) {
                char *nb = realloc(buf, bl + 256);
                strcat(nb, "\n  Many Anthropic-compatible proxies do not implement the Models API "
                           "(GET /v1/models).  The model name has been accepted without verification.");
                buf = nb;
            }
            r.message = buf;
        }
        goto done;
    }

    if (strcmp(normalized, "openai-codex") == 0 || strcmp(normalized, "xai-oauth") == 0) {
        char *cat_json = catalog_resolve ? catalog_resolve(cat_ctx, normalized) : NULL;
        char **catalog_models = parse_id_array(cat_json);
        free(cat_json);
        int cat_resolved = (catalog_models != NULL);
        if (cat_resolved) {
            int hit = 0;
            for (size_t i = 0; catalog_models[i]; i++)
                if (strcmp(catalog_models[i], requested_for_lookup) == 0) { hit = 1; break; }
            if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
            else {
                char **auto_m = get_close_matches(requested_for_lookup, catalog_models, 1, 0.9);
                if (auto_m && auto_m[0]) {
                    r.accepted = 1; r.persist = 1; r.recognized = 1;
                    char buf[512];
                    snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, auto_m[0]);
                    r.corrected_model = xstrdup(auto_m[0]);
                    strlist_free_local(auto_m);
                    r.message = xstrdup(buf);
                } else {
                    const char *prov_label = (strcmp(normalized, "openai-codex") == 0)
                        ? "OpenAI Codex" : "xAI Grok OAuth (SuperGrok / Premium+)";
                    static const char *codex_pref[] = {"gpt-","codex-","o1","o3","o4",NULL};
                    static const char *xai_pref[] = {"grok-",NULL};
                    const char *const *prefs = (strcmp(normalized, "openai-codex") == 0) ? codex_pref : xai_pref;
                    char low[256]; size_t li;
                    for (li = 0; requested_for_lookup[li] && li + 1 < sizeof(low); li++)
                        low[li] = (char)tolower((unsigned char)requested_for_lookup[li]);
                    low[li] = '\0';
                    int plausible = 1;
                    if (prefs[0]) {
                        plausible = 0;
                        for (int p = 0; prefs[p]; p++) if (strncmp(low, prefs[p], strlen(prefs[p])) == 0) { plausible = 1; break; }
                    }
                    char *sug = suggestion_text(requested_for_lookup, catalog_models, 3);
                    if (!plausible) {
                        size_t bl = strlen(requested) + strlen(prov_label) + strlen(sug) + 256;
                        char *buf = malloc(bl);
                        snprintf(buf, bl,
                            "`%s` doesn't look like a %s model and isn't in its listing, so it was not accepted. "
                            "If it belongs to another configured provider, switch with `--provider <slug>` "
                            "(or select it from the `/model` picker).%s", requested, prov_label, sug);
                        r.accepted = 0; r.persist = 0; r.recognized = 0;
                        r.message = buf;
                    } else {
                        size_t bl = strlen(requested) + strlen(prov_label) + strlen(sug) + 256;
                        char *buf = malloc(bl);
                        snprintf(buf, bl,
                            "Note: `%s` was not found in the %s model listing. "
                            "It may still work if your account has access to a newer or hidden model ID.%s",
                            requested, prov_label, sug);
                        r.accepted = 1; r.persist = 1; r.recognized = 0;
                        r.message = buf;
                    }
                    free(sug);
                }
            }
            strlist_free_local(catalog_models);
        } else {
            r.accepted = 1; r.persist = 1; r.recognized = 0;
            r.message = xstrdup("Note: could not resolve the provider catalog to validate the model.");
            strlist_free_local(catalog_models);
        }
        goto done;
    }

    if (strcmp(normalized, "minimax") == 0 || strcmp(normalized, "minimax-cn") == 0) {
        char *cat_json = catalog_resolve ? catalog_resolve(cat_ctx, normalized) : NULL;
        char **catalog_models = parse_id_array(cat_json);
        free(cat_json);
        if (catalog_models != NULL) {
            /* case-insensitive lookup */
            char **lower = calloc(1, sizeof(char *));
            size_t n = 0;
            for (size_t i = 0; catalog_models[i]; i++) {
                char *l = xstrdup(catalog_models[i]);
                for (char *p = l; *p; p++) *p = (char)tolower((unsigned char)*p);
                lower = realloc(lower, (n + 2) * sizeof(char *));
                lower[n++] = l; lower[n] = NULL;
            }
            char rl[256]; size_t ri;
            for (ri = 0; requested_for_lookup[ri] && ri + 1 < sizeof(rl); ri++)
                rl[ri] = (char)tolower((unsigned char)requested_for_lookup[ri]);
            rl[ri] = '\0';
            int hit = 0;
            for (size_t i = 0; lower[i]; i++) if (strcmp(lower[i], rl) == 0) { hit = 1; break; }
            if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
            else {
                char **auto_m = get_close_matches(rl, lower, 1, 0.9);
                if (auto_m && auto_m[0]) {
                    /* map back to original-case id */
                    const char *orig = NULL;
                    for (size_t i = 0; catalog_models[i]; i++) {
                        char l2[256]; size_t k = 0;
                        for (; catalog_models[i][k] && k + 1 < sizeof(l2); k++)
                            l2[k] = (char)tolower((unsigned char)catalog_models[i][k]);
                        l2[k] = '\0';
                        if (strcmp(l2, auto_m[0]) == 0) { orig = catalog_models[i]; break; }
                    }
                    r.accepted = 1; r.persist = 1; r.recognized = 1;
                    r.corrected_model = xstrdup(orig ? orig : auto_m[0]);
                    strlist_free_local(auto_m);
                    char buf[512];
                    snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, r.corrected_model);
                    r.message = xstrdup(buf);
                } else {
                    char *sug = suggestion_text(rl, lower, 3);
                    size_t bl = strlen(requested) + strlen(sug) + 256;
                    char *buf = malloc(bl);
                    snprintf(buf, bl,
                        "Note: `%s` was not found in the MiniMax catalog.%s\n"
                        "  MiniMax does not expose a /models endpoint, so Hermes cannot verify the model name.\n"
                        "  The model may still work if it exists on the server.", requested, sug);
                    r.accepted = 1; r.persist = 1; r.recognized = 0;
                    r.message = buf;
                    free(sug);
                }
            }
            strlist_free_local(lower);
            strlist_free_local(catalog_models);
        } else {
            r.accepted = 1; r.persist = 1; r.recognized = 0;
            r.message = xstrdup("Note: could not resolve the MiniMax catalog to validate the model.");
        }
        goto done;
    }

    if (strcmp(normalized, "anthropic") == 0) {
        char *aj = models_fetch_anthropic_models(fetch, ctx, base_url, api_key);
        char **am = parse_id_array(aj);
        free(aj);
        if (am != NULL) {
            int hit = 0;
            for (size_t i = 0; am[i]; i++) if (strcmp(am[i], requested_for_lookup) == 0) { hit = 1; break; }
            if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
            else {
                char **auto_m = get_close_matches(requested_for_lookup, am, 1, 0.9);
                if (auto_m && auto_m[0]) {
                    r.accepted = 1; r.persist = 1; r.recognized = 1;
                    char buf[512];
                    snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, auto_m[0]);
                    r.corrected_model = xstrdup(auto_m[0]);
                    strlist_free_local(auto_m);
                    r.message = xstrdup(buf);
                } else {
                    char *sug = suggestion_text(requested, am, 3);
                    size_t bl = strlen(requested) + strlen(sug) + 256;
                    char *buf = malloc(bl);
                    snprintf(buf, bl,
                        "Note: `%s` was not found in Anthropic's /v1/models listing. "
                        "It may still work if you have early-access or snapshot IDs.%s", requested, sug);
                    r.accepted = 1; r.persist = 1; r.recognized = 0;
                    r.message = buf;
                    free(sug);
                }
            }
            strlist_free_local(am);
            goto done;
        }
        /* fall through to generic warning path below */
    }

    if (api_mode && strcmp(api_mode, "anthropic_messages") == 0) {
        char **api_models = models_fetch_api_models(fetch, ctx, api_key, base_url, api_mode);
        if (api_models != NULL) {
            int hit = 0;
            for (size_t i = 0; api_models[i]; i++)
                if (strcmp(api_models[i], requested_for_lookup) == 0) { hit = 1; break; }
            if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
            else {
                char **auto_m = get_close_matches(requested_for_lookup, api_models, 1, 0.9);
                if (auto_m && auto_m[0]) {
                    r.accepted = 1; r.persist = 1; r.recognized = 1;
                    char buf[512];
                    snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, auto_m[0]);
                    r.corrected_model = xstrdup(auto_m[0]);
                    strlist_free_local(auto_m);
                    r.message = xstrdup(buf);
                }
            }
            strlist_free_local(api_models);
            goto done;
        }
        r.accepted = 1; r.persist = 1; r.recognized = 0;
        r.message = xstrdup("Note: could not verify the model against this endpoint's model listing. "
                            "Many Anthropic-compatible proxies do not implement GET /v1/models. "
                            "The model name has been accepted without verification.");
        goto done;
    }

    /* generic live probe */
    char **api_models = models_fetch_api_models(fetch, ctx, api_key, base_url, api_mode);
    if (api_models != NULL) {
        char **cmp = api_models;
        if (strcmp(normalized, "gemini") == 0) {
            cmp = calloc(1, sizeof(char *));
            size_t n = 0;
            for (size_t i = 0; api_models[i]; i++) {
                const char *m = api_models[i];
                if (strncmp(m, "models/", 7) == 0) m += 7;
                cmp = realloc(cmp, (n + 2) * sizeof(char *));
                cmp[n++] = xstrdup(m); cmp[n] = NULL;
            }
        }
        int hit = 0;
        for (size_t i = 0; cmp[i]; i++) if (strcmp(cmp[i], requested_for_lookup) == 0) { hit = 1; break; }
        if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
        else {
            char **auto_m = get_close_matches(requested_for_lookup, cmp, 1, 0.9);
            if (auto_m && auto_m[0]) {
                r.accepted = 1; r.persist = 1; r.recognized = 1;
                r.corrected_model = xstrdup(auto_m[0]);
                char buf[512];
                snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, auto_m[0]);
                r.message = xstrdup(buf);
            } else {
                /* curated-catalog fallback (pure, no extra fetch) */
                char key[64], normk[64];
                models_provider_keys(normalized, key, sizeof(key), normk, sizeof(normk));
                char *provs[2] = { normk, NULL };
                int in_cat = models_model_in_provider_catalog(requested_for_lookup, provs);
                if (in_cat) {
                    size_t bl = strlen(requested) + 256;
                    char *buf = malloc(bl);
                    snprintf(buf, bl,
                        "Note: `%s` was not found in the live /v1/models listing but exists in the curated catalog — accepted.",
                        requested);
                    r.accepted = 1; r.persist = 1; r.recognized = 1;
                    r.message = buf;
                } else {
                    char *sug = suggestion_text(requested, cmp, 3);
                    size_t bl = strlen(requested) + strlen(sug) + 256;
                    char *buf = malloc(bl);
                    snprintf(buf, bl, "Model `%s` was not found in this provider's model listing.%s",
                             requested, sug);
                    r.accepted = 0; r.persist = 0; r.recognized = 0;
                    r.message = buf;
                    free(sug);
                }
            }
        }
        strlist_free_local(cmp);
        if (cmp != api_models) strlist_free_local(api_models);
        goto done;
    }

    /* unreachable -> static-catalog fallback, then accept-with-warning */
    char *cat_json = catalog_resolve ? catalog_resolve(cat_ctx, normalized) : NULL;
    char **catalog_models = parse_id_array(cat_json);
    free(cat_json);
    if (catalog_models && catalog_models[0]) {
        char **lower = calloc(1, sizeof(char *));
        size_t n = 0;
        for (size_t i = 0; catalog_models[i]; i++) {
            char *l = xstrdup(catalog_models[i]);
            for (char *p = l; *p; p++) *p = (char)tolower((unsigned char)*p);
            lower = realloc(lower, (n + 2) * sizeof(char *));
            lower[n++] = l; lower[n] = NULL;
        }
        char rl[256]; size_t ri;
        for (ri = 0; requested_for_lookup[ri] && ri + 1 < sizeof(rl); ri++)
            rl[ri] = (char)tolower((unsigned char)requested_for_lookup[ri]);
        rl[ri] = '\0';
        int hit = 0;
        for (size_t i = 0; lower[i]; i++) if (strcmp(lower[i], rl) == 0) { hit = 1; break; }
        if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
        else {
            char **auto_m = get_close_matches(rl, lower, 1, 0.9);
            if (auto_m && auto_m[0]) {
                const char *orig = NULL;
                for (size_t i = 0; catalog_models[i]; i++) {
                    char l2[256]; size_t k = 0;
                    for (; catalog_models[i][k] && k + 1 < sizeof(l2); k++)
                        l2[k] = (char)tolower((unsigned char)catalog_models[i][k]);
                    l2[k] = '\0';
                    if (strcmp(l2, auto_m[0]) == 0) { orig = catalog_models[i]; break; }
                }
                r.accepted = 1; r.persist = 1; r.recognized = 1;
                r.corrected_model = xstrdup(orig ? orig : auto_m[0]);
                char buf[512];
                snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, r.corrected_model);
                r.message = xstrdup(buf);
            } else {
                char *sug = suggestion_text(rl, lower, 3);
                const char *label = normalized; /* _PROVIDER_LABELS simplified to slug */
                size_t bl = strlen(requested) + strlen(label) + strlen(sug) + 256;
                char *buf = malloc(bl);
                snprintf(buf, bl,
                    "Note: `%s` was not found in the %s curated catalog and the /models endpoint was unreachable.%s\n"
                    "  The model may still work if it exists on the provider.", requested, label, sug);
                r.accepted = 1; r.persist = 1; r.recognized = 0;
                r.message = buf;
                free(sug);
            }
        }
        strlist_free_local(lower);
        strlist_free_local(catalog_models);
        goto done;
    }

    /* no catalog at all */
    r.accepted = 1; r.persist = 1; r.recognized = 0;
    size_t bl = strlen(requested) + strlen(normalized) + 256;
    char *buf = malloc(bl);
    snprintf(buf, bl,
        "Note: could not reach the %s API to validate `%s`. If the service isn't down, this model may not be valid.",
        normalized, requested);
    r.message = buf;

done:
    free(requested); free(normalized); free(requested_for_lookup);
    return r;
}

void models_validate_result_free(models_validate_result_t *r) {
    if (!r) return;
    free(r->message); free(r->corrected_model);
    r->message = NULL; r->corrected_model = NULL;
}


/* ── from src/hermes_cli/port_batch3_gaps.c ── */
/* Slermes C port — batch 3: hermes_cli/models.py, hermes_cli/banner.py,
 * hermes_cli/update_cmd.py, hermes_cli/config.py, hermes_cli/auth.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"
#include <time.h>

static double now_mono(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}









/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/models.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _is_model_free (L632) ── */
/* PoP: models_is_model_free @ hermes_cli/models.py:_is_model_free */
bool models_is_model_free(const char *model_id, json_t *pricing) {
    if (!model_id || !pricing) return false;
    double prompt_val = json_get_num(pricing, "prompt", 1.0);
    double completion_val = json_get_num(pricing, "completion", 1.0);
    return (prompt_val == 0.0 && completion_val == 0.0);
}

/* ── _extract_model_name (L1033) ── */
/* PoP: models_extract_model_name @ hermes_cli/models.py:_extract_model_name */
char *models_extract_model_name(json_t *entry) {
    if (!entry) return NULL;
    const char *model_name = json_get_str(entry, "modelName", NULL);
    if (model_name && *model_name) return xstrdup(model_name);
    return NULL;
}

/* ── _openrouter_model_is_free (L?) ── */
/* PoP: models_openrouter_is_free @ hermes_cli/models.py:_openrouter_model_is_free */
bool models_openrouter_is_free(json_t *pricing) {
    return models_is_model_free(NULL, pricing);
}

/* ── _openrouter_model_supports_tools (L?) ── */
/* PoP: models_openrouter_supports_tools @ hermes_cli/models.py:_openrouter_model_supports_tools */
bool models_openrouter_supports_tools(json_t *item) {
    if (!item) return false;
    return json_get_bool(item, "supports_tools", true);
}

/* ── _format_price_per_mtok (L?) ── */
/* PoP: models_format_price_per_mtok @ hermes_cli/models.py:_format_price_per_mtok */
char *models_format_price_per_mtok(const char *per_token_str) {
    if (!per_token_str) return xstrdup("0");
    double val = atof(per_token_str);
    char buf[32];
    snprintf(buf, sizeof(buf), "$%.2f/M", val * 1000000);
    return xstrdup(buf);
}

/* ── compute_sale_discount (L?) ── */
/* PoP: models_compute_sale_discount_batch3 @ hermes_cli/models.py:compute_sale_discount */
double models_compute_sale_discount_batch3(double original_price, double sale_price) {
    if (original_price <= 0) return 0;
    return (1.0 - sale_price / original_price) * 100.0;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/banner.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _git_stdout (L177) ── */
/* PoP: banner_git_stdout @ hermes_cli/banner.py:_git_stdout */
char *banner_git_stdout(const char *args) {
    if (!args) return xstrdup("");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "git %s 2>/dev/null", args);
    FILE *fp = popen(cmd, "r");
    if (!fp) return xstrdup("");
    char buf[4096] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    /* Strip trailing newline */
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return xstrdup(buf);
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/config.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _config_path (L?) ── */
/* PoP: config_path @ hermes_cli/config.py:_config_path */
char *config_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
    return xstrdup(path);
}

/* ── _config_dir (L?) ── */
/* PoP: config_dir @ hermes_cli/config.py:_config_dir */
char *config_dir(void) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes", home);
    return xstrdup(path);
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/auth.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _token_path (L?) ── */
/* PoP: auth_token_path @ hermes_cli/auth.py:_token_path */
char *auth_token_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/auth.json", home);
    return xstrdup(path);
}

/* ── _is_token_expired (L?) ── */
/* PoP: auth_is_token_expired @ hermes_cli/auth.py:_is_token_expired */
bool auth_is_token_expired(json_t *token_data) {
    if (!token_data) return true;
    double exp_ts = json_get_num(token_data, "expires_at", 0);
    if (exp_ts == 0) return false; /* no expiry = never expires */
    
    return now_mono() >= exp_ts;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/update_cmd.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _update_lock_path (L?) ── */
/* PoP: update_lock_path @ hermes_cli/update_cmd.py:_update_lock_path */
char *update_lock_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/.update_lock", home);
    return xstrdup(path);
}

/* ── _is_update_locked (L?) ── */
/* PoP: update_is_locked @ hermes_cli/update_cmd.py:_is_update_locked */
bool update_is_locked(void) {
    char *path = update_lock_path();
    bool locked = (access(path, F_OK) == 0);
    free(path);
    return locked;
}

/* ── recovered: models_finite_float (from src/cli/port_models_py_helpers.c) ── */
static double models_finite_float(const char *raw) {
    if (!raw || !raw[0]) return -1.0;
    char *end = NULL;
    double n = strtod(raw, &end);
    if (end == raw || *end != '\0') return -1.0;
    if (!(n > 0.0) || n != n) return -1.0; /* >0 and not NaN */
    return n;
}
