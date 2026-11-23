/*
 * config_env.c — faithful C11 port of the pure helpers in upstream
 * gateway/config_env.py.
 *
 * That module applies environment-variable overrides to the gateway config.
 * The parts that are pure data transforms -- the loader-layer projection
 * (_only_keys, _platform_layers) and the deterministic platform ordering
 * (_platform_rank) -- are reproduced exactly here. The env/runtime step
 * functions require live config/plugin objects and are intentionally not
 * claimed as PoP exports.
 *
 * PoP: cfgwenv_only_keys       @ gateway/config_env.py:_only_keys
 * PoP: cfgwenv_platform_layers @ gateway/config_env.py:_platform_layers
 * PoP: cfgwenv_platform_rank   @ gateway/config_env.py:_platform_rank
 */
#include "hermes_json.h"
#include <string.h>
#include "noc_cfgwenv.h"

static int name_in(const json_t *names, const char *key) {
    if (!names || names->type != JSON_ARRAY || !key) return 0;
    for (size_t i = 0; i < names->c.count; i++) {
        json_t *v = names->c.items[i];
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, key)) return 1;
    }
    return 0;
}

/* if not isinstance(section, dict): return {}
 * return {key: copy.deepcopy(value) for key,value in section.items() if key in names}
 * (JSON values are immutable-by-value here, so json_copy == deepcopy). */
static json_t *only_keys_new(const json_t *section, const json_t *names) {
    json_t *out = json_object();
    if (!section || section->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < section->c.count; i++) {
        const char *k = section->c.keys[i];
        if (k && name_in(names, k))
            json_set(out, k, json_copy(section->c.items[i]));
    }
    return out;
}

json_t *cfgwenv_only_keys(const json_t *section, const json_t *names) {
    return only_keys_new(section, names);
}

/* **spread of a dict into a literal: json_set replaces an existing key in place
 * while preserving its insertion position — identical to Python's dict merge. */
static void merge_set(json_t *dst, const json_t *src) {
    if (!src || src->type != JSON_OBJECT) return;
    for (size_t i = 0; i < src->c.count; i++)
        json_set(dst, src->c.keys[i], json_copy(src->c.items[i]));
}

static const json_t *as_object_or_null(const json_t *v) {
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

struct json_t *cfgwenv_platform_layers(const struct json_t *yaml_cfg,
                                       const struct json_t *names) {
    /* gateway_section = yaml_cfg.get("gateway") if isinstance(..., dict) else {} */
    const json_t *gs = as_object_or_null(json_obj_get(yaml_cfg, "gateway"));

    /* top = {"platforms": _only_keys(yaml_cfg.get("platforms"), names),
     *         **_only_keys(yaml_cfg, names)} */
    json_t *top = json_object();
    json_set(top, "platforms",
             only_keys_new(json_obj_get(yaml_cfg, "platforms"), names));
    json_t *spread = only_keys_new(yaml_cfg, names);
    merge_set(top, spread);
    json_free(spread);

    /* nested = {"platforms": _only_keys(gateway_section.get("platforms"), names),
     *            **_only_keys(gateway_section, names)} */
    json_t *nested = json_object();
    json_set(nested, "platforms",
             only_keys_new(json_obj_get(gs, "platforms"), names));
    spread = only_keys_new(gs, names);
    merge_set(nested, spread);
    json_free(spread);

    json_t *result = json_array();
    json_append(result, top);
    json_append(result, nested);
    return result;
}

struct json_t *cfgwenv_platform_rank(const char *name, const struct json_t *members) {
    size_t n = (members && members->type == JSON_ARRAY) ? members->c.count : 0;
    long long rank = (long long)n;
    if (name) {
        for (size_t i = 0; i < n; i++) {
            json_t *v = members->c.items[i];
            if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, name)) {
                rank = (long long)i;
                break;
            }
        }
    }
    return json_number((double)rank);
}
