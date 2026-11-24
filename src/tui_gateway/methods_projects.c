/*
 * methods_projects.c - C home for upstream Hermes tui_gateway/methods_projects.py.
 * PoP: w16_methodspro_3121a4_pick @ tui_gateway/methods_projects.py:_pick
 * PoP: w16_methodspro_3121a4_messaging_sources @ tui_gateway/methods_projects.py:_MESSAGING_SESSION_SOURCES
 * PoP: w16_methodspro_3121a4_tree_excluded_sources @ tui_gateway/methods_projects.py:_PROJECT_TREE_EXCLUDED_SOURCES
 * PoP: w16_methodspro_3121a4_error_codes @ tui_gateway/methods_projects.py:_E_PROJECTS/_E_NO_PROJECT/_E_PROJECT_ARG
 *
 * Faithful ports of the module's pure param picker and its static policy
 * tables. The registered projects RPCs own live projects.db state and stay
 * with the live Python owner.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w16_methodspro_3121a4.h"

static json_t *jcopy(const json_t *v) { return v ? json_copy(v) : json_null(); }

struct json_t *w16_methodspro_3121a4_pick(const struct json_t *params,
        const struct json_t *keys) {
    json_t *out = json_object();
    size_t n = (keys && keys->type == JSON_ARRAY) ? keys->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *kv = json_get(keys, i);
        const char *k = (kv && kv->type == JSON_STRING) ? kv->str_val : NULL;
        if (!k) continue;
        /* {k: params.get(k) for k in keys} -- missing keys become null */
        json_set(out, k, jcopy(json_obj_get(params, k)));
    }
    return out;
}

static const char *const MESSAGING_SOURCES[] = {
    "telegram", "discord", "slack", "mattermost", "matrix", "signal", "whatsapp",
    "bluebubbles", "photon", "homeassistant", "email", "sms", "webhook", "api_server",
    "weixin", "wecom", "qqbot", "yuanbao", "dingtalk", "feishu"
};

struct json_t *w16_methodspro_3121a4_messaging_sources(void) {
    json_t *out = json_array();
    for (size_t i = 0; i < sizeof(MESSAGING_SOURCES) / sizeof(MESSAGING_SOURCES[0]); i++)
        json_append(out, json_string(MESSAGING_SOURCES[i]));
    return out;
}

struct json_t *w16_methodspro_3121a4_tree_excluded_sources(void) {
    static const char *const head[] = {"cron", "kanban", "oneshot"};
    json_t *out = json_array();
    for (size_t i = 0; i < sizeof(head) / sizeof(head[0]); i++)
        json_append(out, json_string(head[i]));
    for (size_t i = 0; i < sizeof(MESSAGING_SOURCES) / sizeof(MESSAGING_SOURCES[0]); i++)
        json_append(out, json_string(MESSAGING_SOURCES[i]));
    return out;
}

struct json_t *w16_methodspro_3121a4_error_codes(void) {
    json_t *out = json_array();
    json_append(out, json_number(5061.0));  /* _E_PROJECTS */
    json_append(out, json_number(5062.0));  /* _E_NO_PROJECT */
    json_append(out, json_number(5063.0));  /* _E_PROJECT_ARG */
    return out;
}
