/*
 * blueprint_catalog.c — canonical C11 port of cron/blueprint_catalog.py.
 * Blueprint form schemas, slash commands, deeplinks, and value templating.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: blueprint_form_schema @ cron/blueprint_catalog.py:blueprint_form_schema */
json_t *blueprint_form_schema(json_t *blueprint) {
    if (!blueprint) return json_object();
    json_t *schema = json_object();
    json_set(schema, "key", json_copy(json_obj_get(blueprint, "key")));
    json_set(schema, "title", json_copy(json_obj_get(blueprint, "title")));
    json_set(schema, "description", json_copy(json_obj_get(blueprint, "description")));
    json_set(schema, "category", json_copy(json_obj_get(blueprint, "category")));
    json_set(schema, "tags", json_copy(json_obj_get(blueprint, "tags")));
    json_t *slots = json_obj_get(blueprint, "slots");
    json_t *fields = json_array();
    if (slots) {
        size_t n = json_len(slots);
        for (size_t i = 0; i < n; i++) {
            json_t *s = json_get(slots, i);
            json_t *field = json_object();
            json_set(field, "name", json_copy(json_obj_get(s, "name")));
            json_set(field, "type", json_copy(json_obj_get(s, "type")));
            json_set(field, "label", json_copy(json_obj_get(s, "label")));
            json_set(field, "default", json_copy(json_obj_get(s, "default")));
            json_set(field, "options", json_copy(json_obj_get(s, "options")));
            json_set(field, "optional", json_copy(json_obj_get(s, "optional")));
            json_set(field, "strict", json_copy(json_obj_get(s, "strict")));
            json_set(field, "help", json_copy(json_obj_get(s, "help")));
            json_append(fields, field);
        }
    }
    json_set(schema, "fields", fields);
    return schema;
}

/* ─── blueprint_slash_command(blueprint, values=None) -> str ─── */
/* PoP: blueprint_slash_command @ cron/blueprint_catalog.py:blueprint_slash_command */
json_t *blueprint_slash_command(json_t *blueprint, json_t *values) {
    if (!blueprint) return json_string("");
    const char *key = json_get_str(blueprint, "key", "");
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "/blueprint %s", key);
    json_t *slots = json_obj_get(blueprint, "slots");
    if (slots) {
        size_t n = json_len(slots);
        for (size_t i = 0; i < n; i++) {
            json_t *s = json_get(slots, i);
            const char *name = json_get_str(s, "name", "");
            json_t *val = values ? json_obj_get(values, name) : NULL;
            json_t *defval = json_obj_get(s, "default");
            const char *sval = NULL;
            char buf[256] = {0};
            if (val && json_is_string(val)) sval = json_get_str(val, "", "");
            else if (val && json_is_number(val)) {
                snprintf(buf, sizeof(buf), "%g", json_get_num(val, "", 0));
                sval = buf;
            } else if (defval && json_is_string(defval))
                sval = json_get_str(defval, "", "");
            if (!sval || !sval[0]) {
                int optional = json_get_bool(s, "optional", false);
                if (optional) continue;
                sval = "";
            }
            size_t len = strlen(cmd);
            if (strlen(sval) > 0)
                snprintf(cmd + len, sizeof(cmd) - len, " %s=%s", name, sval);
            else
                snprintf(cmd + len, sizeof(cmd) - len, " %s=", name);
        }
    }
    return json_string(cmd);
}

/* ─── blueprint_deeplink(blueprint, values=None) -> str ─── */
/* PoP: blueprint_deeplink @ cron/blueprint_catalog.py:blueprint_deeplink */
json_t *blueprint_deeplink(json_t *blueprint, json_t *values) {
    if (!blueprint) return json_string("");
    const char *key = json_get_str(blueprint, "key", "");
    char url[4096];
    snprintf(url, sizeof(url), "hermes://blueprint/%s", key);
    /* Add query params for slot values */
    json_t *slots = json_obj_get(blueprint, "slots");
    int first = 1;
    if (slots) {
        size_t n = json_len(slots);
        for (size_t i = 0; i < n; i++) {
            json_t *s = json_get(slots, i);
            const char *name = json_get_str(s, "name", "");
            json_t *val = values ? json_obj_get(values, name) : NULL;
            if (!val) val = json_obj_get(s, "default");
            if (!val) continue;
            const char *sval = NULL;
            char buf[256] = {0};
            if (json_is_string(val)) sval = json_get_str(val, "", "");
            else if (json_is_number(val)) {
                snprintf(buf, sizeof(buf), "%g", json_get_num(val, "", 0));
                sval = buf;
            }
            if (!sval || !sval[0]) continue;
            size_t len = strlen(url);
            if (first)
                snprintf(url + len, sizeof(url) - len, "?%s=%s", name, sval);
            else
                snprintf(url + len, sizeof(url) - len, "&%s=%s", name, sval);
            first = 0;
        }
    }
    return json_string(url);
}

/* ─── fill_blueprint(blueprint, values) -> dict ─── */
/* PoP: fill_blueprint @ cron/blueprint_catalog.py:fill_blueprint */
json_t *fill_blueprint(json_t *blueprint, json_t *values) {
    /* Validate values against slots and return create_job kwargs */
    json_t *result = json_object();
    if (!blueprint) return result;
    json_t *slots = json_obj_get(blueprint, "slots");
    if (slots) {
        size_t n = json_len(slots);
        for (size_t i = 0; i < n; i++) {
            json_t *s = json_get(slots, i);
            const char *name = json_get_str(s, "name", "");
            int optional = json_get_bool(s, "optional", false);
            json_t *val = values ? json_obj_get(values, name) : NULL;
            if (!val) {
                if (optional) continue;
                /* Missing required slot — would raise BlueprintFillError in Python */
                json_set(result, "error", json_string(name));
                return result;
            }
            char buf[256] = {0};
            if (json_is_string(val))
                snprintf(buf, sizeof(buf), "%s", json_get_str(val, "", ""));
            else if (json_is_number(val))
                snprintf(buf, sizeof(buf), "%g", json_get_num(val, "", 0));
            json_set(result, name, json_string(buf));
        }
    }
    return result;
}
