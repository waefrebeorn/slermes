/*
 * agent_runtime_pure.c — Pure-logic cache-policy helpers ported from
 * agent/agent_runtime_helpers.py.  These have no async/loop/DB deps:
 *   cache_ttl_means_disabled
 *   prompt_caching_disabled_from_config
 *   blank_cache_policy_stub  (+ cache_policy_stub_t struct)
 *   _direct_native_anthropic_tool_cache_capability
 *
 * Reuses: port_config_py_helpers (config_py_load_config_readonly),
 *          libpath (provider_base_url_hostname).
 */

#define _POSIX_C_SOURCE 200809L
#include "agent_runtime_pure.h"
#include <port_config_py_helpers.h>  /* config_py_load_config_readonly */
#include <json.h>

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* provider_base_url_hostname is declared in port_provider_registry.c (libpath).
 * Forward-declare here so direct_native_* resolves correctly. */
extern char *provider_base_url_hostname(const char *base_url);

/* config_py_load_config_readonly is declared in port_config_py_helpers.h. */

/* ── cache_ttl_means_disabled ───────────────────────────────────────────── */
/* PoP: cache_ttl_means_disabled @ agent/agent_runtime_helpers.py:cache_ttl_means_disabled */
/*
 * Faithful port of the disable-synonym predicate.  `ttl` is inspected by VALUE
 * (Python `ttl in ("5m","1h")` / `ttl is False` / `ttl is None` / str compare),
 * so we mirror with explicit string/flag semantics.  The C caller passes a
 * (const char *ttl, bool is_bool, bool bool_val) triple to disambiguate the
 * Python None/False/str forms — callers that only hold a string pass
 * (ttl_str, false, false); absent -> treat as None.
 *
 * Returns true when `ttl` means caching is DISABLED.
 */
bool cache_ttl_means_disabled(const char *ttl_str, bool is_bool, bool bool_val)
{
    /* Python: if ttl in ("5m", "1h"): return False */
    if (ttl_str && (strcmp(ttl_str, "5m") == 0 || strcmp(ttl_str, "1h") == 0))
        return false;
    /* Python: if ttl is False or ttl is None: return True */
    if (is_bool && !bool_val)   /* ttl is False */
        return true;
    if (is_bool && bool_val)    /* ttl is True — not a disable synonym */
        return false;
    if (ttl_str == NULL && !is_bool)  /* ttl is None */
        return true;
    /* Python: return str(ttl).lower() in ("off","false","disabled","no","none") */
    if (ttl_str) {
        char lower[64];
        size_t n = strlen(ttl_str);
        if (n >= sizeof(lower)) n = sizeof(lower) - 1;
        for (size_t i = 0; i < n; i++)
            lower[i] = (char)tolower((unsigned char)ttl_str[i]);
        lower[n] = 0;
        if (strcmp(lower, "off") == 0 || strcmp(lower, "false") == 0 ||
            strcmp(lower, "disabled") == 0 || strcmp(lower, "no") == 0 ||
            strcmp(lower, "none") == 0)
            return true;
    }
    return false;
}

/* ── prompt_caching_disabled_from_config ─────────────────────────────────── */
/* PoP: prompt_caching_disabled_from_config @ agent/agent_runtime_helpers.py:prompt_caching_disabled_from_config */
/*
 * Reads prompt_caching.cache_ttl from the readonly config, defaults to "5m",
 * delegates to cache_ttl_means_disabled.  Any config error -> false (caching
 * enabled) matching Python's broad except -> return False.
 */
