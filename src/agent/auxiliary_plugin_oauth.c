/*
 * auxiliary_plugin_oauth.c — C home for upstream Hermes agent/auxiliary_plugin_oauth.py.
 *
 * PoP: w15_auxiliaryp_293888_configured_endpoint @ agent/auxiliary_plugin_oauth.py:_configured_endpoint
 * PoP: w15_auxiliaryp_293888_resolve @ agent/auxiliary_plugin_oauth.py:resolve_plugin_oauth_client
 *
 * Every transport/credential helper in the Python module is an import inside
 * the function; the oracle injects each helper's result so the C port can
 * reproduce the module's own control flow (key choice, base-url chain,
 * client-or-none, model choice) exactly and deterministically.
 */
#include "noc_w15_auxiliaryp_293888.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}
static int truthy(const char *s) { return s && s[0] ? 1 : 0; }

static char *py_strip(const char *s) {
    const char *a = s ? s : "";
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    char *out = (char *)malloc((size_t)(b - a) + 1);
    if (out) { memcpy(out, a, (size_t)(b - a)); out[b - a] = '\0'; }
    return out;
}
static void rstrip_slashes(char *s) {
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '/') s[--n] = '\0';
}

json_t *w15_auxiliaryp_293888_configured_endpoint(const char *config_base_url,
                                                   const char *profile_inference_base_url) {
    const char *v = truthy(config_base_url) ? config_base_url
                                            : (profile_inference_base_url ? profile_inference_base_url : "");
    return json_string(v);
}

json_t *w15_auxiliaryp_293888_resolve(const struct json_t *req,
                                      const char *normalized_api_key,
                                      int pool_selected,
                                      const char *pool_runtime_key,
                                      const char *pool_runtime_base_url,
                                      const char *configured_endpoint,
                                      int profile_client_available,
                                      const char *model_candidate,
                                      const char *normalized_model) {
    (void)model_candidate; /* resolved upstream by _get_aux_model_for_provider */
    const json_t *r = (const json_t *)req;
    const char *provider = json_get_str(r, "provider", "");
    const char *explicit_base = json_get_str(r, "explicit_base_url", "");

    /* api_key: explicit normalized key wins; else the pooled runtime key. */
    const char *key = truthy(normalized_api_key) ? normalized_api_key
                                                 : (pool_selected ? (pool_runtime_key ? pool_runtime_key : "") : "");

    /* base_url: first truthy of (explicit, pool runtime, configured), then strip/rstrip. */
    const char *raw;
    if (truthy(explicit_base)) raw = explicit_base;
    else if (!truthy(normalized_api_key) && pool_selected && truthy(pool_runtime_base_url))
        raw = pool_runtime_base_url;
    else raw = configured_endpoint ? configured_endpoint : "";
    char *base = py_strip(raw);
    rstrip_slashes(base);

    json_t *result = json_array();
    if (!truthy(key) || !profile_client_available) {
        json_append(result, json_null());
        json_append(result, json_null());
        free(base);
        return result;
    }
    json_t *client = json_object();
    json_set(client, "provider", json_string(provider));
    json_set(client, "api_key", json_string(key));
    json_set(client, "base_url", json_string(base ? base : ""));
    json_append(result, client);
    json_append(result, json_string(normalized_model ? normalized_model : ""));
    free(base);
    return result;
}
