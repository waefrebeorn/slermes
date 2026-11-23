/*
 * side_question_request.c — C port of upstream Hermes agent/side_question_request.py.
 *
 * PoP: w14_sidequesti_c37fa3_has_server_side_tools @ agent/side_question_request.py:has_server_side_tools
 * PoP: w14_sidequesti_c37fa3_runtime_key @ agent/side_question_request.py:runtime_key
 *
 * The two request-shape helpers are pure: has_server_side_tools mirrors the
 * frozensets of client tool types / routing-only OpenRouter plugins, and
 * runtime_key mirrors tuple(getattr(agent, n, None) for n in (...)).
 */
#include "noc_w14_sidequesti_c37fa3.h"
#include "hermes_json.h"

#include <string.h>

static int is_client_tool_type(const char *s) {
    return s && ((!strcmp(s, "function")) || (!strcmp(s, "custom")));
}
static int is_routing_only_plugin(const char *s) {
    return s && !strcmp(s, "pareto-router");
}

/* True when a json node is truthy under Python rules (for our JSON inputs). */
static int py_truthy(const struct json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && *v->str_val;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default:          return 0;
    }
}

/* Iterate a python-iterable-shaped JSON value: arrays and object values are
 * the only shapes used by the live code's tools[]/plugins[] lists. */
static size_t seq_len(const struct json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count;
    return 0;
}
static const struct json_t *seq_at(const struct json_t *v, size_t i) {
    if (!v || i >= seq_len(v)) return NULL;
    if (v->type == JSON_OBJECT) return v->c.items ? v->c.items[i] : NULL;
    return v->c.items ? v->c.items[i] : NULL;
}

int w14_sidequesti_c37fa3_has_server_side_tools(const struct json_t *api_kwargs) {
    const struct json_t *extra = api_kwargs ? json_obj_get(api_kwargs, "extra_body") : NULL;
    const struct json_t *tools = api_kwargs ? json_obj_get(api_kwargs, "tools") : NULL;
    const struct json_t *plugins = api_kwargs ? json_obj_get(api_kwargs, "plugins") : NULL;
    const struct json_t *etools = NULL, *eplugins = NULL;
    size_t i, n;

    if (!extra || extra->type != JSON_OBJECT) extra = NULL;
    if (extra) {
        etools = json_obj_get(extra, "tools");
        eplugins = json_obj_get(extra, "plugins");
    }

    for (int pass = 0; pass < 2; pass++) {
        const struct json_t *seq = pass == 0 ? tools : etools;
        n = seq_len(seq);
        for (i = 0; i < n; i++) {
            const struct json_t *tool = seq_at(seq, i);
            if (!tool || tool->type != JSON_OBJECT) continue;
            const struct json_t *tv = json_obj_get(tool, "type");
            if (!tv || !py_truthy(tv)) continue;
            if (tv->type == JSON_STRING) {
                if (!is_client_tool_type(tv->str_val)) return 1;
            } else {
                return 1;
            }
        }
    }

    if ((api_kwargs && json_has(api_kwargs, "web_search_options")) ||
        (extra && json_has(extra, "web_search_options")))
        return 1;

    for (int pass = 0; pass < 2; pass++) {
        const struct json_t *seq = pass == 0 ? plugins : eplugins;
        n = seq_len(seq);
        for (i = 0; i < n; i++) {
            const struct json_t *p = seq_at(seq, i);
            int routing = 0;
            if (p && p->type == JSON_OBJECT) {
                const struct json_t *idv = json_obj_get(p, "id");
                if (idv && idv->type == JSON_STRING && is_routing_only_plugin(idv->str_val))
                    routing = 1;
            }
            if (!routing) return 1;
        }
    }
    return 0;
}

struct json_t *w14_sidequesti_c37fa3_runtime_key(const struct json_t *agent) {
    static const char *const names[5] = {
        "model", "provider", "base_url", "api_mode", "reasoning_config"
    };
    struct json_t *out = json_array();
    if (!out) return NULL;
    for (int i = 0; i < 5; i++) {
        const struct json_t *v = agent ? json_obj_get(agent, names[i]) : NULL;
        json_append(out, v ? json_copy(v) : json_null());
    }
    return out;
}
