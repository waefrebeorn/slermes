/*
 * port_cli_config_load.c — pure helpers from hermes_cli/cli_config_load.py.
 */

#include "port_cli_config_load.h"
#include "port_fast_mode.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

char *ccl_parse_service_tier_config(const char *raw)
{
    return fm_parse_service_tier(raw);
}

/* Deep copy a json node via serialize/reparse (libjson has no deep-copy API). */
static json_t *ccl_clone(const json_t *n)
{
    char *ser = json_serialize(n);
    json_t *copy;
    if (!ser) return NULL;
    copy = json_parse(ser, NULL);
    free(ser);
    return copy;
}

static void ccl_update_object(json_t *dst, json_t *src)
{
    size_t i;
    for (i = 0; i < src->c.count; i++) {
        if (src->c.keys && src->c.items)
            json_set(dst, src->c.keys[i], ccl_clone(src->c.items[i]));
    }
}

char *ccl_merge_file_config(const char *defaults_json, const char *file_json)
{
    json_t *def = defaults_json ? json_parse(defaults_json, NULL) : NULL;
    json_t *file = file_json ? json_parse(file_json, NULL) : NULL;
    char *out;

    if (!def) {
        if (file) json_free(file);
        return NULL;
    }
    if (!file) {
        out = json_serialize(def);
        json_free(def);
        return out;
    }

    /* --- model normalization --- */
    json_t *fm = json_obj_get(file, "model");
    json_t *dm = json_obj_get(def, "model");
    if (fm && fm->type == JSON_STRING) {
        /* defaults["model"]["default"] = file["model"] */
        if (dm && dm->type == JSON_OBJECT)
            json_set(dm, "default", json_string(fm->str_val ? fm->str_val : ""));
    } else if (fm && fm->type == JSON_OBJECT) {
        if (dm && dm->type == JSON_OBJECT)
            ccl_update_object(dm, fm);
        /* promote model.model -> model.default when default missing */
        if (fm && fm->c.keys && fm->c.items) {
            json_t *mn = json_obj_get(fm, "model");
            json_t *df = json_obj_get(fm, "default");
            if (mn && df == NULL && dm && dm->type == JSON_OBJECT)
                json_set(dm, "default", ccl_clone(mn));
        }
    }

    /* --- deep-merge sections --- */
    if (file->c.keys && file->c.items) {
        size_t i;
        for (i = 0; i < file->c.count; i++) {
            const char *key = file->c.keys[i];
            json_t *value = file->c.items[i];
            if (strcmp(key, "model") == 0)
                continue;
            json_t *d = json_obj_get(def, key);
            if (d && d->type == JSON_OBJECT) {
                if (value && value->type == JSON_OBJECT) {
                    ccl_update_object(d, value);
                } else if (value && value->type != JSON_NULL) {
                    json_set(def, key, ccl_clone(value));
                }
            } else {
                json_set(def, key, ccl_clone(value));
            }
        }
    }

    /* --- legacy max_turns --- */
    if (file->c.keys && file->c.items && json_has(file, "max_turns")) {
        json_t *agent_file = json_obj_get(file, "agent");
        int agent_has = 0;
        if (agent_file && agent_file->type == JSON_OBJECT) {
            json_t *am = json_obj_get(agent_file, "max_turns");
            if (am && am->type != JSON_NULL) agent_has = 1;
        }
        if (!agent_has) {
            json_t *da = json_obj_get(def, "agent");
            json_t *mt = json_obj_get(file, "max_turns");
            if (da && da->type == JSON_OBJECT && mt)
                json_set(da, "max_turns", ccl_clone(mt));
        }
    }

    out = json_serialize(def);
    json_free(def);
    json_free(file);
    return out;
}
