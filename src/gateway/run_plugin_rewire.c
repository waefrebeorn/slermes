/*
 * run_plugin_rewire.c — C home for upstream Hermes gateway/run_plugin_rewire.py.
 *
 * PoP: w11_runplugin_285a53_count @ gateway/run_plugin_rewire.py:_count_adapters
 * PoP: w11_runplugin_285a53_rewire_count @ gateway/run_plugin_rewire.py:GatewayPluginRewireMixin._rewire_plugin_handlers
 */
#include "noc_w11_runplugin_285a53.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

/* The adapter set selected by the live methods:
 *   profile_name is None -> runner.adapters or {}
 *   else                 -> (runner._profile_adapters or {}).get(profile_name) or {} */
static const struct json_t *select_adapters(const struct json_t *adapters,
                                             const struct json_t *profile_adapters,
                                             const char *profile_name) {
    if (profile_name == NULL) return adapters;
    if (!profile_adapters || profile_adapters->type != JSON_OBJECT) return NULL;
    const struct json_t *v = json_obj_get(profile_adapters, profile_name);
    if (!v || v->type == JSON_NULL) return NULL;
    if ((v->type == JSON_OBJECT || v->type == JSON_ARRAY) && v->c.count == 0) return NULL;
    return v;
}

static int node_len(const struct json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_OBJECT) return (int)json_object_size(v); /* json_len(object)==0! */
    if (v->type == JSON_ARRAY)  return (int)json_len(v);
    return 0;
}

int w11_runplugin_285a53_count(const struct json_t *adapters,
                               const struct json_t *profile_adapters,
                               const char *profile_name) {
    return node_len(select_adapters(adapters, profile_adapters, profile_name));
}

static int fail_contains(const struct json_t *fail, const char *name) {
    if (!fail || fail->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(fail); ++i) {
        const struct json_t *v = json_get(fail, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, name))
            return 1;
    }
    return 0;
}

int w11_runplugin_285a53_rewire_count(const struct json_t *adapters,
                                      const struct json_t *profile_adapters,
                                      const char *profile_name,
                                      const struct json_t *fail) {
    const struct json_t *a = select_adapters(adapters, profile_adapters, profile_name);
    if (!a || a->type != JSON_OBJECT || json_object_size(a) == 0) return 0;
    int count = 0;
    for (size_t i = 0; i < json_object_size(a); ++i) {
        const char *k = json_object_get_key_at(a, i);
        if (!k || fail_contains(fail, k)) continue;
        ++count;
    }
    return count;
}
