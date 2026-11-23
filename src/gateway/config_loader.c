/*
 * config_loader.c — C home for upstream Hermes gateway/config_loader.py.
 *
 * PoP: w8_configloade_847bbe_bridge_lookup @ gateway/config_loader.py:_bridge_lookup
 * PoP: w8_configloade_847bbe_quick_commands_ok @ gateway/config_loader.py:_quick_commands_ok
 * PoP: w8_configloade_847bbe_str_keyed @ gateway/config_loader.py:_str_keyed
 * PoP: w8_configloade_847bbe_platform_section @ gateway/config_loader.py:platform_section
 * PoP: w8_configloade_847bbe_snapshot_authored_extra @ gateway/config_loader.py:snapshot_authored_extra
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w8_configloade_847bbe.h"

static bool w8_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool w8_has(const json_t *o, const char *k) {
    return w8_is_obj(o) && json_obj_get(o, k) != NULL;
}

/* (found, value) for one key under the given YAML->gw_data fallback mode. */
json_t *w8_configloade_847bbe_bridge_lookup(const json_t *yaml_cfg,
        const json_t *gateway_section, const json_t *gw_data,
        const char *key, const char *mode) {
    bool nested = w8_is_obj(gateway_section);
    bool found = false;
    json_t *value = NULL; /* owned on return */

    if (!strcmp(mode, "presence") || !strcmp(mode, "gwdata")) {
        if (w8_has(yaml_cfg, key)) {
            value = json_copy(json_obj_get(yaml_cfg, key));
            found = true;
        } else if (nested && w8_has(gateway_section, key)
                   && (!strcmp(mode, "presence") || !w8_has(gw_data, key))) {
            value = json_copy(json_obj_get(gateway_section, key));
            found = true;
        }
        if (!found) value = json_null();
    } else if (!strcmp(mode, "nested")) {
        if (nested && w8_has(gateway_section, key)) {
            value = json_copy(json_obj_get(gateway_section, key));
            found = true;
        } else {
            value = json_null();
        }
    } else {
        /* "none" / "dict": value = yaml_cfg.get(key) */
        value = w8_has(yaml_cfg, key) ? json_copy(json_obj_get(yaml_cfg, key)) : json_null();
        if (!strcmp(mode, "none")) {
            if (value->type == JSON_NULL && nested && w8_has(gateway_section, key)) {
                json_free(value);
                value = json_copy(json_obj_get(gateway_section, key));
            }
            found = (value->type != JSON_NULL);
        } else { /* "dict" */
            if (value->type != JSON_OBJECT && nested && w8_has(gateway_section, key)) {
                json_free(value);
                value = json_copy(json_obj_get(gateway_section, key));
            }
            found = (value->type == JSON_OBJECT);
        }
    }

    json_t *out = json_array();
    json_append(out, json_bool(found));
    if (found && value) json_append(out, value);
    else { if (value) json_free(value); json_append(out, json_null()); }
    return out;
}

json_t *w8_configloade_847bbe_quick_commands_ok(const json_t *value) {
    /* Python logs a warning for non-dicts and returns False. */
    return json_bool(w8_is_obj(value));
}

json_t *w8_configloade_847bbe_str_keyed(const json_t *value) {
    /* JSON object keys are strings already; Python's {str(k): v} is a copy. */
    return value ? json_copy(value) : json_null();
}

json_t *w8_configloade_847bbe_platform_section(const json_t *yaml_cfg,
        const char *name, const json_t *gateway_platforms) {
    json_t *section = w8_has(yaml_cfg, name) ? json_obj_get(yaml_cfg, name) : NULL;
    bool toplevel = w8_is_obj(section);
    json_t *result = section; /* Python returns (section, toplevel) directly */
    if (!toplevel) {
        const json_t *srcs[2] = {
            gateway_platforms,
            w8_has(yaml_cfg, "platforms") ? json_obj_get(yaml_cfg, "platforms") : NULL
        };
        for (int i = 0; i < 2; i++) {
            if (w8_is_obj(srcs[i]) && w8_has(srcs[i], name)
                    && w8_is_obj(json_obj_get(srcs[i], name))) {
                result = json_obj_get(srcs[i], name);
                break;
            }
        }
        if (!result) result = section; /* may be NULL -> JSON null */
    }
    json_t *out = json_array();
    json_append(out, result ? json_copy(result) : json_null());
    json_append(out, json_bool(toplevel));
    return out;
}

json_t *w8_configloade_847bbe_snapshot_authored_extra(const json_t *platforms_data) {
    json_t *out = json_object();
    if (w8_is_obj(platforms_data)) {
        for (size_t i = 0; i < json_object_size(platforms_data); i++) {
            const char *name = json_object_get_key_at(platforms_data, i);
            json_t *plat = json_object_get_at(platforms_data, i);
            if (!name || !w8_is_obj(plat)) continue;
            json_t *extra = w8_has(plat, "extra") ? json_obj_get(plat, "extra") : NULL;
            json_set(out, name, w8_is_obj(extra) ? json_copy(extra) : json_object());
        }
    }
    return out;
}
