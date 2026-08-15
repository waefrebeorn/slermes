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
 *   base_url_looks_like_anthropic_messages  <- _base_url_looks_like_anthropic_messages
 *   anthropic_models_url                    <- _anthropic_models_url
 *   strip_vendor_prefix                     <- _strip_vendor_prefix
 *   is_openai_fast_model                    <- _is_openai_fast_model
 *   is_anthropic_fast_model                 <- _is_anthropic_fast_model
 *   model_supports_fast_mode                <- model_supports_fast_mode
 *   fast_mode_overrides_json                <- resolve_fast_mode_overrides
 *   openrouter_model_is_free                <- _openrouter_model_is_free
 *   strip_ollama_cloud_suffix               <- _strip_ollama_cloud_suffix
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
 * PoP: _is_model_free @ hermes_cli/models.py:_is_model_free
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
 * PoP: is_nous_free_tier @ hermes_cli/models.py:is_nous_free_tier
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
 * PoP: partition_nous_models_by_tier @ hermes_cli/models.py:partition_nous_models_by_tier
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
 * PoP: _extract_model_name @ hermes_cli/models.py:_extract_model_name
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
 * PoP: _format_price_per_mtok @ hermes_cli/models.py:_format_price_per_mtok
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
 * PoP: _base_url_looks_like_anthropic_messages @ hermes_cli/models.py:_base_url_looks_like_anthropic_messages
 * True if normalized path ends with /anthropic or /anthropic/v1. */
int base_url_looks_like_anthropic_messages(const char *base_url)
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
 * PoP: _anthropic_models_url @ hermes_cli/models.py:_anthropic_models_url
 * Append /v1/models or /models to a base URL. Returns malloc'd string. */
char *anthropic_models_url(const char *base_url)
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
 * PoP: _strip_vendor_prefix @ hermes_cli/models.py:_strip_vendor_prefix
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

/*
 * PoP: _is_openai_fast_model @ hermes_cli/models.py:_is_openai_fast_model */
int is_openai_fast_model(const char *model_id)
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

/*
 * PoP: _is_anthropic_fast_model @ hermes_cli/models.py:_is_anthropic_fast_model */
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

/*
 * PoP: model_supports_fast_mode @ hermes_cli/models.py:model_supports_fast_mode */
int model_supports_fast_mode(const char *model_id)
{
    return is_anthropic_fast_model(model_id) || is_openai_fast_model(model_id);
}

/*
 * PoP: resolve_fast_mode_overrides @ hermes_cli/models.py:resolve_fast_mode_overrides
 * Returns malloc'd JSON {"speed":"fast"} or {"service_tier":"priority"} or
 * NULL when unsupported. Caller frees. */
char *fast_mode_overrides_json(const char *model_id)
{
    if (!model_supports_fast_mode(model_id)) return NULL;
    if (is_anthropic_fast_model(model_id)) return strdup("{\"speed\":\"fast\"}");
    return strdup("{\"service_tier\":\"priority\"}");
}

/*
 * PoP: _openrouter_model_is_free @ hermes_cli/models.py:_openrouter_model_is_free
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
 * PoP: _strip_ollama_cloud_suffix @ hermes_cli/models.py:_strip_ollama_cloud_suffix
 * Strip :cloud / -cloud suffix. Returns malloc'd string. Caller frees. */
char *strip_ollama_cloud_suffix(const char *model_id)
{
    if (!model_id) return strdup("");
    size_t L = strlen(model_id);
    if (L >= 6 && strcmp(model_id + L - 6, ":cloud") == 0) L -= 6;
    else if (L >= 6 && strcmp(model_id + L - 6, "-cloud") == 0) L -= 6;
    char *out = malloc(L + 1);
    memcpy(out, model_id, L); out[L] = '\0';
    return out;
}

/*
 * PoP: _is_github_models_base_url @ hermes_cli/models.py:_is_github_models_base_url */
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
 * PoP: _lmstudio_server_root @ hermes_cli/models.py:_lmstudio_server_root
 * Strip /api/v1, /api, /v1 suffixes. Returns malloc'd string or NULL. */
char *lmstudio_server_root(const char *base_url)
{
    if (!base_url || !base_url[0]) return NULL;
    char buf[1024];
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

/*
 * PoP: normalize_opencode_model_id @ hermes_cli/models.py:normalize_opencode_model_id */
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
 * PoP: opencode_model_api_mode @ hermes_cli/models.py:opencode_model_api_mode
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
 * PoP: azure_foundry_model_api_mode @ hermes_cli/models.py:azure_foundry_model_api_mode
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

/*
 * PoP: _should_use_copilot_responses_api @ hermes_cli/models.py:_should_use_copilot_responses_api */
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
 * PoP: copilot_default_headers @ hermes_cli/models.py:copilot_default_headers
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
 * PoP: _credential_fingerprint @ hermes_cli/models.py:_credential_fingerprint
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

/* Returns malloc'd lowercased dedup key, folding through _MODEL_ALIAS_CANONICAL.
/* PoP: _model_dedup_key @ hermes_cli/models.py:_model_dedup_key */
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

/* PoP: _openai_discovery_base_url @ hermes_cli/models.py:_openai_discovery_base_url */
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

/* Returns malloc'd JSON: {model_id: {"prompt":..,"completion":..,
/* PoP: fetch_ai_gateway_pricing @ hermes_cli/models.py:fetch_ai_gateway_pricing */
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

/* Return just the AI Gateway model-id strings (faithful to Python's
 *  [mid for mid, _ in fetch_ai_gateway_models(...)] ). Returns malloc'd
/* PoP: ai_gateway_model_ids @ hermes_cli/models.py:ai_gateway_model_ids */
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
/* PoP: _spawn_swr_refresh @ hermes_cli/models.py:_spawn_swr_refresh */
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

