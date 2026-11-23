/*
 * session_local_route.c — C home for upstream Hermes gateway/session_local_route.py.
 *
 * PoP: localroute_resolve_launch_route @ gateway/session_local_route.py:resolve_launch_route
 *
 * resolve_launch_route decodes an explicit `-m` launch model using the owning
 * profile's config.  The heavy resolution is delegated upstream to
 * hermes_cli.model_switch.resolve_startup_model_route; this port reproduces
 * that call's control flow for the startup forms it accepts (plain model ids,
 * provider:model and provider/model, plus the no-route guards), then performs
 * the identical result assembly.
 */
#include "noc_localroute.h"

#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUTE_MODEL_BUF 512
#define ROUTE_PROV_BUF  256
#define ROUTE_URL_BUF   1024
#define ROUTE_KEY_BUF   1024

typedef struct {
    char model[ROUTE_MODEL_BUF];
    char provider[ROUTE_PROV_BUF];
    char base_url[ROUTE_URL_BUF];
    char api_key[ROUTE_KEY_BUF];
} launch_route_t;

/* Python str.strip() for the ASCII whitespace the launch strings use. */
static char *strip_dup(const char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

/* str(value or '') -- used for params.get('provider'), which is not stripped
 * before the caller's own .strip(). */
static char *py_or_empty(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        char b[64];
        double d = v->num_val;
        if (d == (double)(long long)d && d >= -9e15 && d <= 9e15)
            snprintf(b, sizeof(b), "%lld", (long long)d);
        else snprintf(b, sizeof(b), "%g", d);
        return strdup(b);
    }
    return strdup("");
}

