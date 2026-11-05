/*
 * port_aux_task_builder.c — pure builder (see header).
 */

#include "port_aux_task_builder.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int bat_in_list(const json_t *arr, const char *key)
{
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (i = 0; i < arr->c.count; i++) {
        const json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && it->str_val &&
            strcmp(it->str_val, key) == 0)
            return 1;
    }
    return 0;
}

static int bat_key_valid(const char *key)
{
    if (!key || !key[0]) return 0;
    for (const unsigned char *p = (const unsigned char *)key; *p; p++)
        if (!(isalnum(*p) || *p == '_')) return 0;
    return 1;
}

static int bat_in_registered(const json_t *reg, const char *key)
{
    if (!reg || reg->type != JSON_OBJECT) return 0;
    return json_obj_get(reg, key) != NULL;
}

static json_t *bat_clone(const json_t *n)
{
    char *ser = json_serialize(n);
    json_t *c;
    if (!ser) return NULL;
    c = json_parse(ser, NULL);
    free(ser);
    return c;
}

char *bat_aux_task_entry(const char *me, const char *owner_id, const char *key,
                         const char *display_name, const char *description,
                         const char *defaults_json, const char *inherit_from,
                         const char *registered_json, const char *builtin_keys_json)
{
    (void)me;
    const char *m = me ? me : "";
    json_t *defaults = NULL, *reg = NULL, *builtin = NULL;
    json_t *entry = NULL, *task_defaults = NULL;
    const char *inh = inherit_from;
    char *out = NULL;

    (void)m;

    if (!bat_key_valid(key)) return NULL; /* ValueError: invalid/empty key */

    builtin = builtin_keys_json ? json_parse(builtin_keys_json, NULL) : NULL;
    reg = registered_json ? json_parse(registered_json, NULL) : NULL;

    if (bat_in_list(builtin, key)) goto reject; /* reserved key */

    if (reg) {
        const json_t *existing = json_obj_get(reg, key);
        if (existing != NULL && existing->type == JSON_OBJECT) {
            const json_t *pl = json_obj_get(existing, "plugin");
            if (!pl || pl->type != JSON_STRING || !pl->str_val ||
                strcmp(pl->str_val, owner_id ? owner_id : "") != 0)
                goto reject; /* registered by a different plugin */
        }
    }

    defaults = defaults_json ? json_parse(defaults_json, NULL) : NULL;

    /* inherit_from reset: self-reference, or neither built-in nor registered. */
    if (inh != NULL &&
        (strcmp(inh, key) == 0 ||
         (!bat_in_list(builtin, inh) && !bat_in_registered(reg, inh))))
        inh = NULL;

    if (inh != NULL) {
        /* dict(defaults or {}) */
        task_defaults = defaults ? bat_clone(defaults) : json_object();
    } else {
        /* {"provider":"auto","model":"","base_url":"","api_key":"","timeout":60,
         *  "extra_body":{}, **(defaults or {})} */
        json_t *base = json_object();
        json_set(base, "provider", json_string("auto"));
        json_set(base, "model", json_string(""));
        json_set(base, "base_url", json_string(""));
        json_set(base, "api_key", json_string(""));
        json_set(base, "timeout", json_int(60));
        json_set(base, "extra_body", json_object());
        if (defaults && defaults->type == JSON_OBJECT) {
            size_t i;
            for (i = 0; i < defaults->c.count; i++) {
                if (defaults->c.keys && defaults->c.items)
                    json_set(base, defaults->c.keys[i],
                             bat_clone(defaults->c.items[i]));
            }
        }
        task_defaults = base;
    }

    entry = json_object();
    json_set(entry, "key", json_string(key));
    json_set(entry, "display_name",
             json_string(display_name ? display_name : ""));
    json_set(entry, "description",
             json_string(description ? description : ""));
    json_set(entry, "defaults", task_defaults);
    task_defaults = NULL; /* owned */
    json_set(entry, "inherit_from", inh ? json_string(inh) : json_null());
    json_set(entry, "plugin", json_string(owner_id ? owner_id : ""));
    json_set(entry, "plugin_key", json_string(owner_id ? owner_id : ""));

    out = json_serialize(entry);

reject:
    if (task_defaults) json_free(task_defaults);
    if (entry) json_free(entry);
    if (defaults) json_free(defaults);
    if (reg) json_free(reg);
    if (builtin) json_free(builtin);
    return out;
}
