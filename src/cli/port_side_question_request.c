/*
 * port_side_question_request.c — Port of Python: agent/side_question_request.py
 * (pure request-shape checks).
 */

#include "port_side_question_request.h"
#include "hermes_strutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* OpenRouter request plugins that only pick the model; anything else executes. */
static const char *const ROUTING_ONLY_PLUGINS[] = { "pareto-router" };
/* Transports rewrite client tools; only these types stay client-side. */
static const char *const CLIENT_TOOL_TYPES[] = { "function", "custom" };



/* PoP: sq_runtime_key @ agent/side_question_request.py:runtime_key */
json_t *sq_runtime_key(const char *model, int has_provider, const char *provider,
                       const char *base_url, const char *api_mode,
                       const char *reasoning_config_json)
{
    json_t *key = json_array();
    if (!key) return NULL;
    json_append(key, model ? json_string(model) : json_null());
    json_append(key, (has_provider && provider) ? json_string(provider) : json_null());
    json_append(key, base_url ? json_string(base_url) : json_null());
    json_append(key, api_mode ? json_string(api_mode) : json_null());
    json_append(key, reasoning_config_json ? json_parse(reasoning_config_json, NULL)
                                           : json_null());
    return key;
}

/* Iterate a JSON value that Python would treat as a sequence (`x or ()`). */
static json_t *seq(json_t *node)
{
    return (node && node->type == JSON_ARRAY) ? node : NULL;
}

/* PoP: sq_has_server_side_tools @ agent/side_question_request.py:has_server_side_tools */
int sq_has_server_side_tools(const char *api_kwargs_json)
{
    json_t *kwargs = api_kwargs_json ? json_parse(api_kwargs_json, NULL) : NULL;
    if (!kwargs || kwargs->type != JSON_OBJECT) {
        if (kwargs) json_free(kwargs);
        return 0;
    }

    json_t *extra = json_obj_get(kwargs, "extra_body");
    if (!extra || extra->type != JSON_OBJECT) extra = NULL;

    json_t *sources[2] = { seq(json_obj_get(kwargs, "tools")),
                           extra ? seq(json_obj_get(extra, "tools")) : NULL };
    for (int s = 0; s < 2; s++) {
        json_t *list = sources[s];
        if (!list) continue;
        for (size_t i = 0; i < list->c.count; i++) {
            json_t *tool = list->c.items[i];
            if (!tool || tool->type != JSON_OBJECT) continue;
            const char *type = json_get_str(tool, "type", NULL);
            if (type && *type &&
                !str_member_of(type, CLIENT_TOOL_TYPES,
                        sizeof(CLIENT_TOOL_TYPES) / sizeof(CLIENT_TOOL_TYPES[0]))) {
                json_free(kwargs);
                return 1;
            }
        }
    }

    if (json_obj_get(kwargs, "web_search_options") ||
        (extra && json_obj_get(extra, "web_search_options"))) {
        json_free(kwargs);
        return 1;
    }

    json_t *plugins[2] = { seq(json_obj_get(kwargs, "plugins")),
                           extra ? seq(json_obj_get(extra, "plugins")) : NULL };
    for (int s = 0; s < 2; s++) {
        json_t *list = plugins[s];
        if (!list) continue;
        for (size_t i = 0; i < list->c.count; i++) {
            json_t *p = list->c.items[i];
            const char *id = (p && p->type == JSON_OBJECT)
                                 ? json_get_str(p, "id", NULL) : NULL;
            if (!(p && p->type == JSON_OBJECT) ||
                !str_member_of(id, ROUTING_ONLY_PLUGINS,
                        sizeof(ROUTING_ONLY_PLUGINS) / sizeof(ROUTING_ONLY_PLUGINS[0]))) {
                json_free(kwargs);
                return 1;
            }
        }
    }
    json_free(kwargs);
    return 0;
}
