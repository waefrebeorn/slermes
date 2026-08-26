/**
 * port_runtime_provider.c — Port of Python: cli.py (runtime provider helpers)
 *
 * Real C implementations for runtime provider functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "port_provider_registry.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: getenv */
char *getenv_fn(const char *name, const char *default_val)
{
    if (!name) {
        hermes_log(LOG_WARNING, "port", "getenv_fn: null name");
        return default_val ? strdup(default_val) : NULL;
    }
    const char *val = getenv(name);
    if (val) {
        hermes_log(LOG_DEBUG, "port", "getenv_fn: %s=%s", name, val);
        return strdup(val);
    }
    hermes_log(LOG_DEBUG, "port", "getenv_fn: %s not set, using default", name);
    return default_val ? strdup(default_val) : NULL;
}

/* Port of Python: _getenv */
char *_getenv(void *ctx, void *name, void *default_val)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_getenv: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "_getenv: called");
    if (name) {
        hermes_log(LOG_DEBUG, "port", "_getenv: name is set");
    }
    if (default_val) {
        hermes_log(LOG_DEBUG, "port", "_getenv: default_val is set");
    }
    return getenv_fn((const char *)name, (const char *)default_val);
}

/* Port of Python: canonical_custom_identity.
 * Real implementation: delegates to the config-backed provider registry, which
 * reverse-looks-up base_url → custom:<name>, then the configured provider when
 * it names a real ``providers:``/``custom_providers:`` entry. (The model-based
 * recovery source is available via provider_canonical_custom_identity's 3rd
 * arg; this 2-arg ABI passes model=NULL, matching the original call sites.) */
char *canonical_custom_identity(const char *base_url, const char *config_provider)
{
    return provider_canonical_custom_identity(base_url, config_provider, NULL);
}

/* PoP: _canonical_custom_identity @ hermes_cli/runtime_provider.py:canonical_custom_identity */
char *_canonical_custom_identity(void *ctx, void *base_url, void *config_provider)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "canonical_custom_identity: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "canonical_custom_identity: called");
    return canonical_custom_identity(
        base_url ? (const char *)base_url : NULL,
        config_provider ? (const char *)config_provider : NULL);
}

/* ── runtime_provider.py resolution surface ───────────────────────────── */

#include "slermes_home.h"
#include "yaml.h"
#include <ctype.h>
#include "auth_helpers.h"

/* config.yaml → json_t (owned by caller; free with json_free). Same load
 * shape as the provider registry's config-backed scans. */
static json_t *rp_load_config(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/config.yaml", slermes_home());
    char *yerr = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &yerr);
    if (yerr) free(yerr);
    if (!doc) return NULL;
    char *js = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!js) return NULL;
    char *jerr = NULL;
    json_t *cfg = json_parse(js, &jerr);
    free(js);
    if (jerr) free(jerr);
    return cfg;
}

/* strip + lowercase into a malloc'd string ("" for NULL). */
static char *rp_strip_lower(const char *s) {
    if (!s) return strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++)
        out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

/*
 * Resolve the provider request from explicit arg → config model.provider →
 * HERMES_INFERENCE_PROVIDER env → "auto". The persisted config selection is
 * deliberately preferred over any stale shell/.env override so chat uses the
 * PoP: resolve_requested_provider @ hermes_cli/runtime_provider.py:resolve_requested_provider
 */
char *runtime_provider_resolve_requested(const char *requested) {
    /* 1. explicit argument wins */
    char *req = rp_strip_lower(requested);
    if (req && *req) return req;
    free(req);

    /* 2. config model.provider */
    json_t *cfg = rp_load_config();
    if (cfg) {
        const json_t *model_cfg = json_obj_get(cfg, "model");
        if (model_cfg && json_is_object(model_cfg)) {
            json_t *p = json_obj_get(model_cfg, "provider");
            if (p && json_is_string(p)) {
                char *cp = rp_strip_lower(json_string_value(p));
                if (cp && *cp) { json_free(cfg); return cp; }
                free(cp);
            }
        }
        json_free(cfg);
    }

    /* 3. env override (profile-scoped getenv in Python; plain env here) */
    char *env_p = rp_strip_lower(getenv("HERMES_INFERENCE_PROVIDER"));
    if (env_p && *env_p) return env_p;
    free(env_p);

    return strdup("auto");
}

/*
 * AuthError instances route through auth.py:format_auth_error; everything else
 * formats via str(error). In C the exception type is an explicit pointer:
 * non-NULL `error` means AuthError; otherwise `fallback` is str(error).
 * PoP: format_runtime_provider_error @ hermes_cli/runtime_provider.py:format_runtime_provider_error
 */
char *runtime_provider_format_error(const auth_error_t *error,
                                    const char *fallback) {
    if (error) return auth_format_error(error);
    return strdup(fallback ? fallback : "");
}

/* ────────────────────────────────────────────────────────────────────────
 * REAL_GAP closure batch — hermes_cli/runtime_provider.py resolution
 * surface. Conventions for this batch:
 *   • Runtime dicts are JSON objects; Python's ``None`` return is NULL.
 *   • Python ``raise AuthError(msg)`` is a returned JSON object carrying
 *     ``"auth_error": msg`` (callers surface it exactly like the exception).
 *   • ``_getenv`` routes through the secret scope in Python; the C core
 *     reads the process env directly (multiplexing off == identical).
 * ──────────────────────────────────────────────────────────────────────── */

#include "libhttp/http.h"
#include "credential_pool.h"
#include "credential_pool_internals.h"   /* get_custom_provider_pool_key */
#include "port_config_py_helpers.h"      /* normalize_extra_headers /
                                          * is_provider_enabled /
                                          * get_compatible_custom_providers */
#include "port_models_helpers.h"         /* azure_foundry_model_api_mode */

#define RP_DEFAULT_CODEX_BASE_URL      "https://chatgpt.com/backend-api/codex"
#define RP_DEFAULT_XAI_OAUTH_BASE_URL  "https://api.x.ai/v1"
#define RP_DEFAULT_QWEN_BASE_URL       "https://portal.qwen.ai/v1"
#define RP_OPENROUTER_BASE_URL         "https://openrouter.ai/api/v1"
#define RP_ANTHROPIC_BASE_URL          "https://api.anthropic.com"
#define RP_COPILOT_INFERENCE_BASE_URL  "https://api.githubcopilot.com"
#define RP_ACTUAL_LOCAL_NOAUTH_PLACEHOLDER "dummy-actual-local-api-key"

/* Pool engine symbols not yet in a public header. */
extern credential_pool_t *load_pool(const char *provider);
extern int credential_pool_select(const credential_pool_t *pool);

