/*
 * auxiliary_async_rebuild.c - C home for upstream Hermes agent/auxiliary_async_rebuild.py.
 * PoP: w16_auxiliarya_3c2054_async_key @ agent/auxiliary_async_rebuild.py:async_api_key
 * PoP: w16_auxiliarya_3c2054_headers @ agent/auxiliary_async_rebuild.py:configured_default_headers
 *
 * Faithful ports of the two credential-carry decisions. The live client is
 * represented in fixtures by a spec object {is_openai, api_key, provider,
 * headers}; the C mirrors the exact branch logic of each Python function.
 * A callable _api_key_provider (spec "provider" present on an OpenAI client)
 * is awaited by the oracle and yields str(provider_value) -- mirrored here as
 * the provider value coerced to a string. No real OpenAI SDK is linked.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_auxiliarya_3c2054.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static json_t *jcopy(const json_t *v) { return v ? json_copy(v) : json_null(); }
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dupstr("None");
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val);
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupstr(buf);
        default: return dupstr("");
    }
}

struct json_t *w16_auxiliarya_3c2054_async_key(const struct json_t *client) {
    int is_openai = json_get_bool(client, "is_openai", 0) ? 1 : 0;
    json_t *prov = json_obj_get(client, "provider");
    json_t *api = json_obj_get(client, "api_key");
    /* provider = getattr(client,'_api_key_provider',None) if isinstance(client,OpenAI) else None
     * if not callable(provider): return client.api_key ; else return str(await provider()) */
    if (is_openai && prov && prov->type != JSON_NULL) {
        char *s = py_str(prov);
        json_t *out = json_string(s ? s : "");
        free(s);
        return out;
    }
    return jcopy(api);
}

struct json_t *w16_auxiliarya_3c2054_headers(const struct json_t *client) {
    int is_openai = json_get_bool(client, "is_openai", 0) ? 1 : 0;
    json_t *configured = json_obj_get(client, "headers");
    /* configured = getattr(client,'_custom_headers',None) if isinstance(client,OpenAI) else None
     * return dict(configured) if isinstance(configured, Mapping) else {} */
    if (is_openai && configured && configured->type == JSON_OBJECT)
        return json_copy(configured);
    return json_object();
}