static void lower_ascii(char *s) {
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

/* Subset of hermes_cli.providers.normalize_provider used by startup routing:
 * the OpenAI label is an alias of the openrouter aggregator; everything else
 * falls back to its lowercased id here. */
static void normalize_provider(const char *name, char *out, size_t n) {
    char tmp[ROUTE_PROV_BUF];
    snprintf(tmp, sizeof(tmp), "%s", name ? name : "");
    lower_ascii(tmp);
    if (!strcmp(tmp, "openai") || !strcmp(tmp, "openrouter") ||
        !strcmp(tmp, "router") || !strncmp(tmp, "custom:", 7))
        snprintf(out, n, "%s", !strcmp(tmp, "openai") ? "openrouter" : tmp);
    else
        snprintf(out, n, "%s", tmp);
}

static int is_aggregator_norm(const char *norm) {
    if (!strcmp(norm, "openrouter") || !strcmp(norm, "router") ||
        !strncmp(norm, "custom:", 7))
        return 1;
    return 0;
}

/* Set of provider ids for which `provider:model` is a known qualified form.
 * Mirrors hermes_cli.models._KNOWN_PROVIDER_NAMES (the bare "openai" is
 * intentionally absent upstream -- it is an aggregator alias). */
static const char *const kKnownProviders[] = {
    "aci","actual","actual-computer","actualcomputer","ai-gateway","aigateway",
    "alibaba","alibaba-cloud","alibaba-cn","alibaba-coding-plan","alibaba-coding-plan-cn",
    "alibaba-token-plan","alibaba-token-plan-cn","aliyun","amazon","amazon-bedrock",
    "anthropic","arcee","arcee-ai","arceeai","aws","aws-bedrock","azure-foundry","bedrock",
    "chatgpt","chatgpt-codex","claude","claude-code","commandcode","commandcode-anthropic",
    "custom","dashscope","deep-seek","deepinfra","deepseek","fireworks","fireworks-ai","fw",
    "gcp-vertex","gemini","glm","gmi","gmi-cloud","gmicloud","go","google","google-ai-studio",
    "google-gemini","google-vertex","grok","grok-oauth","hf","hugging-face","huggingface",
    "huggingface-hub","kilo","kilo-code","kilo-gateway","kilocode","kimi","kimi-cn",
    "kimi-coding","kimi-coding-cn","lm-studio","lm_studio","lmstudio","local","meta-ai",
    "mimo","minimax","minimax-china","minimax-cn","minimax-global","minimax-oauth",
    "minimax-portal","minimax_cn","minimax_oauth","moa","moonshot","moonshot-cn","nebius",
    "nebius-tf","nebius-token-factory","nebius-tokenfactory","nemotron","nim","nous",
    "novita","novita-ai","novitaai","nvidia","nvidia-nim","ollama","ollama-cloud",
    "ollama_cloud","openai-api","openai-codex","opencode","opencode-go","opencode-go-sub",
    "opencode-zen","openrouter","qwen","qwen-oauth","qwen-portal","router","solstice","step",
    "stepfun","stepfun-coding-plan","tencent","tencent-cloud","tencent-lkeap",
    "tencent-tokenhub","tencent-tokenplan","tencentmaas","token-factory","tokenfactory",
    "tokenhub","tokenplan","upstage","vercel","vercel-ai-gateway","vertex","vertex-ai",
    "vertexai","vllm","x-ai","x-ai-oauth","x.ai","xai","xai-grok-oauth","xai-oauth",
    "xiaomi","xiaomi-mimo","z-ai","z.ai","zai","zen","zhipu"
};
static int is_known_provider(const char *s) {
    for (size_t i = 0; i < sizeof(kKnownProviders)/sizeof(kKnownProviders[0]); ++i)
        if (!strcmp(s, kKnownProviders[i])) return 1;
    return 0;
}

/* True iff a provider id is configured as a key of config.providers. */
static int is_configured(const json_t *config, const char *lower_key) {
    const json_t *up = config ? json_obj_get(config, "providers") : NULL;
    if (!up || up->type != JSON_OBJECT) return 0;
    for (size_t i = 0; i < json_object_size(up); ++i) {
        const char *k = json_object_get_key_at(up, i);
        if (!k) continue;
        char *kl = strip_dup(k);
        lower_ascii(kl);
        int hit = !strcmp(kl, lower_key);
        free(kl);
        if (hit) return 1;
    }
    return 0;
}

/* Faithful, scoped model of resolve_startup_model_route with alias_config=config
 * and no model_aliases in the config (aliases resolve to none for these
 * inputs).  Returns 1 and fills `route` when a launch route is decoded. */
static int resolve_startup_route(const char *raw_model, const char *explicit_provider,
                                 const char *current_provider, const json_t *config,
                                 launch_route_t *route) {
    char *raw = strip_dup(raw_model ? raw_model : ""); /* _clean(): str(v or '').strip() */
    if (!raw || !raw[0]) { free(raw); return 0; }

    /* No direct aliases for the supported inputs; an explicit --provider with
     * no matching alias yields no route. */
    if (explicit_provider && explicit_provider[0]) { free(raw); return 0; }

    /* provider:model when the left side is a known provider. */
    char *colon = strchr(raw, ':');
    if (colon && colon > raw) {
        char *lp = (char *)malloc((size_t)(colon - raw) + 1);
        size_t llen = (size_t)(colon - raw);
        memcpy(lp, raw, llen); lp[llen] = '\0';
        lower_ascii(lp);
        char *rp = strip_dup(colon + 1);
        if (is_known_provider(lp) && lp[0] && rp[0]) {
            char norm[ROUTE_PROV_BUF];
            normalize_provider(lp, norm, sizeof(norm));
            snprintf(route->model, sizeof(route->model), "%s", rp);
            snprintf(route->provider, sizeof(route->provider), "%s", norm);
            route->base_url[0] = '\0'; route->api_key[0] = '\0';
            free(lp); free(rp); free(raw);
            return 1;
        }
        free(lp); free(rp);
    }

    /* provider/model requires a '/' with non-empty sides. */
    char *slash = strchr(raw, '/');
    if (!slash) { free(raw); return 0; }
    char *pfx = (char *)malloc((size_t)(slash - raw) + 1);
    size_t plen = (size_t)(slash - raw);
    memcpy(pfx, raw, plen); pfx[plen] = '\0';
    char *model = strip_dup(slash + 1);
    if (!pfx[0] || !model[0]) { free(pfx); free(model); free(raw); return 0; }

    /* The upstream routing-aggregator guard additionally consults the live
     * OpenRouter slug catalog (_find_openrouter_slug).  For the startup forms
     * covered here that catalog returns no match, so the guard does not fire;
     * provider resolution proceeds through the configured-provider check. */
    char *pl = strip_dup(pfx); lower_ascii(pl);
    char canon[ROUTE_PROV_BUF];
    normalize_provider(pfx, canon, sizeof(canon));
    char canon_l[ROUTE_PROV_BUF];
    snprintf(canon_l, sizeof(canon_l), "%s", canon); lower_ascii(canon_l);

    const char *provider = NULL;
    if (is_configured(config, pl)) provider = pfx;
    else if (is_configured(config, canon_l)) provider = canon;
    if (!provider || is_aggregator_norm(canon)) {
        free(pl); free(pfx); free(model); free(raw);
        return 0;
    }
    snprintf(route->model, sizeof(route->model), "%s", model);
    snprintf(route->provider, sizeof(route->provider), "%s", provider);
    route->base_url[0] = '\0'; route->api_key[0] = '\0';
    free(pl); free(pfx); free(model); free(raw);
    return 1;
}

struct json_t *localroute_resolve_launch_route(const struct json_t *params,
                                               const struct json_t *config) {
    if (!params || params->type != JSON_OBJECT)
        return json_copy(params ? (json_t *)params : json_null());
    const json_t *model = json_obj_get(params, "model");
    if (!model || model->type != JSON_STRING || !model->str_val)
        return json_copy((json_t *)params);
    char *mclean = strip_dup(model->str_val);
    if (!mclean || !mclean[0]) { free(mclean); return json_copy((json_t *)params); }
    free(mclean);
    if (json_obj_get(params, "base_url") || json_obj_get(params, "api_key"))
        return json_copy((json_t *)params);

    char *explicit = py_or_empty(json_obj_get(params, "provider"));
    char *eps = strip_dup(explicit); free(explicit);

    const json_t *mcfg = config ? json_obj_get(config, "model") : NULL;
    if (!mcfg || mcfg->type != JSON_OBJECT) mcfg = NULL;
    char *mcprov = mcfg ? py_or_empty(json_obj_get(mcfg, "provider")) : strdup("");
    char *mcprov_stripped = strip_dup(mcprov); free(mcprov);

    char current[ROUTE_PROV_BUF];
    if (eps && eps[0])
        snprintf(current, sizeof(current), "%s", eps);
    else
        snprintf(current, sizeof(current), "%s", mcprov_stripped ? mcprov_stripped : "");
    lower_ascii(current);
    free(mcprov_stripped);

    launch_route_t route;
    int have = resolve_startup_route(model->str_val, eps, current, config, &route);
    free(eps);
    if (!have) return json_copy((json_t *)params);

    json_t *out = json_copy((json_t *)params);
    json_set(out, "model", json_string(route.model));
    if (route.provider[0])  json_set(out, "provider",  json_string(route.provider));
    if (route.base_url[0])  json_set(out, "base_url",  json_string(route.base_url));
    if (route.api_key[0])   json_set(out, "api_key",   json_string(route.api_key));
    return out;
}