bool prompt_caching_disabled_from_config(void)
{
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return false;
    json_t *pc = json_obj_get(cfg, "prompt_caching");
    if (!pc || pc->type != JSON_OBJECT) {
        /* Python: (load_config_readonly().get("prompt_caching", {}) or {}).get("cache_ttl","5m") */
        json_free(cfg);
        return cache_ttl_means_disabled("5m", false, false);  /* default 5m -> not disabled */
    }
    json_t *ttl = json_obj_get(pc, "cache_ttl");
    const char *ttl_str = NULL;
    bool is_bool = false, bool_val = false;
    if (ttl) {
        if (ttl->type == JSON_STRING) ttl_str = ttl->str_val;
        else if (ttl->type == JSON_BOOL) { is_bool = true; bool_val = ttl->bool_val; }
        else if (ttl->type == JSON_NULL) { /* None */ }
        /* numbers/others: unknown -> not a disable (Python str(ttl).lower()
         * wouldn't match synonyms) */
    }
    json_free(cfg);
    if (ttl_str == NULL && !is_bool && !ttl)
        return cache_ttl_means_disabled("5m", false, false);  /* default 5m */
    return cache_ttl_means_disabled(ttl_str, is_bool, bool_val);
}

/* ── cache_policy_stub_t (SimpleNamespace) ────────────────────────────────── */
/* PoP: blank_cache_policy_stub @ agent/agent_runtime_helpers.py:blank_cache_policy_stub */

/* cache_policy_stub_t — opaque SimpleNamespace replacement. */
struct cache_policy_stub {
    char *provider;
    char *base_url;
    char *api_mode;
    char *model;
    int cache_disabled;
};

struct cache_policy_stub *blank_cache_policy_stub(bool cache_disabled)
{
    struct cache_policy_stub *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    /* provider/base_url/api_mode/model default to "" (empty string).
     * Python SimpleNamespace(provider="", base_url="", api_mode="", model="", _cache_disabled=bool(...)) */
    s->provider = strdup("");
    s->base_url = strdup("");
    s->api_mode = strdup("");
    s->model = strdup("");
    s->cache_disabled = cache_disabled ? 1 : 0;
    return s;
}

struct cache_policy_stub *blank_cache_policy_stub_default(void)
{
    /* Python: when cache_disabled is omitted, falls back to config. */
    return blank_cache_policy_stub(prompt_caching_disabled_from_config());
}

void cache_policy_stub_free(struct cache_policy_stub *s)
{
    if (!s) return;
    free((void*)s->provider);
    free((void*)s->base_url);
    free((void*)s->api_mode);
    free((void*)s->model);
    free(s);
}

/* ── _direct_native_anthropic_tool_cache_capability ─────────────────────── */
/* PoP: _direct_native_anthropic_tool_cache_capability @ agent/agent_runtime_helpers.py:_direct_native_anthropic_tool_cache_capability */
/*
 * Takes effective strings (Python resolves base_url/api_mode from the agent or
 * kwargs).  Caller passes the stub/agent's base_url + api_mode + overrides.
 */
bool direct_native_anthropic_tool_cache_capability(const char *provider,
                                                    const char *base_url,
                                                    const char *api_mode,
                                                    const char *model)
{
    /* Python:
     *   eff_base_url = base_url if base_url is not None else (agent.base_url or "")
     *   eff_api_mode = api_mode if api_mode is not None else (agent.api_mode or "")
     *   return eff_api_mode == "anthropic_messages" and base_url_hostname(eff_base_url) == "api.anthropic.com"
     */
    const char *eff_api_mode = api_mode ? api_mode : "";
    const char *eff_base_url = base_url ? base_url : "";
    if (strcmp(eff_api_mode, "anthropic_messages") != 0)
        return false;
    char *host = provider_base_url_hostname(eff_base_url);
    bool ok = host && strcmp(host, "api.anthropic.com") == 0;
    free(host);
    return ok;
}

/* ── cache_policy_stub accessors ─────────────────────────────────────────── */
const char *cache_policy_stub_provider(struct cache_policy_stub *s) { return s ? s->provider : NULL; }
const char *cache_policy_stub_base_url(struct cache_policy_stub *s) { return s ? s->base_url : NULL; }
const char *cache_policy_stub_api_mode(struct cache_policy_stub *s) { return s ? s->api_mode : NULL; }
const char *cache_policy_stub_model(struct cache_policy_stub *s) { return s ? s->model : NULL; }
bool cache_policy_stub_disabled(struct cache_policy_stub *s) { return s ? (s->cache_disabled != 0) : false; }