/* strip whitespace both ends into a malloc'd string ("" for NULL). */
static char *rp_strip(const char *s) {
    if (!s) return strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* _getenv(name, "") + .strip() — env read returning malloc'd ""-on-miss. */
static char *rp_getenv_stripped(const char *name) {
    const char *raw = name ? getenv(name) : NULL;
    return rp_strip(raw);
}

/* Borrowed string field of a JSON object, "" when absent/non-string. */
static const char *rp_str_field(const json_t *obj, const char *key) {
    const json_t *v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* models.py helpers (declared in models_gaps.h / port_models_pure.h). */
extern char *models_opencode_provider_family2(const char *provider_id);

/* providers.py:nous_api_mode — "anthropic/" prefix ⇒ anthropic_messages. */
static const char *rp_nous_api_mode(const char *model) {
    const char *m = model ? model : "";
    while (*m == ' ' || *m == '\t') m++;
    if (strncasecmp(m, "anthropic/", 10) == 0) return "anthropic_messages";
    return "chat_completions";
}

/*
 * Port of Python: _config_base_url_trustworthy_for_bare_custom
 * GitHub #14676/#27132: model.base_url may back bare ``custom`` only when
 * the YAML provider is already custom (or an alias resolving to custom),
 * and a stale OpenRouter URL can never hijack local custom sessions.
 */
/* PoP: _config_base_url_trustworthy_for_bare_custom @ hermes_cli/runtime_provider.py:_config_base_url_trustworthy_for_bare_custom */
bool _config_base_url_trustworthy_for_bare_custom(const char *cfg_base_url,
                                                  const char *cfg_provider)
{
    char *provider_norm = rp_strip_lower(cfg_provider);
    char *bu = rp_strip(cfg_base_url);
    bool ok = false;
    if (!bu || !*bu) { free(provider_norm); free(bu); return false; }
    if (strcmp(provider_norm, "custom") == 0) {
        ok = true;
        goto done;
    }
    /* Provider aliases that resolve to "custom" at runtime (ollama, vllm,
     * llamacpp, …) are trusted the same way "custom" is. */
    {
        char *canon = provider_resolve_alias(provider_norm);
        if (canon) {
            if (strcmp(canon, "custom") == 0) ok = true;
            free(canon);
        }
    }
    if (ok) goto done;
    if (provider_base_url_host_matches(bu, "openrouter.ai")) { ok = false; goto done; }
    {
        char *host = provider_base_url_hostname(bu);
        ok = provider_loopback_hostname(host);
        free(host);
    }
done:
    free(provider_norm);
    free(bu);
    return ok;
}

/*
 * Port of Python: _fallback_api_mode
 * Precedence: URL detection first, then the provider overlay's declared
 * transport (``overlay`` — what providers.determine_api_mode resolves from
 * the registry transport map), then chat_completions.
 */
/* PoP: _fallback_api_mode @ hermes_cli/runtime_provider.py:_fallback_api_mode */
char *_fallback_api_mode(const char *provider, const char *base_url,
                         const char *model, const char *overlay)
{
    (void)model;
    const char *detected = provider_detect_api_mode_for_url(base_url);
    if (detected) return strdup(detected);
    const char *declared = provider_parse_api_mode(overlay);
    if (declared) return strdup(declared);
    return strdup("chat_completions");
}

/*
 * Port of Python: _resolve_plain_custom_api_mode
 * Custom endpoints stay conservative: a persisted codex_responses mode only
 * survives when URL detection agrees the endpoint really speaks Responses.
 */
/* PoP: _resolve_plain_custom_api_mode @ hermes_cli/runtime_provider.py:_resolve_plain_custom_api_mode */
char *_resolve_plain_custom_api_mode(const json_t *model_cfg,
                                     const char *base_url)
{
    const char *configured_raw = rp_str_field(model_cfg, "api_mode");
    char *configured = NULL;
    {
        const char *parsed = provider_parse_api_mode(configured_raw);
        if (parsed) configured = strdup(parsed);
    }
    const char *detected = provider_detect_api_mode_for_url(base_url);
    if (configured && strcmp(configured, "codex_responses") == 0 &&
        (!detected || strcmp(detected, "codex_responses") != 0)) {
        /* Stale persisted custom api_mode=codex_responses on a non-OpenAI
         * endpoint — ignore it. */
        free(configured);
        configured = NULL;
    }
    char *out = strdup(configured ? configured
                                  : (detected ? detected : "chat_completions"));
    free(configured);
    return out;
}

/*
 * Port of Python: _auto_detect_local_model
 * GET <base_url>/v1/models with a short timeout; when exactly one model is
 * loaded, return its id. Any failure → "".
 */
/* PoP: _auto_detect_local_model @ hermes_cli/runtime_provider.py:_auto_detect_local_model */
char *_auto_detect_local_model(const char *base_url)
{
    if (!base_url || !*base_url) return strdup("");
    char *url = NULL;
    char *stripped = rp_strip(base_url);
    size_t n = strlen(stripped);
    while (n > 0 && stripped[n - 1] == '/') stripped[--n] = '\0';
    if (n >= 3 && strcmp(stripped + n - 3, "/v1") == 0)
        url = strdup(stripped);
    else
        asprintf(&url, "%s/v1", stripped);
    free(stripped);
    if (!url) return strdup("");
    char *models_url = NULL;
    asprintf(&models_url, "%s/models", url);
    free(url);
    if (!models_url) return strdup("");

    http_t *h = http_new(3);   /* Python timeout=(2, 3): 3s read cap */
    http_resp_t *r = h ? http_get(h, models_url, NULL) : NULL;
    free(models_url);
    char *result = strdup("");
    if (r && r->status >= 200 && r->status < 300 && r->body) {
        char *jerr = NULL;
        json_t *doc = json_parse(r->body, &jerr);
        if (jerr) free(jerr);
        if (doc) {
            json_t *data = json_obj_get(doc, "data");
            if (data && data->type == JSON_ARRAY &&
                json_array_size(data) == 1) {
                json_t *only = json_get(data, 0);
                const char *id = rp_str_field(only, "id");
                if (*id) { free(result); result = strdup(id); }
            }
            json_free(doc);
        }
    }
    if (r) http_resp_free(r);
    if (h) http_free(h);
    return result;
}

/*
 * Port of Python: _copilot_runtime_api_mode
 * Persisted explicit mode wins (when the config provider is copilot);
 * otherwise derive from the target model via the Copilot catalog rule:
 * GPT-5.x/codex-family models need codex_responses, everything else uses
 * chat_completions (the catalog network probe degrades to this static rule
 * in the C core, which links the model-family table statically).
 */
/* PoP: _copilot_runtime_api_mode @ hermes_cli/runtime_provider.py:_copilot_runtime_api_mode */
char *_copilot_runtime_api_mode(const json_t *model_cfg, const char *api_key,
                                const char *target_model)
{
    (void)api_key;
    char *configured_provider = rp_strip_lower(rp_str_field(model_cfg, "provider"));
    const char *configured_mode = provider_parse_api_mode(rp_str_field(model_cfg, "api_mode"));
    if (configured_mode &&
        provider_supports_explicit_api_mode("copilot", configured_provider)) {
        free(configured_provider);
        return strdup(configured_mode);
    }
    free(configured_provider);

    char *model_name = rp_strip(target_model);
    if (!model_name || !*model_name) {
        free(model_name);
        model_name = rp_strip(rp_str_field(model_cfg, "default"));
    }
    if (!model_name || !*model_name) {
        free(model_name);
        return strdup("chat_completions");
    }
    /* copilot_model_api_mode(model_name): Responses-API family detection. */
    char *lower = rp_strip_lower(model_name);
    free(model_name);
    bool responses_family =
        strncmp(lower, "gpt-5", 5) == 0 ||
        strncmp(lower, "codex", 5) == 0 ||
        strncmp(lower, "o1", 2) == 0 ||
        strncmp(lower, "o3", 2) == 0 ||
        strncmp(lower, "o4", 2) == 0;
    free(lower);
    return strdup(responses_family ? "codex_responses" : "chat_completions");
}

/*
 * Port of Python: _nous_inference_base_url_override
 * Trusted NOUS_INFERENCE_BASE_URL env override; trailing-slash stripped,
 * blank → "".
 */
/* PoP: _nous_inference_base_url_override @ hermes_cli/runtime_provider.py:_nous_inference_base_url_override */
char *_nous_inference_base_url_override(void)
{
    char *v = rp_getenv_stripped("NOUS_INFERENCE_BASE_URL");
    size_t n = strlen(v);
    while (n > 0 && v[n - 1] == '/') v[--n] = '\0';
    return v;
}

/*
 * Port of Python: _maybe_apply_codex_app_server_runtime
 * Opt-in rewrite api_mode → "codex_app_server" for openai/openai-codex when
 * model.openai_runtime says so; no-op otherwise.
 */
/* PoP: _maybe_apply_codex_app_server_runtime @ hermes_cli/runtime_provider.py:_maybe_apply_codex_app_server_runtime */
char *_maybe_apply_codex_app_server_runtime(const char *provider,
                                            const char *api_mode,
                                            const json_t *model_cfg)
{
    if (!model_cfg || !provider) return strdup(api_mode ? api_mode : "");
    if (strcmp(provider, "openai") != 0 && strcmp(provider, "openai-codex") != 0)
        return strdup(api_mode ? api_mode : "");
    char *runtime = rp_strip_lower(rp_str_field(model_cfg, "openai_runtime"));
    char *out = strdup((runtime && strcmp(runtime, "codex_app_server") == 0)
                           ? "codex_app_server"
                           : (api_mode ? api_mode : ""));
    free(runtime);
    return out;
}

/*
 * Port of Python: _resolve_runtime_from_pool_entry
 * Per-provider api_mode/base_url derivation from a selected pool entry.
 * ``entry_json`` mirrors PooledCredential fields (runtime_base_url, base_url,
 * runtime_api_key, access_token, agent_key, source).
 */
/* PoP: _resolve_runtime_from_pool_entry @ hermes_cli/runtime_provider.py:_resolve_runtime_from_pool_entry */
char *_resolve_runtime_from_pool_entry(const char *provider,
                                       const json_t *entry_json,
                                       const char *requested_provider,
                                       const json_t *model_cfg,
                                       const char *target_model)
{
    if (!provider || !entry_json || entry_json->type != JSON_OBJECT)
        return NULL;
    json_t *mc = (json_t *)(model_cfg && model_cfg->type == JSON_OBJECT
                                ? model_cfg : json_object());
    bool mc_owned = (mc != model_cfg);

    char *effective_model = rp_strip(target_model);
    if (!effective_model || !*effective_model) {
        free(effective_model);
        effective_model = rp_strip(rp_str_field(mc, "default"));
    }

    /* base_url: runtime_base_url or base_url, rstrip('/'). */
    char *base_url = rp_strip(rp_str_field(entry_json, "runtime_base_url"));
    if (!*base_url) {
        free(base_url);
        base_url = rp_strip(rp_str_field(entry_json, "base_url"));
    }
    size_t bn = strlen(base_url);
    while (bn > 0 && base_url[bn - 1] == '/') base_url[--bn] = '\0';

    char *api_key = rp_strip(rp_str_field(entry_json, "runtime_api_key"));
    if (!*api_key) {
        free(api_key);
        api_key = strdup(rp_str_field(entry_json, "access_token"));
    }

    /* api_mode is ALWAYS a heap string here — every assignment goes through
     * RP_SET_MODE so literals get copied and previous values freed. */
    char *api_mode = strdup("chat_completions");
#define RP_SET_MODE(v) do { \
        free(api_mode); \
        api_mode = strdup((v) ? (v) : "chat_completions"); \
    } while (0)

    if (strcmp(provider, "openai-codex") == 0) {
        RP_SET_MODE("codex_responses");
        if (!*base_url) { free(base_url); base_url = strdup(RP_DEFAULT_CODEX_BASE_URL); }
    } else if (strcmp(provider, "xai-oauth") == 0) {
        RP_SET_MODE("codex_responses");
        if (!*base_url) { free(base_url); base_url = strdup(RP_DEFAULT_XAI_OAUTH_BASE_URL); }
    } else if (strcmp(provider, "qwen-oauth") == 0) {
        RP_SET_MODE("chat_completions");
        if (!*base_url) { free(base_url); base_url = strdup(RP_DEFAULT_QWEN_BASE_URL); }
    } else if (strcmp(provider, "minimax-oauth") == 0) {
        /* MiniMax OAuth tokens are Anthropic-Messages-only; never honor a
         * stale model.api_mode from an OpenAI-compatible provider. */
        RP_SET_MODE("anthropic_messages");
        const catalog_provider_t *pc = provider_registry_get(provider);
        if (!*base_url && pc && pc->inference_base_url) {
            free(base_url);
            base_url = strdup(pc->inference_base_url);
        }
    } else if (strcmp(provider, "anthropic") == 0) {
        RP_SET_MODE("anthropic_messages");
        char *cfg_provider = rp_strip_lower(rp_str_field(mc, "provider"));
        char *cfg_base_url = strdup("");
        if (strcmp(cfg_provider, "anthropic") == 0) {
            free(cfg_base_url);
            cfg_base_url = rp_strip(rp_str_field(mc, "base_url"));
            bn = strlen(cfg_base_url);
            while (bn > 0 && cfg_base_url[bn - 1] == '/') cfg_base_url[--bn] = '\0';
            if (!provider_anthropic_base_url_override_ok(cfg_base_url)) {
                free(cfg_base_url);
                cfg_base_url = strdup("");
            }
        }
        free(cfg_provider);
        if (*cfg_base_url) { free(base_url); base_url = cfg_base_url; }
        else { free(cfg_base_url); if (!*base_url) { free(base_url); base_url = strdup(RP_ANTHROPIC_BASE_URL); } }
    } else if (strcmp(provider, "openrouter") == 0) {
        if (!*base_url) { free(base_url); base_url = strdup(RP_OPENROUTER_BASE_URL); }
    } else if (strcmp(provider, "xai") == 0) {
        RP_SET_MODE("codex_responses");
    } else if (strcmp(provider, "nous") == 0) {
        RP_SET_MODE(rp_nous_api_mode(effective_model));
        char *override = _nous_inference_base_url_override();
        if (*override) { free(base_url); base_url = override; }
        else free(override);
    } else if (strcmp(provider, "copilot") == 0) {
        {
            char *mode = _copilot_runtime_api_mode(mc, api_key, effective_model);
            free(api_mode); api_mode = mode;
        }
        if (!*base_url) { free(base_url); base_url = strdup(RP_COPILOT_INFERENCE_BASE_URL); }
    } else if (strcmp(provider, "azure-foundry") == 0) {
        char *cfg_provider = rp_strip_lower(rp_str_field(mc, "provider"));
        if (strcmp(cfg_provider, "azure-foundry") == 0) {
            char *cfg_base_url = rp_strip(rp_str_field(mc, "base_url"));
            bn = strlen(cfg_base_url);
            while (bn > 0 && cfg_base_url[bn - 1] == '/') cfg_base_url[--bn] = '\0';
            if (*cfg_base_url) { free(base_url); base_url = cfg_base_url; }
            else free(cfg_base_url);
            {
                const char *cmode = provider_parse_api_mode(rp_str_field(mc, "api_mode"));
                if (cmode) RP_SET_MODE(cmode);
            }
        }
        free(cfg_provider);
        /* Model-family inference: GPT-5.x/codex/o1-o4 are Responses-only. */
        if (*effective_model && strcmp(api_mode, "anthropic_messages") != 0) {
            char *inferred = azure_foundry_model_api_mode(effective_model);
            if (inferred && *inferred) RP_SET_MODE(inferred);
            free(inferred);
        }
        /* Anthropic-style endpoints: strip trailing /v1 (Anthropic SDK adds
         * its own /v1/messages). */
        if (strcmp(api_mode, "anthropic_messages") == 0) {
            bn = strlen(base_url);
            if (bn >= 3 && strcmp(base_url + bn - 3, "/v1") == 0) base_url[bn - 3] = '\0';
            else if (bn >= 4 && strcmp(base_url + bn - 4, "/v1/") == 0) base_url[bn - 4] = '\0';
        }
    } else {
        char *configured_provider = rp_strip_lower(rp_str_field(mc, "provider"));
        const catalog_provider_t *pc = provider_registry_get(provider);
        bool pool_url_is_default =
            pc && pc->inference_base_url &&
            strcmp(base_url, pc->inference_base_url) == 0;
        if (strcmp(configured_provider, provider) == 0 && pool_url_is_default) {
            char *cfg_base_url = rp_strip(rp_str_field(mc, "base_url"));
            bn = strlen(cfg_base_url);
            while (bn > 0 && cfg_base_url[bn - 1] == '/') cfg_base_url[--bn] = '\0';
            if (*cfg_base_url) { free(base_url); base_url = cfg_base_url; }
            else free(cfg_base_url);
        }
        const char *configured_mode = provider_parse_api_mode(rp_str_field(mc, "api_mode"));
        char *oc_family = models_opencode_provider_family2(provider);
        if (oc_family && *oc_family) {
            /* opencode serves both surfaces: re-derive from the model. */
            RP_SET_MODE(strncmp(effective_model, "anthropic/", 10) == 0
                            ? "anthropic_messages" : "chat_completions");
        } else if (configured_mode &&
                   provider_supports_explicit_api_mode(provider, configured_provider)) {
            RP_SET_MODE(configured_mode);
        } else {
            /* URL detection first, then the provider's declared transport. */
            char *fm = _fallback_api_mode(provider, base_url, effective_model,
                                          rp_str_field(mc, "api_mode"));
            free(api_mode);
            api_mode = fm;
        }
        free(oc_family);
        free(configured_provider);
    }

    /* Optional opt-in: route OpenAI/Codex turns through codex app-server. */
    {
        char *rewritten = _maybe_apply_codex_app_server_runtime(
            provider, api_mode, mc);
        free(api_mode);
        api_mode = rewritten;
    }

    json_t *out = json_object();
    if (!out) {
        free(base_url); free(api_key); free(effective_model);
        free(api_mode);
        if (mc_owned) json_free(mc);
        return NULL;
    }
    json_set(out, "provider", json_string(provider));
    json_set(out, "api_mode", json_string(api_mode));
    json_set(out, "base_url", json_string(base_url));
    json_set(out, "api_key", json_string(api_key));
    {
        const char *src = rp_str_field(entry_json, "source");
        json_set(out, "source", json_string(*src ? src : "pool"));
    }
    /* Python carries the live CredentialPool object here; the C runtime dict
     * records that a pool backs this entry (the pool itself is re-loaded from
     * the auth store by the consumer via load_pool()). */
    json_set(out, "credential_pool_backed", json_bool(true));
    json_set(out, "requested_provider",
             json_string(requested_provider ? requested_provider : ""));

#undef RP_SET_MODE
    free(api_mode);
    free(base_url);
    free(api_key);
    free(effective_model);
    if (mc_owned) json_free(mc);
    return out;
}

/*
 * Port of Python: _try_resolve_from_custom_pool
 * Resolve a credential-pool-backed runtime for a custom endpoint. Returns a
 * runtime JSON object, or NULL when no pool/key exists (Python None).
 */
/* PoP: _try_resolve_from_custom_pool @ hermes_cli/runtime_provider.py:_try_resolve_from_custom_pool */
json_t *_try_resolve_from_custom_pool(const char *base_url,
                                      const char *provider_label,
                                      const char *api_mode_override,
                                      const char *provider_name)
{
    if (!base_url || !*base_url) return NULL;
    const char *pool_key = get_custom_provider_pool_key(base_url, provider_name);
    if (!pool_key || !*pool_key) return NULL;
    credential_pool_t *pool = load_pool(pool_key);
    if (!pool || !credential_pool_has_available(pool)) {
        if (pool) credential_pool_free(pool);
        return NULL;
    }
    int idx = credential_pool_select(pool);
    if (idx < 0) { credential_pool_free(pool); return NULL; }
    const char *key = pool->entries[idx].api_key;
    char *pool_api_key = rp_strip(key);
    if (!*pool_api_key) {
        free(pool_api_key);
        pool_api_key = rp_strip(pool->entries[idx].access_token);
    }
    credential_pool_free(pool);
    if (!*pool_api_key) { free(pool_api_key); return NULL; }
    /* Legacy short/placeholder keys ('123', 'm', …) for local no-auth
     * services stay usable on loopback endpoints (#86864): every other
     * resolution path substitutes "no-key-required" there too. The C core's
     * has_usable_secret floor is enforced by the caller via auth_helpers;
     * loopback endpoints always take the placeholder when no real secret
     * resolved. */
    if (!auth_has_usable_secret(pool_api_key, 4)) {
        char *host = provider_base_url_hostname(base_url);
        bool loopback = provider_loopback_hostname(host);
        free(host);
        if (loopback) {
            free(pool_api_key);
            pool_api_key = strdup("no-key-required");
        }
    }
    json_t *out = json_object();
    if (!out) { free(pool_api_key); return NULL; }
    json_set(out, "provider", json_string(provider_label ? provider_label : "custom"));
    {
        const char *mode = provider_detect_api_mode_for_url(base_url);
        json_set(out, "api_mode",
                 json_string(api_mode_override && *api_mode_override
                                 ? api_mode_override
                                 : (mode ? mode : "chat_completions")));
    }
    json_set(out, "base_url", json_string(base_url));
    json_set(out, "api_key", json_string(pool_api_key));
    {
        char *src = NULL;
        asprintf(&src, "pool:%s", pool_key);
        json_set(out, "source", json_string(src ? src : "pool"));
        free(src);
    }
    json_set(out, "credential_pool_backed", json_bool(true));
    free(pool_api_key);
    return out;
}

/*
 * Port of Python: _lift_max_output_tokens
 * Propagate max_output_tokens / max_tokens (>0 int) from a custom-provider
 * entry onto the resolved runtime dict.
 */
/* PoP: _lift_max_output_tokens @ hermes_cli/runtime_provider.py:_lift_max_output_tokens */
void _lift_max_output_tokens(const json_t *entry, json_t *result)
{
    static const char *keys[] = {"max_output_tokens", "max_tokens"};
    for (size_t i = 0; i < 2; i++) {
        const json_t *v = entry ? json_obj_get(entry, keys[i]) : NULL;
        if (v && v->type == JSON_NUMBER && v->num_val > 0 &&
            v->num_val == (double)(long long)v->num_val) {
            json_set(result, "max_output_tokens",
                     json_number((double)(long long)v->num_val));
            return;
        }
    }
}

/*
 * Port of Python: _lift_extra_headers
 * Copy a validated extra_headers dict from a provider entry.
 * SECURITY: values carry credentials — never log them.
 */
/* PoP: _lift_extra_headers @ hermes_cli/runtime_provider.py:_lift_extra_headers */
void _lift_extra_headers(const json_t *entry, json_t *result)
{
    if (!entry || !result) return;
    const json_t *raw = json_obj_get(entry, "extra_headers");
    if (!raw || raw->type != JSON_OBJECT) return;
    json_t *headers = config_py_normalize_extra_headers(raw);
    if (headers) {
        if (headers->type == JSON_OBJECT && json_array_size(headers) > 0)
            json_set(result, "extra_headers", headers);   /* steals */
        else
            json_free(headers);
    }
}

/* custom_provider_aliases membership test (providers.py): requested matches
 * an entry when it equals the raw/normalized name or the custom:<slug> form
 * of either the display name or the provider key. */
static bool rp_alias_matches(const char *requested_norm,
                             const char *display_name,
                             const char *provider_key)
{
    const char *vals[2] = {display_name, provider_key};
    for (int i = 0; i < 2; i++) {
        char *norm = rp_strip_lower(vals[i]);
        if (!norm) continue;
        char *slug = NULL;
        asprintf(&slug, "custom:%s", norm);
        bool hit = (strcmp(requested_norm, norm) == 0) ||
                   (slug && strcmp(requested_norm, slug) == 0);
        free(norm);
        free(slug);
        if (hit) return true;
    }
    return false;
}

/*
 * Port of Python: _get_named_custom_provider
 * Scan config providers:/custom_providers: for an entry matching the
 * requested name. Returns a JSON object (name/base_url/api_key/model/…),
 * or NULL (Python None). Caller json_free's.
 */
/* PoP: _get_named_custom_provider @ hermes_cli/runtime_provider.py:_get_named_custom_provider */
json_t *_get_named_custom_provider(const char *requested_provider)
{
    char *requested_norm = provider_normalize_custom_name(
        requested_provider ? requested_provider : "");
    if (!requested_norm || !*requested_norm) { free(requested_norm); return NULL; }

    if (strcmp(requested_norm, "auto") == 0) { free(requested_norm); return NULL; }
    if (strcmp(requested_norm, "custom") != 0 &&
        strncmp(requested_norm, "custom:", 7) != 0) {
        /* Defer to the built-in only when the raw name IS the canonical
         * provider — alias matches belong to the user's custom entry. */
        char *canonical = provider_resolve_alias(requested_norm);
        if (canonical) {
            char *canon_norm = rp_strip_lower(canonical);
            free(canonical);
            if (canon_norm && strcmp(canon_norm, requested_norm) == 0) {
                free(canon_norm);
                free(requested_norm);
                return NULL;
            }
            free(canon_norm);
        }
    }

    json_t *config = rp_load_config();
    if (!config) { free(requested_norm); return NULL; }

    /* New-style providers: dict */
    const json_t *providers = json_obj_get(config, "providers");
    if (providers && providers->type == JSON_OBJECT) {
        size_t n = json_array_size(providers);
        for (size_t i = 0; i < n; i++) {
            const char *ep_name = json_object_get_key_at(providers, i);
            const json_t *entry = json_object_get_at(providers, i);
            if (!ep_name || !entry || entry->type != JSON_OBJECT) continue;
            if (!config_py_is_provider_enabled(entry)) continue;
            /* Resolve API key: key_env/api_key_env → inline api_key. */
            const char *key_env_raw = rp_str_field(entry, "key_env");
            if (!*key_env_raw) key_env_raw = rp_str_field(entry, "api_key_env");
            char *key_env = rp_strip(key_env_raw);
            char *resolved_api_key = strdup("");
            if (*key_env) {
                const char *v = getenv(key_env);
                free(resolved_api_key);
                resolved_api_key = rp_strip(v);
            }
            free(key_env);
            if (!*resolved_api_key) {
                free(resolved_api_key);
                resolved_api_key = rp_strip(rp_str_field(entry, "api_key"));
            }
            const char *display_name = rp_str_field(entry, "name");
            if (rp_alias_matches(requested_norm,
                                 *display_name ? display_name : ep_name,
                                 ep_name)) {
                const char *base_url = rp_str_field(entry, "api");
                if (!*base_url) base_url = rp_str_field(entry, "url");
                if (!*base_url) base_url = rp_str_field(entry, "base_url");
                if (*base_url) {
                    json_t *result = json_object();
                    if (result) {
                        json_set(result, "name",
                                 json_string(*display_name ? display_name : ep_name));
                        char *bu = rp_strip(base_url);
                        json_set(result, "base_url", json_string(bu));
                        free(bu);
                        json_set(result, "api_key", json_string(resolved_api_key));
                        json_set(result, "model",
                                 json_string(rp_str_field(entry, "default_model")));
                        const json_t *extra_body = json_obj_get(entry, "extra_body");
                        if (extra_body && extra_body->type == JSON_OBJECT)
                            json_set(result, "extra_body", json_copy(extra_body));
                        _lift_extra_headers(entry, result);
                        {
                            const char *key_cmd = rp_str_field(entry, "key_cmd");
                            if (*key_cmd) json_set(result, "key_cmd", json_string(key_cmd));
                        }
                        /* Accept both transport (v11→v12 migration) and the
                         * legacy api_mode spelling. */
                        const char *mode_raw = rp_str_field(entry, "api_mode");
                        if (!*mode_raw) mode_raw = rp_str_field(entry, "transport");
                        const char *mode = provider_parse_api_mode(mode_raw);
                        if (mode) json_set(result, "api_mode", json_string(mode));
                        _lift_max_output_tokens(entry, result);
                    }
                    free(resolved_api_key);
                    json_free(config);
                    free(requested_norm);
                    return result;
                }
            }
            free(resolved_api_key);
        }
    }

    /* Legacy custom_providers: must be a list; a dict is a config error. */
    const json_t *cps = json_obj_get(config, "custom_providers");
    if (cps && cps->type == JSON_OBJECT) {
        fprintf(stderr,
                "custom_providers in config.yaml is a dict, not a list. "
                "Each entry must be prefixed with '-' in YAML.\n");
        json_free(config);
        free(requested_norm);
        return NULL;
    }
    json_t *compatible = config_py_get_compatible_custom_providers(config);
    json_free(config);
    if (compatible && compatible->type == JSON_ARRAY) {
        size_t n = json_array_size(compatible);
        for (size_t i = 0; i < n; i++) {
            const json_t *entry = json_get(compatible, i);
            if (!entry || entry->type != JSON_OBJECT) continue;
            const char *name = rp_str_field(entry, "name");
            const char *base_url = rp_str_field(entry, "base_url");
            if (!*name || !*base_url) continue;
            const char *provider_key = rp_str_field(entry, "provider_key");
            if (!rp_alias_matches(requested_norm, name, provider_key)) continue;
            json_t *result = json_object();
            if (result) {
                char *nm = rp_strip(name);
                char *bu = rp_strip(base_url);
                char *ak = rp_strip(rp_str_field(entry, "api_key"));
                json_set(result, "name", json_string(nm));
                json_set(result, "base_url", json_string(bu));
                json_set(result, "api_key", json_string(ak));
                free(nm); free(bu); free(ak);
                const char *ke = rp_str_field(entry, "key_env");
                if (*ke) json_set(result, "key_env", json_string(ke));
                if (*provider_key)
                    json_set(result, "provider_key", json_string(provider_key));
                const json_t *extra_body = json_obj_get(entry, "extra_body");
                if (extra_body && extra_body->type == JSON_OBJECT)
                    json_set(result, "extra_body", json_copy(extra_body));
                _lift_extra_headers(entry, result);
                const char *mode = provider_parse_api_mode(rp_str_field(entry, "api_mode"));
                if (mode) json_set(result, "api_mode", json_string(mode));
                char *mn = rp_strip(rp_str_field(entry, "model"));
                if (*mn) json_set(result, "model", json_string(mn));
                free(mn);
                _lift_max_output_tokens(entry, result);
            }
            json_free(compatible);
            free(requested_norm);
            return result;
        }
    }
    if (compatible) json_free(compatible);
    free(requested_norm);
    return NULL;
}

/*
 * Port of Python: _custom_provider_request_overrides
 * {"extra_body": copy} when the entry carries a non-empty extra_body dict.
 */
/* PoP: _custom_provider_request_overrides @ hermes_cli/runtime_provider.py:_custom_provider_request_overrides */
json_t *_custom_provider_request_overrides(const json_t *custom_provider)
{
    const json_t *eb = custom_provider ? json_obj_get(custom_provider, "extra_body") : NULL;
    if (!eb || eb->type != JSON_OBJECT || json_array_size(eb) == 0)
        return json_object();   /* Python {} — caller tests emptiness */
    json_t *out = json_object();
    if (out) json_set(out, "extra_body", json_copy(eb));
    return out;
}

/* First candidate that passes has_usable_secret, else "". Candidates are
 * consumed (freed) either way. */
static char *rp_pick_usable(char **cands, size_t n)
{
    char *picked = strdup("");
    for (size_t i = 0; i < n; i++) {
        if (cands[i] && *cands[i] == '\0') { free(cands[i]); cands[i] = NULL; continue; }
        if (cands[i] && auth_has_usable_secret(cands[i], 4)) {
            free(picked);
            picked = cands[i];
            cands[i] = NULL;
            break;
        }
        free(cands[i]);
        cands[i] = NULL;
    }
    return picked;
}

/*
 * Port of Python: _resolve_named_custom_runtime
 * Bare ``custom`` + explicit base_url → direct-alias runtime (with the
 * #27132 alias-to-custom normalization and host-gated env key fallbacks);
 * otherwise the named custom_providers/providers entry path, with credential
 * pool priority and model/max_output_tokens/extra_headers propagation.
 * Returns a runtime JSON object or NULL (no match).
 */
/* PoP: _resolve_named_custom_runtime @ hermes_cli/runtime_provider.py:_resolve_named_custom_runtime */
json_t *_resolve_named_custom_runtime(const char *requested_provider,
                                      const char *explicit_api_key,
                                      const char *explicit_base_url,
                                      const char *target_model)
{
    char *requested_norm = rp_strip_lower(requested_provider);
    if (*requested_norm && strcmp(requested_norm, "custom") != 0) {
        char *canon = provider_resolve_alias(requested_norm);
        if (canon) {
            if (strcmp(canon, "custom") == 0) {
                free(requested_norm);
                requested_norm = canon;
            } else {
                free(canon);
            }
        }
    }

    char *expl_key = rp_strip(explicit_api_key);
    char *expl_url = rp_strip(explicit_base_url);
    size_t un = strlen(expl_url);
    while (un > 0 && expl_url[un - 1] == '/') expl_url[--un] = '\0';

    json_t *result = NULL;

    if (strcmp(requested_norm, "custom") == 0 && *expl_url) {
        json_t *pool_result = _try_resolve_from_custom_pool(expl_url, "custom", NULL, NULL);
        if (pool_result) {
            json_set(pool_result, "source", json_string("direct-alias"));
            result = pool_result;
            goto done;
        }
        bool da_is_openai = provider_base_url_host_matches(expl_url, "openai.com") ||
                            provider_base_url_host_matches(expl_url, "openai.azure.com");
        bool da_is_openrouter = provider_base_url_host_matches(expl_url, "openrouter.ai");
        /* Host-gated env key fallbacks (#28660): never leak OPENAI_API_KEY to
         * an unrelated endpoint. */
        char *env_oai = da_is_openai ? rp_getenv_stripped("OPENAI_API_KEY") : strdup("");
        char *env_or  = da_is_openrouter ? rp_getenv_stripped("OPENROUTER_API_KEY") : strdup("");
        char *derived = provider_host_derived_api_key(expl_url);
        char *cands[4] = {expl_key, env_oai, env_or, derived};
        char *api_key = rp_pick_usable(cands, 4);
        if (!*api_key) { free(api_key); api_key = strdup("no-key-required"); }
        result = json_object();
        if (result) {
            const char *mode = provider_detect_api_mode_for_url(expl_url);
            json_set(result, "provider", json_string("custom"));
            json_set(result, "api_mode",
                     json_string(mode ? mode : "chat_completions"));
            json_set(result, "base_url", json_string(expl_url));
            json_set(result, "api_key", json_string(api_key));
            json_set(result, "source", json_string("direct-alias"));
            json_set(result, "requested_provider",
                     json_string(requested_provider ? requested_provider : ""));
        }
        free(api_key);
        goto done;
    }

    {
        json_t *cp = _get_named_custom_provider(requested_provider);
        if (!cp) goto done;

        const char *cp_base = rp_str_field(cp, "base_url");
        char *base_url = rp_strip(*expl_url ? expl_url : cp_base);
        size_t bnl = strlen(base_url);
        while (bnl > 0 && base_url[bnl - 1] == '/') base_url[--bnl] = '\0';
        if (!*base_url) { free(base_url); json_free(cp); goto done; }

        /* Credential pool first — mirrors the bare-custom path. */
        {
            json_t *pool_result = _try_resolve_from_custom_pool(
                base_url, "custom", rp_str_field(cp, "api_mode"),
                rp_str_field(cp, "name"));
            if (pool_result) {
                const char *m = target_model && *target_model
                    ? target_model : rp_str_field(cp, "model");
                if (*m) json_set(pool_result, "model", json_string(m));
                const json_t *mot = json_obj_get(cp, "max_output_tokens");
                if (mot && mot->type == JSON_NUMBER)
                    json_set(pool_result, "max_output_tokens", json_copy(mot));
                {
                    json_t *ov = _custom_provider_request_overrides(cp);
                    if (ov && ov->type == JSON_OBJECT && json_array_size(ov) > 0) {
                        json_t *merged = json_obj_get(pool_result, "request_overrides");
                        if (merged && merged->type == JSON_OBJECT) {
                            /* existing overrides win nothing; new ones win */
                            size_t kn = json_array_size(ov);
                            for (size_t k = 0; k < kn; k++) {
                                json_set(merged, json_object_get_key_at(ov, k),
                                         json_copy(json_object_get_at(ov, k)));
                            }
                            json_free(ov);
                        } else {
                            if (merged) json_obj_del(pool_result, "request_overrides");
                            json_set(pool_result, "request_overrides", ov);   /* steals */
                        }
                    } else if (ov) json_free(ov);
                }
                const json_t *eh = json_obj_get(cp, "extra_headers");
                if (eh && eh->type == JSON_OBJECT)
                    json_set(pool_result, "extra_headers", json_copy(eh));
                json_free(cp);
                free(base_url);
                result = pool_result;
                goto done;
            }
        }

        bool cp_is_openai = provider_base_url_host_matches(base_url, "openai.com") ||
                            provider_base_url_host_matches(base_url, "openai.azure.com");
        bool cp_is_openrouter = provider_base_url_host_matches(base_url, "openrouter.ai");
        char *env_cp_ke = rp_getenv_stripped(rp_str_field(cp, "key_env"));
        char *env_oai = cp_is_openai ? rp_getenv_stripped("OPENAI_API_KEY") : strdup("");
        char *env_or = cp_is_openrouter ? rp_getenv_stripped("OPENROUTER_API_KEY") : strdup("");
        char *derived = provider_host_derived_api_key(base_url);
        char *cp_ak = rp_strip(rp_str_field(cp, "api_key"));
        char *cands[6] = {expl_key, cp_ak, env_cp_ke, env_oai, env_or, derived};
        char *api_key = rp_pick_usable(cands, 6);

        /* A key_cmd credential is minted per request rather than resolved
         * once; the C runtime dict carries it verbatim so the wire client's
         * command token source can invoke it per request. An explicit
         * --api-key still wins (escape hatch). */
        const char *key_cmd = rp_str_field(cp, "key_cmd");
        if (*key_cmd && !auth_has_usable_secret(expl_key, 4))
            ;   /* recorded on the result below */

        const char *mode_cfg = rp_str_field(cp, "api_mode");
        const char *detected = provider_detect_api_mode_for_url(base_url);
        result = json_object();
        if (result) {
            json_set(result, "provider", json_string("custom"));
            json_set(result, "api_mode",
                     json_string(*mode_cfg ? mode_cfg
                                           : (detected ? detected : "chat_completions")));
            json_set(result, "base_url", json_string(base_url));
            json_set(result, "api_key",
                     json_string(*api_key ? api_key : "no-key-required"));
            {
                char *src = NULL;
                const char *nm = rp_str_field(cp, "name");
                asprintf(&src, "custom_provider:%s",
                         *nm ? nm : (requested_provider ? requested_provider : "custom"));
                json_set(result, "source", json_string(src ? src : "custom_provider"));
                free(src);
            }
            if (target_model && *target_model) {
                json_set(result, "model", json_string(target_model));
            } else {
                const char *cm = rp_str_field(cp, "model");
                if (*cm) json_set(result, "model", json_string(cm));
            }
            const json_t *mot = json_obj_get(cp, "max_output_tokens");
            if (mot && mot->type == JSON_NUMBER)
                json_set(result, "max_output_tokens", json_copy(mot));
            const json_t *eh = json_obj_get(cp, "extra_headers");
            if (eh && eh->type == JSON_OBJECT)
                json_set(result, "extra_headers", json_copy(eh));
            {
                json_t *ov = _custom_provider_request_overrides(cp);
                if (ov && ov->type == JSON_OBJECT && json_array_size(ov) > 0)
                    json_set(result, "request_overrides", ov);   /* steals */
                else if (ov)
                    json_free(ov);
            }
            if (*key_cmd && !auth_has_usable_secret(expl_key, 4))
                json_set(result, "key_cmd", json_string(key_cmd));

            /* OpenCode family re-derivation: static api_mode 503s for
             * /v1/responses-only models (#85589). */
            char *oc_family = models_opencode_provider_family2(requested_provider);
            if ((!oc_family || !*oc_family)) {
                char *host = provider_base_url_hostname(base_url);
                if (host && strcasecmp(host, "opencode.ai") == 0) {
                    free(oc_family);
                    oc_family = strdup(strstr(base_url, "/zen/go") ? "opencode-go"
                                                                   : "opencode-zen");
                }
                free(host);
            }
            if (oc_family && *oc_family && !*mode_cfg) {
                char *eff = rp_strip(target_model);
                if (!*eff) { free(eff); eff = rp_strip(rp_str_field(cp, "model")); }
                if (!*eff) {
                    json_t *mc = rp_load_config();
                    if (mc) {
                        const json_t *mnode = json_obj_get(mc, "model");
                        free(eff);
                        eff = rp_strip(rp_str_field(mnode, "default"));
                        json_free(mc);
                    }
                }
                if (*eff) {
                    json_set(result, "api_mode",
                             json_string(strncmp(eff, "anthropic/", 10) == 0
                                             ? "anthropic_messages"
                                             : "chat_completions"));
                    /* normalize_opencode_base_url: strip /re-add the /v1
                     * suffix by API mode. */
                    {
                        const char *am = rp_str_field(result, "api_mode");
                        size_t bl = strlen(base_url);
                        char *nb = NULL;
                        if (strcmp(am, "anthropic_messages") == 0) {
                            if (bl >= 3 && strcmp(base_url + bl - 3, "/v1") == 0) {
                                nb = strndup(base_url, bl - 3);
                            }
                        } else if (!(bl >= 3 && strcmp(base_url + bl - 3, "/v1") == 0)) {
                            asprintf(&nb, "%s/v1", base_url);
                        }
                        if (nb) {
                            json_set(result, "base_url", json_string(nb));
                            free(nb);
                        }
                    }
                }
                free(eff);
            }
            free(oc_family);
        }
        free(api_key);
        free(base_url);
        json_free(cp);
    }

done:
    free(requested_norm);
    free(expl_key);
    free(expl_url);
    return result;
}

/*
 * Port of Python: _resolve_openrouter_runtime
 * OpenAI-compatible fallback resolver (openrouter or bare custom endpoint):
 * base_url precedence explicit > CUSTOM_BASE_URL > trusted config base_url >
 * OPENROUTER_BASE_URL > default; host-gated API-key selection (#28660,
 * GHSA-76xc-57q6-vm5m); credential pool for custom endpoints.
 */
/* PoP: _resolve_openrouter_runtime @ hermes_cli/runtime_provider.py:_resolve_openrouter_runtime */
json_t *_resolve_openrouter_runtime(const char *requested_provider,
                                    const char *explicit_api_key,
                                    const char *explicit_base_url)
{
    json_t *model_cfg = rp_load_config();
    const json_t *mc = NULL;
    if (model_cfg) {
        const json_t *mnode = json_obj_get(model_cfg, "model");
        if (mnode && mnode->type == JSON_OBJECT) mc = mnode;
    }
    const char *cfg_base_url_raw = rp_str_field(mc, "base_url");
    char *cfg_base_url = (mc && json_obj_get(mc, "base_url") &&
                          json_obj_get(mc, "base_url")->type == JSON_STRING)
                             ? rp_strip(cfg_base_url_raw) : strdup("");
    char *cfg_provider = mc ? rp_strip_lower(rp_str_field(mc, "provider")) : strdup("");
    char *cfg_api_key = strdup("");
    {
        static const char *keys[] = {"api_key", "api"};
        for (size_t i = 0; i < 2 && !*cfg_api_key; i++) {
            const char *v = rp_str_field(mc, keys[i]);
            if (*v) { free(cfg_api_key); cfg_api_key = rp_strip(v); }
        }
    }
    char *requested_norm = rp_strip_lower(requested_provider);
    /* #27132: aliases resolving to "custom" follow the same rules. */
    if (*requested_norm && strcmp(requested_norm, "custom") != 0) {
        char *canon = provider_resolve_alias(requested_norm);
        if (canon) {
            if (strcmp(canon, "custom") == 0) {
                free(requested_norm);
                requested_norm = canon;
            } else {
                free(canon);
            }
        }
    }

    char *env_or_base = rp_getenv_stripped("OPENROUTER_BASE_URL");
    char *env_custom_base = rp_getenv_stripped("CUSTOM_BASE_URL");
    char *expl_key = rp_strip(explicit_api_key);
    char *expl_url = rp_strip(explicit_base_url);
    size_t un = strlen(expl_url);
    while (un > 0 && expl_url[un - 1] == '/') expl_url[--un] = '\0';

    bool use_config_base_url = false;
    if (*cfg_base_url && !*expl_url) {
        if (strcmp(requested_norm, "auto") == 0) {
            if (!*cfg_provider || strcmp(cfg_provider, "auto") == 0)
                use_config_base_url = true;
        } else if (strcmp(requested_norm, "custom") == 0 &&
                   _config_base_url_trustworthy_for_bare_custom(cfg_base_url,
                                                                cfg_provider)) {
            use_config_base_url = true;
        }
    }

    char *base_url = strdup("");
    {
        const char *parts[5];
        parts[0] = *expl_url ? expl_url : NULL;
        parts[1] = *env_custom_base ? env_custom_base : NULL;
        parts[2] = (use_config_base_url && *cfg_base_url) ? cfg_base_url : NULL;
        parts[3] = *env_or_base ? env_or_base : NULL;
        parts[4] = RP_OPENROUTER_BASE_URL;
        for (int i = 0; i < 5; i++) {
            if (parts[i]) { free(base_url); base_url = strdup(parts[i]); break; }
        }
        size_t bn = strlen(base_url);
        while (bn > 0 && base_url[bn - 1] == '/') base_url[--bn] = '\0';
    }

    bool is_openrouter_url = provider_base_url_host_matches(base_url, "openrouter.ai");
    bool is_openrouter_context =
        is_openrouter_url ||
        (strcmp(requested_norm, "openrouter") == 0 && *env_or_base &&
         strcmp(base_url, env_or_base) == 0);

    char *api_key = NULL;
    if (is_openrouter_context) {
        char *k1 = rp_getenv_stripped("OPENROUTER_API_KEY");
        char *k2 = rp_getenv_stripped("OPENAI_API_KEY");
        char *cands[3] = {expl_key, k1, k2};
        api_key = rp_pick_usable(cands, 3);
    } else {
        bool ollama_url = provider_base_url_host_matches(base_url, "ollama.com");
        bool openai_url = provider_base_url_host_matches(base_url, "openai.com");
        bool openai_azure = provider_base_url_host_matches(base_url, "openai.azure.com");
        char *cfg_k = use_config_base_url ? strdup(cfg_api_key) : strdup("");
        char *k_ollama = ollama_url ? rp_getenv_stripped("OLLAMA_API_KEY") : strdup("");
        char *k_openai = (openai_url || openai_azure)
                             ? rp_getenv_stripped("OPENAI_API_KEY") : strdup("");
        char *k_or = is_openrouter_url ? rp_getenv_stripped("OPENROUTER_API_KEY")
                                       : strdup("");
        char *derived = provider_host_derived_api_key(base_url);
        char *cands[6] = {expl_key, cfg_k, k_ollama, k_openai, k_or, derived};
        api_key = rp_pick_usable(cands, 6);
    }

    const char *source =
        (*expl_key || *expl_url) ? "explicit" : "env/config";
    const char *effective_provider =
        strcmp(requested_norm, "custom") == 0 ? "custom" : "openrouter";

    if (strcmp(effective_provider, "custom") == 0 && *base_url) {
        json_t *pool_result = _try_resolve_from_custom_pool(
            base_url, effective_provider,
            provider_parse_api_mode(rp_str_field(mc, "api_mode")),
            strcmp(requested_norm, "custom") != 0 ? requested_provider : NULL);
        if (pool_result) {
            free(api_key); free(base_url);
            free(expl_url); free(expl_key); free(env_custom_base);
            free(env_or_base); free(cfg_base_url); free(cfg_provider);
            free(cfg_api_key); free(requested_norm);
            if (model_cfg) json_free(model_cfg);
            return pool_result;
        }
    }

    if (strcmp(effective_provider, "custom") == 0 && !*api_key &&
        !is_openrouter_url) {
        free(api_key);
        api_key = strdup("no-key-required");   /* OpenAI SDK needs non-empty */
    }

    json_t *out = json_object();
    if (out) {
        const char *mode = NULL;
        if (strcmp(effective_provider, "custom") == 0) {
            mode = NULL;   /* filled by _resolve_plain_custom_api_mode below */
        } else {
            mode = provider_parse_api_mode(rp_str_field(mc, "api_mode"));
            if (!mode) mode = provider_detect_api_mode_for_url(base_url);
        }
        json_set(out, "provider", json_string(effective_provider));
        if (strcmp(effective_provider, "custom") == 0) {
            char *pm = _resolve_plain_custom_api_mode(mc, base_url);
            json_set(out, "api_mode", json_string(pm));
            free(pm);
        } else {
            json_set(out, "api_mode",
                     json_string(mode ? mode : "chat_completions"));
        }
        json_set(out, "base_url", json_string(base_url));
        json_set(out, "api_key", json_string(api_key));
        json_set(out, "source", json_string(source));
    }
    free(api_key); free(base_url);
    free(expl_url); free(expl_key); free(env_custom_base);
    free(env_or_base); free(cfg_base_url); free(cfg_provider);
    free(cfg_api_key); free(requested_norm);
    if (model_cfg) json_free(model_cfg);
    return out;
}

/*
 * Port of Python: _resolve_azure_foundry_runtime
 * Azure Foundry: model.base_url + model.api_mode (+ entra_id auth), with
 * model-family Responses-API inference and /v1 stripping for Anthropic
 * endpoints. Missing credentials → {"auth_error": …} (Python AuthError).
 */
/* PoP: _resolve_azure_foundry_runtime @ hermes_cli/runtime_provider.py:_resolve_azure_foundry_runtime */
json_t *_resolve_azure_foundry_runtime(const char *requested_provider,
                                       const json_t *model_cfg,
                                       const char *explicit_api_key,
                                       const char *explicit_base_url,
                                       const char *target_model)
{
    char *expl_key = rp_strip(explicit_api_key);
    char *expl_url = rp_strip(explicit_base_url);
    size_t un = strlen(expl_url);
    while (un > 0 && expl_url[un - 1] == '/') expl_url[--un] = '\0';

    char *cfg_provider = model_cfg ? rp_strip_lower(rp_str_field(model_cfg, "provider"))
                                   : strdup("");
    char *cfg_base_url = strdup("");
    char *cfg_api_mode = strdup("chat_completions");
    char *cfg_auth_mode = strdup("api_key");
    const json_t *cfg_entra = NULL;
    if (strcmp(cfg_provider, "azure-foundry") == 0 && model_cfg) {
        free(cfg_base_url);
        cfg_base_url = rp_strip(rp_str_field(model_cfg, "base_url"));
        size_t bn = strlen(cfg_base_url);
        while (bn > 0 && cfg_base_url[bn - 1] == '/') cfg_base_url[--bn] = '\0';
        const char *cm = provider_parse_api_mode(rp_str_field(model_cfg, "api_mode"));
        if (cm) { free(cfg_api_mode); cfg_api_mode = strdup(cm); }
        const char *am = rp_str_field(model_cfg, "auth_mode");
        free(cfg_auth_mode);
        cfg_auth_mode = (*am) ? rp_strip_lower(am) : strdup("api_key");
        const json_t *en = json_obj_get(model_cfg, "entra");
        if (en && en->type == JSON_OBJECT) cfg_entra = en;
    }

    /* Model-family inference: GPT-5.x/codex/o1-o4 are Responses-only. */
    char *effective_model = rp_strip(target_model);
    if (!*effective_model && model_cfg)
        { free(effective_model); effective_model = rp_strip(rp_str_field(model_cfg, "default")); }
    if (*effective_model && strcmp(cfg_api_mode, "anthropic_messages") != 0) {
        char *inferred = azure_foundry_model_api_mode(effective_model);
        if (inferred && *inferred) { free(cfg_api_mode); cfg_api_mode = strdup(inferred); }
        free(inferred);
    }

    char *env_base = rp_getenv_stripped("AZURE_FOUNDRY_BASE_URL");
    size_t eb = strlen(env_base);
    while (eb > 0 && env_base[eb - 1] == '/') env_base[--eb] = '\0';

    char *base_url = strdup(*expl_url ? expl_url
                                      : (*cfg_base_url ? cfg_base_url : env_base));
    if (!*base_url) {
        json_t *err = json_object();
        if (err)
            json_set(err, "auth_error",
                     json_string("Azure Foundry requires a base URL. Set it via "
                                 "'hermes model' or the AZURE_FOUNDRY_BASE_URL "
                                 "environment variable."));
        free(base_url); free(env_base); free(effective_model);
        free(cfg_auth_mode); free(cfg_api_mode); free(cfg_base_url);
        free(cfg_provider); free(expl_url); free(expl_key);
        return err;
    }

    if (strcmp(cfg_api_mode, "anthropic_messages") == 0) {
        size_t bn = strlen(base_url);
        if (bn >= 3 && strcmp(base_url + bn - 3, "/v1") == 0) base_url[bn - 3] = '\0';
        else if (bn >= 4 && strcmp(base_url + bn - 4, "/v1/") == 0) base_url[bn - 4] = '\0';
    }

    json_t *out = json_object();
    if (!out) goto oom;

    if (strcmp(cfg_auth_mode, "entra_id") == 0) {
        if (*expl_key) {
            /* Explicit --api-key wins over config entra_id (escape hatch). */
            json_set(out, "auth_mode", json_string("api_key"));
            json_set(out, "source", json_string("explicit"));
        } else {
            /* Entra ID token minting lives in azure_identity_adapter.c; the
             * resolver records the auth contract and scope so the wire client
             * builds its per-request token provider. */
            const char *scope = cfg_entra ? rp_str_field(cfg_entra, "scope") : "";
            json_set(out, "auth_mode", json_string("entra_id"));
            json_set(out, "entra_scope",
                     json_string(*scope ? scope : "https://ai.azure.com/.default"));
            json_set(out, "source", json_string("entra_id"));
        }
    } else {
        char *api_key = strdup(*expl_key ? expl_key : "");
        if (!*api_key) {
            free(api_key);
            api_key = rp_getenv_stripped("AZURE_FOUNDRY_API_KEY");
        }
        if (!*api_key) {
            json_free(out);
            out = json_object();
            if (out)
                json_set(out, "auth_error",
                         json_string("Azure Foundry requires an API key. Set "
                                     "AZURE_FOUNDRY_API_KEY in ~/.hermes/.env or "
                                     "run 'hermes model' to configure. To use "
                                     "keyless Microsoft Entra ID auth instead, set "
                                     "model.auth_mode: entra_id in config.yaml."));
            goto done_err;
        }
        json_set(out, "api_key", json_string(api_key));
        json_set(out, "auth_mode", json_string("api_key"));
        json_set(out, "source",
                 json_string((*expl_key || *expl_url) ? "explicit" : "config"));
        free(api_key);
        goto finish;
    }

finish:
    /* api_key field for both the explicit-key and entra paths (entra leaves
     * it absent — minted per request by the adapter). */
    if (*expl_key && strcmp(cfg_auth_mode, "entra_id") == 0)
        json_set(out, "api_key", json_string(expl_key));
    json_set(out, "provider", json_string("azure-foundry"));
    json_set(out, "api_mode", json_string(cfg_api_mode));
    json_set(out, "base_url", json_string(base_url));
    {
        json_t *clean_entra = json_object();
        if (clean_entra) {
            if (cfg_entra) {
                const char *cs = rp_str_field(cfg_entra, "scope");
                if (*cs) json_set(clean_entra, "scope", json_string(cs));
            }
            json_set(out, "entra", clean_entra);   /* steals */
        }
    }
    if (!json_obj_get(out, "source"))
        json_set(out, "source", json_string("config"));
    json_set(out, "requested_provider",
             json_string(requested_provider ? requested_provider : ""));
    goto done_err;

oom:
    out = NULL;
done_err:
    free(base_url); free(env_base); free(effective_model);
    free(cfg_auth_mode); free(cfg_api_mode); free(cfg_base_url);
    free(cfg_provider); free(expl_url); free(expl_key);
    return out;
}

/*
 * Port of Python: _resolve_explicit_runtime
 * Runtime resolution when --api-key/--base-url were passed explicitly.
 * Returns a runtime JSON object, NULL when no explicit values apply, or an
 * {"auth_error": …} object where Python raises AuthError.
 */
/* PoP: _resolve_explicit_runtime @ hermes_cli/runtime_provider.py:_resolve_explicit_runtime */
json_t *_resolve_explicit_runtime(const char *provider,
                                  const char *requested_provider,
                                  const json_t *model_cfg,
                                  const char *explicit_api_key,
                                  const char *explicit_base_url,
                                  const char *target_model)
{
    char *expl_key = rp_strip(explicit_api_key);
    char *expl_url = rp_strip(explicit_base_url);
    size_t un = strlen(expl_url);
    while (un > 0 && expl_url[un - 1] == '/') expl_url[--un] = '\0';

    json_t *out = NULL;
    if (!*expl_key && !*expl_url) goto done;

    if (strcmp(provider, "anthropic") == 0) {
        char *cfg_provider = model_cfg
            ? rp_strip_lower(rp_str_field(model_cfg, "provider")) : strdup("");
        char *cfg_base_url = strdup("");
        if (strcmp(cfg_provider, "anthropic") == 0 && model_cfg) {
            free(cfg_base_url);
            cfg_base_url = rp_strip(rp_str_field(model_cfg, "base_url"));
            size_t cbn = strlen(cfg_base_url);
            while (cbn > 0 && cfg_base_url[cbn - 1] == '/') cfg_base_url[--cbn] = '\0';
            if (!provider_anthropic_base_url_override_ok(cfg_base_url)) {
                free(cfg_base_url);
                cfg_base_url = strdup("");
            }
        }
        free(cfg_provider);
        const char *burl = (*expl_url ? expl_url
                                      : (*cfg_base_url ? cfg_base_url
                                                       : RP_ANTHROPIC_BASE_URL));
        if (*expl_key) {
            out = json_object();
            if (out) {
                json_set(out, "provider", json_string("anthropic"));
                json_set(out, "api_mode", json_string("anthropic_messages"));
                json_set(out, "base_url", json_string(burl));
                json_set(out, "api_key", json_string(expl_key));
                json_set(out, "source", json_string("explicit"));
                json_set(out, "requested_provider",
                         json_string(requested_provider ? requested_provider : ""));
            }
        } else {
            /* resolve_anthropic_token() chain unavailable in this unit —
             * surface the same AuthError Python raises without credentials. */
            out = json_object();
            if (out)
                json_set(out, "auth_error",
                         json_string("No Anthropic credentials found. Set "
                                     "ANTHROPIC_TOKEN or ANTHROPIC_API_KEY, run "
                                     "'claude setup-token', or authenticate with "
                                     "'claude /login'."));
        }
        free(cfg_base_url);
        goto done;
    }

    if (strcmp(provider, "openai-codex") == 0) {
        const char *burl = *expl_url ? expl_url : RP_DEFAULT_CODEX_BASE_URL;
        out = json_object();
        if (out) {
            json_set(out, "provider", json_string("openai-codex"));
            json_set(out, "api_mode", json_string("codex_responses"));
            json_set(out, "base_url", json_string(burl));
            json_set(out, "api_key", json_string(expl_key));
            json_set(out, "source", json_string("explicit"));
            json_set(out, "requested_provider",
                     json_string(requested_provider ? requested_provider : ""));
        }
        goto done;
    }

    if (strcmp(provider, "nous") == 0) {
        char *override = _nous_inference_base_url_override();
        char *state_url = rp_getenv_stripped("NOUS_INFERENCE_URL");
        char *burl = strdup(*expl_url ? expl_url
                            : (*override ? override
                                         : ((*state_url) ? state_url
                                                         : "https://inference-api.nousresearch.com/v1")));
        size_t bn2 = strlen(burl);
        while (bn2 > 0 && burl[bn2 - 1] == '/') burl[--bn2] = '\0';
        if (*expl_key) {
            out = json_object();
            if (out) {
                json_set(out, "provider", json_string("nous"));
                json_set(out, "api_mode",
                         json_string(rp_nous_api_mode(
                             (target_model && *target_model)
                                 ? target_model
                                 : rp_str_field(model_cfg, "default"))));
                json_set(out, "base_url", json_string(burl));
                json_set(out, "api_key", json_string(expl_key));
                json_set(out, "source", json_string("explicit"));
                json_set(out, "requested_provider",
                         json_string(requested_provider ? requested_provider : ""));
            }
        } else {
            out = json_object();
            if (out)
                json_set(out, "auth_error",
                         json_string("No Nous credentials found. Run 'hermes "
                                     "auth login nous' to authenticate."));
        }
        free(burl); free(state_url); free(override);
        goto done;
    }

    if (strcmp(provider, "azure-foundry") == 0) {
        out = _resolve_azure_foundry_runtime(
            requested_provider, model_cfg, expl_key, expl_url, target_model);
        goto done;
    }

    {
        const catalog_provider_t *pc = provider_registry_get(provider);
        if (pc && strcmp(pc->auth_type, "api_key") == 0) {
            char *env_url = pc->base_url_env_var
                                ? rp_getenv_stripped(pc->base_url_env_var)
                                : strdup("");
            size_t eu = strlen(env_url);
            while (eu > 0 && env_url[eu - 1] == '/') env_url[--eu] = '\0';
            const char *burl;
            if (*expl_url) burl = expl_url;
            else if ((strcmp(provider, "kimi-coding") == 0 ||
                      strcmp(provider, "kimi-coding-cn") == 0))
                burl = "";   /* resolved from creds below */
            else burl = (*env_url && *env_url) ? env_url : pc->inference_base_url;

            if (!*expl_key) {
                /* Credential resolvers are env-backed in the C core. */
                const char *const *envs = pc->api_key_env_vars;
                char *resolved = strdup("");
                if (envs) {
                    for (int i = 0; envs[i]; i++) {
                        char *v = rp_getenv_stripped(envs[i]);
                        if (*v) { free(resolved); resolved = v; break; }
                        free(v);
                    }
                }
                out = json_object();
                if (out) {
                    if (!*resolved) {
                        json_set(out, "auth_error",
                                 json_string("No usable credentials found for "
                                             "explicit provider resolution."));
                    } else {
                        char *mode = NULL;
                        if (strcmp(provider, "copilot") == 0) {
                            mode = _copilot_runtime_api_mode(
                                model_cfg, resolved, target_model);
                        } else if (strcmp(provider, "xai") == 0 ||
                                   strcmp(provider, "actual") == 0) {
                            mode = strdup("codex_responses");
                        } else {
                            char *cp2 = model_cfg
                                ? rp_strip_lower(rp_str_field(model_cfg, "provider"))
                                : strdup("");
                            const char *cm = provider_parse_api_mode(
                                rp_str_field(model_cfg, "api_mode"));
                            if (cm && provider_supports_explicit_api_mode(provider, cp2))
                                mode = strdup(cm);
                            else
                                mode = _fallback_api_mode(
                                    provider, burl,
                                    (target_model && *target_model)
                                        ? target_model
                                        : rp_str_field(model_cfg, "default"),
                                    rp_str_field(model_cfg, "api_mode"));
                            free(cp2);
                        }
                        json_set(out, "provider", json_string(provider));
                        json_set(out, "api_mode", json_string(mode));
                        json_set(out, "base_url", json_string(burl));
                        json_set(out, "api_key", json_string(resolved));
                        json_set(out, "source", json_string("explicit"));
                        json_set(out, "requested_provider",
                                 json_string(requested_provider ? requested_provider : ""));
                        free(mode);
                    }
                }
                free(resolved);
            } else {
                char *mode = NULL;
                char *cp2 = model_cfg
                    ? rp_strip_lower(rp_str_field(model_cfg, "provider")) : strdup("");
                const char *cm = provider_parse_api_mode(
                    rp_str_field(model_cfg, "api_mode"));
                if (strcmp(provider, "copilot") == 0)
                    mode = _copilot_runtime_api_mode(model_cfg, expl_key, target_model);
                else if (strcmp(provider, "xai") == 0 || strcmp(provider, "actual") == 0)
                    mode = strdup("codex_responses");
                else if (cm && provider_supports_explicit_api_mode(provider, cp2))
                    mode = strdup(cm);
                else
                    mode = _fallback_api_mode(
                        provider, burl,
                        (target_model && *target_model)
                            ? target_model : rp_str_field(model_cfg, "default"),
                        rp_str_field(model_cfg, "api_mode"));
                free(cp2);
                out = json_object();
                if (out) {
                    json_set(out, "provider", json_string(provider));
                    json_set(out, "api_mode", json_string(mode));
                    json_set(out, "base_url", json_string(burl));
                    json_set(out, "api_key", json_string(expl_key));
                    json_set(out, "source", json_string("explicit"));
                    json_set(out, "requested_provider",
                             json_string(requested_provider ? requested_provider : ""));
                }
                free(mode);
            }
            free(env_url);
            goto done;
        }
    }

done:
    free(expl_key);
    free(expl_url);
    return out;
}

/*
 * Port of Python: resolve_runtime_provider
 * Top-level orchestration. Args may be NULL/""; returns a runtime JSON
 * object, or {"auth_error": …} / {"error": …} where Python raises.
 */
/* PoP: resolve_runtime_provider @ hermes_cli/runtime_provider.py:resolve_runtime_provider */
char *resolve_runtime_provider(const char *requested,
                               const char *explicit_api_key,
                               const char *explicit_base_url,
                               const char *target_model)
{
    char *requested_provider = runtime_provider_resolve_requested(requested);
    char *expl_key = rp_strip(explicit_api_key);
    char *expl_url = rp_strip(explicit_base_url);
    size_t un = strlen(expl_url);
    while (un > 0 && expl_url[un - 1] == '/') expl_url[--un] = '\0';

    json_t *full_cfg = rp_load_config();
    json_t *result = NULL;
    char *err_buf = NULL;

    /* Honour providers.<name>.enabled: false for every provider kind. */
    if (full_cfg) {
        const json_t *provs = json_obj_get(full_cfg, "providers");
        if (provs && provs->type == JSON_OBJECT) {
            const json_t *block = json_obj_get(provs, requested_provider);
            if (block && block->type == JSON_OBJECT &&
                !config_py_is_provider_enabled(block)) {
                asprintf(&err_buf,
                         "error: provider '%s' is disabled in config "
                         "(providers.%s.enabled: false)",
                         requested_provider, requested_provider);
                goto emit_error;
            }
        }
    }

    if (strcmp(requested_provider, "moa") == 0) {
        result = json_object();
        if (result) {
            json_set(result, "provider", json_string("moa"));
            json_set(result, "api_mode", json_string("chat_completions"));
            json_set(result, "base_url", json_string("moa://local"));
            json_set(result, "api_key", json_string("moa-virtual-provider"));
            json_set(result, "source", json_string("moa-virtual-provider"));
            json_set(result, "requested_provider", json_string(requested_provider));
        }
        goto emit;
    }

    /* Azure Anthropic short-circuit: explicit Azure endpoint with
     * provider=anthropic uses the Azure key directly. */
    if (strcmp(requested_provider, "anthropic") == 0 &&
        provider_base_url_host_matches(expl_url, "azure.com")) {
        char *az = expl_key && *expl_key ? strdup(expl_key)
                                          : rp_getenv_stripped("AZURE_ANTHROPIC_KEY");
        if (!*az) { free(az); az = rp_getenv_stripped("ANTHROPIC_API_KEY"); }
        result = json_object();
        if (result) {
            json_set(result, "provider", json_string("anthropic"));
            json_set(result, "api_mode", json_string("anthropic_messages"));
            json_set(result, "base_url", json_string(expl_url));
            json_set(result, "api_key", json_string(az));
            json_set(result, "source", json_string("azure-explicit"));
            json_set(result, "requested_provider", json_string(requested_provider));
        }
        free(az);
        goto emit;
    }

    /* Azure Foundry resolves before the custom/pool/generic paths so
     * model.base_url + model.api_mode always drive it. */
    if (strcmp(requested_provider, "azure-foundry") == 0) {
        json_t *mc = rp_load_config();
        json_t *mnode = NULL;
        const json_t *mn = mc ? json_obj_get(mc, "model") : NULL;
        if (mn && mn->type == JSON_OBJECT) mnode = json_copy(mn);
        if (mc) json_free(mc);
        result = _resolve_azure_foundry_runtime(
            requested_provider, mnode, expl_key, expl_url, target_model);
        if (mnode) json_free(mnode);
        goto emit;
    }

    /* Named custom runtime (covers bare custom + direct aliases + entries). */
    result = _resolve_named_custom_runtime(requested_provider, expl_key,
                                           expl_url, target_model);
    if (result) goto emit;

    /* auto/unset provider + non-cloud config base_url routes through the
     * OpenAI-compatible resolver directly (#3846): host matching keeps
     * look-alike URLs from leaking cloud credentials. */
    if (!*expl_url && !*expl_key) {
        json_t *mc_root = rp_load_config();
        const json_t *mn = mc_root ? json_obj_get(mc_root, "model") : NULL;
        char *cfg_provider = mn ? rp_strip_lower(rp_str_field(mn, "provider")) : strdup("");
        char *cfg_base_url = mn ? rp_strip(rp_str_field(mn, "base_url")) : strdup("");
        if (*cfg_base_url &&
            (strcmp(cfg_provider, "auto") == 0 || !*cfg_provider) &&
            !provider_base_url_host_matches(cfg_base_url, "openrouter.ai") &&
            !provider_base_url_host_matches(cfg_base_url, "anthropic.com") &&
            !provider_base_url_host_matches(cfg_base_url, "openai.com")) {
            result = _resolve_openrouter_runtime(requested_provider, expl_key,
                                                 expl_url);
        }
        free(cfg_provider);
        free(cfg_base_url);
        if (mc_root) json_free(mc_root);
        if (result) goto emit;
    }

    /* Generic fallback: the OpenAI-compatible resolver handles openrouter,
     * auto-detected env-key providers, and unknown requests — the same final
     * fallback Python takes after its OAuth-specific branches. */
    result = _resolve_openrouter_runtime(requested_provider, expl_key, expl_url);

emit:
    if (result) {
        if (requested_provider && !json_obj_get(result, "requested_provider"))
            json_set(result, "requested_provider", json_string(requested_provider));
        char *serialized = json_serialize(result);
        json_free(result);
        free(err_buf);
        free(requested_provider);
        free(expl_key);
        free(expl_url);
        if (full_cfg) json_free(full_cfg);
        return serialized;
    }
emit_error:
    {
        char *out = NULL;
        if (!err_buf) err_buf = strdup("error: runtime provider resolution failed");
        asprintf(&out, "{\"error\": \"%s\"}", err_buf);
        free(err_buf);
        free(requested_provider);
        free(expl_key);
        free(expl_url);
        if (full_cfg) json_free(full_cfg);
        return out;
    }
}

/* PoP: models_opencode_provider_family2 @ hermes_cli/models.py:opencode_provider_family
 * Resolve a provider id to its OpenCode family ("opencode-zen" / "opencode-go"
 * / "opencode-free"), or NULL. Case-insensitive; covers the built-in slugs and
 * custom providers whose name extends a family slug. Caller frees non-NULL. */
char *models_opencode_provider_family2(const char *provider_id)
{
    extern const char *model_normalize_provider(const char *provider);
    if (!provider_id) return NULL;
    /* raw = str(provider_id).strip().lower() */
    const char *b = provider_id;
    while (*b == ' ' || *b == '\t') b++;
    size_t n = strlen(b);
    while (n > 0 && (b[n-1]==' '||b[n-1]=='\t')) n--;
    if (n == 0) return NULL;
    char *raw = malloc(n + 1);
    if (!raw) return NULL;
    for (size_t k = 0; k < n; k++)
        raw[k] = (char)((b[k] >= 'A' && b[k] <= 'Z') ? b[k] + 32 : b[k]);
    raw[n] = '\0';

    char *result = NULL;
    const char *canonical = model_normalize_provider(provider_id);
    if (canonical &&
        (strcmp(canonical, "opencode-zen") == 0 ||
         strcmp(canonical, "opencode-go") == 0 ||
         strcmp(canonical, "opencode-free") == 0)) {
        result = strdup(canonical);
    } else if (strncmp(raw, "opencode-free", 13) == 0) {
        result = strdup("opencode-free");
    } else if (strncmp(raw, "opencode-go", 11) == 0) {
        result = strdup("opencode-go");
    } else if (strncmp(raw, "opencode-zen", 12) == 0) {
        result = strdup("opencode-zen");
    }
    free(raw);
    return result;
}
