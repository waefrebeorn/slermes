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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>

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
