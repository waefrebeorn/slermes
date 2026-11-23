/*
 * session_info_route.c — faithful C11 port of upstream gateway/session_info_route.py.
 *
 * The module resolves the channel override a chat turn runs on for display
 * (/new banner, /footer preview). Its runtime collaborators -- the config,
 * source carrier, gateway.run resolvers and hermes_cli provider-identity helper
 * -- are injected through the seam parameters, leaving the module's own decision
 * logic fully reproduced here.
 *
 * PoP: sessinfo_route @ gateway/session_info_route.py:channel_override_route
 */
#include "hermes_json.h"
#include <string.h>
#include "noc_sessinfo.h"

/* Python `x or None` for a string field: "" / NULL -> NULL, else the string. */
static const char *str_or_none(const json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    if (!v || v->type == JSON_NULL || v->type != JSON_STRING || !v->str_val || !v->str_val[0])
        return NULL;
    return v->str_val;
}

/* Python `dict.get(key)` where the value may be anything: return a borrowed node. */
static json_t *field(const json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    return (v && v->type == JSON_NULL) ? NULL : v;
}

struct json_t *sessinfo_route(const json_t *source, int config_present,
                              const json_t *override, const json_t *runtime,
                              int runtime_raises, const json_t *identity) {
    json_t *result = json_array();
    json_append(result, json_null());
    json_append(result, json_null());
    json_append(result, json_null());

    /* if source is None or config is None: return None, None, None */
    if (!source || source->type == JSON_NULL || !config_present)
        return result;

    /* ch = _get_channel_override(...); if ch is None: return triple None */
    if (!override || override->type == JSON_NULL)
        return result;

    /* model = ch.model or None */
    const char *model = str_or_none(override, "model");

    /* if not ch.provider: return model, None, None */
    const char *ch_provider = str_or_none(override, "provider");
    if (!ch_provider) {
        json_array_set(result, 0, model ? json_string(model) : json_null());
        return result;
    }

    /* resolver raising: return model, None, ch.provider
     * (the fixture's runtime_raises models ANY Exception from the resolver). */
    if (runtime_raises) {
        json_array_set(result, 0, model ? json_string(model) : json_null());
        json_array_set(result, 2, json_string(ch_provider));
        return result;
    }

    /* provider = session_provider_identity(...) or ch.provider */
    const char *ident = (identity && identity->type == JSON_STRING && identity->str_val &&
                         identity->str_val[0]) ? identity->str_val : NULL;
    const char *provider = ident ? ident : ch_provider;

    /* route = {"provider": provider, "base_url": runtime.get("base_url") or "",
     *          "api_key": runtime.get("api_key"), "_runtime_resolved": True} */
    const char *base_url = str_or_none(runtime, "base_url"); /* "" or NULL -> "" */
    json_t *route = json_object();
    json_set(route, "provider", json_string(provider));
    json_set(route, "base_url", json_string(base_url ? base_url : ""));
    json_t *api_key = field(runtime, "api_key");
    json_set(route, "api_key", api_key ? json_copy(api_key) : json_null());
    json_set(route, "_runtime_resolved", json_bool(true));

    /* return model or runtime.get("model") or None, route, None */
    const char *ret_model = model;
    if (!ret_model) ret_model = str_or_none(runtime, "model");
    json_array_set(result, 0, ret_model ? json_string(ret_model) : json_null());
    json_array_set(result, 1, route);
    return result;
}
